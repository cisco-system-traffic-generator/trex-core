#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CIfManager_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.dev_cfg = CDeviceCfg('./functional_tests/config.yaml')
        self.if_mng  = CIfManager()

    # main testing method to check the entire class
    def test_load_config (self):
        self.if_mng.load_config(self.dev_cfg)

        # check the number of items in each qeury
        assert_equal( len(self.if_mng.get_if_list()), 4 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Client)), 2 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Client, is_duplicated = True)), 1 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Client, is_duplicated = False)), 1 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Server)), 2 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Server, is_duplicated = True)), 1 )
        assert_equal( len(self.if_mng.get_if_list(if_type = IFType.Server, is_duplicated = False)), 1 )
        assert_equal( len(self.if_mng.get_duplicated_if()), 2 )
        assert_equal( len(self.if_mng.get_dual_if_list()), 2 )

        # check the classification with intf name
        assert_equal( map(CIfObj.get_name, self.if_mng.get_if_list() ), ['GigabitEthernet0/0/1','GigabitEthernet0/0/2','GigabitEthernet0/0/3','GigabitEthernet0/0/4'] )
        assert_equal( map(CIfObj.get_name, self.if_mng.get_if_list(is_duplicated = True) ), ['GigabitEthernet0/0/3','GigabitEthernet0/0/4'] )
        assert_equal( map(CIfObj.get_name, self.if_mng.get_if_list(is_duplicated = False) ), ['GigabitEthernet0/0/1','GigabitEthernet0/0/2'] )
        assert_equal( map(CIfObj.get_name, self.if_mng.get_duplicated_if() ), ['GigabitEthernet0/0/3', 'GigabitEthernet0/0/4'] )

        # check the classification with vrf name
        assert_equal( map(CDualIfObj.get_vrf_name, self.if_mng.get_dual_if_list() ), [None, 'dup'] )

    def tearDown(self):
        pass
