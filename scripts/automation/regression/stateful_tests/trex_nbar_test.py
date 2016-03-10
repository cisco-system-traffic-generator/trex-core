#!/router/bin/python
from trex_general_test import CTRexGeneral_Test
from tests_exceptions import *
from interfaces_e import IFType
from nose.tools import nottest
from misc_methods import print_r

class CTRexNbar_Test(CTRexGeneral_Test):
    """This class defines the NBAR testcase of the T-Rex traffic generator"""
    def __init__(self, *args, **kwargs):
    	super(CTRexNbar_Test, self).__init__(*args, **kwargs)
        self.unsupported_modes = ['loopback'] # obviously no NBar in loopback
    	pass

    def setUp(self):
        super(CTRexNbar_Test, self).setUp() # launch super test class setUp process
#       self.router.kill_nbar_flows()
        self.router.clear_cft_counters()
        self.router.clear_nbar_stats()

    def match_classification (self):
        nbar_benchmark = self.get_benchmark_param("nbar_classification")
        test_classification = self.router.get_nbar_stats()
        print "TEST CLASSIFICATION:"
        print test_classification
        missmatchFlag = False
        missmatchMsg = "NBAR classification contians a missmatch on the following protocols:"
        fmt = '\n\t{0:15} | Expected: {1:>3.2f}%, Got: {2:>3.2f}%'
        noise_level = 0.045 # percents

        for cl_intf in self.router.get_if_manager().get_if_list(if_type = IFType.Client):
            client_intf = cl_intf.get_name()

            # removing noise classifications
            for key, value in test_classification[client_intf]['percentage'].items():
                if value <= noise_level:
                    print 'Removing noise classification: %s' % key
                    del test_classification[client_intf]['percentage'][key]

            if len(test_classification[client_intf]['percentage']) != (len(nbar_benchmark) + 1):    # adding 'total' key to nbar_benchmark
                raise ClassificationMissmatchError ('The total size of classification result does not match the provided benchmark.')

            for protocol, bench in nbar_benchmark.iteritems():
                if protocol != 'total':
                    try:
                        bench = float(bench)
                        protocol = protocol.replace('_','-')
                        protocol_test_res = test_classification[client_intf]['percentage'][protocol]
                        deviation = 100 * abs(bench/protocol_test_res - 1) # percents
                        difference = abs(bench - protocol_test_res)
                        if (deviation > 10 and difference > noise_level):   # allowing 10% deviation and 'noise_level'% difference
                            missmatchFlag = True
                            missmatchMsg += fmt.format(protocol, bench, protocol_test_res)
                    except KeyError as e:
                        missmatchFlag = True
                        print e
                        print "Changes missmatchFlag to True. ", "\n\tProtocol {0} isn't part of classification results on interface {intf}".format( protocol, intf = client_intf )
                        missmatchMsg += "\n\tProtocol {0} isn't part of classification results on interface {intf}".format( protocol, intf = client_intf )
                    except ZeroDivisionError as e:
                        print "ZeroDivisionError: %s" % protocol
                        pass
        if missmatchFlag:
            self.fail(missmatchMsg)


    def test_nbar_simple(self):
        # test initializtion
        deviation_compare_value = 0.03   # default value of deviation - 3%
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
        print ("\nLATEST RESULT OBJECT:")
        print trex_res
        print ("\nLATEST DUMP:")
        print trex_res.get_latest_dump()


        self.check_general_scenario_results(trex_res, check_latency = False) # NBAR can cause latency
        #       test_norm_cpu = 2*(trex_res.result['total-tx']/(core*trex_res.result['cpu_utilization']))
        trex_tx_pckt  = trex_res.get_last_value("trex-global.data.m_total_tx_pkts")
        cpu_util = trex_res.get_last_value("trex-global.data.m_cpu_util")
        cpu_util_hist = trex_res.get_value_list("trex-global.data.m_cpu_util")
        print "cpu util is:", cpu_util
        print cpu_util_hist
        test_norm_cpu = 2 * trex_tx_pckt / (core * cpu_util)
        print "test_norm_cpu is:", test_norm_cpu

        
        if self.get_benchmark_param('cpu2core_custom_dev'):
            # check this test by custom deviation
            deviation_compare_value = self.get_benchmark_param('cpu2core_dev')
            print "Comparing test with custom deviation value- {dev_val}%".format( dev_val = int(deviation_compare_value*100) )

        # need to be fixed !
        #if ( abs((test_norm_cpu/self.get_benchmark_param('cpu_to_core_ratio')) - 1) > deviation_compare_value):
        #    raise AbnormalResultError('Normalized bandwidth to CPU utilization ratio exceeds benchmark boundaries')

        self.match_classification()

        assert True

    @nottest
    def test_rx_check (self):
        # test initializtion
        self.router.configure_basic_interfaces()

        self.router.config_pbr(mode = "config")
        self.router.config_nbar_pd()

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            rx_check = sample_rate,
            d = 100,   
            f = 'cap2/sfr.yaml',
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

#       if trex_res.result['rx_check_tx']==trex_res.result['rx_check_rx']:  # rx_check verification shoud pass
#           assert trex_res.result['rx_check_verification'] == "OK"
#       else:
#           assert trex_res.result['rx_check_verification'] == "FAIL"

    # the name intentionally not matches nose default pattern, including the test should be specified explicitly
    def NBarLong(self):
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
        print ("\nLATEST RESULT OBJECT:")
        print trex_res

        self.check_general_scenario_results(trex_res, check_latency = False)


    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
