#!/router/bin/python
from trex_general_test import CTRexGeneral_Test
from CPlatform import CStaticRouteConfig
from tests_exceptions import *
#import sys
import time

class CTRexIMIX_Test(CTRexGeneral_Test):
    """This class defines the IMIX testcase of the T-Rex traffic generator"""
    def __init__(self, *args, **kwargs):
        # super(CTRexIMIX_Test, self).__init__()
        CTRexGeneral_Test.__init__(self, *args, **kwargs)
        pass

    def setUp(self):
        super(CTRexIMIX_Test, self).setUp() # launch super test class setUp process
        # CTRexGeneral_Test.setUp(self)       # launch super test class setUp process
        # self.router.clear_counters()
        pass

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
        print ("\nLATEST RESULT OBJECT:")
        print trex_res

        self.check_general_scenario_results(trex_res)
        
        self.check_CPU_benchmark(trex_res, 10.0)

    # the name intentionally not matches nose default pattern, including the test should be specified explicitly
    def dummy(self):
        self.assertEqual(1, 2, 'boo')
        self.assertEqual(2, 2, 'boo')
        self.assertEqual(2, 3, 'boo')
        #print ''
        #print dir(self)
        #print locals()
        #print ''
        #print_r(unittest.TestCase)
        #print ''
        #print_r(self)
        print ''
        #print unittest.TestCase.shortDescription(self)
        #self.skip("I'm just a dummy test")


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
        print ("\nLATEST RESULT OBJECT:")
        print trex_res

        self.check_general_scenario_results(trex_res)

        self.check_CPU_benchmark(trex_res, 10.0)


    def test_static_routing_imix (self):
        if self.is_loopback: # in loopback mode this test acts same as test_routing_imix, disable to avoid duplication
            self.skip()
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
            p  = True,
            nc = True,
            d = 60,   
            f = 'cap2/imix_fast_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()
        
        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print ("\nLATEST RESULT OBJECT:")
        print trex_res
        print ("\nLATEST DUMP:")
        print trex_res.get_latest_dump()

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res, 10)


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
        print ("\nLATEST RESULT OBJECT:")
        print trex_res

        self.check_general_scenario_results(trex_res)

        self.check_CPU_benchmark(trex_res, 10)

        
    def test_jumbo(self):
        if not self.is_loopback:
            self.skip('Verify drops in router') # TODO: verify and remove ASAP
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p = True,
            nc = True,
            d = 100,   
            f = 'cap2/imix_9k.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()
        
        # trex_res is a CTRexResults instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print ("\nLATEST RESULT OBJECT:")
        print trex_res

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res, minimal_cpu = 0, maximal_cpu = 10)

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        # remove nbar config here
        pass

if __name__ == "__main__":
    pass
