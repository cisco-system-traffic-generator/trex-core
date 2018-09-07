from scapy.layers.dhcp import DHCP, BOOTP
from scapy.layers.l2 import Ether, LLC, SNAP
from scapy.layers.dot11 import Dot11, Dot11QoS
from scapy.layers.inet import IP, UDP
from scapy.utils import checksum, inet_aton
from scapy.contrib.capwap import Dot11_swapped
from scapy.packet import NoPayload
from collections import namedtuple, OrderedDict
import socket
import random

from trex.common.services.trex_service_fast_parser import FastParser, ParserError

import struct

import pprint


class DHCPParser(FastParser):

    def get_options(self, pkt_bytes, info):
        # min length
        if len(pkt_bytes) < info['offset']:
            return None

        options = pkt_bytes[info['offset']:]

        opt = OrderedDict()
        index = 0
        while index < len(options):

            # o  = ord(options[index])
            o = options[index]
            index += 1

            # end
            if o == 255:
                break

            # pad
            elif o == 0:
                continue

            # fetch length
            # olen = ord(options[index])
            olen = options[index]
            index += 1

            # message type
            if o in self.opts:
                ot = self.opts[o]
                if ot['type'] == 'byte':
                    opt[ot['name']] = struct.unpack_from(
                        '!B', options, index)[0]

                elif ot['type'] == 'int':
                    opt[ot['name']] = struct.unpack_from(
                        '!I', options, index)[0]

                elif ot['type'] == 'str':
                    opt[ot['name']] = struct.unpack_from(
                        '!{0}s'.format(olen), options, index)[0]

                else:
                    raise Exception('unknown type: {0}'.format(ot['type']))

            # advance
            index += olen

        return opt

    def set_options(self, pkt_bytes, info, options):
        output = bytes()

        for o, v in options.items():
            if o in self.opts:
                ot = self.opts[o]

                # write tag
                output += struct.pack('!B', ot['id'])

                # write the size and value
                if ot['type'] == 'byte':
                    output += struct.pack('!B', 1)
                    output += struct.pack('!B', v)

                elif ot['type'] == 'int':
                    output += struct.pack('!B', 4)
                    output += struct.pack('!I', v)

                elif ot['type'] == 'str':
                    output += struct.pack('!B', len(v))
                    output += struct.pack('!{0}s'.format(len(v)), v)

        # write end
        output += struct.pack('!B', 255)

        return pkt_bytes[:info['offset']] + output + pkt_bytes[info['offset'] + len(output):]

class EthernetDHCPParser(DHCPParser):

    def __init__(self):
        base_pkt = (
            Ether(dst="ff:ff:ff:ff:ff:ff") /
            IP(src="0.0.0.0", dst="255.255.255.255") /
            UDP(sport=68, dport=67, chksum=0) /
            BOOTP(chaddr=b'123456', xid=55555, yiaddr='0.0.0.0') /
            DHCP(options=[("message-type", "discover"), "end"])
        )

        FastParser.__init__(self, base_pkt)

        self.add_field('Ethernet.dst', 'dstmac')
        self.add_field('Ethernet.src', 'srcmac')

        self.add_field('IP.ihl',      'ihl', fmt="!B")
        self.add_field('IP.dst',      'dstip')
        self.add_field('IP.src',      'srcip')
        self.add_field('IP.chksum',   'chksum')
        self.add_field("IP.id", 'ip_id')
        self.add_field("IP.len", "ip_len")

        # self.add_field('UDP.chksum', 'udp_chksum')
        self.add_field('UDP.len', 'udp_len')

        self.add_field('BOOTP.xid', 'xid')
        self.add_field('BOOTP.chaddr', 'chaddr')
        self.add_field('BOOTP.ciaddr', 'ciaddr')
        self.add_field('BOOTP.yiaddr', 'yiaddr')
        self.add_field('DHCP options.options', 'options',
                       getter=self.get_options, setter=self.set_options)

        msg_types = [{'id': 1, 'name': 'discover'},
                     {'id': 2, 'name': 'offer'},
                     {'id': 3, 'name': 'request'},
                     {'id': 5, 'name': 'ack'},
                     {'id': 6, 'name': 'nack'},
                     ]

        self.msg_types = {}

        for t in msg_types:
            self.msg_types[t['id']] = t
            self.msg_types[t['name']] = t

        opts = [{'id': 53, 'name': 'message-type',   'type': 'byte'},
                {'id': 54, 'name': 'server_id',      'type': 'int'},
                {'id': 50, 'name': 'requested_addr', 'type': 'int'},
                {'id': 51, 'name': 'lease-time',     'type': 'int'},
                {'id': 58, 'name': 'renewal_time',   'type': 'int'},
                {'id': 59, 'name': 'rebinding_time', 'type': 'int'},
                {'id': 1,  'name': 'subnet_mask',    'type': 'int'},
                {'id': 15, 'name': 'domain',         'type': 'str'},
                {'id': 3,  'name': 'router',         'type': 'int'},
                {'id': 43, 'name': 'vendor-specific', 'type': 'int'},
                ]

        self.opts = {}

        for opt in opts:
            self.opts[opt['id']] = opt
            self.opts[opt['name']] = opt

        self.obj = self.clone()

    def disc(self, xid, client_mac):
        """ Generate a DHCPDISCOVER packet. (Ethernet)

        Args:
            xid: dhcp transaction id
            client_mac: mac address of the client
        """
        obj = self.clone()
        obj.options = {'message-type': 1}

        obj.xid = xid
        obj.chaddr = client_mac

        obj.srcmac = client_mac

        obj.fix_chksum()

        return obj.raw()

    def req(self, xid, client_mac, yiaddr, server_ip):
        """Generate a DHCPREQUEST packet. (Ethernet)

        Args:
            xid: dhcp transaction id
            client_mac: mac address of the client
            yiaddr: offered ip address for the client in the last DHCPOFFER
            server_ip: ip of the dhcp server
        """

        # generate a new packet
        
        obj = self.clone()
        # obj = self.obj
        obj.options = {'message-type': 3, 'requested_addr': yiaddr, 'server_id': server_ip}
        # compensate for added options
        obj.udp_len += 12
        obj.ip_len += 12

        obj.xid = xid
        obj.chaddr = client_mac

        obj.srcmac = client_mac

        obj.fix_chksum()
        raw = obj.raw()

        obj.udp_len -= 12
        obj.ip_len -= 12

        return raw

class Dot11DHCPParser(DHCPParser):

    def __init__(self):

        base_pkt = (
            Dot11(FCfield='to-DS',
                    subtype=8,
                    type='Data',
                    ID=0,
                    addr1="ff:ff:ff:ff:ff:ff",
                    addr2="ff:ff:ff:ff:ff:ff",
                    addr3="ff:ff:ff:ff:ff:ff",
                    ) /
            Dot11QoS() /
            LLC(dsap=170, ssap=170, ctrl=3) /  # aa, aa, 3
            SNAP(code=2048) /  # ethertype : ipv4
            IP(src="0.0.0.0", dst="255.255.255.255") /
            UDP(sport=68, dport=67, chksum=0) /
            BOOTP(chaddr=b'123456', xid=55555, yiaddr='0.0.0.0') /
            DHCP(options=[("message-type", "discover"), "end"])
        )

        FastParser.__init__(self, base_pkt)

        self.add_field('802.11.addr1', 'addr1')
        self.add_field('802.11.addr2', 'addr2')
        self.add_field('802.11.addr3', 'addr3')

        # for dhcp offer compatibility with ethernet/dot11
        self.add_field('802.11.addr3', 'srcmac')
        self.add_field('802.11.addr1', 'dstmac')

        self.add_field('IP.ihl',      'ihl', fmt="!B")
        self.add_field('IP.dst',      'dstip')
        self.add_field('IP.src',      'srcip')
        self.add_field('IP.chksum',   'chksum')
        self.add_field("IP.id", 'ip_id')
        self.add_field("IP.len", "ip_len")

        # self.add_field('UDP.chksum', 'udp_chksum')
        self.add_field('UDP.len', 'udp_len')

        self.add_field('BOOTP.xid', 'xid')
        self.add_field('BOOTP.chaddr', 'chaddr')
        self.add_field('BOOTP.ciaddr', 'ciaddr')
        self.add_field('BOOTP.yiaddr', 'yiaddr')
        self.add_field('DHCP options.options', 'options',
                       getter=self.get_options, setter=self.set_options)

        msg_types = [{'id': 1, 'name': 'discover'},
                     {'id': 2, 'name': 'offer'},
                     {'id': 3, 'name': 'request'},
                     {'id': 5, 'name': 'ack'},
                     {'id': 6, 'name': 'nack'},
                     ]

        self.msg_types = {}

        for t in msg_types:
            self.msg_types[t['id']] = t
            self.msg_types[t['name']] = t

        opts = [{'id': 53, 'name': 'message-type',   'type': 'byte'},
                {'id': 54, 'name': 'server_id',      'type': 'int'},
                {'id': 50, 'name': 'requested_addr', 'type': 'int'},
                {'id': 51, 'name': 'lease-time',     'type': 'int'},
                {'id': 58, 'name': 'renewal_time',   'type': 'int'},
                {'id': 59, 'name': 'rebinding_time', 'type': 'int'},
                {'id': 1,  'name': 'subnet_mask',    'type': 'int'},
                {'id': 15, 'name': 'domain',         'type': 'str'},
                {'id': 3,  'name': 'router',         'type': 'int'},
                {'id': 43, 'name': 'vendor-specific', 'type': 'int'},
                ]

        self.opts = {}

        for opt in opts:
            self.opts[opt['id']] = opt
            self.opts[opt['name']] = opt

        self.obj = self.clone()

    def disc(self, xid, client_mac, ra_mac):
        """ Generate a DHCPDISCOVER packet. (Dot11)

        Args:
            xid: dhcp transaction id
            client_mac: mac address of the client
            ra_mac: client's ap radio mac
        """

        # generate a new packet
        # obj = self.obj
        obj = self.clone()
        obj.options = {'message-type': 1}

        obj.xid = xid
        obj.chaddr = client_mac

        obj.addr1 = ra_mac
        obj.addr2 = client_mac
        obj.addr3 = b'\xff\xff\xff\xff\xff\xff'

        obj.fix_chksum()

        return obj.raw()

    def req(self, xid, client_mac, yiaddr, ra_mac, server_ip):
        """Generate a DHCPREQUEST packet. (Dot11)

        Args:
            xid: dhcp transaction id
            client_mac: mac address of the client
            yiaddr: offered ip address for the client in the last DHCPOFFER
            ra_mac: client's ap radio mac
            server_ip: ip of the dhcp server
        """

        # generate a new packet
        
        obj = self.clone()
        # obj = self.obj
        obj.options = {'message-type': 3, 'requested_addr': yiaddr, 'server_id': server_ip}
        # compensate for added options
        obj.udp_len += 12
        obj.ip_len += 12

        obj.xid = xid
        obj.chaddr = client_mac

        obj.addr1 = ra_mac  # chaddr
        obj.addr2 = client_mac  # radio_mac
        obj.addr3 = b'\xff\xff\xff\xff\xff\xff'

        obj.fix_chksum()
        raw = obj.raw()

        obj.udp_len -= 12
        obj.ip_len -= 12

        return raw

    # def release(self, xid, client_mac, client_ip, station_mac, server_mac, server_ip):
    #     '''
    #         generate a release request packet
    #     '''

    #     # generate a new packet
    #     obj = self.clone()

    #     obj.addr3 = server_mac
    #     obj.addr1 = client_mac
    #     obj.addr2 = station_mac

    #     obj.dstip = server_ip
    #     obj.srcip = client_ip

    #     obj.fix_chksum()

    #     obj.ciaddr = client_ip
    #     obj.chaddr = client_mac
    #     obj.xid = xid

    #     obj.options = {'message-type': 7, 'server_id': server_ip}

    #     return obj.raw()
