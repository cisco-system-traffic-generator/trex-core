#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CDeviceCfg_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
    	self.dev_cfg = CDeviceCfg('./unit_tests/functional_tests/config.yaml')

    def test_get_interfaces_cfg(self):
        assert_equal (self.dev_cfg.get_interfaces_cfg(), 
        	[{'client': {'src_mac_addr': '0000.0001.0000', 'name': 'GigabitEthernet0/0/1', 'dest_mac_addr': '0000.1000.0000'}, 'vrf_name': None, 'server': {'src_mac_addr': '0000.0002.0000', 'name': 'GigabitEthernet0/0/2', 'dest_mac_addr': '0000.2000.0000'}}, {'client': {'src_mac_addr': '0000.0003.0000', 'name': 'GigabitEthernet0/0/3', 'dest_mac_addr': '0000.3000.0000'}, 'vrf_name': 'dup', 'server': {'src_mac_addr': '0000.0004.0000', 'name': 'GigabitEthernet0/0/4', 'dest_mac_addr': '0000.4000.0000'}}]
        	)

    def tearDown(self):
        pass
