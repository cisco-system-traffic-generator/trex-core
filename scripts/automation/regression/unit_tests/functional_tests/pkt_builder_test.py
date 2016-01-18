#!/router/bin/python

import pkt_bld_general_test
from client_utils.packet_builder import *
import dpkt
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises
import os
import random
import pprint

class CTRexPktBuilderSanity_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        pass

    def test_decode_ip_addr(self):
        # test ipv4 case
        assert_equal(CTRexPktBuilder._decode_ip_addr('1.2.3.4', "ipv4"), '\x01\x02\x03\x04')
        assert_equal(CTRexPktBuilder._decode_ip_addr('127.0.0.1', "ipv4"), '\x7F\x00\x00\x01')
        assert_raises(CTRexPktBuilder.IPAddressError, CTRexPktBuilder._decode_ip_addr, '1.2.3.4.5', "ipv4")
        assert_raises(CTRexPktBuilder.IPAddressError, CTRexPktBuilder._decode_ip_addr, '1.2.3.4', "ipv6")
        # test ipv6  case
        assert_equal(CTRexPktBuilder._decode_ip_addr("5001::DB8:1:3333:1:1", "ipv6"),
                                                     'P\x01\x00\x00\x00\x00\r\xb8\x00\x0133\x00\x01\x00\x01')
        assert_raises(CTRexPktBuilder.IPAddressError, CTRexPktBuilder._decode_ip_addr, 
                      '2001::DB8:1:2222::1:1', "ipv6")

    def test_decode_mac_addr(self):
        assert_equal(CTRexPktBuilder._decode_mac_addr('00:de:34:ef:2e:f4'), '\x00\xde4\xef.\xf4')
        assert_equal(CTRexPktBuilder._decode_mac_addr('00-de-55-ef-2e-f4'), '\x00\xdeU\xef.\xf4')
        assert_raises(CTRexPktBuilder.MACAddressError, CTRexPktBuilder._decode_mac_addr, 
                      '00:de:34:ef:2e:f4:f4')
        assert_raises(CTRexPktBuilder.MACAddressError, CTRexPktBuilder._decode_mac_addr, 
                      '1.2.3.4')
        assert_raises(CTRexPktBuilder.MACAddressError, CTRexPktBuilder._decode_mac_addr, 
                      '00 de 34 ef 2e f4 f4')

    def test_gen_layer_name(self):
        pkt = CTRexPktBuilder()
        assert_equal(pkt._gen_layer_name("eth"), "eth_1")
        pkt._pkt_by_hdr = {'eth':None} # mock header pointer data
        assert_equal(pkt._gen_layer_name("eth"), "eth_1")
        pkt._pkt_by_hdr.update({'eth_1':None}) # more mock header pointer data
        assert_equal(pkt._gen_layer_name("eth"), "eth_2")

    def test_set_layer_attr_basic(self):
        pkt = CTRexPktBuilder()
        pkt._pkt_by_hdr['ip'] = dpkt.ip.IP()
        # case 1 - test full value assignment
        pkt.set_layer_attr('ip', 'src', '\x01\x02\x03\x04')
        assert_equal(pkt._pkt_by_hdr['ip'].src, '\x01\x02\x03\x04')
        # case 2 - test bit assignment
        pkt.set_layer_bit_attr('ip', 'off', dpkt.ip.IP_DF)
        pkt.set_layer_bit_attr('ip', 'off', dpkt.ip.IP_MF)
        assert_equal(bin(pkt._pkt_by_hdr['ip'].off), '0b110000000000000')
        # case 3 - test assignment of not-exist attribute
        assert_raises(ValueError, pkt.set_layer_bit_attr, 'ip', 'src_dst', 0)
        # case 4.1 - test assignment of data attribute - without dpkt.Packet object
        assert_raises(ValueError, pkt.set_layer_bit_attr, 'ip', 'data', "Not a dpkt.Packet object")
        # case 4.2 - test assignment of data attribute - with dpkt.Packet object - tested under CTRexPktBuilder_Test class
#       tcp = dpkt.tcp.TCP()
        self.print_packet(pkt._pkt_by_hdr['ip'])
#       pkt.set_layer_attr('ip', 'data', tcp)
        # case 5 - test assignment of not-exist layer
        assert_raises(KeyError, pkt.set_layer_bit_attr, 'no_such_layer', 'src', 0)

    def tearDown(self):
        pass


class CTRexPktBuilder_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        self.pkt_bld = CTRexPktBuilder()
        self.pkt_bld.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
        self.pp = pprint.PrettyPrinter(indent=4)

    def test_add_pkt_layer(self):
        ip = dpkt.ip.IP(src='\x01\x02\x03\x04', dst='\x05\x06\x07\x08', p=1)
        self.pkt_bld.add_pkt_layer("l3", ip)
        tcp = dpkt.tcp.TCP(sport = 8080)
        self.pkt_bld.add_pkt_layer("l4_tcp", tcp)
        assert_equal(len(self.pkt_bld._pkt_by_hdr), 3)
        assert_equal(self.pkt_bld._pkt_by_hdr.keys(), ['l2', 'l3', 'l4_tcp'])
        self.print_packet(self.pkt_bld._packet)
        assert_raises(ValueError, self.pkt_bld.add_pkt_layer, 'l2', dpkt.ethernet.Ethernet())

    def test_set_ip_layer_addr(self):
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.set_ip_layer_addr("l3", "src", "1.2.3.4")
        self.print_packet(self.pkt_bld._packet)
        assert_equal(self.pkt_bld._pkt_by_hdr['l3'].src, '\x01\x02\x03\x04')
        # check that only IP layer is using this function
        assert_raises(ValueError, self.pkt_bld.set_ip_layer_addr, 'l2', "src", "1.2.3.4")
        
    def test_calc_offset(self):
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        assert_equal(self.pkt_bld._calc_offset("l3", "src", 4), (14, 14+12))

    def test_set_ipv6_layer_addr(self):
        ip6 = dpkt.ip6.IP6()
        self.pkt_bld.add_pkt_layer("l3", ip6)
        self.pkt_bld.set_ipv6_layer_addr("l3", "src", "5001::DB8:1:3333:1:1")
        self.print_packet(self.pkt_bld._packet)
        assert_equal(self.pkt_bld._pkt_by_hdr['l3'].src, 'P\x01\x00\x00\x00\x00\r\xb8\x00\x0133\x00\x01\x00\x01')
        # check that only IP layer is using this function
        assert_raises(ValueError, self.pkt_bld.set_ipv6_layer_addr, 'l2', "src", "5001::DB8:1:3333:1:1")

    def test_set_eth_layer_addr(self):
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.set_eth_layer_addr("l2", "src", "00:de:34:ef:2e:f4")
        self.print_packet(self.pkt_bld._packet)
        assert_equal(self.pkt_bld._pkt_by_hdr['l2'].src, '\x00\xde4\xef.\xf4')
        # check that only IP layer is using this function
        assert_raises(ValueError, self.pkt_bld.set_eth_layer_addr, 'l3', "src", "\x00\xde4\xef.\xf4")

    def test_set_layer_attr(self):
        # extend the set_layer_attr_basic test by handling the following case:
        # replace some header data with another layer, causing other layers to disconnect
        # this also tests the _reevaluate_packet method
        ip = dpkt.ip.IP(src='\x01\x02\x03\x04', dst='\x05\x06\x07\x08', p=1)
        self.pkt_bld.add_pkt_layer("l3_ip", ip)
        tcp = dpkt.tcp.TCP(sport = 8080)
        self.pkt_bld.add_pkt_layer("l4_tcp", tcp)
        # sanity: try changing data attr with non-dpkt.Packet instance
        assert_raises(ValueError, self.pkt_bld.set_layer_attr, 'l2', 'data', "HelloWorld")
        # now, add different L3 layer instead of existting one, L4 would disconnect
        old_layer_count = len(self.pkt_bld._pkt_by_hdr)
        new_ip = dpkt.ip.IP(src='\x05\x06\x07\x08', dst='\x01\x02\x03\x04')
        print "\nBefore disconnecting layers:"
        print "============================",
        self.print_packet(self.pkt_bld._packet)
        self.pkt_bld.set_layer_attr('l2', 'data', new_ip)
        print "\nAfter disconnecting layers:"
        print "===========================",
        self.print_packet(self.pkt_bld._packet)
        assert_not_equal(old_layer_count, len(self.pkt_bld._pkt_by_hdr))
        assert_equal(len(self.pkt_bld._pkt_by_hdr), 1)  # only Eth layer appears

    def test_set_pkt_payload(self):
        payload = "HelloWorld"
        # test for setting a payload to an empty packet
        empty_pkt = CTRexPktBuilder()
        assert_raises(AttributeError, empty_pkt.set_pkt_payload, payload)
        # add content to packet
        ip = dpkt.ip.IP(src='\x01\x02\x03\x04', dst='\x05\x06\x07\x08', p=1)
        self.pkt_bld.add_pkt_layer("l3_ip", ip)
        tcp = dpkt.tcp.TCP(sport = 8080)
        self.pkt_bld.add_pkt_layer("l4_tcp", tcp)
        # now, set a payload for the packet
        self.pkt_bld.set_pkt_payload(payload)
        self.print_packet(self.pkt_bld._packet)
        assert_equal(self.pkt_bld._pkt_by_hdr['l4_tcp'].data, payload)

    def test_load_packet(self):
        # add content to packet 
        ip = dpkt.ip.IP(src='\x01\x02\x03\x04', dst='\x05\x06\x07\x08', p=1)
        self.pkt_bld.add_pkt_layer("l3_ip", ip)
        tcp = dpkt.tcp.TCP(sport = 8080)
        self.pkt_bld.add_pkt_layer("l4_tcp", tcp)
        self.pkt_bld.set_pkt_payload("HelloWorld")

        new_pkt = CTRexPktBuilder()
        new_pkt.load_packet(self.pkt_bld._packet)
        self.print_packet(new_pkt._packet)
        assert_equal(len(new_pkt._pkt_by_hdr), 4)
        assert_equal(new_pkt._pkt_by_hdr.keys(),
                     ['ip_1', 
                      'tcp_1',
                      'pkt_final_payload',
                      'ethernet_1'
                      ]
                     )
        assert_equal(new_pkt._pkt_by_hdr['pkt_final_payload'], "HelloWorld")

    def test_get_packet(self):
        # get a pointer to the packet
        assert(self.pkt_bld.get_packet(get_ptr=True) is self.pkt_bld._packet)
        # get a copy of the packet
        assert(not(self.pkt_bld.get_packet() is self.pkt_bld._packet))

    def test_get_layer(self):
        assert_equal(self.pkt_bld.get_layer('no_such_layer'), None)
        assert(not(self.pkt_bld.get_layer('l2') is self.pkt_bld._packet))
        assert(type(self.pkt_bld.get_layer('l2')).__name__, "ethernet")

    def test_dump_to_pcap(self):
        # set Ethernet layer attributes
        self.pkt_bld.set_eth_layer_addr("l2", "src", "00:15:17:a7:75:a3")
        self.pkt_bld.set_eth_layer_addr("l2", "dst", "e0:5f:b9:69:e9:22")
        self.pkt_bld.set_layer_attr("l2", "type", dpkt.ethernet.ETH_TYPE_IP)
        # set IP layer attributes
        self.pkt_bld.add_pkt_layer("l3_ip", dpkt.ip.IP())
        self.pkt_bld.set_ip_layer_addr("l3_ip", "src", "21.0.0.2")
        self.pkt_bld.set_ip_layer_addr("l3_ip", "dst", "22.0.0.12")
        self.pkt_bld.set_layer_attr("l3_ip", "p", dpkt.ip.IP_PROTO_TCP)
        # set TCP layer attributes
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_layer_attr("l4_tcp", "sport", 13311)
        self.pkt_bld.set_layer_attr("l4_tcp", "dport", 80)
        self.pkt_bld.set_layer_attr("l4_tcp", "flags", 0)
        self.pkt_bld.set_layer_attr("l4_tcp", "win", 32768)
        self.pkt_bld.set_layer_attr("l4_tcp", "seq", 0)
        # set packet payload, for example HTTP GET request
        self.pkt_bld.set_pkt_payload('GET /10k_60k HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n')
        # finally, set IP header len with relation to payload data
        self.pkt_bld.set_layer_attr("l3_ip", "len", len(self.pkt_bld.get_layer('l3_ip')))

        filepath = "unit_tests/functional_tests/test.pcap"
        self.pkt_bld.dump_pkt_to_pcap(filepath)
        assert os.path.isfile(filepath)
        # remove pcap after creation - masked for now
        # os.remove(filepath)
        filepath = "/not/a/valid/path/test.pcap"
        assert_raises(IOError, self.pkt_bld.dump_pkt_to_pcap, filepath)
        # check that dump is not available for empty packet
        new_pkt = CTRexPktBuilder()
        assert_raises(CTRexPktBuilder.EmptyPacketError, new_pkt.dump_pkt_to_pcap, filepath)

    def test_dump_pkt(self):
        # check that dump is not available for empty packet
        new_pkt = CTRexPktBuilder()
        assert_raises(CTRexPktBuilder.EmptyPacketError, new_pkt.dump_pkt)

        # set Ethernet layer attributes
        self.pkt_bld.set_eth_layer_addr("l2", "src", "00:15:17:a7:75:a3")
        self.pkt_bld.set_eth_layer_addr("l2", "dst", "e0:5f:b9:69:e9:22")
        self.pkt_bld.set_layer_attr("l2", "type", dpkt.ethernet.ETH_TYPE_IP)
        # set IP layer attributes
        self.pkt_bld.add_pkt_layer("l3_ip", dpkt.ip.IP())
        self.pkt_bld.set_ip_layer_addr("l3_ip", "src", "21.0.0.2")
        self.pkt_bld.set_ip_layer_addr("l3_ip", "dst", "22.0.0.12")
        self.pkt_bld.set_layer_attr("l3_ip", "p", dpkt.ip.IP_PROTO_ICMP)
        # set ICMP layer attributes
        self.pkt_bld.add_pkt_layer("icmp", dpkt.icmp.ICMP())
        self.pkt_bld.set_layer_attr("icmp", "type", dpkt.icmp.ICMP_ECHO)
        # set Echo(ICMP) layer attributes
        self.pkt_bld.add_pkt_layer("icmp_echo", dpkt.icmp.ICMP.Echo())
        self.pkt_bld.set_layer_attr("icmp_echo", "id", 24528)
        self.pkt_bld.set_layer_attr("icmp_echo", "seq", 11482)
        self.pkt_bld.set_pkt_payload('hello world')

        # finally, set IP header len with relation to payload data
        self.pkt_bld.set_layer_attr("l3_ip", "len", len(self.pkt_bld.get_layer('l3_ip')))

        self.print_packet(self.pkt_bld.get_packet())
        assert_equal(self.pkt_bld.dump_pkt(), {
                     'binary': [224, 95, 185, 105, 233, 34, 0, 21, 23, 167, 117, 163, 8, 0, 69, 0, 0, 39, 0, 0, 0, 0, 64, 1, 79, 201, 21, 0, 0, 2, 22, 0, 0, 12, 8, 0, 217, 134, 95, 208, 44, 218, 104, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100],
                     'meta': '',
                    })

    def test_set_vm_ip_range_ipv4(self):
        # set some mock packet
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_pkt_payload("HelloWorld")

        self.pkt_bld.set_vm_ip_range("l3", "src", 
                                     "10.0.0.1", "10.0.0.1", "10.0.0.255", 1,
                                     "inc")
#       self.pkt_bld.set_vm_custom_range(layer_name="l3",
#                                        hdr_field="tos",
#                                        init_val="10", start_val="10", end_val="200", add_val=2, val_size=1,
#                                        operation="inc")
        print ''
        self.pp.pprint(self.pkt_bld.vm.dump())
        assert_equal(self.pkt_bld.vm.dump(),
                {   'instructions': [   {   'init_value': '167772161',
                                            'max_value': '167772415',
                                            'min_value': '167772161',
                                            'name': 'l3__src',
                                            'op': 'inc',
                                            'size': 4,
                                            'type': 'flow_var'},
                                        {   'add_value': 1,
                                            'is_big_endian': False,
                                            'name': 'l3__src',
                                            'pkt_offset': 26,
                                            'type': 'write_flow_var'},
                                        {   'pkt_offset': 14, 'type': 'fix_checksum_ipv4'}],
                    'split_by_var': ''}
        )

    def test_set_vm_ip_range_ipv4_no_checksum(self):
        # set some mock packet
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_pkt_payload("HelloWorld")

        self.pkt_bld.set_vm_ip_range(ip_layer_name="l3", 
                                     ip_field="src", 
                                     ip_init="10.0.0.1", ip_start="10.0.0.1", ip_end="10.0.0.255", 
                                     add_value=1,
                                     operation="inc",
                                     add_checksum_inst=False)
        print ''
        self.pp.pprint(self.pkt_bld.vm.dump())
        assert_equal(self.pkt_bld.vm.dump(),
                {   'instructions': [   {   'init_value': '167772161',
                                            'max_value': '167772415',
                                            'min_value': '167772161',
                                            'name': 'l3__src',
                                            'op': 'inc',
                                            'size': 4,
                                            'type': 'flow_var'},
                                        {   'add_value': 1,
                                            'is_big_endian': False,
                                            'name': 'l3__src',
                                            'pkt_offset': 26,
                                            'type': 'write_flow_var'}],
                    'split_by_var': ''}
        )

    def test_set_vm_ip_range_ipv6(self):
        # set some mock packet
        ip6 = dpkt.ip6.IP6()
        self.pkt_bld.add_pkt_layer("l3", ip6)
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_pkt_payload("HelloWorld")

        self.pkt_bld.set_vm_ip_range(ip_layer_name="l3", 
                                     ip_field="src", 
                                     ip_init="5001::DB8:1:3333:1:1", ip_start="5001::DB8:1:3333:1:1", ip_end="5001::DB8:1:3333:1:F", 
                                     add_value=1,
                                     operation="inc",
                                     ip_type="ipv6")
        print ''
        self.pp.pprint(self.pkt_bld.vm.dump())
        assert_equal(self.pkt_bld.vm.dump(),
                {   'instructions': [   {   'init_value': '65537',
                                            'max_value': '65551',
                                            'min_value': '65537',
                                            'name': 'l3__src',
                                            'op': 'inc',
                                            'size': 4,
                                            'type': 'flow_var'},
                                        {   'add_value': 1,
                                            'is_big_endian': False,
                                            'name': 'l3__src',
                                            'pkt_offset': 34,
                                            'type': 'write_flow_var'}],
                    'split_by_var': ''}
        )

    def test_set_vm_eth_range(self):
        pass

    def test_set_vm_custom_range(self):
        # set some mock packet
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_pkt_payload("HelloWorld")

        self.pkt_bld.set_vm_custom_range(layer_name="l3", 
                                         hdr_field="tos", 
                                         init_val=10, start_val=10, end_val=200, add_val=2, val_size=1,
                                         operation="inc")
        print ''
        self.pp.pprint(self.pkt_bld.vm.dump())
        assert_equal(self.pkt_bld.vm.dump(),
                {   'instructions': [   {   'init_value': '10',
                                            'max_value': '200',
                                            'min_value': '10',
                                            'name': 'l3__tos',
                                            'op': 'inc',
                                            'size': 1,
                                            'type': 'flow_var'},
                                        {   'add_value': 2,
                                            'is_big_endian': False,
                                            'name': 'l3__tos',
                                            'pkt_offset': 15,
                                            'type': 'write_flow_var'},
                                        {   'pkt_offset': 14, 'type': 'fix_checksum_ipv4'}],
                    'split_by_var': ''}
        )

    def test_various_ranges(self):
        # set some mock packet
        ip = dpkt.ip.IP()
        self.pkt_bld.add_pkt_layer("l3", ip)
        self.pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
        self.pkt_bld.set_pkt_payload("HelloWorld")

        self.pkt_bld.set_vm_ip_range("l3", "src", 
                                     "10.0.0.1", "10.0.0.1", "10.0.0.255", 1,
                                     "inc")
        self.pkt_bld.set_vm_custom_range(layer_name="l3",
                                         hdr_field="tos",
                                         init_val=10, start_val=10, end_val=200, add_val=2, val_size=1,
                                         operation="inc")
        print ''
        self.pp.pprint(self.pkt_bld.vm.dump())
        assert_equal(self.pkt_bld.vm.dump(),
                {'instructions': [{'init_value': '167772161',
                                   'max_value': '167772415',
                                   'min_value': '167772161',
                                   'name': 'l3__src',
                                   'op': 'inc',
                                   'size': 4,
                                   'type': 'flow_var'},
                                  {'init_value': '10',
                                   'max_value': '200',
                                   'min_value': '10',
                                   'name': 'l3__tos',
                                   'op': 'inc',
                                   'size': 1,
                                   'type': 'flow_var'},
                                  {'add_value': 2,
                                   'is_big_endian': False,
                                   'name': 'l3__tos',
                                   'pkt_offset': 15,
                                   'type': 'write_flow_var'},
                                  {'add_value': 1,
                                   'is_big_endian': False,
                                   'name': 'l3__src',
                                   'pkt_offset': 26,
                                   'type': 'write_flow_var'},
                                  {'pkt_offset': 14, 'type': 'fix_checksum_ipv4'}],
                 'split_by_var': ''}
        )

    def tearDown(self):
        pass


if __name__ == "__main__":
    pass

