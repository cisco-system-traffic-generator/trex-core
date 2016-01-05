#!/router/bin/python

import functional_general_test
import misc_methods
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises


class MiscMethods_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.ipv4_gen = misc_methods.get_network_addr()
        self.ipv6_gen = misc_methods.get_network_addr(ip_type = 'ipv6')
        pass

    def test_ipv4_gen(self):
        for i in range(1, 255):
            assert_equal( next(self.ipv4_gen), [".".join( map(str, [1, 1, i, 0])), '255.255.255.0'] )

    def test_ipv6_gen(self):
        tmp_ipv6_addr = ['2001', 'DB8', 0, '2222', 0, 0, 0, 0]
        for i in range(0, 255):
            tmp_ipv6_addr[2] = hex(i)[2:]
            assert_equal( next(self.ipv6_gen), ":".join( map(str, tmp_ipv6_addr)) )

    def test_get_ipv4_client_addr(self):
        tmp_ipv4_addr = next(self.ipv4_gen)[0]
        assert_equal ( misc_methods.get_single_net_client_addr(tmp_ipv4_addr), '1.1.1.1')
        assert_raises (ValueError, misc_methods.get_single_net_client_addr, tmp_ipv4_addr, {'3' : 255} )
    
    def test_get_ipv6_client_addr(self):
        tmp_ipv6_addr = next(self.ipv6_gen)
        assert_equal ( misc_methods.get_single_net_client_addr(tmp_ipv6_addr, {'7' : 1}, ip_type = 'ipv6'), '2001:DB8:0:2222:0:0:0:1')    
        assert_equal ( misc_methods.get_single_net_client_addr(tmp_ipv6_addr, {'7' : 2}, ip_type = 'ipv6'), '2001:DB8:0:2222:0:0:0:2')    
        assert_raises (ValueError, misc_methods.get_single_net_client_addr, tmp_ipv6_addr, {'7' : 70000} )
        

    @raises(ValueError)
    def test_ipv4_client_addr_exception(self):
        tmp_ipv4_addr = next(self.ipv4_gen)[0]
        misc_methods.get_single_net_client_addr(tmp_ipv4_addr, {'4' : 1})

    @raises(ValueError)
    def test_ipv6_client_addr_exception(self):
        tmp_ipv6_addr = next(self.ipv6_gen)
        misc_methods.get_single_net_client_addr(tmp_ipv6_addr, {'8' : 1}, ip_type = 'ipv6')

    @raises(StopIteration)
    def test_gen_ipv4_to_limit (self):
        while(True):
            next(self.ipv4_gen)

    @raises(StopIteration)
    def test_gen_ipv6_to_limit (self):
        while(True):
            next(self.ipv6_gen)

    def tearDown(self):
        pass
