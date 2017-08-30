from trex_stl_lib.api import *
from .trex_stl_service import STLService, STLServiceFilter
from .trex_stl_service_int import STLServiceCtx, simpy, TXBuffer
import time
from collections import deque
from scapy.all import *
from scapy.contrib.capwap import *
from trex_openssl import *
import threading
import sys
import time
import base64

'''
FSMs for AP:
   * Discover WLC
   * Establish DTLS session
   * Join WLC
   * Add client (station)
   * Shutdown DTLS session
   * Maintenance (arp, ping, capwap echo request, fetches rx and dispatches to rx_buffer of APs)
'''



class STLServiceBufferedCtx(STLServiceCtx):
    ''' Same as parent, but does not use capture to get packets, uses AP's rx_buffer '''
    def _run(self, services):
        self._reset()
        self._add(services)
        if len(self.filters) > 1:
            raise Exception('Services here should have one common filter per AP')

        self.filter = list(self.filters.values())[0]['inst']
        if not hasattr(self.filter, 'services_per_ap'):
            raise Exception('Services here should have filter with attribute services_per_ap, got %s, type: %s' % (self.filter, type(self.filter)))

        # create an environment
        self.env          = simpy.rt.RealtimeEnvironment(factor = 1, strict = False)
        self.tx_buffer    = TXBuffer(self.env, self.client, self.port)

        # create processes
        for service in self.services:
            pipe = self._pipe()
            self.services[service]['pipe'] = pipe
            p = self.env.process(service.run(pipe))
            self._on_process_create(p)

        try:
            tick_process = self.env.process(self._tick_process())
            self.env.run(until = tick_process)
        finally:
            self._reset()


    def _tick_process (self):
        while True:

                self.tx_buffer.send_all()
    
                for ap, services in self.filter.services_per_ap.items():
                    for _ in range(len(ap.rx_buffer)):
                        try:
                            scapy_pkt = ap.rx_buffer.popleft()
                        except IndexError:
                            break
                        for service in services:
                            self.services[service]['pipe']._on_rx_pkt(scapy_pkt, None)
    
                # if no other process exists - exit
                if self.active_services == 0:
                    return
                else:
                    # backoff
                    yield self.env.timeout(0.05)


'''
Just assign services to AP, it will get packets from AP's rx_buffer
'''
class STLServiceFilterPerAp(STLServiceFilter):
    def __init__(self):
        self.services_per_ap = {}

    def add(self, service):
        if service.ap in self.services_per_ap:
            self.services_per_ap[service.ap].append(service)
        else:
            self.services_per_ap[service.ap] = [service]


'''
Used to fetch RX packets for all APs
Decrypts them if possible
Sends echo request (control keep alive)
Answers to async config changes
Does not use SimPy
'''
class STLServiceApBgMaintenance:
    bpf_filter = ('arp or (ip and (icmp or udp src port 5246 or '  # arp, ping, capwap control
                + '(udp src port 5247 and (udp[11] & 8 == 8 or '   # capwap data keep-alive
                + 'udp[16:2] == 16 or '                            # client assoc. resp
                + 'udp[48:2] == 2054 or '                          # client arp
                + '(udp[48:2] == 2048 and udp[59] == 1)))))')      # client ping

    ARP_ETHTYPE = b'\x08\x06'
    IP_ETHTYPE  = b'\x08\x00'
    ICMP_PROTO = b'\x01'
    UDP_PROTO = b'\x11'
    CAPWAP_CTRL_PORT = b'\x14\x7e'
    CAPWAP_DATA_PORT = b'\x14\x7f'
    WLAN_ASSOC_RESP  = b'\x00\x10'
    ARP_REQ = b'\x00\x01'
    ARP_REP = b'\x00\x02'
    ICMP_REQ = b'\x08'

    def __init__(self, ap_mngr, port_id):
        self.ap_mngr = ap_mngr
        self.port_id = port_id
        self.ap_per_ip = {}
        self.client_per_ip = {}
        self.bg_client = self.ap_mngr.bg_client
        self.port = self.bg_client.ports[port_id]
        self.capture_id = None
        self.bg_thread = None

################
#     API      #
################


    def run(self):
        self.bg_thread = threading.Thread(target = self.main_loop_wrapper)
        self.bg_thread.name = 'BG Thread (port %s)' % self.port_id
        self.bg_thread.daemon = True
        self.bg_thread.start()


    def is_running(self):
        return self.bg_thread and self.bg_thread.is_alive()


    def stop(self):
        try:
            self.bg_client.stop_capture(self.capture_id)
        except:
            pass
        self.capture_id = None


##################
#    INTERNAL    #
##################


    def log(self, msg, level = LoggerApi.VERBOSE_REGULAR):
        if not msg.startswith('(WLC) '):
            msg = '(WLC) %s' % msg
        self.ap_mngr.trex_client.logger.async_log('\n' + bold(msg), level)


    def err(self, msg):
        self.log(msg, LoggerApi.VERBOSE_QUIET)


    def fatal(self, msg):
        self.err(msg)
        self.stop()


    def send(self, pkts):
        assert type(pkts) is list
        push_pkts = [{'binary': base64.b64encode(bytes(p) if isinstance(p, Ether) else p).decode(),
                      'use_port_dst_mac': False,
                      'use_port_src_mac': False} for p in pkts]
        rc = self.port.push_packets(push_pkts, False)
        #if not rc:
        #    self.err(rc.err())


    def recv(self):
        pkts = []
        self.bg_client.fetch_capture_packets(self.capture_id, pkts, 1000)
        if len(pkts) > 995:
            self.err('Too much packets in rx queue (%s)' % len(pkts))
        return pkts


    def shutdown_ap(self, ap):
        try:
            for client in ap.clients:
                client.disconnect()
            if ap.is_dtls_established():
                with ap.ssl_lock:
                    libssl.SSL_shutdown(ap.ssl)
                tx_pkt = ap.wrap_capwap_pkt(b'\1\0\0\0' + ap.ssl_read())
                self.send([tx_pkt])
        finally:
            ap.reset_vars()


    def main_loop_wrapper(self):
        err_msg = ''
        try:
            self.capture_id = self.bg_client.start_capture(rx_ports = self.port_id, bpf_filter = self.bpf_filter, limit = 5000)['id']
            self.main_loop()
        except KeyboardInterrupt:
            pass
        except Exception as e:
            if self.capture_id: # if no id -> got stop()
                if not isinstance(e, STLError):
                    import traceback
                    traceback.print_exc()
                err_msg = ' (Exception: %s)' % e
        finally:
            try:
                self.bg_client.stop_capture(self.capture_id)
            except:
                pass
            if self.port_id in self.ap_mngr.service_ctx:
                if self.ap_per_ip:
                    self.err('Background thread on port %s died%s. Disconnecting APs.' % (self.port_id, err_msg))
                else:
                    self.err('Background thread on port %s died%s.' % (self.port_id, err_msg))
                for ap in self.ap_per_ip.values():
                    self.shutdown_ap(ap)


    def handle_ap_arp(self, rx_bytes, send_pkts):
        ip = rx_bytes[38:42]
        if ip not in self.ap_per_ip: # check IP
            return
        ap = self.ap_per_ip[ip]
        dst_mac = rx_bytes[:6]
        if dst_mac not in (b'\xff\xff\xff\xff\xff\xff', ap.mac_src): # check MAC
            ap.err('Bad MAC (%s), although IP of AP (%s)' % (str2mac(dst_mac), str2ip(ip)))
            return
        rx_pkt = Ether(rx_bytes)
        if ap.is_debug:
            ap.debug('AP %s got ARP' % ap.name)
            rx_pkt.show2()

        rx_arp = rx_pkt[ARP]
        if rx_arp.op == 1: # 'who-has'
            if rx_arp.psrc == rx_arp.pdst:    # GARP
                return
            if rx_arp.pdst == ap.ip_hum:      # arp request to AP
                #print 'Arp to AP'
                tx_pkt = rx_pkt.copy()
                tx_pkt.dst = tx_pkt.src
                tx_pkt.src = ap.mac_src
                tx_pkt[ARP].op = 'is-at'
                tx_pkt[ARP].hwdst = rx_arp.hwsrc
                tx_pkt[ARP].pdst = rx_arp.psrc
                tx_pkt[ARP].hwsrc = ap.mac_src
                tx_pkt[ARP].psrc = ap.ip_hum
                #tx_pkt.show2()
                send_pkts.append(tx_pkt)

        elif rx_arp.op == 2: # 'is-at'
            ap.rx_buffer.append(rx_pkt)


    def handle_ap_icmp(self, rx_bytes, ap, send_pkts):
        rx_pkt = Ether(rx_bytes)
        icmp_pkt = rx_pkt[ICMP]
        if icmp_pkt.type == 8: # echo-request
            #print 'Ping to AP!'
            #rx_pkt.show2()
            if rx_pkt[IP].dst == ap.ip_hum: # ping to AP
                tx_pkt = rx_pkt.copy()
                tx_pkt.src, tx_pkt.dst = tx_pkt.dst, tx_pkt.src
                tx_pkt[IP].src, tx_pkt[IP].dst = tx_pkt[IP].dst, tx_pkt[IP].src
                tx_pkt[ICMP].type = 'echo-reply'
                del tx_pkt[ICMP].chksum
                #tx_pkt.show2()
                send_pkts.append(tx_pkt)

        elif icmp_pkt.type == 0: # echo-reply
            ap.rx_buffer.append(rx_pkt)


    def process_capwap_ctrl(self, rx_bytes, ap, send_pkts):
        ap.info('Got CAPWAP CTRL at AP %s' % ap.name)
        rx_pkt = CAPWAP_CTRL(rx_bytes[42:])
        if ap.is_debug:
            rx_pkt.show2()
        capwap_assemble = ap.capwap_assemble
        if not ap.is_dtls_established():
            if CAPWAP_Control_Header in rx_pkt and rx_pkt.control_header.type == 2:
                ap.mac_dst = str2mac(rx_bytes[6:12])
                ap.ip_dst = str2ip(rx_bytes[26:30])
            ap.rx_buffer.append(rx_pkt)
            return

        if rx_pkt.is_dtls(): # decrypt
            dtls_buf = bytes(rx_pkt.payload)
            if (dtls_buf[0:1] == '\x15'):
                ap.is_connected = False
                self.err("Server sent DTLS alert to AP '%s'." % ap.name)
            rx_pkt_buf = ap.decrypt(dtls_buf)
            if not rx_pkt_buf:
                return
            if rx_pkt_buf[0:1] not in (b'\0', b'\1'): # definitely not CAPWAP... should we debug it?
                ap.debug('Not CAPWAP, skipping: %s' % hex(rx_pkt_buf))
                return
            rx_pkt = CAPWAP_CTRL(rx_pkt_buf)
            ap.last_recv_ts = time.time()

        if ap.is_debug:
            rx_pkt.show2()
        if rx_pkt.header.is_fragment():
            if capwap_assemble:
                assert ap.capwap_assemble['header'].fragment_id == rx_pkt.header.fragment_id, 'Got CAPWAP fragments with out of order (different fragment ids)'
                control_str = bytes(rx_pkt[CAPWAP_Control_Header_Fragment])
                if rx_pkt.header.fragment_offset * 8 != len(capwap_assemble['buf']):
                    self.err('Fragment offset and data length mismatch')
                    capwap_assemble.clear()
                    return
                #if rx_pkt.header.fragment_offset * 8 > len(capwap_assemble['buf']):
                #    print('Fragment offset: %s, data so far length: %s (not enough data)' % (rx_pkt.header.fragment_offset, len(capwap_assemble['buf'])))
                #elif rx_pkt.header.fragment_offset * 8 < len(capwap_assemble['buf']):
                #    capwap_assemble['buf'] = capwap_assemble['buf'][:rx_pkt.header.fragment_offset * 8]

                capwap_assemble['buf'] += control_str

                if rx_pkt.is_last_fragment():
                    capwap_assemble['assembled'] = CAPWAP_CTRL(
                        header = capwap_assemble['header'],
                        control_header = CAPWAP_Control_Header(capwap_assemble['buf'])
                        )

            else:
                if rx_pkt.is_last_fragment():
                    self.err('Got CAPWAP first fragment that is also last fragment!')
                    return
                if rx_pkt.header.fragment_offset != 0:
                    rx_pkt.show2()
                    self.err('Got out of order CAPWAP fragment, does not start with zero offset')
                    return
                capwap_assemble['header'] = rx_pkt.header
                capwap_assemble['header'].flags &= ~0b11000
                capwap_assemble['buf'] = bytes(rx_pkt[CAPWAP_Control_Header_Fragment])
                capwap_assemble['ap'] = ap

        elif capwap_assemble:
            self.err('Got not fragment in middle of assemble of fragments (OOO).')
            capwap_assemble.clear()
        else:
            assert CAPWAP_Control_Header in rx_pkt, rx_pkt.command()
            capwap_assemble['assembled'] = rx_pkt

        rx_pkt = capwap_assemble.get('assembled')
        if not rx_pkt:
            return
        capwap_assemble.clear()

        if ap.is_debug and CAPWAP_CTRL in rx_pkt:
            rx_pkt[CAPWAP_CTRL].show2()
        if CAPWAP_Control_Header not in rx_pkt:
            return
        if rx_pkt.control_header.type == 7: # Configuration Update Request
            #rx_pkt.show2()
            CAPWAP_PKTS.parse_message_elements(rx_pkt, ap, self.ap_mngr) # get info from incoming packet
            seq = rx_pkt.control_header.sequence_number
            tx_pkt = ap.get_config_update_capwap(seq)
            if ap.is_debug:
                CAPWAP_CTRL(tx_pkt.value).show2()
            send_pkts.append(ap.wrap_capwap_pkt(b'\1\0\0\0' + ap.encrypt(tx_pkt)))
        elif rx_pkt.control_header.type == 14: # Echo Response
            ap.echo_resp_timer = None
        elif rx_pkt.control_header.type == 17: # Reset Request
            self.err('AP %s got Reset request, shutting down' % ap.name)
            #send_pkts.append(ap.wrap_capwap_pkt(b'\1\0\0\0' + ap.encrypt(tx_pkt)))
            self.shutdown_ap(ap)
        else:
            #rx_pkt.show2()
            ap.rx_buffer.append(rx_pkt)


    def handle_client_arp(self, dot11_bytes, ap, send_pkts):
        ip = dot11_bytes[58:62]
        client = self.client_per_ip.get(ip)
        if not client:
            return
        if client.ap is not ap:
            self.err('Got ARP to client %s via wrong AP (%s)' % (client.ip, ap.name))
            return

        if dot11_bytes[40:42] == self.ARP_REQ: # 'who-has'
            if dot11_bytes[48:52] == dot11_bytes[58:62]: # GARP
                return
            tx_pkt = ap.wrap_pkt_by_wlan(client, ap.get_arp_pkt(client, 'is-at', dot11_bytes[42:48], str2ip(dot11_bytes[48:52])))
            send_pkts.append(tx_pkt)

        elif dot11_bytes[40:42] == self.ARP_REP: # 'is-at'
            ap.rx_buffer.append(Dot11_swapped(dot11_bytes))


    def handle_client_icmp(self, dot11_bytes, ap, send_pkts):
        ip = dot11_bytes[50:54]
        client = self.client_per_ip.get(ip)
        if not client:
            return
        if client.ap is not ap:
            self.err('Got ARP to client %s via wrong AP (%s)' % (client.ip, ap.name))
            return

        if dot11_bytes[54:55] == self.ICMP_REQ:
            rx_pkt = Dot11_swapped(dot11_bytes)
            tx_pkt = Ether(src = client.mac, dst = rx_pkt.addr3) / rx_pkt[IP].copy()
            tx_pkt[IP].src, tx_pkt[IP].dst = tx_pkt[IP].dst, tx_pkt[IP].src
            tx_pkt[ICMP].type = 'echo-reply'
            del tx_pkt[ICMP].chksum
            send_pkts.append(ap.wrap_pkt_by_wlan(client, tx_pkt))


    def main_loop(self):
        echo_send_timer = PassiveTimer(1)
        send_pkts = []
        while True:
            now_time = time.time()
            self.send(send_pkts)
            send_pkts = []
            resps = self.recv()

            try:
                if not self.ap_mngr.service_ctx[self.port_id]['synced']: # update only if required
                    with self.ap_mngr.bg_lock:
                        aps = list(self.ap_mngr.aps)
                        clients = list(self.ap_mngr.clients)
                        self.ap_mngr.service_ctx[self.port_id]['synced'] = True
                    self.ap_per_ip = dict([(ap.ip_src, ap) for ap in aps if ap.port_id == self.port_id])
                    self.client_per_ip = dict([(client.ip_bytes, client) for client in clients])
            except KeyError as e:
                if self.port_id not in self.ap_mngr.service_ctx:
                    return

            if echo_send_timer.has_expired():
                echo_send_timer = PassiveTimer(0.5)

                for ap in self.ap_per_ip.values():
                    if ap.is_connected:
                        if ap.echo_resp_timer and ap.echo_resp_timer.has_expired(): # retry echo
                            if ap.echo_resp_retry > 0:
                                ap.echo_resp_timeout *= 2
                                ap.echo_resp_timer = PassiveTimer(ap.echo_resp_timeout)
                                ap.echo_resp_retry -= 1
                                tx_pkt = ap.get_echo_capwap()
                                send_pkts.append(ap.get_echo_wrap(ap.encrypt(tx_pkt)))
                            else:
                                self.err("Timeout in echo response for AP '%s', disconnecting" % ap.name)
                                self.shutdown_ap(ap)

                for ap in self.ap_per_ip.values():
                    if ap.is_connected:
                        if time.time() > ap.last_echo_req_ts + ap.echo_req_interval: # new echoes
                            tx_pkt = ap.get_echo_capwap()
                            ap.last_echo_req_ts = time.time()
                            ap.echo_resp_timeout = ap.capwap_RetransmitInterval
                            ap.echo_resp_timer = PassiveTimer(ap.echo_resp_timeout)
                            ap.echo_resp_retry = ap.capwap_MaxRetransmit
                            send_pkts.append(ap.get_echo_wrap(ap.encrypt(tx_pkt)))
                    if len(send_pkts) > 200:
                        break

            if not resps and not send_pkts:
                time.sleep(0.01)
                continue

            for resp in resps:
                rx_bytes = resp['binary']
                dst_mac = rx_bytes[:7]
                ether_type = rx_bytes[12:14]

                if ether_type == self.ARP_ETHTYPE:
                    self.handle_ap_arp(rx_bytes, send_pkts)

                elif ether_type == self.IP_ETHTYPE:
                    ip = rx_bytes[30:34]
                    if ip not in self.ap_per_ip: # check IP
                        continue
                    ap = self.ap_per_ip[ip]
                    dst_mac = rx_bytes[:6]
                    if dst_mac not in ('\xff\xff\xff\xff\xff\xff', ap.mac_src): # check MAC
                        ap.err('Bad MAC (%s), although IP of AP (%s)' % (str2mac(dst_mac), str2ip(ip)))
                        continue

                    ip_proto = rx_bytes[23:24]

                    if ip_proto == self.ICMP_PROTO:
                        self.handle_ap_icmp(rx_bytes, ap, send_pkts)

                    elif ip_proto == self.UDP_PROTO:
                        udp_port_str = rx_bytes[36:38]
                        if udp_port_str != ap.udp_port_str: # check UDP port
                            ap.err('Bad UDP port (%s), although IP of AP (%s)' % (str2int(udp_port), str2ip(ip)))
                            continue
                        udp_src = rx_bytes[34:36]

                        if udp_src == self.CAPWAP_CTRL_PORT:
                            self.process_capwap_ctrl(rx_bytes, ap, send_pkts)

                        elif udp_src == self.CAPWAP_DATA_PORT:
                            if ord(rx_bytes[45:46]) & 0b1000: # CAPWAP Data Keep-alive
                                ap.rx_buffer.append(CAPWAP_DATA(rx_bytes[42:]))
                                continue

                            dot11_offset = 42 + ((ord(rx_bytes[43:44]) & 0b11111000) >> 1)
                            dot11_bytes = rx_bytes[dot11_offset:]

                            if dot11_bytes[:2] == self.WLAN_ASSOC_RESP: # Client assoc. response
                                ap.rx_buffer.append(Dot11_swapped(dot11_bytes))

                            elif dot11_bytes[32:34] == self.ARP_ETHTYPE:
                                self.handle_client_arp(dot11_bytes, ap, send_pkts)

                            elif dot11_bytes[32:34] == self.IP_ETHTYPE and dot11_bytes[43:44] == self.ICMP_PROTO:
                                self.handle_client_icmp(dot11_bytes, ap, send_pkts)



class STLServiceAp(STLService):
    requires_dtls = True
    def __init__(self, ap, verbose_level = STLService.WARN):
        STLService.__init__(self, verbose_level)
        self.ap = ap
        self.name = '%s of %s' % (self.__class__.__name__, ap.name)


    def get_filter_type(self):
        return STLServiceFilterPerAp


    @staticmethod
    def wrap_capwap_static(ap, dst_mac, dst_ip, dst_port, pkt):
        assert ap and dst_mac and dst_ip and dst_port
        return (
            Ether(src = ap.mac_src, dst = dst_mac) /
            IP(src = ap.ip_hum, dst = dst_ip) /
            UDP(sport = ap.udp_port, dport = dst_port) /
            pkt)


    def timeout(self):
        self.ap.warn('Timeout in FSM %s' % self.name)


    def log(self, msg):
        self.ap.info(msg)


    def err(self, msg):
        self.ap.err('Error in FSM %s: %s' % (self.name, msg))


    def run(self, pipe):
        if self.requires_dtls and not self.ap.is_dtls_established():
            self.err('DTLS is not established for AP %s' % self.ap.name)
            return
        self.ap.info('Starting FSM %s' % self.name)
        run_gen = self.run_with_buffer()
        send_data = None
        while True:
            if self.requires_dtls and not self.ap.is_dtls_established():
                self.log('DTLS session got closed for AP %s, exiting FSM' % self.ap.name)
                break
            try:
                action = run_gen.send(send_data)
            except StopIteration:
                action = 'done'
            if type(action) is tuple and len(action) == 2:
                action, val = action

            if action == 'get':
                send_data = None
                resp = yield pipe.async_wait_for_pkt(time_sec = val, limit = 1)
                if resp:
                    send_data = resp[0]['pkt']

            elif action == 'put':
                if type(val) is list:
                    pipe.async_tx_pkt([PacketBuffer(v) for v in val])
                else:
                    pipe.async_tx_pkt(PacketBuffer(val))

            elif action == 'sleep':
                pipe.async_wait(val)

            elif action == 'done':
                self.log('Finished successfully FSM %s' % self.name)
                break

            elif action == 'err':
                self.err(val)
                break

            elif action == 'time':
                self.timeout()
                break

            else:
                raise Exception('Incorrect action in FSM %s: %s' % (self.name, action))


def hex(buf, delimiter = ' '):
    if not buf:
        return 'Empty buffer'
    return delimiter.join(['%02x' % (c if type(c) is int else ord(c)) for c in buf])

################ FSMs ##################

class STLServiceApDiscoverWLC(STLServiceAp):
    requires_dtls = False

    def run_with_buffer(self):
        RetransmitInterval = self.ap.capwap_RetransmitInterval
        for _ in range(max(2, self.ap.capwap_MaxRetransmit)):
            RetransmitInterval *= 2
            discovery_pkt = self.ap.wrap_capwap_pkt(CAPWAP_PKTS.discovery(self.ap), dst_mac = 'ff:ff:ff:ff:ff:ff', dst_ip = '255.255.255.255')
            yield ('put', discovery_pkt)
            timer = PassiveTimer(RetransmitInterval)
            while not timer.has_expired():
                rx_pkt = yield ('get', RetransmitInterval)
                if not rx_pkt:
                    continue
                if not rx_pkt or CAPWAP_Control_Header not in rx_pkt or rx_pkt.control_header.type != 2:
                    continue
                if self.ap.is_debug:
                    rx_pkt.show2()

                for ME in rx_pkt.control_header.message_elements:
                    if ME.type == 10:
                        self.ap.ip_dst = ME.ip_address # give higher priority to ME
                    if ME.type == 1:
                        if ME.max_wtps <= ME.active_wtps:
                            yield ('err', 'WLC reached maximal number of APs connected (%s)!' % ME.max_wtps)
                self.log('Got discovery response from %s' % self.ap.ip_dst)
                return

        yield 'time'


class STLServiceApEstablishDTLS(STLServiceAp):
    requires_dtls = False
    aps_by_ssl = {}

    @staticmethod
    def openssl_callback(ssl, where, ret):
        pipe = STLServiceApEstablishDTLS.aps_by_ssl[ssl]['pipe']
        ap = STLServiceApEstablishDTLS.aps_by_ssl[ssl]['ap']

        if libcrypto.BIO_ctrl_pending(ap.out_bio):
            ssl_data = ap.ssl_read()
            if ssl_data:
                pkt = STLServiceAp.wrap_capwap_static(ap, ap.mac_dst, ap.ip_dst, 5246, b'\1\0\0\0' + ssl_data)
                pipe.async_tx_pkt(PacketBuffer(pkt))

        return 0

    ssl_info_callback_type = CFUNCTYPE(c_int, c_void_p, c_int, c_int)
    ssl_info_callback_func = ssl_info_callback_type(openssl_callback.__func__)

    def run(self, pipe):
        assert self.ap.ssl and (self.ap.ssl not in self.aps_by_ssl)
        #assert not self.ap.is_dtls_established(), 'AP %s has already established DTLS connection!' % self.ap.name
        self.aps_by_ssl[self.ap.ssl] = {'ap': self.ap, 'pipe': pipe}
        with self.ap.ssl_lock:
            libssl.SSL_clear(self.ap.ssl)
            libssl.SSL_set_info_callback(self.ap.ssl, self.ssl_info_callback_func) # set ssl callback
            libssl.SSL_do_handshake(self.ap.ssl)
        try:
            timer = PassiveTimer(5)
            self.ap.info('Start handshake')
            while not timer.has_expired():
                if self.ap.is_handshake_done():
                    return
                if self.ap.is_dtls_closed():
                    self.err('DTLS session got closed for ap %s' % self.ap.name)
                    return

                resps = yield pipe.async_wait_for_pkt(time_sec = 1, limit = 1)
                if not resps:
                    continue
                pkt = resps[0]['pkt']
                if isinstance(pkt, CAPWAP_CTRL) and pkt.is_dtls():
                    dtls_buf = bytes(pkt.payload)
                    self.ap.decrypt(dtls_buf)

            self.timeout()

        finally:
            with self.ap.ssl_lock:
                libssl.SSL_set_info_callback(self.ap.ssl, None) # remove ssl callback
            if self.ap.ssl in self.aps_by_ssl:
                del self.aps_by_ssl[self.ap.ssl]


class STLServiceApEncryptedControl(STLServiceAp):

    def control_round_trip(self, tx_pkt, expected_response_type):
        RetransmitInterval = self.ap.capwap_RetransmitInterval
        if isinstance(tx_pkt, Packet) and self.ap.is_debug:
            tx_pkt.show2()

        for _ in range(self.ap.capwap_MaxRetransmit):
            RetransmitInterval *= 2
            tx_pkt = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.encrypt(tx_pkt))
            yield ('put', tx_pkt)

            timer = PassiveTimer(RetransmitInterval)
            while not timer.has_expired():
                rx_pkt = yield ('get', RetransmitInterval)
                if not rx_pkt:
                    continue

                if CAPWAP_Control_Header not in rx_pkt:
                    continue
                ctrl_header = rx_pkt[CAPWAP_Control_Header]
                if ctrl_header.type != expected_response_type:
                    continue

                result_code = None
                for ME in ctrl_header.message_elements:
                    if ME.type == 33:
                        result_code = ME.result_code
                        break
                if result_code not in (None, 0, 2):
                    yield ('err', 'Not successful result %s - %s.' % (result_code, capwap_result_codes.get(result_code, 'Unknown')))

                if self.ap.is_debug:
                    rx_pkt.show2()

                yield ('resp', rx_pkt)

        yield 'time'


class STLServiceApJoinWLC(STLServiceApEncryptedControl):
    def run_with_buffer(self):
        self.log('Sending Join Request')
        ctrl_gen = self.control_round_trip(CAPWAP_PKTS.join(self.ap), 4)
        send_data = None
        while True:
            action = ctrl_gen.send(send_data)
            if type(action) is tuple and action[0] == 'resp':
                resp = action[1]
                ctrl_gen.close()
                break
            else:
                send_data = yield action

        for ME in resp[CAPWAP_Control_Header].message_elements:
            if ME.type == 4:
                self.ap.wlc_name = ME.ac_name
        self.log('Got Join Response')

        self.log('Sending Configuration Status Request')
        ctrl_gen = self.control_round_trip(CAPWAP_PKTS.conf_status_req(self.ap), 6)
        send_data = None
        while True:
            action = ctrl_gen.send(send_data)
            if type(action) is tuple and action[0] == 'resp':
                resp = action[1]
                ctrl_gen.close()
                break
            else:
                send_data = yield action
        self.log('Got Configuration Status Response')

        self.log('Sending Change State Event Request')
        ctrl_gen = self.control_round_trip(CAPWAP_PKTS.change_state(self.ap), 12)
        send_data = None
        while True:
            action = ctrl_gen.send(send_data)
            if type(action) is tuple and action[0] == 'resp':
                resp = action[1]
                ctrl_gen.close()
                break
            else:
                send_data = yield action
        self.log('Got Change State Event Response')

        self.log('Going to ack all config updates and try to get SSID')
        while self.ap.last_recv_ts + 5 > time.time(): # ack all config updates in BG thread
            if self.ap.SSID:
                break
            if self.ap.is_dtls_closed():
                return
            yield ('sleep', 0.5)

        if not self.ap.SSID:
            yield ('err', 'Did not get SSID from WLC!')

        self.log('Sending Keep-alive.')
        RetransmitInterval = self.ap.capwap_RetransmitInterval
        for _ in range(self.ap.capwap_MaxRetransmit):
            RetransmitInterval *= 2
            tx_pkt = self.ap.wrap_capwap_pkt(CAPWAP_PKTS.keep_alive(self.ap), dst_port = 5247)
            if self.ap.is_debug:
                Ether(tx_pkt).show2()
            yield ('put', tx_pkt)
            timer = PassiveTimer(RetransmitInterval)
            while not timer.has_expired():
                rx_pkt = yield ('get', RetransmitInterval)
                if rx_pkt and isinstance(rx_pkt, CAPWAP_DATA) and rx_pkt.is_keepalive():
                    self.log('Received Keep-alive response.')
                    self.ap.last_echo_req_ts = time.time()
                    self.ap.is_connected = True
                    return
        yield 'time'


class STLServiceApAddClients(STLServiceAp):
    def __init__(self, ap, clients, verbose_level = STLService.WARN):
        STLServiceAp.__init__(self, ap, verbose_level)
        assert type(clients) is list
        assert all([hasattr(c, 'mac') and hasattr(c, 'ip') for c in clients]), 'Clients should have attributes mac and ip'
        self.clients = clients


    def run_with_buffer(self):
        self.log('Sending Association requests.')
        need_assoc_resp_clients = dict([(client.mac, client) for client in self.clients])

        RetransmitInterval = self.ap.capwap_RetransmitInterval
        for _ in range(self.ap.capwap_MaxRetransmit):
            if not need_assoc_resp_clients:
                break
            RetransmitInterval *= 2
            tx_pkts = []
            for client in need_assoc_resp_clients.values():
                tx_pkt = CAPWAP_PKTS.client_assoc(self.ap, slot_id = 0, client_mac = client.mac)
                tx_pkts.append(self.ap.wrap_capwap_pkt(tx_pkt, dst_port = 5247))
            yield ('put', tx_pkts)

            timer = PassiveTimer(RetransmitInterval)
            while not timer.has_expired():
                if not need_assoc_resp_clients:
                    break
                rx_pkt = yield ('get', RetransmitInterval)
                if rx_pkt and isinstance(rx_pkt, Dot11_swapped):
                    if self.ap.is_debug:
                        rx_pkt.show2()
                    if rx_pkt.type == 0 and rx_pkt.subtype == 1 and rx_pkt.addr1 in need_assoc_resp_clients:
                        self.log('Received Association response for %s.' % need_assoc_resp_clients[rx_pkt.addr1].ip)
                        need_assoc_resp_clients[rx_pkt.addr1].is_associated = True
                        del need_assoc_resp_clients[rx_pkt.addr1]

        if need_assoc_resp_clients:
            yield ('err', 'No Association response for clients: %s' % ', '.join([client.ip for client in need_assoc_resp_clients.values()]))

        need_arp_resp_clients = dict([(client.ip, client) for client in self.clients])

        RetransmitInterval = self.ap.capwap_RetransmitInterval
        for _ in range(self.ap.capwap_MaxRetransmit):
            if not need_arp_resp_clients:
                return

            RetransmitInterval *= 2
            tx_pkts = []
            for client in need_arp_resp_clients.values():
                tx_pkts.append(self.ap.wrap_pkt_by_wlan(client, self.ap.get_garp_pkt(client)))
                tx_pkts.append(self.ap.wrap_pkt_by_wlan(client, self.ap.get_arp_pkt(client, 'who-has', 'ff:ff:ff:ff:ff:ff', self.ap.ip_dst)))
            yield ('put', tx_pkts)

            timer = PassiveTimer(RetransmitInterval)
            while not timer.has_expired():
                if not need_arp_resp_clients:
                    return
                rx_pkt = yield ('get', RetransmitInterval)
                if rx_pkt and isinstance(rx_pkt, Dot11_swapped) and ARP in rx_pkt:
                    rx_arp = rx_pkt[ARP]
                    client = need_arp_resp_clients.get(rx_arp.pdst)
                    if not client or client.mac != rx_arp.hwdst or client.mac != rx_pkt[Dot11_swapped].addr1:
                        continue
                    #print 'client for arp resp'
                    if rx_arp.hwsrc != self.ap.mac_dst:
                        self.log('MAC address returned from ARP (%s) differs from WLC MAC (%s)' % (rx_arp.hwsrc, self.ap.mac_dst))
                    client.seen_arp_reply = True
                    del need_arp_resp_clients[rx_arp.pdst]
                    self.log('Got WLC response with MAC: %s' % rx_arp.hwsrc)


class STLServiceApShutdownDTLS(STLServiceAp):
    def run(self, pipe):
        for client in self.ap.clients:
            client.disconnect()
        if not self.ap.is_dtls_established():
            return
        with self.ap.ssl_lock:
            libssl.SSL_shutdown(self.ap.ssl)
        tx_pkt = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + self.ap.ssl_read())
        self.ap.reset_vars()
        yield pipe.async_tx_pkt(PacketBuffer(tx_pkt))




