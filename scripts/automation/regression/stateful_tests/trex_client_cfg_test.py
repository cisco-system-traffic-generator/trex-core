#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test, CTRexScenario
from .trex_nbar_test import CTRexNbar_Test
from CPlatform import CStaticRouteConfig
from .tests_exceptions import *
#import sys
import time
from nose.tools import nottest

# Testing client cfg ARP resolve. Actually, just need to check that TRex run finished with no errors.
# If resolve will fail, TRex will exit with exit code != 0
class CTRexClientCfg_Test(CTRexNbar_Test):
    """This class defines the IMIX testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        # super(CTRexClientCfg_Test, self).__init__()
        CTRexGeneral_Test.__init__(self, *args, **kwargs)

    def setUp(self):
        if CTRexScenario.setup_name == 'kiwi02':
            self.skip("Can't run currently on kiwi02")
        super(CTRexClientCfg_Test, self).setUp() # launch super test class setUp process
        pass

    def test_client_cfg_nbar(self):
        # test initializtion
        if self.is_loopback:
            return
        else:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")
            self.router.config_nbar_pd()
            
        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex (
            c = core,
            m = mult,
            nc  = True,
            p = True,
            d = 100,
            f = 'avl/sfr_delay_10_1g.yaml',
            client_cfg = 'automation/regression/cfg/client_cfg.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        self.check_general_scenario_results(trex_res, check_latency = False) # no latency with client config
        self.match_classification()

    def test_client_cfg_vlan(self):
        # test initializtion
        if self.is_loopback:
            return
        else:
            self.router.configure_basic_interfaces(vlan = True)
            self.router.config_pbr(mode = "config", vlan = True)
            self.router.config_nbar_pd()

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex (
            c = core,
            m = mult,
            nc  = True,
            p = True,
            d = 60,
            f = 'cap2/dns.yaml',
            limit_ports = 4,
            client_cfg = 'automation/regression/cfg/client_cfg_vlan.yaml')

        trex_res = self.trex.sample_to_run_finish()
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        self.check_general_scenario_results(trex_res, check_latency = False) # no latency with client config
        
    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
