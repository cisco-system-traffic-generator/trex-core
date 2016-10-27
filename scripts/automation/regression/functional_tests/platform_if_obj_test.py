#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CIfObj_Test(functional_general_test.CGeneralFunctional_Test):
    test_idx = 1

    def setUp(self):
        self.if_1 = CIfObj('gig0/0/1', '1.1.1.1', '2001:DB8:0:2222:0:0:0:1', '0000.0001.0000', '0000.0001.0000', 0, IFType.Client)
        self.if_2 = CIfObj('TenGig0/0/0', '1.1.2.1', '2001:DB8:1:2222:0:0:0:1', '0000.0002.0000', '0000.0002.0000', 0, IFType.Server)
        CIfObj_Test.test_idx += 1

    def test_id_allocation(self):
        assert (self.if_1.get_id() < self.if_2.get_id() < CIfObj._obj_id)

    def test_isClient(self):
        assert_equal (self.if_1.is_client(), True)

    def test_isServer(self):
        assert_equal (self.if_2.is_server(), True)

    def test_get_name (self):
        assert_equal (self.if_1.get_name(), 'gig0/0/1')
        assert_equal (self.if_2.get_name(), 'TenGig0/0/0')

    def test_get_src_mac_addr (self):
        assert_equal (self.if_1.get_src_mac_addr(), '0000.0001.0000')

    def test_get_dest_mac (self):
        assert_equal (self.if_2.get_dest_mac(), '0000.0002.0000')

    def test_get_ipv4_addr (self):
        assert_equal (self.if_1.get_ipv4_addr(), '1.1.1.1' )
        assert_equal (self.if_2.get_ipv4_addr(), '1.1.2.1' ) 

    def test_get_ipv6_addr (self):
        assert_equal (self.if_1.get_ipv6_addr(), '2001:DB8:0:2222:0:0:0:1' )
        assert_equal (self.if_2.get_ipv6_addr(), '2001:DB8:1:2222:0:0:0:1' )

    def test_get_type (self):
        assert_equal (self.if_1.get_if_type(), IFType.Client)
        assert_equal (self.if_2.get_if_type(), IFType.Server)

    def tearDown(self):
        pass
