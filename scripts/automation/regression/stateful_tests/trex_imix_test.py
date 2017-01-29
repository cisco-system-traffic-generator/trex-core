#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test
from CPlatform import CStaticRouteConfig
from .tests_exceptions import *
#import sys
import time
from nose.tools import nottest

class CTRexIMIX_Test(CTRexGeneral_Test):
    """This class defines the IMIX testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        # super(CTRexIMIX_Test, self).__init__()
        CTRexGeneral_Test.__init__(self, *args, **kwargs)

    def setUp(self):
        super(CTRexIMIX_Test, self).setUp() # launch super test class setUp process
        # CTRexGeneral_Test.setUp(self)       # launch super test class setUp process
        # self.router.clear_counters()
        pass

    def test_short_flow(self):
        """ short UDP flow with 64B packets, this test with small number of active flows """
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        mult  = self.get_benchmark_param('multiplier')
        core  = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            d = 30,   
            f = 'cap2/cur_flow.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)

    def test_short_flow_high_active(self):
        """ short UDP flow with 64B packets, this test with 8M  active flows """
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        mult  = self.get_benchmark_param('multiplier')
        core  = self.get_benchmark_param('cores')
        active_flows =self.get_benchmark_param('active_flows') 


        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            d = 60,   
            active_flows = active_flows,
            f = 'cap2/cur_flow.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)

    def test_short_flow_high_active2(self):
        """ short UDP flow with 64B packets, this test with 8M  active flows """
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        mult  = self.get_benchmark_param('multiplier')
        core  = self.get_benchmark_param('cores')
        active_flows =self.get_benchmark_param('active_flows') 


        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            d = 60,   
            active_flows = active_flows,
            f = 'cap2/cur_flow_single.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)

    def test_routing_imix_64(self):
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

#       self.trex.set_yaml_file('cap2/imix_64.yaml')
        mult  = self.get_benchmark_param('multiplier')
        core  = self.get_benchmark_param('cores')

#       trex_res = self.trex.run(multiplier = mult, cores = core, duration = 30, l = 1000, p = True)
        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            d = 30,   
            f = 'cap2/imix_64.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)



    # the name intentionally not matches nose default pattern, including the test should be specified explicitly
    def dummy(self):
        ret = self.trex.start_trex(
            c = 1,
            m = 1,
            p  = True,
            nc = True,
            d = 5,
            f = 'cap2/imix_fast_1g.yaml',
            l = 1000,
            trex_development = True)

        trex_res = self.trex.sample_to_run_finish()
        print(trex_res)

    def test_routing_imix (self):
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

#       self.trex.set_yaml_file('cap2/imix_fast_1g.yaml')
        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            d = 60,   
            f = 'cap2/imix_fast_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)

        self.check_CPU_benchmark(trex_res)


    def test_static_routing_imix (self):
        if self.is_loopback:
            self.skip('In loopback mode the test is same as test_routing_imix')
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()

            # Configure static routing based on benchmark data input
            stat_route_dict = self.get_benchmark_param('stat_route_dict')
            stat_route_obj = CStaticRouteConfig(stat_route_dict)
            self.router.config_static_routing(stat_route_obj, mode = "config")

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            e  = True,
            nc = True,
            d = 60,   
            f = 'cap2/imix_fast_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        print("\nLATEST DUMP:")
        print(trex_res.get_latest_dump())

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)


    def test_static_routing_imix_asymmetric (self):
        # test initializtion
        if not self.is_loopback:
            self.router.configure_basic_interfaces()

            # Configure static routing based on benchmark data input
            stat_route_dict = self.get_benchmark_param('stat_route_dict')
            stat_route_obj = CStaticRouteConfig(stat_route_dict)
            self.router.config_static_routing(stat_route_obj, mode = "config")

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            nc = True,
            d = 100,   
            f = 'cap2/imix_fast_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResults instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)

        self.check_CPU_benchmark(trex_res, minimal_cpu = 25)


    def test_jumbo(self, duration = 100, **kwargs):
        if not self.is_loopback:
            self.router.configure_basic_interfaces(mtu = 9216)
            self.router.config_pbr(mode = "config")

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p = True,
            nc = True,
            d = duration,
            f = 'cap2/imix_9k.yaml',
            l = 1000,
            **kwargs)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResults instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res, minimal_cpu = 0, maximal_cpu = 10)

    # don't include it to regular nose search
    @nottest
    def test_warm_up(self):
        try:
            self._testMethodName = 'test_jumbo'
            self.test_jumbo(duration = 5, trex_development = True)
        except Exception as e:
            print('Ignoring this error: %s' % e)
        if self.fail_reasons:
            print('Ignoring this error(s):\n%s' % '\n'.join(self.fail_reasons))
            self.fail_reasons = []

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        # remove nbar config here
        pass

if __name__ == "__main__":
    pass
