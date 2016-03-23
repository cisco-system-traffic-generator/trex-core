#!/router/bin/python

from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal


class CCommandCache_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.cache = CCommandCache()
        self.cache.add('IF', "ip nbar protocol-discovery", 'GigabitEthernet0/0/1')
        self.cache.add('IF', "ip nbar protocol-discovery", 'GigabitEthernet0/0/2')
        self.cache.add('conf', "arp 1.1.1.1 0000.0001.0000 arpa")
        self.cache.add('conf', "arp 1.1.2.1 0000.0002.0000 arpa")
        self.cache.add('exec', "show ip nbar protocol-discovery stats packet-count")

    def test_add(self):
        assert_equal(self.cache.cache['IF'],
            {'GigabitEthernet0/0/1' : ['ip nbar protocol-discovery'],
             'GigabitEthernet0/0/2' : ['ip nbar protocol-discovery']
            })
        assert_equal(self.cache.cache['CONF'],
            ["arp 1.1.1.1 0000.0001.0000 arpa",
             "arp 1.1.2.1 0000.0002.0000 arpa"]
            )
        assert_equal(self.cache.cache['EXEC'],
            ["show ip nbar protocol-discovery stats packet-count"])

    def test_dump_config (self):
        import sys
        from io import BytesIO
        saved_stdout = sys.stdout
        try:
            out = BytesIO()
            sys.stdout = out
            self.cache.dump_config()
            output = out.getvalue().strip()
            assert_equal(output, 
                "configure terminal\ninterface GigabitEthernet0/0/1\nip nbar protocol-discovery\ninterface GigabitEthernet0/0/2\nip nbar protocol-discovery\nexit\narp 1.1.1.1 0000.0001.0000 arpa\narp 1.1.2.1 0000.0002.0000 arpa\nexit\nshow ip nbar protocol-discovery stats packet-count"
                )
        finally:
            sys.stdout = saved_stdout

    def test_get_config_list (self):
        assert_equal(self.cache.get_config_list(),
            ["configure terminal", "interface GigabitEthernet0/0/1", "ip nbar protocol-discovery", "interface GigabitEthernet0/0/2", "ip nbar protocol-discovery", "exit", "arp 1.1.1.1 0000.0001.0000 arpa", "arp 1.1.2.1 0000.0002.0000 arpa", "exit", "show ip nbar protocol-discovery stats packet-count"]
            )

    def test_clear_cache (self):
        self.cache.clear_cache()
        assert_equal(self.cache.cache,
            {"IF"   : {},
             "CONF" : [],
             "EXEC" : []}
            )

    def tearDown(self):
        self.cache.clear_cache()
