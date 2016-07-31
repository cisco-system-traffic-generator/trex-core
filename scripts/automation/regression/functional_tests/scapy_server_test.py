# scapy server unit test

import sys,os
scapy_server_path = os.path.abspath(os.path.join(os.pardir, 'trex_control_plane', 'stl', 'examples'))
print scapy_server_path
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir, 'trex_control_plane','stl'))
sys.path.append(scapy_server_path)
sys.path.append(stl_pathname)



#import stl_path
import trex_stl_lib
from trex_stl_lib.api import *
from copy import deepcopy

import tempfile
import md5

import outer_packages
from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import nottest
from nose.plugins.attrib import attr

from scapy_server import *


class scapy_server_tester(functional_general_test.CGeneralFunctional_Test):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    ''' 
    test for db and field update - checking check_update_test()
    '''
    def test_check_update(self):
        allData = get_all()
        allDataParsed = json.loads(allData)
        dbMD5 = allDataParsed['DB_md5']
        fieldMD5 = allDataParsed['fields_md5']
        result = check_update(dbMD5,fieldMD5)
        result = json.loads(result)
        '''
        if result[0][0] == 'Success' and result[1][0] == 'Success':
            print 'check_update_test [md5 comparison test]: Success'
        else:
            print 'check_update_test [md5 comparison test]: md5s of fields or db do not match the source'
        '''
        resT1 = (result[0][0] == 'Success' and result[1][0] == 'Success')
        assert_equal(resT1,True)

        result = check_update(json.dumps('falseMD5'),json.dumps('falseMD5'))
        result = json.loads(result)
        '''
        if result[0][0] == 'Fail' and result[1][0] == 'Fail':
            print 'check_update_test [wrong md5s return failure]: Success'
        else:
            print 'check_update_test [wrong md5s return failure]: md5s of fields or db return Success for invalid value'
        '''
        resT2 = (result[0][0] == 'Fail' and result[1][0] == 'Fail')
        assert_equal(resT2,True)

        result = check_update(dbMD5,json.dumps('falseMD5'))
        result = json.loads(result)
        '''
        if result[0][0] == 'Fail' and result[1][0] == 'Success':
            print 'check_update_test [wrong field md5 returns error, correct db md5]: Success'
        else:
            print 'md5 of field return Success for invalid value'
        '''
        resT3 = (result[0][0] == 'Fail' and result[1][0] == 'Success')
        assert_equal(resT3,True)

        result = check_update(json.dumps('falseMD5'),fieldMD5)
        result = json.loads(result)
        '''
        if result[0][0] == 'Success' and result[1][0] == 'Fail':
            print 'check_update_test [wrong db md5 returns error, correct field md5]: Success'
        else:
            print 'md5 of db return Success for invalid value'
        '''
        resT4 = (result[0][0] == 'Success' and result[1][0] == 'Fail')
        assert_equal(resT4,True)


    def test_check_updating_db(self):
        #assume i got old db
        result = check_update(json.dumps('falseMD5'),json.dumps('falseMD5'))
        result = json.loads(result)
        if result[0][0] == 'Fail' or result[1][0] == 'Fail':
            newAllData = get_all()
            allDataParsed = json.loads(newAllData)
            dbMD5 = allDataParsed['DB_md5']
            fieldMD5 = allDataParsed['fields_md5']
            result = check_update(dbMD5,fieldMD5)
            result = json.loads(result)
            '''
            if result[0][0] == 'Success' and result[1][0] == 'Success':
                print 'check_updating_db [got old db and updated it]: Success'
            else:
                print'check_updating_db [got old db and updated it]: FAILED'
            '''
            resT1 = (result[0][0] == 'Success' and result[1][0] == 'Success')
            assert_equal(resT1,True)
        else:
            raise Exception("scapy_server_test: check_updating_db failed")


# testing pkt = Ether()/IP()/TCP()/"test" by defualt
    def test_build_packet(self,original_pkt = json.dumps('Ether()/IP()/TCP()/"test"')):
        test_pkt = original_pkt
        original_pkt = eval(json.loads(original_pkt))
        test_res = build_pkt(test_pkt)
        test_res = json.loads(test_res)
        test_pkt_buffer = json.loads(test_res[2])
        test_pkt_buffer = test_pkt_buffer.decode('base64')
        '''
        if test_pkt_buffer == str(original_pkt):
            print 'build_pkt test [scapy packet and string-defined packet comparison]: Success'
        else:
            print 'build_pkt test [scapy packet and string-defined packet comparison]: FAILED'
        '''
        resT1 = (test_pkt_buffer == str(original_pkt))
        assert_equal(resT1,True)


#testing offsets of packet IP() by default
    def test_get_all_offsets(self,original_pkt = json.dumps('IP()')):
        test_pkt = original_pkt
        original_pkt = eval(json.loads(original_pkt))
        tested_offsets_by_layers = get_all_pkt_offsets(test_pkt)
        tested_offsets_by_layers = json.loads(tested_offsets_by_layers)
        layers = json.loads(test_pkt).split('/')
        offsets_by_layers = {}
        for layer in layers:
            fields_list = []
            for f in original_pkt.fields_desc:
                size = f.get_size_bytes()
                if f.name is 'load':
                    size = len(original_pkt)
                fields_list.append([f.name, f.offset, size])
            original_pkt = original_pkt.payload
            offsets_by_layers[layer] = fields_list
        '''
        if tested_offsets_by_layers == offsets_by_layers:
            print 'Success'
        else:
            print 'test_get_all_offsets[comparison of offsets in given packet]: FAILED'
        '''
        resT1 = (tested_offsets_by_layers == offsets_by_layers)
        assert_equal(resT1,True)

    def test_multi_packet(self):
        e0 = json.dumps('Ether()')
        e1 = json.dumps('Ether()/IP()')
        e2 = json.dumps('TCP()')
        e3 = json.dumps('UDP()')
        e4 = json.dumps('Ether()/IP()/TCP()/"test"')
        e5 = json.dumps('Ether()/IP()/UDP()')
        packets = [e0,e1,e2,e3,e4,e5]

        for packet in packets:
            self.test_get_all_offsets(packet)

        for packet in packets:
            self.test_build_packet(packet)








