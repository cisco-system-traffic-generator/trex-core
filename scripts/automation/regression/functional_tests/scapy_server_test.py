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
from scapy_server import *


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
            for f in original_pkt.fields_desc:
                size = f.get_size_bytes()
                if f.name is 'load':
                    size = len(original_pkt)
                fields_dict[f.name]= [f.offset, size]
            original_pkt = original_pkt.payload
            layer_name = layer.partition('(')[0] #clear layer name to include only alpha-numeric
            layer_name = re.sub(r'\W+', '',layer_name)
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








