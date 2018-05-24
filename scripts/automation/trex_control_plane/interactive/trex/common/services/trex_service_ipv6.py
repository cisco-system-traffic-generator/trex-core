
from ...common.services.trex_service import Service, ServiceFilter

from ...stl.trex_stl_packet_builder_scapy import *
from ..trex_types import *
from ..trex_vlan import VLAN
from ..trex_exceptions import TRexError

from scapy.layers.l2 import Ether
from scapy.layers.inet6 import *
import time
import random
from collections import defaultdict

class ServiceFilterIPv6(ServiceFilter):
    '''
        Service filter for IPv6 services
    '''
    def __init__ (self):
        self.services = defaultdict(list)

    def add(self, service):
        self.services[tuple(service.vlan)].append(service)

    def lookup(self, pkt):
        scapy_pkt = Ether(pkt)
        if IPv6 not in scapy_pkt:
            return []
        vlans = [vlan for vlan in VLAN.extract(scapy_pkt) if vlan != 0]

        return self.services.get(tuple(vlans), [])

    def get_bpf_filter(self):
        return 'ip6 or (vlan and ip6) or (vlan and ip6)'

class ServiceIPv6(Service):

    def __init__(self, ctx, dst_ip, vlan, timeout, verbose_level):
        Service.__init__(self, verbose_level)
        self.ctx        = ctx
        self.port       = ctx.port_obj
        self.dst_ip     = dst_ip
        self.vlan       = vlan
        self.timeout    = timeout
        self.attr       = self.port.get_ts_attr()
        self.src_mac    = self.attr['layer_cfg']['ether']['src']
        mac_for_ip      = self.port.info.get('hw_mac', self.src_mac)
        self.src_ip     = generate_ipv6(mac_for_ip)
        self.mld_ip     = generate_ipv6_solicited_node(mac_for_ip)
        self.record     = {}

    def get_filter_type (self):
        return ServiceFilterIPv6

    def is_mult_required(self):
        return True

    def generate_ns(self, dst_mac, dst_ip):
        pkt = (Ether(src = self.src_mac, dst = dst_mac) /
               IPv6(src = self.src_ip, dst = dst_ip) /
               ICMPv6ND_NS(tgt = dst_ip) /
               ICMPv6NDOptSrcLLAddr(lladdr = self.src_mac))
        return self.vlan.embed(pkt)

    def generate_na(self, dst_mac, dst_ip):
        pkt = (Ether(src = self.src_mac, dst = dst_mac) /
               IPv6(src = self.src_ip, dst = dst_ip) /
               ICMPv6ND_NA(tgt = self.src_ip, R = 0, S = 1, O = 1))
        return self.vlan.embed(pkt)

    def generate_ns_na(self, dst_mac, dst_ip):
        return [self.generate_ns(dst_mac, dst_ip), self.generate_na(dst_mac, dst_ip)]

    def get_record (self):
        return self.record


class ServiceIPv6Scan(ServiceIPv6):
    ''' Ping with given IPv6 (usually all nodes address) and wait for responses until timeout '''

    dst_mld_ip = 'ff02::16'
    dst_mld_mac = multicast_mac_from_ipv6(dst_mld_ip)

    def __init__ (self, ctx, dst_ip, timeout = 3, verbose_level = Service.ERROR):
        vlan = VLAN(ctx.port_obj.get_vlan_cfg())
        ServiceIPv6.__init__(self, ctx, dst_ip, vlan, timeout, verbose_level)

    def generate_request(self):
        dst_mac = multicast_mac_from_ipv6(self.dst_ip)

        mld_pkt = (Ether(src = self.src_mac, dst = self.dst_mld_mac) /
                   IPv6(src = self.src_ip, dst = self.dst_mld_ip, hlim = 1) /
                   IPv6ExtHdrHopByHop(options = [RouterAlert(), PadN()]) /
                   ICMPv6MLReportV2() /
                   MLDv2Addr(type = 4, len = 0, multicast_addr = 'ff02::2'))
        ping_pkt = (Ether(src = self.src_mac, dst = dst_mac) /
                    IPv6(src = self.src_ip, dst = self.dst_ip, hlim = 1) /
                    ICMPv6EchoRequest())
        return [self.vlan.embed(mld_pkt), self.vlan.embed(ping_pkt)]


    def on_pkt_rx(self, pkt):
        # convert to scapy
        scapy_pkt = Ether(pkt['pkt'])

        if ICMPv6ND_NS in scapy_pkt and ICMPv6NDOptSrcLLAddr in scapy_pkt:
            node_mac = scapy_pkt[ICMPv6NDOptSrcLLAddr].lladdr
            node_ip = scapy_pkt[IPv6].src
            if node_ip not in self.record:
                return self.generate_ns_na(node_mac, node_ip)

        elif ICMPv6ND_NA in scapy_pkt:
            is_router = scapy_pkt[ICMPv6ND_NA].R
            node_ip = scapy_pkt[ICMPv6ND_NA].tgt
            dst_ip  = scapy_pkt[IPv6].dst
            node_mac = scapy_pkt.src
            if dst_ip == self.src_ip:
                self.record[node_ip] = {'type': 'Router' if is_router else 'Host', 'mac': node_mac}

        elif ICMPv6EchoReply in scapy_pkt:
            node_mac = scapy_pkt.src
            node_ip = scapy_pkt[IPv6].src
            if node_ip == self.dst_ip and node_ip != 'ff02::1' and node_ip not in self.record: # for ping ipv6
                self.record[node_ip] = {'type': 'N/A', 'mac': node_mac}
            if node_ip not in self.record:
                if node_ip == self.dst_ip and node_ip != 'ff02::1':
                    self.record[node_ip] = {'type': 'N/A', 'mac': node_mac}
                else:
                    return self.generate_ns_na(node_mac, node_ip)

    def run(self, pipe):
        self.record = {}
        end_time = time.time() + self.timeout
        pkts = self.generate_request() * 2
        for pkt in pkts:
            yield pipe.async_tx_pkt(pkt)

        while True:
            # wait for RX packet(s)
            rx_pkts = yield pipe.async_wait_for_pkt(time_sec = 0.1)
            if time.time() > end_time:
                break
            for rx_pkt in rx_pkts:
                tx_pkts = self.on_pkt_rx(rx_pkt) or []
                for tx_pkt in tx_pkts:
                    yield pipe.async_tx_pkt(tx_pkt)
        self.on_timeout()


    def on_timeout(self):
        self.record = [{'type': v['type'], 'mac':  v['mac'], 'ipv6': k} for k, v in self.record.items()]


class ServiceICMPv6(ServiceIPv6):
    '''
    Ping some IPv6 location.
    If the dest MAC is found from scanning, use it.
    Otherwise, send to default port dest.
    '''

    def __init__ (self, ctx, dst_ip, pkt_size = 64, timeout = 3, dst_mac = None, verbose_level = Service.ERROR, vlan = None):
        vlan = VLAN(vlan)
        ServiceIPv6.__init__(self, ctx, dst_ip, vlan, timeout, verbose_level)
        self.pkt_size   = pkt_size
        self.dst_mac    = dst_mac or self.port.get_ts_attr()['layer_cfg']['ether']['dst']
        self.id         = random.getrandbits(16)
        self.record     = {}

    def run(self, pipe):
        if self.dst_mac is None and not self.port.is_resolved():
            raise TRexError('ping - port has an unresolved destination, cannot determine next hop MAC address')
        self.record = {}
        req = self.generate_request()
        start_tx_res = yield pipe.async_tx_pkt(req)
        end_time = time.time() + self.timeout
        while not self.record:
            # wait for RX packet(s)
            rx_pkts = yield pipe.async_wait_for_pkt(time_sec = 0.1)
            if time.time() > end_time:
                break
            for rx_pkt in rx_pkts:
                tx_pkts = self.on_pkt_rx(rx_pkt, start_tx_res['ts']) or []
                for tx_pkt in tx_pkts:
                    yield pipe.async_tx_pkt(tx_pkt)
        if not self.record:
            self.on_timeout()

    # return a list of streams for request
    def generate_request(self):
        ping_pkt = (Ether(src = self.src_mac, dst = self.dst_mac) / 
                    IPv6(src = self.src_ip, dst = self.dst_ip) /
                    ICMPv6EchoRequest(id = self.id))
        pad = max(0, self.pkt_size - len(ping_pkt))
        ping_pkt /= pad * 'x'
        return self.vlan.embed(ping_pkt)


    def on_pkt_rx(self, pkt, start_ts):
        scapy_pkt = Ether(pkt['pkt'])

        if ICMPv6EchoReply in scapy_pkt:
            if scapy_pkt[ICMPv6EchoReply].id != self.id: # not our ping
                return
            if scapy_pkt[IPv6].dst != self.src_ip: # not our ping
                return
            node_ip = scapy_pkt[IPv6].src
            hlim = scapy_pkt[IPv6].hlim

            dt = pkt['ts'] - start_ts
            self.record['formatted_string'] = 'Reply from {0}: bytes={1}, time={2:.2f}ms, hlim={3}'.format(node_ip, len(pkt['pkt']), dt * 1000, hlim)
            self.record['src_ip'] = node_ip
            self.record['rtt'] = dt * 1000
            self.record['ttl'] = hlim
            self.record['status'] = 'success'

        elif ICMPv6ND_NS in scapy_pkt and ICMPv6NDOptSrcLLAddr in scapy_pkt:
            if scapy_pkt[ICMPv6ND_NS].tgt != self.src_ip: # not our pkt
                return
            node_mac = scapy_pkt[ICMPv6NDOptSrcLLAddr].lladdr
            node_ip = scapy_pkt[IPv6].src
            return self.generate_ns_na(node_mac, node_ip)

        elif ICMPv6DestUnreach in scapy_pkt:
            if scapy_pkt[IPv6].dst != self.src_ip: # not our pkt
                return
            node_ip = scapy_pkt[IPv6].src
            self.record['formatted_string'] = 'Reply from {0}: Destination host unreachable'.format(node_ip)
            self.record['status'] = 'unreachable'


    # return the str of a timeout err
    def on_timeout(self):
        self.record['formatted_string'] = 'Request timed out.'
        self.record['status'] = 'timeout'


