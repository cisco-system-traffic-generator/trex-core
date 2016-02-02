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

