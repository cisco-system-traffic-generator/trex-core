#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test
from .tests_exceptions import *
import time
from nose.tools import assert_equal

class CTRexIPv6_Test(CTRexGeneral_Test):
    """This class defines the IPv6 testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        super(CTRexIPv6_Test, self).__init__(*args, **kwargs)

    def setUp(self):
        super(CTRexIPv6_Test, self).setUp() # launch super test class setUp process
#       print " before sleep setup !!"
#       time.sleep(100000);
#       pass

    def test_ipv6_simple(self):
        if self.is_virt_nics:
            self.skip('--ipv6 flag does not work correctly in with virtual NICs') # TODO: fix
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()

            self.router.config_pbr(mode = "config")
            self.router.config_ipv6_pbr(mode = "config")

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            ipv6 = True,
            d = 60,   
            f = 'avl/sfr_delay_10_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        
        self.check_CPU_benchmark (trex_res, 10.0)

        assert True


    def test_ipv6_negative (self):
        if self.is_loopback:
            self.skip('The test checks ipv6 drops by device and we are in loopback setup')
        # test initializtion
        self.router.configure_basic_interfaces()

        # NOT CONFIGURING IPv6 INTENTIONALLY TO GET DROPS!
        self.router.config_pbr(mode = "config")
        
        # same params as test_ipv6_simple
        mult = self.get_benchmark_param('multiplier', test_name = 'test_ipv6_simple')
        core = self.get_benchmark_param('cores', test_name = 'test_ipv6_simple')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            ipv6 = True,
            d = 60,   
            f = 'avl/sfr_delay_10_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        trex_tx_pckt    = float(trex_res.get_last_value("trex-global.data.m_total_tx_pkts"))
        trex_drops      = int(trex_res.get_total_drops())

        trex_drop_rate  = trex_res.get_drop_rate()

        # make sure that at least 50% of the total transmitted packets failed
        self.assert_gt((trex_drops/trex_tx_pckt), 0.5, 'packet drop ratio is not high enough')

        

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        # remove config here
        pass

if __name__ == "__main__":
    pass
