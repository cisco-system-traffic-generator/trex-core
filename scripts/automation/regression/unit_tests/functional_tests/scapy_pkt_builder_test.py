#!/router/bin/python

import pkt_bld_general_test

#HACK FIX ME START
import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CURRENT_PATH, '../../../trex_control_plane/stl/'))
#HACK FIX ME END
from trex_stl_lib.trex_stl_packet_builder_scapy import *

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
        raw1 = CTRexScRaw( [ CTRexVmDescFlowVar(name="a",min_value="16.0.0.1",max_value="16.0.0.10",init_value="16.0.0.1",size=4,op="inc"),
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

        assert_equal( pkt_builder.get_vm_data(), {'split_by_var': '', 'instructions': [{'name': 'a', 'max_value': 268435466, 'min_value': 268435457, 'init_value': 268435457, 'size': 4, 'type': 'flow_var', 'step':1,'op': 'inc'}, {'is_big_endian': True, 'pkt_offset': 26, 'type': 'write_flow_var',  'name': 'a', 'add_value': 0}, {'pkt_offset': 14, 'type': 'fix_checksum_ipv4'}]} )
                                                 


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


    def test_simple_mac_default(self):

        pkt =  Ether()/IP()/UDP()


        pkt_builder = CScapyTRexPktBuilder(pkt = pkt);

        assert_equal( pkt_builder.is_def_src_mac () ,True)
        assert_equal( pkt_builder.is_def_dst_mac () ,True)

        pkt =  Ether(src="00:00:00:00:00:01")/IP()/UDP()

        pkt_builder = CScapyTRexPktBuilder(pkt = pkt);

        assert_equal( pkt_builder.is_def_src_mac (), False)
        assert_equal( pkt_builder.is_def_dst_mac (), True)

        pkt =  Ether(dst="00:00:00:00:00:01")/IP()/UDP()

        pkt_builder = CScapyTRexPktBuilder(pkt = pkt);

        assert_equal( pkt_builder.is_def_src_mac (),True)
        assert_equal(  pkt_builder.is_def_dst_mac (),False)




    def test_simple_teredo(self):

        pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=3797,sport=3544)/IPv6(src="2001:0:4137:9350:8000:f12a:b9c8:2815",dst="2001:4860:0:2001::68")/UDP(dport=12,sport=1025)/ICMPv6Unknown()

        pkt.build();
        p_utl=CTRexScapyPktUtl(pkt);

        assert_equal( p_utl.get_field_offet_by_str("IPv6.src"), (50,16) )
        assert_equal( p_utl.get_field_offet_by_str("IPv6.dst"), (66,16) )




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

    def test_simple_scapy_9k_ip_len(self):
        """
        build 9k ipv4 len packet
        """


        ip_pkt_size =9*1024
        p_l2=Ether(src="00:00:00:01:00:00",dst="00:00:00:01:00:00");
        p_l3=    IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)
        pyld_size = ip_pkt_size-len(p_l3);

        pkt=p_l2/p_l3/('\x55'*(pyld_size))

        #pkt.show2();
        #hexdump(pkt);
        assert_equal(len(pkt),9*1024+14)

    def test_simple_scapy_ipv6_1(self):
        """
        build ipv6 packet 
        """

        print "start "
        py='\x55'*(64)

        p=Ether()/IPv6()/UDP(dport=12,sport=1025)/py
        #p.build();
        #p.dump_layers_offset()
        hexdump(p);
        p.show2();

        p_utl=CTRexScapyPktUtl(p);

        assert_equal(p_utl.get_field_offet_by_str("IPv6.src"),(22,16));


    def test_simple_vm2(self):
        raw1 = CTRexScRaw( [ CTRexVmDescFlowVar(name="my_valn",min_value=0,max_value=10,init_value=2,size=1,op="inc"),
                             CTRexVmDescWrFlowVar (fv_name="my_valn",pkt_offset= "802|1Q.vlan" ,offset_fixup=3) # fix the offset as valn is bitfield and not supported right now 
                              ]
                          );

        pkt_builder = CScapyTRexPktBuilder();

        py='5'*128
        pkt=Ether()/ \
        Dot1Q(vlan=12)/ \
                 IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)/IP()/py

        # set packet 
        pkt_builder.set_packet(pkt);
        pkt_builder.add_command ( raw1 )
        pkt_builder.compile();


        d= pkt_builder.get_vm_data()
        assert_equal(d['instructions'][1]['pkt_offset'],17)

    def test_simple_vm3(self):
        try:
            raw1 = CTRexScRaw( [ CTRexVmDescFlowVar(name="my_valn",min_value=0,max_value=10,init_value=2,size=1,op="inc"),
                                 CTRexVmDescWrFlowVar (fv_name="my_valn_err",pkt_offset= "802|1Q.vlan" ,offset_fixup=3) # fix the offset as valn is bitfield and not supported right now 
                                  ]
                              );
    
            pkt_builder = CScapyTRexPktBuilder();
    
            py='5'*128
            pkt=Ether()/ \
            Dot1Q(vlan=12)/ \
                     IP(src="16.0.0.1",dst="48.0.0.1")/ \
                     UDP(dport=12,sport=1025)/IP()/py
    
            # set packet 
            pkt_builder.set_packet(pkt);
            pkt_builder.add_command ( raw1 )
            pkt_builder.compile();
    
    
            d= pkt_builder.get_vm_data()
        except  CTRexPacketBuildException as e:
            assert_equal(str(e), "[errcode:-11] 'variable my_valn_err does not exists  '")

    def test_simple_tuple_gen(self):
        vm = CTRexScRaw( [ CTRexVmDescTupleGen (name="tuple"), # define tuple gen 
                             CTRexVmDescWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             CTRexVmDescFixIpv4(offset = "IP"),                                # fix checksum
                             CTRexVmDescWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                                  ]
                              );
        pkt_builder = CScapyTRexPktBuilder();

        py='5'*128
        pkt=Ether()/ \
        Dot1Q(vlan=12)/ \
                 IP(src="16.0.0.1",dst="48.0.0.1")/ \
                 UDP(dport=12,sport=1025)/IP()/py

        # set packet 
        pkt_builder.set_packet(pkt);
        pkt_builder.add_command ( vm )
        pkt_builder.compile();
        d= pkt_builder.get_vm_data()
        pkt_builder.dump_vm_data_as_yaml()

        assert_equal(d['instructions'][1]['pkt_offset'],30)
        assert_equal(d['instructions'][3]['pkt_offset'],38)

    def test_simple_random_pkt_size(self):

        ip_pkt_size = 9*1024
        p_l2 = Ether();
        p_l3 = IP(src="16.0.0.1",dst="48.0.0.1")
        p_l4 = UDP(dport=12,sport=1025)
        pyld_size = ip_pkt_size-len(p_l3/p_l4);

        pkt =p_l2/p_l3/p_l4/('\x55'*(pyld_size))

        l3_len_fix =-(len(p_l2));
        l4_len_fix =-(len(p_l2/p_l3));

        vm = CTRexScRaw( [ CTRexVmDescFlowVar(name="fv_rand", min_value=64, max_value=len(pkt), size=2, op="random"),
                           CTRexVmDescTrimPktSize("fv_rand"), # total packet size
                           CTRexVmDescWrFlowVar(fv_name="fv_rand", pkt_offset= "IP.len", add_val=l3_len_fix), 
                           CTRexVmDescFixIpv4(offset = "IP"),                                # fix checksum
                           CTRexVmDescWrFlowVar(fv_name="fv_rand", pkt_offset= "UDP.len", add_val=l4_len_fix)  
                          ]
                       )
        pkt_builder = CScapyTRexPktBuilder();

        # set packet 
        pkt_builder.set_packet(pkt);
        pkt_builder.add_command ( vm )
        pkt_builder.compile();
        d= pkt_builder.get_vm_data()
        pkt_builder.dump_vm_data_as_yaml()

        assert_equal(d['instructions'][0]['max_value'],9230)
        assert_equal(d['instructions'][2]['pkt_offset'],16)
        assert_equal(d['instructions'][4]['pkt_offset'],38)

    def test_simple_pkt_loader(self):
        p=RawPcapReader("stl/golden/basic_imix_golden.cap")
        print ""
        for pkt in p:
            print pkt[1]
            print hexdump(str(pkt[0]))
            break;

    def test_simple_pkt_loader1(self):

        pkt_builder = CScapyTRexPktBuilder(pkt = "stl/golden/udp_590.cap");
        print ""
        pkt_builder.dump_as_hex()
        r = pkt_builder.pkt_raw
        assert_equal(ord(r[1]),0x50)
        assert_equal(ord(r[0]),0x00)
        assert_equal(ord(r[0x240]),0x16)
        assert_equal(ord(r[0x24d]),0x79)
        assert_equal(len(r),590)

        print len(r)

    def test_simple_pkt_loader2(self):

        pkt_builder = CScapyTRexPktBuilder(pkt = "stl/golden/basic_imix_golden.cap");
        assert_equal(pkt_builder.pkt_layers_desc (), "Ethernet:IP:UDP:Raw");

    def test_simple_pkt_loader3(self):

        #pkt_builder = CScapyTRexPktBuilder(pkt = "stl/golden/basic_imix_golden.cap");
        #r = pkt_builder.pkt_raw
        #print ""
        #hexdump(str(r))


        #print pkt_builder.pkt_layers_desc ()


        #pkt_builder.set_packet(pkt);

        py='\x55'*(64)

        p=Ether()/IP()/UDP(dport=12,sport=1025)/py
        pkt_str = str(p);
        print ""
        hexdump(pkt_str);
        scapy_pkt = Ether(pkt_str);
        scapy_pkt.show2();

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

