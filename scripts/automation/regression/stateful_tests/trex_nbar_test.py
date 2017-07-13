#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test, CTRexScenario
from .tests_exceptions import *
from interfaces_e import IFType
from nose.tools import nottest
from misc_methods import print_r

class CTRexNbarBase(CTRexGeneral_Test):
    def match_classification (self):
        nbar_benchmark = self.get_benchmark_param("nbar_classification")
        test_classification = self.router.get_nbar_stats()
        print("TEST CLASSIFICATION:")
        print(test_classification)
        missmatchFlag = False
        missmatchMsg = "NBAR classification contians a missmatch on the following protocols:"
        fmt = '\n\t{0:15} | Expected: {1:>3.2f}%, Got: {2:>3.2f}%'
        noise_level = 0.045

        for cl_intf in self.router.get_if_manager().get_if_list(if_type = IFType.Client):
            client_intf = cl_intf.get_name()

            for protocol, golden in nbar_benchmark.items():
                if protocol != 'total':
                    golden = float(golden)
                    protocol = protocol.replace('_','-')
                    try:
                        protocol_test_res = test_classification[client_intf]['percentage'][protocol]
                        deviation = 100 * abs(golden/protocol_test_res - 1) # percents
                        difference = abs(golden - protocol_test_res)
                        if (deviation > 10 and difference > noise_level):   # allowing 10% deviation and 'noise_level'% difference
                            missmatchFlag = True
                            missmatchMsg += fmt.format(protocol, golden, protocol_test_res)
                    except KeyError as e:
                        missmatchFlag = True
                        print(e)
                        print("Changes missmatchFlag to True. ", "\n\tProtocol {0} isn't part of classification results on interface {intf}".format( protocol, intf = client_intf ))
                        missmatchMsg += "\n\tProtocol {0} isn't part of classification results on interface {intf}".format( protocol, intf = client_intf )
                    except ZeroDivisionError as e:
                        print("ZeroDivisionError: %s" % protocol)
                        pass
        if missmatchFlag:
            self.fail(missmatchMsg)

class CTRexNbar_Test(CTRexNbarBase):
    """This class defines the NBAR testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        super(CTRexNbar_Test, self).__init__(*args, **kwargs)
        self.unsupported_modes = ['loopback'] # obviously no NBar in loopback

    def setUp(self):
        super(CTRexNbar_Test, self).setUp() # launch super test class setUp process
#       self.router.kill_nbar_flows()
        self.router.clear_cft_counters()
        self.router.clear_nbar_stats()

    def test_nbar_simple(self):
        # test initializtion
        deviation_compare_value = 0.03   # default value of deviation - 3%

        if not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")
            self.router.config_nbar_pd()

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc  = True,
            d = 100,   
            f = 'avl/sfr_delay_10_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        print("\nLATEST DUMP:")
        print(trex_res.get_latest_dump())

        self.check_general_scenario_results(trex_res, check_latency = False) # NBAR can cause latency
        self.check_CPU_benchmark(trex_res)
        self.match_classification()


    # the name intentionally not matches nose default pattern, including the test should be specified explicitly
    def NBarLong(self):
        if not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")
            self.router.config_nbar_pd()

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc  = True,
            d = 18000, # 5 hours
            f = 'avl/sfr_delay_10_1g.yaml',
            l = 1000)

        trex_res = self.trex.sample_to_run_finish()

        # trex_res is a CTRexResult instance- and contains the summary of the test results
        # you may see all the results keys by simply calling here for 'print trex_res.result'
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res, check_latency = False)


    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
