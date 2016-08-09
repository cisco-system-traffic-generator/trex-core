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


class Scapy_server_wrapper():
    def __init__(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.dest_scapy_port =5555
        self.socket.connect("tcp://10.56.216.133:"+str(self.dest_scapy_port)) #ip address of csi-trex-11

    def call_method(self,method_name,method_params):
        json_rpc_req = { "jsonrpc":"2.0","method": method_name ,"params": method_params, "id":"1"}
        request = json.dumps(json_rpc_req)
        self.socket.send(request)
        #  Get the reply.
        message = self.socket.recv()
#       print("Received reply %s [ %s ]" % (request, message))
        message_parsed = json.loads(message)
        try:
            result = message_parsed['result']
        except:
            result = {'error':message_parsed['error']}
        finally:
            return result

    def get_all(self):
        return self.call_method('get_all',[])

    def check_update(self,db_md5,field_md5):
        result = self.call_method('check_update',[db_md5,field_md5])
        if result!=True:
            if 'error' in result.keys():
                if "Fields DB is not up to date" in result['error']['message:']:
                    raise ScapyException("Fields DB is not up to date")
                if "Protocol DB is not up to date" in result['error']['message:']:
                    raise ScapyException("Protocol DB is not up to date")
        return result

    def build_pkt(self,pkt_descriptor):
        return self.call_method('build_pkt',[pkt_descriptor])
        
    def get_all_pkt_offsets(self,pkt_desc):
        return self.call_method('get_all_pkt_offsets',[pkt_desc])




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


# testing pkt = Ether()/IP()/TCP()/"test" by defualt
    def test_build_packet(self,original_pkt='Ether()/IP()/TCP()/"test"'):
        test_pkt = original_pkt
        original_pkt = eval(original_pkt)
        test_res = self.s.build_pkt(test_pkt)
        test_pkt_buffer = test_res['buffer']
        resT1 = (test_pkt_buffer == binascii.b2a_base64(str(original_pkt)))
        assert_equal(resT1,True)


#testing offsets of packet IP() by default
    def test_get_all_offsets(self,original_pkt = 'IP()'):
        test_pkt = original_pkt
        original_pkt = eval(original_pkt)
        original_pkt.build()
        tested_offsets_by_layers = self.s.get_all_pkt_offsets(test_pkt)
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

    def test_multi_packet(self):
        e0 = 'Ether()'
        e1 = 'Ether()/IP()'
        e2 = 'TCP()'
        e3 = 'UDP()'
        e4 = 'Ether()/IP()/TCP()/"test"'
        e5 = 'Ether()/IP()/UDP()'
        packets = [e0,e1,e2,e3,e4,e5]
        for packet in packets:
            self.test_get_all_offsets(packet)

        for packet in packets:
            self.test_build_packet(packet)

    def test_offsets_and_buffer(self,mac_src='ab:cd:ef:12:34:56',mac_dst='98:76:54:32:1a:bc',ip_src='127.1.1.1',ip_dst='192.168.1.1'):
        pkt = Ether(src=mac_src,dst=mac_dst)/IP(src=ip_src,dst=ip_dst)/TCP()
        pkt_descriptor = "Ether(src='"+mac_src+"',dst='"+mac_dst+"')/IP(src='"+ip_src+"',dst='"+ip_dst+"')/TCP()"
        pkt_offsets = self.s.get_all_pkt_offsets(pkt_descriptor)
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




class scapy_server_tester(scapy_service_tester):
    def setUp(self):
        self.s = Scapy_server_wrapper()




