#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test
from CPlatform import CStaticRouteConfig
from .tests_exceptions import *
#import sys
import time
from nose.tools import nottest

# Testing client cfg ARP resolve. Actually, just need to check that TRex run finished with no errors.
# If resolve will fail, TRex will exit with exit code != 0
class CTRexClientCfg_Test(CTRexGeneral_Test):
    """This class defines the IMIX testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        # super(CTRexClientCfg_Test, self).__init__()
        CTRexGeneral_Test.__init__(self, *args, **kwargs)

    def setUp(self):
        super(CTRexClientCfg_Test, self).setUp() # launch super test class setUp process
        pass

    def test_client_cfg(self):
        # test initializtion
        if self.is_loopback:
            return
        else:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        ret = self.trex.start_trex(
            c = 1,
            m = 1,
            d = 10,
            f = 'cap2/dns.yaml',
            v = 3,
            client_cfg = 'automation/regression/cfg/client_cfg.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
