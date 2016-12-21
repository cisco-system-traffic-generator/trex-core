#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test
from .tests_exceptions import *
import time
from CPlatform import CStaticRouteConfig, CNatConfig
from nose.tools import assert_equal


class CTRexNoNat_Test(CTRexGeneral_Test):#(unittest.TestCase):
    """This class defines the NAT testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        super(CTRexNoNat_Test, self).__init__(*args, **kwargs)
        self.unsupported_modes = ['loopback'] # NAT requires device

    def setUp(self):
        super(CTRexNoNat_Test, self).setUp() # launch super test class setUp process

    def check_nat_stats (self, nat_stats):
        pass


    def test_nat_learning(self):
        # test initializtion
        self.router.configure_basic_interfaces()

        stat_route_dict = self.get_benchmark_param('stat_route_dict')
        stat_route_obj = CStaticRouteConfig(stat_route_dict)
        self.router.config_static_routing(stat_route_obj, mode = "config")

        self.router.config_nat_verify()         # shutdown duplicate interfaces

#       self.trex.set_yaml_file('cap2/http_simple.yaml')
        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

#       trex_res = self.trex.run(multiplier = mult, cores = core, duration = 100, l = 1000, learn_verify = True)
        ret = self.trex.start_trex(
            c = core,
            m = mult,
            learn_verify = True,
            d = 100,   
            f = 'cap2/http_simple.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        print("\nLATEST DUMP:")
        print(trex_res.get_latest_dump())


        expected_nat_opened = self.get_benchmark_param('nat_opened')
        learning_stats = trex_res.get_last_value("trex-global.data", ".*nat.*") # extract all nat data

        if self.get_benchmark_param('allow_timeout_dev'):
            nat_timeout_ratio = float(learning_stats['m_total_nat_time_out']) / learning_stats['m_total_nat_open']
            if nat_timeout_ratio > 0.005:
                self.fail('TRex nat_timeout ratio %f > 0.5%%' % nat_timeout_ratio)
        else:
            self.check_results_eq (learning_stats, 'm_total_nat_time_out', 0.0)
        self.check_results_eq (learning_stats, 'm_total_nat_no_fid', 0.0)
        self.check_results_gt (learning_stats, 'm_total_nat_learn_error', 0.0)
#
        self.check_results_gt (learning_stats, 'm_total_nat_open', expected_nat_opened)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res, minimal_cpu = 10, maximal_cpu = 85)

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass


class CTRexNat_Test(CTRexGeneral_Test):#(unittest.TestCase):
    """This class defines the NAT testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        super(CTRexNat_Test, self).__init__(*args, **kwargs)
        self.unsupported_modes = ['loopback'] # NAT requires device

    def setUp(self):
        super(CTRexNat_Test, self).setUp() # launch super test class setUp process
        # config nat here
        

    def check_nat_stats (self, nat_stats):
        pass


    def test_nat_simple_mode1(self):
        self.nat_simple_helper(learn_mode=1, traffic_file='cap2/http_simple.yaml')

    def test_nat_simple_mode2(self):
        self.nat_simple_helper(learn_mode=2, traffic_file='cap2/http_simple.yaml')

    def test_nat_simple_mode3(self):
        self.nat_simple_helper(learn_mode=3, traffic_file='cap2/http_simple.yaml')

    def test_nat_simple_mode1_udp(self):
        self.nat_simple_helper(learn_mode=1, traffic_file='cap2/dns.yaml')

    def test_nat_simple_mode3_udp(self):
        self.nat_simple_helper(learn_mode=3, traffic_file='cap2/dns.yaml')

    def nat_simple_helper(self, learn_mode=1, traffic_file='cap2/http_simple.yaml'):
        # test initializtion
        self.router.configure_basic_interfaces()

        
        stat_route_dict = self.get_benchmark_param('stat_route_dict')
        stat_route_obj = CStaticRouteConfig(stat_route_dict)
        self.router.config_static_routing(stat_route_obj, mode = "config")

        nat_dict = self.get_benchmark_param('nat_dict')
        nat_obj  = CNatConfig(nat_dict)
        self.router.config_nat(nat_obj)

#       self.trex.set_yaml_file('cap2/http_simple.yaml')
        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

#       trex_res = self.trex.run(nc=False,multiplier = mult, cores = core, duration = 100, l = 1000, learn = True)
        ret = self.trex.start_trex(
            c = core,
            m = mult,
            learn_mode = learn_mode,
            d = 100,
            f = traffic_file,
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        print("\nLATEST DUMP:")
        print(trex_res.get_latest_dump())

        trex_nat_stats = trex_res.get_last_value("trex-global.data", ".*nat.*") # extract all nat data
        if self.get_benchmark_param('allow_timeout_dev'):
            nat_timeout_ratio = float(trex_nat_stats['m_total_nat_time_out']) / trex_nat_stats['m_total_nat_open']
            if nat_timeout_ratio > 0.005:
                self.fail('TRex nat_timeout ratio %f > 0.5%%' % nat_timeout_ratio)
        else:
            self.check_results_eq (trex_nat_stats,'m_total_nat_time_out', 0.0)
        self.check_results_eq (trex_nat_stats,'m_total_nat_no_fid', 0.0)
        self.check_results_gt (trex_nat_stats,'m_total_nat_open', 6000)


        self.check_general_scenario_results(trex_res, check_latency = False) # NAT can cause latency
##       test_norm_cpu = 2*(trex_res.result['total-tx']/(core*trex_res.result['cpu_utilization']))
#        trex_tx_pckt  = trex_res.get_last_value("trex-global.data.m_total_tx_bps")
#        cpu_util = int(trex_res.get_last_value("trex-global.data.m_cpu_util"))
#        test_norm_cpu = 2*(trex_tx_pckt/(core*cpu_util))
#        print "test_norm_cpu is: ", test_norm_cpu

        self.check_CPU_benchmark(trex_res, minimal_cpu = 10, maximal_cpu = 85)

        #if ( abs((test_norm_cpu/self.get_benchmark_param('cpu_to_core_ratio')) - 1) > 0.03):
        #    raiseraise AbnormalResultError('Normalized bandwidth to CPU utilization ratio exceeds 3%')

        nat_stats = self.router.get_nat_stats()
        print(nat_stats)

        self.assert_gt(nat_stats['total_active_trans'], 5000, 'total active translations is not high enough')
        self.assert_gt(nat_stats['dynamic_active_trans'], 5000, 'total dynamic active translations is not high enough')
        self.assertEqual(nat_stats['static_active_trans'], 0, "NAT statistics nat_stats['static_active_trans'] should be zero")
        self.assert_gt(nat_stats['num_of_hits'], 50000, 'total nat hits is not high enough')

    def tearDown(self):
        self.router.clear_nat_translations()
        CTRexGeneral_Test.tearDown(self)


if __name__ == "__main__":
    pass
