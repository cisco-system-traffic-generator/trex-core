from .trex_wireless_device_service import WirelessDeviceService
from ..trex_wireless_ap import AP
from .trex_stl_dhcp_parser import DHCPParser, Dot11DHCPParser

import time
import random
import socket
from scapy.contrib.capwap import *

# parser = DHCPParser()

def ipv4_num_to_str(num):
    return socket.inet_ntoa(struct.pack('!I', num))
    
def ipv4_str_to_num (ipv4_str):
    return struct.unpack("!I", socket.inet_aton(ipv4_str))[0]

class DHCP:
    """DHCP Protocol Definition."""
    # DHCP States
    INIT, SELECTING, REQUESTING, BOUND, RENEWING, REBINDING, REBOOTING, INIT_REBOOT = range(8)

    # DHCP Messages
    DHCPDISCOVER, DHCPOFFER, DHCPREQUEST, DHCPDECLINE, DHCPACK, DHCPNAK, DHCPRELEASE, DHCPINFORM = range(1,9)

    def __str__ (self):
        return "ip: {0}, server_ip: {1}, subnet: {2}, domain: {3}, lease_time: {4}".format(self.device_ip, self.server_ip, self.subnet, self.domain, self.lease)

class WirelessServiceDHCP(WirelessDeviceService):
    SLOT_TIME = 2 # exponential backoff slot time, after n retries, timeout = slot^(n+1) + uniform(-1, 1)
    MAX_RETRIES = 3 # maximum number of retries before the timeout stops being increase, maximum timeout ~= 64seconds
    WAIT_FOR_STATE = 1 # seconds to wait when DHCP cannot start or has been timed out

    def __init__(self, device, env, tx_conn, done_event=None, max_concurrent=float('inf'), dhcp_parser=None, **kw):
        """Instanciate a DHCP service on a wireless device.
        
        Args:
            device (WirelessDevice): device that owns the service
            env: simpy environment
            tx_conn: Connection (e.g. pipe end with 'send' method) used to send packets.
                ServiceClient should send packet via this connection as if a real device would send them.
                e.g. Dot11 / Dot11QoS / LLC / SNAP / IP / UDP / DHCP here.
                One can use a wrapper on the connection to provide capwap encapsulation.
            done_event: event that will be awaken when the service completes for the device
            dhcp_parser: DHCPParser (Dot11 or Ethernet)
        """
        if not dhcp_parser:
            raise ValueError("WirelessServiceDHCP should be given a DHCPParser for construction")

        self.parser = dhcp_parser
        
        super().__init__(device=device, env=env, tx_conn=tx_conn, done_event=done_event, max_concurrent=max_concurrent, **kw)

        # Protocol Specific Variables
        self.xid = random.getrandbits(32)  # transaction ID
        self.mac_bytes = self.device.mac_bytes  # device mac address
        self.retries = 0 # number of retries
        self.state = DHCP.INIT  # FSM state
        self.offer = None # DHCP Offer received and selected
        self.timed_out = False # True if the DHCP process has timed out too many times (more than MAX_RETRIES)
        self.total_retries = 0 # total number of retries (for statistics)

    def raise_event_dhcp_done(self):
        """Raise event for succeeded DHCP."""
        raise NotImplementedError

    def is_ready(self, device):
        """Return True if the device is ready to start DHCP (e.g. for client, client state should be associated)"""
        # To be implemented by subclass
        raise NotImplementedError

    def stop(self, hard=False):
        self.state = DHCP.INIT

    @property
    def wait_time(self):
        """Return the next delay to wait for packets."""
        # exponential backoff with randomized +/- 1
        return pow(WirelessServiceDHCP.SLOT_TIME, self.retries + 1) + random.uniform(-1, 1)

    def timeout(self):
        """Increase the delay to wait, for next retransmission, or rollback to INIT state."""

        if self.retries < WirelessServiceDHCP.MAX_RETRIES:
            self.retries += 1
            self.total_retries += 1
        if self.retries >= WirelessServiceDHCP.MAX_RETRIES:
            self.retries = 0
            self.total_retries += 1
            self.state = DHCP.INIT # rollback
            self.offer = None
            self.timed_out = True

    def reset(self):
        """Perform a complete rollback of the FSM."""
        self.retries = 0
        self.state = DHCP.INIT
        self.offer = None
        self.timed_out = False

    def run(self):
        """Run the DHCP Service for the device."""

        # ~~ Notes ~~
        # A DHCP Client should only receive DHCPOFFER, DHCPACK and DHCPNAK messages from a DHCP server
        # If filtering is possible, consider it

        device = self.device
        if not isinstance(device, AP):
            ap = device.ap
        else:
            ap = device

        if device.ip is not None:
            device.logger.info(
                "IP address is fixed for this device, DHCP will not be run")
            return

        device.logger.info("DHCP set to start")
        yield self.async_request_start(first_start=True)
        device.logger.info("DHCP started")

        # run loop
        while True:

            if device.is_closed:
                # if device is closed the service should stop
                device.logger.info("DHCP service stopped: device is stopped")
                self.reset()
                yield self.async_request_stop(done=True, success=False)
                return

            if not self.is_ready(device):
                if self.timed_out:
                    device.logger.info("DHCP service timed out")
                    self.reset()
                    yield self.async_request_stop(done=True, success=False)
                    return
                    # yield self.async_wait(WirelessServiceDHCP.WAIT_FOR_STATE)
                    # continue

                device.logger.info("DHCP service waiting for device to be ready, current state: {}".format(device.state))
                self.reset()
                yield self.async_request_stop(done=True, success=False)
                yield self.async_wait(WirelessServiceDHCP.WAIT_FOR_STATE)
                yield self.async_request_start(first_start=False)
                continue

            if self.state == DHCP.INIT:
                # input NONE
                # output DHCPDISCOVER

                device.logger.debug("DHCP sending DHCPDISCOVER")
                if not isinstance(device, AP):
                    self.send_pkt(self.parser.disc(self.xid, device.mac_bytes, ap.radio_mac_bytes))
                else:
                    self.send_pkt(self.parser.disc(self.xid, device.mac_bytes))
                self.state = DHCP.SELECTING
                
                continue
            elif self.state == DHCP.SELECTING:
                # input DHCPOFFER(s)
                # output DHCPREQUEST

                if not self.offer:
                    device.logger.debug("DHCP waiting DHCPOFFERs")
                    pkts = yield self.async_recv_pkt(time_sec=self.wait_time)

                    if not pkts:
                        device.logger.debug("DHCP received no DHCPOFFER")
                        device.logger.debug("DHCP rollback")
                        # rollback to INIT
                        self.state = DHCP.INIT
                        self.timeout()
                        continue
                    
                    pkt = pkts[0]
                    pkt = self.parser.parse(pkt) if pkt else None
                    offer = pkt if pkt and pkt.xid == self.xid and pkt.options['message-type'] == DHCP.DHCPOFFER else None
                    
                    if not offer:
                        device.logger.debug("DHCP received no DHCPOFFER")
                        device.logger.debug("DHCP rollback")
                        # rollback to INIT
                        self.state = DHCP.INIT
                        self.timeout()
                        continue
                    
                    self.offer = offer

                    server_ip = self.offer.options['server_id']
                    device.logger.debug("DHCP received DHCPOFFER from {}".format(ipv4_num_to_str(server_ip)))
                    
                assert(self.offer)

                # send DHCPOFFER
                device.logger.debug("DHCP sending DHCPREQUEST")
                if not isinstance(device, AP):
                    self.send_pkt(self.parser.req(self.xid, self.mac_bytes, self.offer.yiaddr, ap.radio_mac_bytes, server_ip))
                else:
                    self.send_pkt(self.parser.req(self.xid, self.mac_bytes, self.offer.yiaddr, server_ip))

                self.state = DHCP.REQUESTING
                continue
            elif self.state == DHCP.REQUESTING:
                # input DHCPACK OR DHCPNAK
                # output NONE

                device.logger.debug("DHCP waiting DHCPACK or DHCPNAK")
                pkts = yield self.async_recv_pkt(time_sec=self.wait_time)

                if not pkts:
                    device.logger.debug("DHCP received no DHCPACK nor DHCPNAK")
                    device.logger.debug("DHCP rollback")
                    # rollback to SELECTING
                    self.state = DHCP.SELECTING
                    self.timeout() # if too many trials, rollback to INIT
                    continue

                pkt = pkts[0]

                # parse received packet
                acknack = self.parser.parse(pkt) if pkt else None
                acknack = acknack if acknack and acknack.xid == self.xid and acknack.options['message-type'] in (DHCP.DHCPACK, DHCP.DHCPNAK) else None
              
                if not acknack:
                    device.logger.debug("DHCP received no DHCPACK nor DHCPNAK")
                    device.logger.debug("DHCP rollback")
                    # rollback to SELECTING
                    self.state = DHCP.SELECTING
                    self.timeout() # if too many trials, rollback to INIT
                    continue
                
                if acknack.options['message-type'] == DHCP.DHCPACK:
                    device.logger.debug("DHCP received DHCPACK")
                    self.state = DHCP.BOUND
                    continue
                else:
                    device.logger.debug("DHCP received DHCPNAK")
                    device.logger.debug("DHCP rollback")
                    # complete rollback to INIT
                    self.reset()
                    yield self.async_wait(WirelessServiceDHCP.WAIT_FOR_STATE)
                    continue
            
            elif self.state == DHCP.BOUND:
                # input NONE
                # output NONE

                # parse the offer and save it
                self.record = self.DHCPRecord(self.offer)

                # Set IP of Client :
                device.setIPAddress(self.offer.yiaddr)
                self.device.gateway_ip = self.record.router

                self.raise_event_dhcp_done()
                
                device.logger.info("DHCP finished with IP: {} and Gateway: {}".format(device.ip, device.gateway_ip))
                break

        # done
        device.logger.info("DHCP done")
        self.add_service_info("total_retries", self.total_retries)
        self.reset()
        yield self.async_request_stop(done=True, success=True)
        return

    class DHCPRecord(object):
        """A DHCPRecord represents the state of DHCP after being given an IP, in the BOUND state."""
        def __init__ (self, offer):
            
            self.server_mac = offer.srcmac
            self.device_mac = offer.dstmac
            
            options = offer.options
            
            self.server_ip = ipv4_num_to_str(options['server_id']) if 'server_id' in options else None
            self.subnet    = ipv4_num_to_str(options['subnet_mask']) if 'subnet_mask' in options else None
            self.device_ip = ipv4_num_to_str(offer.yiaddr)
            self.router    = ipv4_num_to_str(options['router']) if 'router' in options else None

            self.domain     = options.get('domain', 'N/A')
            self.lease      = options.get('lease-time', 'N/A')

