#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CDualIfObj_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.if_1   = CIfObj('gig0/0/1', '1.1.1.1', '2001:DB8:0:2222:0:0:0:1', '0000.0001.0000', '0000.0001.0000', IFType.Client)
        self.if_2   = CIfObj('gig0/0/2', '1.1.2.1', '2001:DB8:1:2222:0:0:0:1', '0000.0002.0000', '0000.0002.0000', IFType.Server)
        self.if_3   = CIfObj('gig0/0/3', '1.1.3.1', '2001:DB8:2:2222:0:0:0:1', '0000.0003.0000', '0000.0003.0000', IFType.Client)
        self.if_4   = CIfObj('gig0/0/4', '1.1.4.1', '2001:DB8:3:2222:0:0:0:1', '0000.0004.0000', '0000.0004.0000', IFType.Server)
        self.dual_1 = CDualIfObj(None, self.if_1, self.if_2)
        self.dual_2 = CDualIfObj('dup', self.if_3, self.if_4)

    def test_id_allocation(self):
        assert (self.dual_1.get_id() < self.dual_2.get_id() < CDualIfObj._obj_id)

    def test_get_vrf_name (self):
        assert_equal ( self.dual_1.get_vrf_name() , None )
        assert_equal ( self.dual_2.get_vrf_name() , 'dup' )

    def test_is_duplicated (self):
        assert_equal ( self.dual_1.is_duplicated() , False )
        assert_equal ( self.dual_2.is_duplicated() , True )

    def tearDown(self):
        pass