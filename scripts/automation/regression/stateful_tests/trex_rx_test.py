#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test
from CPlatform import CStaticRouteConfig, CNatConfig
from .tests_exceptions import *
#import sys
import time
import copy
from nose.tools import nottest
import traceback

class CTRexRx_Test(CTRexGeneral_Test):
    """This class defines the rx testcase of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        CTRexGeneral_Test.__init__(self, *args, **kwargs)
        self.unsupported_modes = ['virt_nics'] # TODO: fix

    def setUp(self):
        CTRexGeneral_Test.setUp(self)


    def check_rx_errors(self, trex_res, allow_error_tolerance = True):
        try:
            # counters to check

            latency_counters_display = {'m_unsup_prot': 0, 'm_no_magic': 0, 'm_no_id': 0, 'm_seq_error': 0, 'm_length_error': 0, 'm_no_ipv4_option': 0, 'm_tx_pkt_err': 0}
            rx_counters = {'m_err_drop': 0, 'm_err_aged': 0, 'm_err_no_magic': 0, 'm_err_wrong_pkt_id': 0, 'm_err_fif_seen_twice': 0, 'm_err_open_with_no_fif_pkt': 0, 'm_err_oo_dup': 0, 'm_err_oo_early': 0, 'm_err_oo_late': 0, 'm_err_flow_length_changed': 0}

            # get relevant TRex results

            try:
                ports_names = trex_res.get_last_value('trex-latecny-v2.data', 'port\-\d+')
                if not ports_names:
                    raise AbnormalResultError('Could not find ports info in TRex results, path: trex-latecny-v2.data.port-*')
                for port_name in ports_names:
                    path = 'trex-latecny-v2.data.%s.stats' % port_name
                    port_result = trex_res.get_last_value(path)
                    if not port_result:
                        raise AbnormalResultError('Could not find port stats in TRex results, path: %s' % path)
                    for key in latency_counters_display:
                        latency_counters_display[key] += port_result[key]

                # using -k flag in TRex produces 1 error per port in latency counter m_seq_error, allow it until issue resolved. For comparing use dict with reduces m_seq_error number.
                latency_counters_compare = copy.deepcopy(latency_counters_display)
                latency_counters_compare['m_seq_error'] = max(0, latency_counters_compare['m_seq_error'] - len(ports_names))

                path = 'rx-check.data.stats'
                rx_check_results = trex_res.get_last_value(path)
                if not rx_check_results:
                    raise AbnormalResultError('No TRex results by path: %s' % path)
                for key in rx_counters:
                    rx_counters[key] = rx_check_results[key]

                path = 'rx-check.data.stats.m_total_rx'
                total_rx = trex_res.get_last_value(path)
                if total_rx is None:
                    raise AbnormalResultError('No TRex results by path: %s' % path)
                elif not total_rx:
                    raise AbnormalResultError('Total rx_check (%s) packets is zero.' % path)

                print('Total packets checked: %s' % total_rx)
                print('Latency counters: %s' % latency_counters_display)
                print('rx_check counters: %s' % rx_counters)

            except KeyError as e:
                self.fail('Expected key in TRex result was not found.\n%s' % traceback.print_exc())

            # the check. in loopback expect 0 problems, at others allow errors <error_tolerance>% of total_rx

            total_errors = sum(rx_counters.values()) + sum(latency_counters_compare.values())
            error_tolerance = self.get_benchmark_param('error_tolerance')
            if not error_tolerance or not allow_error_tolerance:
                error_tolerance = 0
            error_percentage = total_errors * 100.0 / total_rx

            if total_errors > 0:
                if self.is_loopback or error_percentage > error_tolerance:
                    self.fail('Too much errors in rx_check. (~%s%% of traffic)' % error_percentage)
                else:
                    print('There are errors in rx_check (%f%%), not exceeding allowed limit (%s%%)' % (error_percentage, error_tolerance))
            else:
                print('No errors in rx_check.')
        except Exception as e:
            print(traceback.print_exc())
            self.fail('Errors in rx_check: %s' % e)

    def test_rx_check_sfr(self):
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p = True,
            nc = True,
            rx_check = sample_rate,
            d = 100,
            f = 'avl/sfr_delay_10_1g_no_bundeling.yaml',
            l = 1000,
            k = 10,
            learn_verify = True,
            l_pkt_mode = 2)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        #print ("\nLATEST DUMP:")
        #print trex_res.get_latest_dump()

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)
        self.check_rx_errors(trex_res)


    def test_rx_check_http(self):
        if not self.is_loopback:
            # TODO: skip as test_rx_check_http_negative will cover it
            #self.skip('This test is covered by test_rx_check_http_negative')
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")

        core  = self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            rx_check = sample_rate,
            d = 100,
            f = 'cap2/http_simple.yaml',
            l = 1000,
            k = 10,
            learn_verify = True,
            l_pkt_mode = 2)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)
        self.check_rx_errors(trex_res)


    def test_rx_check_sfr_ipv6(self):
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')
            self.router.config_ipv6_pbr(mode = "config")

        core  = self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p = True,
            nc = True,
            rx_check = sample_rate,
            d = 100,
            f = 'avl/sfr_delay_10_1g_no_bundeling.yaml',
            l = 1000,
            k = 10,
            ipv6 = True)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        #print ("\nLATEST DUMP:")
        #print trex_res.get_latest_dump()

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)
        self.check_rx_errors(trex_res)


    def test_rx_check_http_ipv6(self):
        if not self.is_loopback:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = "config")
            self.router.config_ipv6_pbr(mode = "config")

        core  = self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p  = True,
            nc = True,
            rx_check = sample_rate,
            d = 100,
            f = 'cap2/http_simple.yaml',
            l = 1000,
            k = 10,
            ipv6 = True)

        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)

        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)
        self.check_rx_errors(trex_res)

    #@nottest
    def test_rx_check_http_negative(self):
        if self.is_loopback:
            self.skip('This test uses NAT, not relevant for loopback')

        self.router.configure_basic_interfaces()
        self.router.config_pbr(mode = "config")

        core  = self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        sample_rate = self.get_benchmark_param('rx_sample_rate')

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            p = True,
            rx_check = sample_rate,
            d = 60,
            f = 'cap2/http_simple.yaml',
            l = 1000,
            k = 10,
            learn_verify = True,
            l_pkt_mode = 2)

        print('Run for 40 seconds, expect no errors')
        trex_res = self.trex.sample_x_seconds(40)
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        self.check_general_scenario_results(trex_res)
        self.check_CPU_benchmark(trex_res)
        self.check_rx_errors(trex_res)

        print('Run until finish, expect errors')
        old_errors = copy.deepcopy(self.fail_reasons)
        nat_dict = self.get_benchmark_param('nat_dict', test_name = 'test_nat_simple_mode1')
        nat_obj  = CNatConfig(nat_dict)
        self.router.config_nat(nat_obj)
        self.router.config_zbf()
        trex_res = self.trex.sample_to_run_finish()
        self.router.config_no_zbf()
        self.router.config_no_nat(nat_obj)
        #self.router.clear_nat_translations()
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        self.check_rx_errors(trex_res, allow_error_tolerance = False)
        if self.fail_reasons == old_errors:
            self.fail('Expected errors here, got none.')
        else:
            print('Got errors as expected.')
            self.fail_reasons = old_errors

    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
