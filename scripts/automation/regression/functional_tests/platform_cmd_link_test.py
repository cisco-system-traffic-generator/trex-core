#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CCommandLink_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.cache = CCommandCache()
        self.cache.add('IF', "ip nbar protocol-discovery", 'GigabitEthernet0/0/1')
        self.cache.add('IF', "ip nbar protocol-discovery", 'GigabitEthernet0/0/2')
        self.cache.add('conf', "arp 1.1.1.1 0000.0001.0000 arpa")
        self.cache.add('conf', "arp 1.1.2.1 0000.0002.0000 arpa")
        self.cache.add('exec', "show ip nbar protocol-discovery stats packet-count")
        self.com_link = CCommandLink()

    def test_transmit(self):
        # test here future implemntatin of platform physical link
        pass

    def test_run_cached_command (self):
        self.com_link.run_command([self.cache])

        assert_equal (self.com_link.get_history(), 
            ["configure terminal", "interface GigabitEthernet0/0/1", "ip nbar protocol-discovery", "interface GigabitEthernet0/0/2", "ip nbar protocol-discovery", "exit", "arp 1.1.1.1 0000.0001.0000 arpa", "arp 1.1.2.1 0000.0002.0000 arpa", "exit", "show ip nbar protocol-discovery stats packet-count"]
            )

        self.com_link.clear_history()
        self.com_link.run_single_command(self.cache)
        assert_equal (self.com_link.get_history(), 
            ["configure terminal", "interface GigabitEthernet0/0/1", "ip nbar protocol-discovery", "interface GigabitEthernet0/0/2", "ip nbar protocol-discovery", "exit", "arp 1.1.1.1 0000.0001.0000 arpa", "arp 1.1.2.1 0000.0002.0000 arpa", "exit", "show ip nbar protocol-discovery stats packet-count"]
            )

    def test_run_single_command(self):
        self.com_link.run_single_command("show ip nbar protocol-discovery stats packet-count")
        assert_equal (self.com_link.get_history(), 
            ["show ip nbar protocol-discovery stats packet-count"]
            )

    def test_run_mixed_commands (self):
        self.com_link.run_single_command("show ip nbar protocol-discovery stats packet-count")
        self.com_link.run_command([self.cache])
        self.com_link.run_command(["show ip interface brief"])

        assert_equal (self.com_link.get_history(), 
            ["show ip nbar protocol-discovery stats packet-count",
             "configure terminal", "interface GigabitEthernet0/0/1", "ip nbar protocol-discovery", "interface GigabitEthernet0/0/2", "ip nbar protocol-discovery", "exit", "arp 1.1.1.1 0000.0001.0000 arpa", "arp 1.1.2.1 0000.0002.0000 arpa", "exit", "show ip nbar protocol-discovery stats packet-count",
             "show ip interface brief"]
            )

    def test_clear_history (self):
        self.com_link.run_command(["show ip interface brief"])
        self.com_link.clear_history()
        assert_equal (self.com_link.get_history(), [])

    def tearDown(self):
        self.cache.clear_cache()


