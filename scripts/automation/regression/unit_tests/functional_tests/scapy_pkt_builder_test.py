#!/router/bin/python

import pkt_bld_general_test
from client_utils.scapy_packet_builder import *
from scapy.all import *
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises
import os
import random
import pprint

class CTRexPktBuilderSanitySCapy_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        pass

    def test_simple_vm1(self):
        raw1 = CTRexScRaw( [ CTRexVmDescFlowVar(name="a",min_val="16.0.0.1",max_val="16.0.0.10",init_val="16.0.0.1",size=4,op="inc"),
                              CTRexVmDescWrFlowVar (fv_name="a",pkt_offset= "IP.src"),
                              CTRexVmDescFixIpv4(offset = "IP")]
                          );

        pkt_builder = CScapyTRexPktBuilder();

        py='5'*128
        pkt=Ether()/ \
                 IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)/IP()/py

        # set packet 
        pkt_builder.set_packet(pkt);
        pkt_builder.add_command ( raw1 )
        pkt_builder.compile();

        pkt_builder.dump_scripts ()

        print pkt_builder.get_vm_data()

        assert_equal( pkt_builder.get_vm_data(), {'split_by_var': '', 'instructions': [{'init_val': 268435457, 'name': 'a', 'min_val': 268435457, 'max_val': 268435466, 'size': 4, 'type': 'flow_var', 'op': 'inc'}, {'pkt_offset': 26, 'type': 'write_flow_var', 'name': 'a', 'add_val': 0, 'is_big': True}, {'type': 'fix_checksum_ipv4', 'offset': 14}]})

    def test_simple_no_vm1(self):

        pkt_builder = CScapyTRexPktBuilder();

        py='5'*128
        pkt=Ether()/ \
                 IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)/IP()/py

        # set packet 
        pkt_builder.set_packet(pkt);

        pkt_builder.compile();

        pkt_builder.dump_scripts ()

        assert_equal( pkt_builder.get_vm_data(),
                {   'instructions': [ ],
                    'split_by_var': ''}
        )

    def test_simple_scapy_vlan(self):

        py='5'*(9)
        p1=Ether(src="00:00:00:01:00:00",dst="00:00:00:01:00:00")/ \
                 Dot1Q(vlan=12)/ \
                 Dot1Q(vlan=17)/ \
                 IP(src="10.0.0.10",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)/py

        p1.build();
        p1.dump_layers_offset()
        p1.show2();
        hexdump(p1);
        #wrpcap("ipv4_udp_9k.pcap", p1);

        p_utl=CTRexScapyPktUtl(p1);

        assert_equal(p_utl.get_pkt_layers(),"Ethernet:802.1Q:802.1Q:IP:UDP:Raw")
        assert_equal(p_utl.layer_offset("802.1Q",0),14);
        assert_equal(p_utl.layer_offset("802.1Q",1),18);
        assert_equal(p_utl.get_field_offet_by_str("802|1Q.vlan"),(14,0));
        assert_equal(p_utl.get_field_offet_by_str("802|1Q:1.vlan"),(18,0));
        assert_equal(p_utl.get_field_offet_by_str("IP.src"),(34,4));

    def test_simple_scapy_128_udp(self):
        """
        build 128 byte packet with 0x35 as pyld
        """


        pkt_size =128 
        p1=Ether(src="00:00:00:01:00:00",dst="00:00:00:01:00:00")/ \
                 IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)
        pyld_size=pkt_size-len(p1);

        pkt=p1/('5'*(pyld_size))

        pkt.show2();
        hexdump(pkt);
        assert_equal(len(pkt),128)



    def tearDown(self):
        pass


class CTRexPktBuilderScapy_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        pass;
        #self.pkt_bld = CTRexPktBuilder()
        #self.pkt_bld.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
        #self.pp = pprint.PrettyPrinter(indent=4)

    def tearDown(self):
        pass


if __name__ == "__main__":
    pass

