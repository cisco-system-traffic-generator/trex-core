# scapy server unit test

import sys,os
scapy_server_path = os.path.abspath(os.path.join(os.pardir, 'trex_control_plane', 'stl', 'services','scapy_server'))
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir, 'trex_control_plane','stl'))
sys.path.append(scapy_server_path)
sys.path.append(stl_pathname)


#import stl_path
import trex_stl_lib
from trex_stl_lib.api import *
from copy import deepcopy

import tempfile
import hashlib
from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import nottest
from nose.plugins.attrib import attr
import binascii
from scapy_service import *
from pprint import pprint
import zmq
import json
import scapy_zmq_server
import threading
from scapy_zmq_client import Scapy_server_wrapper

class scapy_service_tester(functional_general_test.CGeneralFunctional_Test):
    def setUp(self):
        self.s = Scapy_service()

    def tearDown(self):
        pass

    ''' 
    test for db and field update - checking check_update_test()
    '''
    def test_check_update(self):
        allData = self.s.get_all()
        allDataParsed = allData
        dbMD5 = allDataParsed['db_md5']
        fieldMD5 = allDataParsed['fields_md5']
        resT1 = self.s.check_update(dbMD5,fieldMD5)
        assert_equal(resT1,True)
        try:
            resT2 = False
            resT2 = self.s.check_update('falseMD5','falseMD5')
        except Exception as e:
            if e.message == "Fields DB is not up to date":
                resT2 = True
            else:
                raise
        assert_equal(resT2,True)
        try:
            resT3 = False
            resT3 = self.s.check_update(dbMD5,'falseMD5')
        except Exception as e:
            if e.message == "Fields DB is not up to date":
                resT3 = True
            else:
                raise
        assert_equal(resT3,True)
        try:
            resT4 = False
            resT4 = self.s.check_update('falseMD5',fieldMD5)
        except Exception as e:
            if e.message == "Protocol DB is not up to date":
                resT4 = True
            else:
                raise
        assert_equal(resT4,True)


    def test_check_updating_db(self):
        #assume i got old db
        try:
            result = self.s.check_update('falseMD5','falseMD5')
        except:
            newAllData = self.s.get_all()
            dbMD5 = newAllData['db_md5']
            fieldMD5 = newAllData['fields_md5']
            result = self.s.check_update(dbMD5,fieldMD5)
            assert_equal(result,True)
        else:
            raise Exception("scapy_server_test: check_updating_db failed")


    def _build_packet_test_method(self,original_pkt):
        test_pkt = original_pkt
        original_pkt = eval(original_pkt)
        test_res = self.s.build_pkt(test_pkt)
        test_pkt_buffer = test_res['buffer']
        resT1 = (test_pkt_buffer == binascii.b2a_base64(str(original_pkt)))
        assert_equal(resT1,True)


#testing offsets of a packet
    def _get_all_offsets_test_method(self,original_pkt):
        test_pkt = original_pkt
        original_pkt = eval(original_pkt)
        original_pkt.build()
        tested_offsets_by_layers = self.s._get_all_pkt_offsets(test_pkt)
        layers = (test_pkt).split('/')
        offsets_by_layers = {}
        for layer in layers:
            fields_dict = {}
            layer_name = layer.partition('(')[0] #clear layer name to include only alpha-numeric
            layer_name = re.sub(r'\W+', '',layer_name)
            for f in original_pkt.fields_desc:
                size = f.get_size_bytes()
                name = f.name
                if f.name is 'load':
                    size = len(original_pkt)
                    layer_name = 'Raw'
                fields_dict[f.name]= [f.offset, size]
                fields_dict['global_offset'] = original_pkt.offset
            original_pkt = original_pkt.payload
            offsets_by_layers[layer_name] = fields_dict
        resT1 = (tested_offsets_by_layers == offsets_by_layers)
        assert_equal(resT1,True)
    
    def _offsets_and_buffer_test_method(self,mac_src,mac_dst,ip_src,ip_dst):
        pkt = Ether(src=mac_src,dst=mac_dst)/IP(src=ip_src,dst=ip_dst)/TCP()
        pkt_descriptor = "Ether(src='"+mac_src+"',dst='"+mac_dst+"')/IP(src='"+ip_src+"',dst='"+ip_dst+"')/TCP()"
        pkt_offsets = self.s._get_all_pkt_offsets(pkt_descriptor)
        pkt_buffer = str(pkt)
        #--------------------------Dest-MAC--------------------
        mac_start_index = pkt_offsets['Ether']['dst'][0]+pkt_offsets['Ether']['global_offset']
        mac_end_index = mac_start_index+pkt_offsets['Ether']['dst'][1]
        assert_equal(binascii.b2a_hex(pkt_buffer[mac_start_index:mac_end_index]),mac_dst.translate(None,':'))
        #--------------------------Src-MAC---------------------
        mac_start_index = pkt_offsets['Ether']['src'][0]+pkt_offsets['Ether']['global_offset']
        mac_end_index = mac_start_index+pkt_offsets['Ether']['src'][1]
        assert_equal(binascii.b2a_hex(pkt_buffer[mac_start_index:mac_end_index]),mac_src.translate(None,':'))
        #--------------------------Dest-IP---------------------
        ip_start_index = pkt_offsets['IP']['dst'][0]+pkt_offsets['IP']['global_offset']
        ip_end_index= ip_start_index+pkt_offsets['IP']['dst'][1]
        assert_equal(binascii.b2a_hex(pkt_buffer[ip_start_index:ip_end_index]),binascii.hexlify(socket.inet_aton(ip_dst)))
        #--------------------------Src-IP----------------------
        ip_start_index = pkt_offsets['IP']['src'][0]+pkt_offsets['IP']['global_offset']
        ip_end_index= ip_start_index+pkt_offsets['IP']['src'][1]
        assert_equal(binascii.b2a_hex(pkt_buffer[ip_start_index:ip_end_index]),binascii.hexlify(socket.inet_aton(ip_src)))

    def test_multi_packet(self):
        packets= [
            'Ether()',
            'Ether()/IP()',
            'TCP()',
            'UDP()',
            'Ether()/IP()/TCP()/"test"',
            'Ether()/IP()/UDP()',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")',
            'Ether()/Dot1Q(vlan=12)/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/TCP(dport=12,sport=1025)',
            'Ether()/Dot1Q(vlan=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/Dot1Q(vlan=12)/IPv6(src="::5")/TCP(dport=12,sport=1025)',
            'Ether()/IP()/UDP()/IPv6(src="::5")/TCP(dport=12,sport=1025)',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether() / IP(src = "16.0.0.1", dst = "48.0.0.1") / UDP(dport = 12, sport = 1025)',
            'Ether()/IP()/UDP()',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/IP(src="16.0.0.2",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            'Ether()/IP(src="16.0.0.3",dst="48.0.0.1")/UDP(dport=12,sport=1025)',
            r'Ether()/IP()/IPv6()/IP(dst="48.0.0.1",options=IPOption("\x01\x01\x01\x00"))/UDP(dport=12,sport=1025)',
            r'Ether()/IP(dst="48.0.0.1",options=IPOption("\x01\x01\x01\x00"))/UDP(dport=12,sport=1025)',
            'Ether()',
            'Ether()/IP()/UDP(sport=1337,dport=4789)/VXLAN(vni=42)/Ether()/IP()/("x"*20)',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=3797,sport=3544)/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")/UDP(dport=12,sport=1025)/ICMPv6Unknown()',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(sport=1025)/DNS()',
            'Ether()/MPLS(label=17,cos=1,s=0,ttl=255)/MPLS(label=0,cos=1,s=1,ttl=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/("x"*20)',
            'Ether()/MPLS(label=17,cos=1,s=0,ttl=255)/MPLS(label=12,cos=1,s=1,ttl=12)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/("x"*20)',
            'Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/ICMP(type=3)',
            'Ether()/IP()/GRE()/("x"*2)']

        for packet in packets:
            self._get_all_offsets_test_method(packet)

        for packet in packets:
            self._build_packet_test_method(packet)

    

    def test_offsets_and_buffer(self):
        self._offsets_and_buffer_test_method('ab:cd:ef:12:34:56','98:76:54:32:1a:bc','127.1.1.1','192.168.1.1')
        self._offsets_and_buffer_test_method('bb:bb:bb:bb:bb:bb','aa:aa:aa:aa:aa:aa','1.1.1.1','0.0.0.0')

class scapy_server_thread(threading.Thread):
    def __init__(self,thread_id,server_port=5555):
        threading.Thread.__init__(self)
        self.thread_id = thread_id
        self.server_port = server_port

    def run(self):
        print('\nStarted scapy thread server')
        scapy_zmq_server.main(self.server_port)
        print('Thread server closed')

# Scapy_server_wrapper is the CLIENT for the scapy server, it wraps the CLIENT: its default port is set to 5555, default server ip set to localhost
class scapy_server_tester(scapy_service_tester):
    def setUp(self):
        self.thread1 = scapy_server_thread(thread_id=1)
        self.thread1.start()
        self.s = Scapy_server_wrapper(dest_scapy_port=5555,server_ip_address='localhost') 

    def tearDown(self):
        self.s.call_method('shut_down',[])
        self.thread1.join()




