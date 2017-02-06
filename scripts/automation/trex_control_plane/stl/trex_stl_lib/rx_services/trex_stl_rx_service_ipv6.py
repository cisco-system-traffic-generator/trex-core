from .trex_stl_rx_service_api import RXServiceAPI

from ..trex_stl_streams import STLStream, STLTXSingleBurst
from ..trex_stl_packet_builder_scapy import *
from ..trex_stl_types import *

from scapy.layers.l2 import Ether
from scapy.layers.inet6 import *
import time


class RXServiceIPv6(RXServiceAPI):

    def __init__(self, port, dst_ip, *a, **k):
        RXServiceAPI.__init__(self, port, *a, **k)
        self.attr = port.get_ts_attr()
        self.src_mac = self.attr['layer_cfg']['ether']['src']
        mac_for_ip   = port.info.get('hw_mac', self.src_mac)
        self.src_ip  = generate_ipv6(mac_for_ip)
        self.mld_ip  = generate_ipv6_solicited_node(mac_for_ip)
        self.dst_ip  = dst_ip
        self.responses = {}

    def pre_execute(self):
        return self.port.ok()

    def send_intermediate(self, streams):
        self.port.remove_all_streams()
        self.port.add_streams(streams)
        mult = {'op': 'abs', 'type' : 'percentage', 'value': 100}
        self.port.start(mul = mult, force = True, duration = -1, mask = 0xffffffff)

    def generate_ns(self, dst_mac, dst_ip):
        pkt = (Ether(src = self.src_mac, dst = dst_mac) /
               IPv6(src = self.src_ip, dst = dst_ip) /
               ICMPv6ND_NS(tgt = dst_ip) /
               ICMPv6NDOptSrcLLAddr(lladdr = self.src_mac))
        return STLStream(packet = STLPktBuilder(pkt = pkt), mode = STLTXSingleBurst(total_pkts = 2))

    def generate_na(self, dst_mac, dst_ip):
        pkt = (Ether(src = self.src_mac, dst = dst_mac) /
               IPv6(src = self.src_ip, dst = dst_ip) /
               ICMPv6ND_NA(tgt = self.src_ip, R = 0, S = 1, O = 1))
        return STLStream(packet = STLPktBuilder(pkt = pkt), mode = STLTXSingleBurst(total_pkts = 2))

    def generate_ns_na(self, dst_mac, dst_ip):
        return [self.generate_ns(dst_mac, dst_ip), self.generate_na(dst_mac, dst_ip)]

    def execute(self, *a, **k):
        mult = self.attr['multicast']['enabled']
        try:
            if mult != True:
                self.port.set_attr(multicast = True) # response might be multicast
            return RXServiceAPI.execute(self, *a, **k)
        finally:
            if mult != True:
                self.port.set_attr(multicast = False)


class RXServiceIPv6Scan(RXServiceIPv6):
    ''' Ping with given IPv6 (usually all nodes address) and wait for responses until timeout '''

    def get_name(self):
        return 'IPv6 scanning'

    def generate_request(self):
        dst_mac = multicast_mac_from_ipv6(self.dst_ip)
        dst_mld_ip = 'ff02::16'
        dst_mld_mac = multicast_mac_from_ipv6(dst_mld_ip)

        mld_pkt = (Ether(src = self.src_mac, dst = dst_mld_mac) /
                   IPv6(src = self.src_ip, dst = dst_mld_ip, hlim = 1) /
                   IPv6ExtHdrHopByHop(options = [RouterAlert(), PadN()]) /
                   ICMPv6MLReportV2() /
                   MLDv2Addr(type = 4, len = 0, multicast_addr = 'ff02::2'))
        ping_pkt = (Ether(src = self.src_mac, dst = dst_mac) /
                    IPv6(src = self.src_ip, dst = self.dst_ip, hlim = 1) /
                    ICMPv6EchoRequest())

        mld_stream = STLStream(packet = STLPktBuilder(pkt = mld_pkt),
                               mode = STLTXSingleBurst(total_pkts = 2))
        ping_stream = STLStream(packet = STLPktBuilder(pkt = ping_pkt),
                                mode = STLTXSingleBurst(total_pkts = 2))
        return [mld_stream, ping_stream]


    def on_pkt_rx(self, pkt, start_ts):
        # convert to scapy
        scapy_pkt = Ether(pkt['binary'])

        if scapy_pkt.haslayer('ICMPv6ND_NS') and scapy_pkt.haslayer('ICMPv6NDOptSrcLLAddr'):
            node_mac = scapy_pkt.getlayer(ICMPv6NDOptSrcLLAddr).lladdr
            node_ip = scapy_pkt.getlayer(IPv6).src
            if node_ip not in self.responses:
                self.send_intermediate(self.generate_ns_na(node_mac, node_ip))

        elif scapy_pkt.haslayer('ICMPv6ND_NA'):
            is_router = scapy_pkt.getlayer(ICMPv6ND_NA).R
            node_ip = scapy_pkt.getlayer(ICMPv6ND_NA).tgt
            dst_ip  = scapy_pkt.getlayer(IPv6).dst
            node_mac = scapy_pkt.src
            if node_ip not in self.responses and dst_ip == self.src_ip:
                self.responses[node_ip] = {'type': 'Router' if is_router else 'Host', 'mac': node_mac}

        elif scapy_pkt.haslayer('ICMPv6EchoReply'):
            node_mac = scapy_pkt.src
            node_ip = scapy_pkt.getlayer(IPv6).src
            if node_ip == self.dst_ip:
                return self.port.ok([{'ipv6': node_ip, 'mac': node_mac}])
            if node_ip not in self.responses:
                self.send_intermediate(self.generate_ns_na(node_mac, node_ip))


    def on_timeout(self):
        return self.port.ok([{'type': v['type'], 'mac':  v['mac'], 'ipv6': k} for k, v in self.responses.items()])


class RXServiceICMPv6(RXServiceIPv6):
    '''
    Ping some IPv6 location.
    If the dest MAC is found from scanning, use it.
    Otherwise, send to default port dest.
    '''

    def __init__(self, port, pkt_size, dst_mac = None, *a, **k):
        super(RXServiceICMPv6, self).__init__(port, *a, **k)
        self.pkt_size = pkt_size
        self.dst_mac  = dst_mac
        self.result = {}

    def get_name(self):
        return 'PING6'

    def pre_execute(self):
        if self.dst_mac is None and not self.port.is_resolved():
            return self.port.err('ping - port has an unresolved destination, cannot determine next hop MAC address')
        return self.port.ok()


    # return a list of streams for request
    def generate_request(self):
        attrs = self.port.get_ts_attr()

        if self.dst_mac is None:
            self.dst_mac = attrs['layer_cfg']['ether']['dst']

        ping_pkt = (Ether(src = self.src_mac, dst = self.dst_mac) / 
                    IPv6(src = self.src_ip, dst = self.dst_ip) /
                    ICMPv6EchoRequest())
        pad = max(0, self.pkt_size - len(ping_pkt))
        ping_pkt /= pad * 'x'

        return STLStream( packet = STLPktBuilder(pkt = ping_pkt), mode = STLTXSingleBurst(total_pkts = 2) )


    def on_pkt_rx(self, pkt, start_ts):
        scapy_pkt = Ether(pkt['binary'])

        if scapy_pkt.haslayer('ICMPv6EchoReply'):
            node_ip = scapy_pkt.getlayer(IPv6).src
            hlim = scapy_pkt.getlayer(IPv6).hlim
            dst_ip = scapy_pkt.getlayer(IPv6).dst
            if dst_ip != self.src_ip: # not our ping
                return

            dt = pkt['ts'] - start_ts
            self.result['formatted_string'] = 'Reply from {0}: bytes={1}, time={2:.2f}ms, hlim={3}'.format(node_ip, len(pkt['binary']), dt * 1000, hlim)
            self.result['src_ip'] = node_ip
            self.result['rtt'] = dt * 1000
            self.result['ttl'] = hlim
            self.result['status'] = 'success'
            return self.port.ok(self.result)

        if scapy_pkt.haslayer('ICMPv6ND_NS') and scapy_pkt.haslayer('ICMPv6NDOptSrcLLAddr'):
            node_mac = scapy_pkt.getlayer(ICMPv6NDOptSrcLLAddr).lladdr
            node_ip = scapy_pkt.getlayer(IPv6).src
            dst_ip = scapy_pkt.getlayer(IPv6).dst
            if dst_ip != self.src_ip: # not our ping
                return
            self.send_intermediate(self.generate_ns_na(node_mac, node_ip))

        if scapy_pkt.haslayer('ICMPv6DestUnreach'):
            node_ip = scapy_pkt.getlayer(IPv6).src
            dst_ip = scapy_pkt.getlayer(IPv6).dst
            if dst_ip != self.src_ip: # not our ping
                return
            self.result['formatted_string'] = 'Reply from {0}: Destination host unreachable'.format(node_ip)
            self.result['status'] = 'unreachable'
            return self.port.ok(self.result)

    # return the str of a timeout err
    def on_timeout(self):
        self.result['formatted_string'] = 'Request timed out.'
        self.result['status'] = 'timeout'
        return self.port.ok(self.result)


