"""
ARP service implementation

Description:
    <FILL ME HERE>

How to use:
    <FILL ME HERE>

Author:
  Andreas Bourges

"""
from .trex_stl_service import STLService, STLServiceFilter
from ..trex_stl_vlan import VLAN
from ..trex_stl_types import listify

from scapy.layers.l2 import Ether, ARP, Dot1Q, Dot1AD
from scapy.layers.inet6 import IPv6,ICMPv6ND_NA, ICMPv6ND_NS, ICMPv6NDOptSrcLLAddr, ICMPv6NDOptDstLLAddr
from scapy.utils6 import in6_ptoc, in6_ctop, in6_getnsmac, in6_getnsma

from collections import defaultdict
import ipaddr
import time
import socket


class STLServiceFilterIPv6NDP(STLServiceFilter):
    '''
        Service filter for NDP services
    '''
    def __init__ (self):
        self.services = defaultdict(list)

    def add (self, service):
        # forward packets according to the SRC/DST IP
        self.services[(service.src_ip, service.dst_ip, tuple(service.vlan))].append(service)
        self.services[(service.src_ip, tuple(service.vlan))].append(service)

    def lookup (self, pkt):
        
        # use scapy to parse
        scapy_pkt = Ether(pkt)
        
        # not ARP
        if 'ICMPv6ND_NA' not in scapy_pkt and 'ICMPv6ND_NS' not in scapy_pkt:
            return []
            
        vlans = VLAN.extract(scapy_pkt)
        
        # ignore VLAN 0 - hash as empty VLAN
        vlans = vlans if vlans != [0] else []

        if ICMPv6ND_NA in scapy_pkt:
            return self.services.get( (scapy_pkt[IPv6].dst, scapy_pkt[IPv6].src, tuple(vlans)), [] ) 

        # NS from neighbor to verify ourselves
        if ICMPv6ND_NS in scapy_pkt:
            return self.services.get( (scapy_pkt[ICMPv6ND_NS].tgt, tuple(vlans)), [] ) 

   
    def get_bpf_filter (self):
        # a simple BPF pattern for ARP (this is not duplicate, it is for QinQ)
        return 'icmp6 or (vlan and icmp6) or (vlan and icmp6)'
        

class STLServiceIPv6ND(STLService):
    '''
        ARP service - generate ARP requests
    '''

    def __init__ (self, ctx, dst_ip, src_ip = '0.0.0.0', retries=1, src_mac = None, vlan = None, vlan_encaps=None, timeout = 5, verify_timeout=6, verbose_level = STLService.INFO):
        
        # init the base object
        super(STLServiceIPv6ND, self).__init__(verbose_level)

        self.src_ip      = src_ip
        self.retries       = retries
        self.dst_ip      = dst_ip
        self.vlan        = VLAN(vlan)
        self.vlan_encaps = vlan_encaps
        self.timeout = timeout
        self.verify_timeout = verify_timeout
        self.resolved    = False
        self.verbose_level = verbose_level
        
        if src_mac != None:
            self.src_mac = src_mac
        else:
            self.src_mac     = ctx.get_src_mac()
        self.record = None
        
        self.nd_dst_ipv6 = socket.inet_ntop(socket.AF_INET6, (in6_getnsma(socket.inet_pton(socket.AF_INET6, self.dst_ip))))
        self.nd_dst_mac = in6_getnsmac(socket.inet_pton(socket.AF_INET6, self.nd_dst_ipv6))

        self.record = IPv6Neighbor(self.src_mac, self.src_ip, self.dst_ip)

        
    def get_filter_type (self):
        return STLServiceFilterIPv6NDP

    def handle_ns_request(self, pipe, packet):
        self.log("ND: received NS: %s <-- %s,%s" % ( packet[ICMPv6ND_NS].tgt, packet[IPv6].src, packet[Ether].src))
        # packet.show()
        na_response = Ether(src=packet[Ether].dst, dst=packet[Ether].src)/ \
                       IPv6(src=packet[ICMPv6ND_NS].tgt, dst=packet[IPv6].src, hlim = 255)/ \
                       ICMPv6ND_NA(tgt=packet[ICMPv6ND_NS].tgt, R = 0, S = 1, O = 1)/ \
                       ICMPv6NDOptDstLLAddr(lladdr=packet[Ether].dst)

        if not self.vlan.is_default():
            self.vlan.embed(na_response, fmt=self.vlan_encaps)

        pipe.async_tx_pkt(na_response)
        self.record.verified()
        self.log("ND: sending NA: %s,%s -> %s,%s" % (packet[ICMPv6ND_NS].tgt, packet[Ether].dst,packet[IPv6].src,packet[Ether].src))

    def run (self, pipe):
        '''
            Will execute IPv6 NS request
        '''

        # re-initialize in case of 2nd run
        self.record.initialize()
        

        ns_request = Ether(src=self.src_mac, dst=self.nd_dst_mac)/ \
                      IPv6(src=self.src_ip, dst=self.nd_dst_ipv6)/ \
                      ICMPv6ND_NS(tgt=self.dst_ip)/ \
                      ICMPv6NDOptSrcLLAddr(lladdr=self.src_mac)

        # add VLAN to the packet if needed
        if not self.vlan.is_default():
            self.vlan.embed(ns_request, fmt=self.vlan_encaps)

        for retry in range(0, (self.retries+1)):
            # send neighbor solicitation
            tx_info = yield pipe.async_tx_pkt(ns_request)
            self.log("ND: sent  NS: %s,%s -> %s (retry %s)" % (self.src_ip,self.src_mac,self.dst_ip, retry))
            
            # wait for RX packet
            pkts = yield pipe.async_wait_for_pkt(time_sec = self.timeout)
            if not pkts:
                self.log("ND: timeout for %s,%s <-- %s (retry %s)" % ( self.src_ip, self.src_mac, self.dst_ip, retry))
                continue

            for p in pkts:
                # parse record
                response = Ether(p['pkt'])
                if ICMPv6NDOptDstLLAddr in response:
                    self.log("ND: received NA: %s <- %s, %s" % (response[IPv6].dst, response[IPv6].src, response[ICMPv6NDOptDstLLAddr].lladdr))
                    self.record.update(response)
                if ICMPv6ND_NS in response:
                    self.handle_ns_request(pipe, response)
        
        if not self.record.is_resolved():
            return

        start_time = time.time()
        while (time.time() - start_time)  < self.verify_timeout:
            pkts = yield pipe.async_wait_for_pkt(time_sec = self.verify_timeout)
            if not pkts:
                pass
            else:
                # print "building NA response..."
                # print "nr of NA packets: %s " % len(pkts)
                for packet in pkts:
                    p = Ether(packet['pkt'])
                    if ICMPv6ND_NA in p:
                        self.log("ND: late arrival of NA from {0} for {1} discarded".format(p[IPv6].src,p[IPv6].dst))
                    elif ICMPv6ND_NS not in p:
                        self.log("ND: got martian packet: {0}".format(p.summary()))
                    else:
                        self.handle_ns_request(pipe, p)

    def get_record (self):
        return self.record

        
class IPv6Neighbor(object):

    def __init__ (self, src_mac=None, src_ip = 'N/A', dst_ip = 'N/A'):
        self.src_ip  = src_ip
        self.dst_ip  = dst_ip
        self.dst_mac = None
        self.src_mac = src_mac
        self.state = "UNREACHABLE"
        self.neighbor_verifications = 0
        
    def __nonzero__ (self):
        return self.dst_mac is not None
        
    __bool__  = __nonzero__
    
    def __str__ (self):
        if self.dst_mac:
            return "Recieved NA reply from: {0}, hw: {1}".format(self.dst_ip, self.dst_mac)
        else:
            return "Failed to receive NA response from {0}".format(self.dst_ip)

    def verified(self):
        self.neighbor_verifications += 1

    def initialize(self):
        self.state = "UNREACHABLE"

    def is_resolved(self):
        return (self.state == "REACHABLE")

    def update(self, response):
        self.dst_mac = response[ICMPv6NDOptDstLLAddr].lladdr
        self.state="REACHABLE"
