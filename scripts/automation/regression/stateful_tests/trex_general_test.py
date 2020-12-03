#!/router/bin/python

__copyright__ = "Copyright 2014"

"""
Name:
     trex_general_test.py


Description:

    This script creates the functionality to test the performance of the TRex traffic generator
    The tested scenario is a TRex TG directly connected to a Cisco router.

::

    Topology:

       -------                         --------
      |       | Tx---1gig/10gig----Rx |        |
      | TRex  |                       | router |
      |       | Rx---1gig/10gig----Tx |        |
       -------                         --------

"""
from nose.plugins import Plugin
from nose.plugins.skip import SkipTest
import trex_scenario
from trex_scenario import CTRexScenario
import misc_methods
import pprint
import sys
import os
# from CPlatformUnderTest import *
from CPlatform import *
import termstyle
import threading
from .tests_exceptions import *
from platform_cmd_link import *
import unittest
from glob import glob
from datetime import datetime, timedelta
import yaml
import tempfile
import shutil


class CTRexGeneral_Test(unittest.TestCase):
    """This class defines the general stateful testcase of the TRex traffic generator"""

    @classmethod
    def setUpClass(cls):
        cls.GAManager         = CTRexScenario.GAManager
        cls.astf_trex         = CTRexScenario.astf_trex
        cls.benchmark         = CTRexScenario.benchmark
        cls.configuration     = CTRexScenario.configuration
        cls.elk               = CTRexScenario.elk
        cls.modes             = CTRexScenario.modes
        cls.no_daemon         = CTRexScenario.no_daemon
        cls.stl_trex          = CTRexScenario.stl_trex
        cls.bird_trex         = CTRexScenario.bird_trex
        cls.emu_trex          = CTRexScenario.emu_trex
        cls.trex              = CTRexScenario.trex
        cls.trex_crashed      = CTRexScenario.trex_crashed

        cls.is_VM             = 'VM' in cls.modes
        cls.is_dummy_ports    = 'dummy' in cls.modes
        cls.is_linux_stack    = 'linux_stack' in cls.modes
        cls.is_bird           = 'bird' in cls.modes
        cls.is_emu            = 'emu' in cls.modes
        cls.is_loopback       = 'loopback' in cls.modes
        cls.is_lowend         = 'lowend' in cls.modes
        cls.is_switch         = 'switch' in cls.modes
        cls.is_vdev           = 'vdev' in cls.modes
        cls.is_vf_nics        = 'vf_nics' in cls.modes
        cls.is_virt_nics      = 'virt_nics' in cls.modes

        if not CTRexScenario.is_init:
            if cls.trex and not cls.no_daemon: # stateful
                CTRexScenario.trex_version = cls.trex.get_trex_version()
            #update elk const object 
            if cls.elk:
                timediff  = timedelta(hours=2) # workaround to get IL timezone
                date_str  = CTRexScenario.trex_version['Date'].strip()
                timestamp = datetime.strptime(date_str, '%b %d %Y , %H:%M:%S') - timediff

                img               = CTRexScenario.elk_info['info']['image']
                img['sha']        = CTRexScenario.trex_version['Git SHA']
                img['build_time'] = timestamp.strftime("%Y/%m/%d %H:%M:%S")
                img['version']    = CTRexScenario.trex_version['Version']

                setup = CTRexScenario.elk_info['info']['setup']
                if cls.is_loopback :
                    setup['dut'] = 'loopback' 
                else:
                    setup['dut'] = 'router' 

                if cls.is_VM:
                    setup['baremetal'] = False
                    setup['hypervisor'] = 'ESXi'       #TBD
                else:
                    setup['baremetal'] = True
            if not cls.is_loopback:
                # initilize the scenario based on received configuration, once per entire testing session
                CTRexScenario.router = CPlatform(CTRexScenario.router_cfg['silent_mode'])
                device_cfg           = CDeviceCfg()
                device_cfg.set_platform_config(CTRexScenario.router_cfg['config_dict'])
                device_cfg.set_tftp_config(CTRexScenario.router_cfg['tftp_config_dict'])
                CTRexScenario.router.load_platform_data_from_file(device_cfg)
                CTRexScenario.router.launch_connection(device_cfg)
                if CTRexScenario.router_cfg['forceImageReload']:
                    image_d = CTRexScenario.router.get_running_image_details();
                    running_image = image_d['image']
                    print('Current router image: %s' % running_image)
                    if cls.elk:
                        setup['dut'] = image_d.get('model','router');
                        print('Current router model : %s' % setup['dut'])
                    needed_image = device_cfg.get_image_name()
                    if not CTRexScenario.router.is_image_matches(needed_image):
                        print('Setting router image: %s' % needed_image)
                        CTRexScenario.router.config_tftp_server(device_cfg)
                        CTRexScenario.router.load_platform_image(needed_image)
                        CTRexScenario.router.set_boot_image(needed_image)
                        CTRexScenario.router.reload_platform(device_cfg)
                        CTRexScenario.router.launch_connection(device_cfg)
                        running_image = CTRexScenario.router.get_running_image_details()['image'] # verify image
                        if not CTRexScenario.router.is_image_matches(needed_image):
                            cls.fail('Unable to set router image: %s, current image is: %s' % (needed_image, running_image))
                    else:
                        print('Matches needed image: %s' % needed_image)
                    CTRexScenario.router_image = running_image

            if cls.modes:
                print(termstyle.green('\t!!!\tRunning with modes: %s, not suitable tests will be skipped.\t!!!' % list(cls.modes)))

            CTRexScenario.is_init = True
            print(termstyle.green("Done instantiating TRex scenario!\n"))

        cls.router            = CTRexScenario.router


    def get_elk_obj (self):
        obj=trex_scenario.copy_elk_info ()
        return (obj);

#   def assert_dict_eq (self, dict, key, val, error=''):
#           v1 = int(dict[key]))
#           self.assertEqual(v1, int(val), error)
#
#   def assert_dict_gt (self, d, key, val, error=''):
#           v1 = int(dict[key])
#           self.assert_gt(v1, int(val), error)

    def assertEqual(self, v1, v2, s):
        if v1 != v2:
            error='ERROR '+str(v1)+' !=  '+str(v2)+ '   '+s;
            self.fail(error)

    def assert_gt(self, v1, v2, s):
        if not v1 > v2:
            error='ERROR {big} <  {small}      {str}'.format(big = v1, small = v2, str = s)
            self.fail(error)

    def check_results_eq (self,res,name,val):
        if res is None:
            self.fail('TRex results cannot be None !')
            return

        if name not in res:
            self.fail('TRex results does not include key %s' % name)
            return

        if res[name] != float(val):
            self.fail('TRex results[%s]==%f and not as expected %f ' % (name, res[name], val))

    # create config file in server machine, based on default config file
    # input: list of arguments, while the last one is value, and the rest is "path" in config
    # example: alter_config_file('memory', 'dp_flows', 4000000)
    # returns path of created config file in server
    def alter_config_file(self, *args):
        assert len(args) > 1, 'alter_config_file: should be at least 2 arguments (name and value)'
        config_str = self.trex.get_trex_config()
        config_dict = yaml.load(config_str)[0]
        section = config_dict
        args = list(args)
        val = args.pop(-1)
        for i, arg in enumerate(args, 1):
            if type(section) is not dict:
                raise Exception('Can only change dict type sections in config file, type of section %s is %s' % (arg, type(section)))
            if i == len(args):
                section[arg] = val
            else:
                if arg not in section or section[arg] is None:
                    section[arg] = {}
                section = section[arg]
        tmp_dir = tempfile.mkdtemp()
        try:
            tmp_file =  'tmp_trex_cfg.yaml'
            with open(os.path.join(tmp_dir, tmp_file), 'w') as f:
                f.write(yaml.dump([config_dict]))
            res = self.trex.push_files(f.name)
            if not res:
                raise Exception('Could not save file')
            return os.path.join(self.trex.get_trex_files_path(), tmp_file)
        finally:
            shutil.rmtree(tmp_dir)

    def check_CPU_benchmark (self, trex_res, err = 25, minimal_cpu = 10, maximal_cpu = 85, elk_name = ''):
        cpu_util          = trex_res.get_avg_steady_state_value('trex-global.data.m_cpu_util_raw')
        trex_tx_bps       = trex_res.get_avg_steady_state_value('trex-global.data.m_tx_bps')
        trex_tx_pps       = trex_res.get_avg_steady_state_value('trex-global.data.m_tx_pps')
        expected_norm_cpu = self.get_benchmark_param('bw_per_core')
        cores             = self.get_benchmark_param('cores')
        ports_count       = trex_res.get_ports_count()
        total_dp_cores    = cores * (ports_count/2);
        if not (cpu_util and ports_count and cores):
            print("Can't calculate CPU benchmark, need to divide by zero: cpu util: %s, ports: %s, cores: %s" % (cpu_util, ports_count, cores))
            test_norm_cpu = -1
        else:
            test_norm_cpu = trex_tx_bps / (cpu_util * ports_count * cores * 2.5e6)

        if '1G' in self.modes:
            minimal_cpu /= 10.0

        if not self.is_virt_nics:
            if cpu_util > maximal_cpu:
                self.fail("CPU is too high (%s%%), probably queue full." % cpu_util )
            #if cpu_util < minimal_cpu:
            #    self.fail("CPU is too low (%s%%), can't verify performance in such low CPU%%." % cpu_util )

        print("TRex CPU utilization: %g%%, norm_cpu is : %g Gb/core" % (round(cpu_util, 2), round(test_norm_cpu, 2)))
        if test_norm_cpu < 0:
            return

        if not expected_norm_cpu:
            expected_norm_cpu = 1

        calc_error_precent = abs(100.0 * test_norm_cpu / expected_norm_cpu - 100)
        print('Err percent: %s' % calc_error_precent)
        #if calc_error_precent > err and cpu_util > 10:
        #    self.fail('Excepted bw_per_core ratio: %s, got: %g' % (expected_norm_cpu, round(test_norm_cpu)))

        trex_tx_gbps       = trex_tx_bps/1e9
        trex_tx_mpps       = trex_tx_pps/1e6

        trex_tx_gbps_pc  = trex_tx_gbps*100.0/(cpu_util*total_dp_cores);
        trex_tx_mpps_pc  = trex_tx_mpps*100.0/(cpu_util*total_dp_cores)

        trex_tx_pckt    = trex_res.get_last_value("trex-global.data.m_total_tx_pkts")
        trex_drops      = trex_res.get_total_drops()
        trex_drop_precent = trex_drops *100.0/trex_tx_pckt;

        # report benchmarks
        if self.elk : 
            elk_obj = self.get_elk_obj()
            print("Reporting to elk")
            obj ={ "name" : self.get_name()+elk_name,
                        "type"  : "stateful",
                        "cores" : total_dp_cores,
                        "cpu%"  : cpu_util,
                        "mpps" :  (trex_tx_mpps),
                        "streams_count" :1,
                        "mpps_pc" :  (trex_tx_mpps_pc),
                        "gbps_pc" :  (trex_tx_gbps_pc),
                        "gbps" :  (trex_tx_gbps),
                        "kcps"  : (trex_res.get_last_value("trex-global.data.m_tx_cps")/1000.0),
                        "avg-pktsize" : round((1000.0*trex_tx_gbps/(8.0*trex_tx_mpps))),
                };

            if trex_res.get_is_latency_exists():
                __min = min(trex_res.get_min_latency().values())
                __max = max(trex_res.get_max_latency().values())
                __avr = max(trex_res.get_avg_latency().values())
                __jitter = max(trex_res.get_jitter_latency().values())
                __max_win =  max(trex_res.get_avg_window_latency ().values())

                obj["latecny"]= { "min" : __min,
                                  "max" : __max,
                                  "avr" : __avr,
                                  "jitter" : __jitter,
                                  "max-win" : __max_win,
                                  "drop-rate" :trex_drop_precent
                                 };
            elk_obj['test'] =obj;
            pprint.pprint(elk_obj['test']);
            self.elk.perf.push_data(elk_obj)

    # actually greater or equal
    def check_results_gt(self, res, name, val):
        if res is None:
            self.fail('TRex results can not be None !')
            return

        if name not in res:
            self.fail('TRex results does not include key %s' % name)
            return

        if res[name] < float(val):
            self.fail('TRex results[%s] is %f and not as expected greater than %f ' % (name, res[name], val))


    # actually less or equal
    def check_results_lt(self, res, name, val):
        if res is None:
            self.fail('TRex results can not be None !')
            return

        if name not in res:
            self.fail('TRex results does not include key %s' % name)
            return

        if res[name] > float(val):
            self.fail('TRex results[%s] is %f and not as expected less than %f ' % (name, res[name], val))


    def check_for_trex_crash(self):
        pass

    def get_benchmark_param (self, param, sub_param = None, test_name = None, default = None):
        if not test_name:
            test_name = self.get_name()
        if test_name not in self.benchmark:
            if default ==None:
               self.skip('No data in benchmark.yaml for test: %s, param: %s. Skipping.' % (test_name, param))
            else:
                return default
        if sub_param:
            return self.benchmark[test_name][param].get(sub_param, default)
        else:
            return self.benchmark[test_name].get(param, default)

    def check_general_scenario_results (self, trex_res, check_latency = True, skip_expected = False):
        """ 

           skip expected is for case of TCP that those value does not exits beacuse we can'r predict them 

        """
        
        try:
            # check history size is enough
            if len(trex_res._history) < 5:
                self.fail('TRex results list is too short. Increase the test duration or check unexpected stopping.')

            # check if test is valid

            if skip_expected == False: 
              if not trex_res.is_done_warmup():
                  self.fail('TRex did not reach warm-up situtaion. Results are not valid.')

              #check that BW is not much more than expected
              trex_exp_bps = trex_res.get_expected_tx_rate().get('m_tx_expected_bps') / 1e6
              trex_cur_bps = max(trex_res.get_value_list('trex-global.data.m_tx_bps')) / 1e6

              assert trex_exp_bps > 0, 'Expected BPS is zero: %s' % trex_exp_bps

              if trex_exp_bps * 1.05 + 10 < trex_cur_bps:
                    msg = 'Got BW (%gMbps) that is %g%% more than expected (%gMbps)!' % (round(trex_cur_bps, 2), round(100.0 * trex_cur_bps / trex_exp_bps - 100, 2), round(trex_exp_bps, 2))
                    print('WARNING: %s' % msg)

            # check TRex number of drops
            trex_tx_pckt    = trex_res.get_last_value("trex-global.data.m_total_tx_pkts")
            trex_drops      = trex_res.get_total_drops()
            trex_drop_rate  = trex_res.get_drop_rate()
            if ( trex_drops > 0.001 * trex_tx_pckt) and (trex_drop_rate > 0.0):     # deliberately mask kickoff drops when TRex first initiated
                self.fail('Number of packet drops larger than 0.1% of all traffic')

            # check queue full, queue drop, allocation error
            m_total_alloc_error = trex_res.get_last_value("trex-global.data.m_total_alloc_error")
            m_total_queue_full = trex_res.get_last_value("trex-global.data.m_total_queue_full")
            m_total_queue_drop = trex_res.get_last_value("trex-global.data.m_total_queue_drop")
            self.assert_gt(1000, m_total_alloc_error, 'Got allocation errors. (%s), please review multiplier and templates configuration.' % m_total_alloc_error)
            self.assert_gt(1000, m_total_queue_drop, 'Too much queue_drop (%s), please review multiplier.' % m_total_queue_drop)

            if self.is_VM:
                allowed_queue_full = 10000 + trex_tx_pckt / 100
            else:
                allowed_queue_full = 1000 + trex_tx_pckt / 1000
            self.assert_gt(allowed_queue_full, m_total_queue_full, 'Too much queue_full (%s), please review multiplier.' % m_total_queue_full)

            # # check TRex expected counters
            #trex_exp_rate = trex_res.get_expected_tx_rate().get('m_tx_expected_bps')
            #assert trex_exp_rate is not None
            #trex_exp_gbps = trex_exp_rate/(10**9)

            if check_latency:
                # check that max latency does not exceed 1 msec
                if self.configuration.trex['trex_name'] == '10.56.217.210': # temporary workaround for latency issue in kiwi02, remove it ASAP. http://trex-tgn.cisco.com/youtrack/issue/trex-194
                    allowed_latency = 8000
                elif self.is_VM or self.is_virt_nics:
                    allowed_latency = 9999999
                else: # no excuses, check 1ms
                    allowed_latency = 1000
                if max(trex_res.get_max_latency().values()) > allowed_latency:
                    self.fail('LatencyError: Maximal latency exceeds %s (usec)' % allowed_latency)

                # check that avg latency does not exceed 1 msec
                if self.is_VM or self.is_virt_nics:
                    allowed_latency = 9999999
                else: # no excuses, check 1ms
                    allowed_latency = 1000
                if max(trex_res.get_avg_latency().values()) > allowed_latency:
                    self.fail('LatencyError: Average latency exceeds %s (usec)' % allowed_latency)

                ports_names = trex_res.get_last_value('trex-latecny-v2.data', 'port\-\d+')
                if not ports_names:
                    raise AbnormalResultError('Could not find ports info in TRex results, path: trex-latecny-v2.data.port-*')
                for port_name in ports_names:
                    path = 'trex-latecny-v2.data.%s.hist.cnt' % port_name
                    lat_count = trex_res.get_last_value(path)
                    if lat_count == 0:
                        self.fail('LatencyError: Number of latency packets received on %s is 0' % port_name)

            if not self.is_loopback:
                # check router number of drops --> deliberately masked- need to be figured out!!!!!
                pkt_drop_stats = self.router.get_drop_stats()
#               assert pkt_drop_stats['total_drops'] < 20

                # check for trex-router packet consistency
                # TODO: check if it's ok
                print('router drop stats: %s' % pkt_drop_stats)
                print('TRex drop stats: %s' % trex_drops)
                #self.assertEqual(pkt_drop_stats, trex_drops, "TRex's and router's drop stats don't match.")

        except KeyError as e:
            self.fail(e)
            #assert False

        # except AssertionError as e:
        #     e.args += ('TRex has crashed!') 
        #     raise

    @staticmethod
    def unzip_client_package():
        client_pkg_files = glob('%s/trex_client*.tar.gz' % CTRexScenario.scripts_path)
        if not len(client_pkg_files):
            raise Exception('Could not find client package')
        if len(client_pkg_files) > 1:
            raise Exception('Found more than one client packages')
        if not os.path.exists('%s/trex_client' % CTRexScenario.scripts_path):
            print('\nUnzipping package')
            return_code, _, stderr = misc_methods.run_command("tar -xzf %s -C %s" % (client_pkg_files[0], CTRexScenario.scripts_path))
            if return_code:
                raise Exception('Could not untar the client package: %s' % stderr)
        else:
            print('\nClient package is untarred')

    # We encountered error, don't fail the test immediately
    def fail(self, reason = 'Unknown error'):
        print('Error: %s' % reason)
        self.fail_reasons.append(reason)

    def clear_fails(self):
        if len(self.fail_reasons):
            print('Clearing fail reasons: %s' % self.fail_reasons)
            self.fail_reasons = []

    # skip running of the test, counts as 'passed' but prints 'skipped'
    @classmethod
    def skip(cls, message = 'Unknown reason'):
        print('Skip: %s' % message)
        cls.skipping = True
        raise SkipTest(message)

    # get name of currently running test
    def get_name(self):
        return self._testMethodName

    def setUp(self):
        self.skipping              = False
        self.fail_reasons          = []
        if hasattr(self, 'unsupported_modes'):
            modes_conflict = self.modes & set(self.unsupported_modes)
            if modes_conflict:
                self.skip("The test can't run with following modes of given setup: %s " % modes_conflict)

        if not self.stl_trex and not self.bird_trex and not self.astf_trex and not self.emu_trex and not self.trex.is_idle():
            print('Warning: TRex is not idle at setUp, trying to stop it.')
            self.trex.force_kill(confirm = False)
        if not self.is_loopback:
            print('')
            if not self.stl_trex and not self.astf_trex and not self.bird_trex and not self.emu_trex:
                if CTRexScenario.router_cfg['forceCleanConfig']:
                    self.router.load_clean_config()
                self.router.clear_counters()
                self.router.clear_packet_drop_stats()

    ########################################################################
    ####                DO NOT ADD TESTS TO THIS FILE                   ####
    ####    Added tests here will held once for EVERY test sub-class    ####
    ########################################################################

    # masked example to such test. uncomment to watch how it affects #
#   def test_isInitialized(self):
#       assert CTRexScenario.is_init == True
    def tearDown(self):
        if not self.stl_trex and not self.astf_trex and not self.bird_trex and not self.emu_trex and not self.trex.is_idle():
            print('Warning: TRex is not idle at tearDown, trying to stop it.')
            self.trex.force_kill(confirm = False)
        if not self.skipping:
            # print server logs of test run
            if self.trex and CTRexScenario.server_logs and not self.no_daemon:
                try:
                    print(termstyle.green('\n>>>>>>>>>>>>>>> Daemon log <<<<<<<<<<<<<<<'))
                    daemon_log = self.trex.get_trex_daemon_log()
                    log_size = len(daemon_log)
                    print(''.join(daemon_log[CTRexScenario.daemon_log_lines:]))
                    CTRexScenario.daemon_log_lines = log_size
                except Exception as e:
                    print("Can't get TRex daemon log:", e)
                try:
                    print(termstyle.green('>>>>>>>>>>>>>>>> Trex log <<<<<<<<<<<<<<<<'))
                    print(''.join(self.trex.get_trex_log()))
                except Exception as e:
                    print("Can't get TRex log:", e)
            if len(self.fail_reasons):
                sys.stdout.flush()
                raise Exception('Test failed. Reasons:\n%s' % '\n'.join(self.fail_reasons))
        sys.stdout.flush()

    def check_for_trex_crash(self):
        pass

    @classmethod
    def get_per_driver_params(cls):
        return {
            'net_af_packet': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                #'no_vlan_even_in_software_mode': True,
            },

            'net_vmxnet3': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan_even_in_software_mode': True,
            },

            'net_ixgbe': {
                'rate_percent': 30,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 300,
                'latency_9k_max_latency': 400,
                'no_vlan': True,
                'no_ipv6': True,
            },

            'net_ixgbe_vf': {
                'rate_percent': 30,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan': True,
                'no_ipv6': True,
                'no_vlan_even_in_software_mode': True,
                'max_pkt_size': 2000, # temporary, until we fix this
            },

            'net_i40e': {
                'rate_percent': 80,
                'rate_percent_soft': 10,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 200,
                'latency_9k_max_latency': 250,
            },

            'net_i40e_vf': {
                'rate_percent': 20,
                'rate_percent_soft': 1,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 300,
                'latency_9k_max_latency': 750,
                'no_vlan_even_in_software_mode': True,
            },

            'net_e1000_igb': {
                'rate_percent': 80,
                'total_pkts': 500,
                'rate_latency': 1,
                'latency_9k_enable': False,
            },

            'net_e1000_em': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan_even_in_software_mode': True,
            },

            'net_virtio': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
            },

            'net_mlx5': {
                'rate_percent': 40,
                'rate_percent_soft': 0.01 if cls.is_vf_nics else 1,
                'total_pkts': 1000,
                'rate_latency': 0.01 if cls.is_vf_nics else 1,
                'latency_9k_enable': False if cls.is_vf_nics else True,
                'latency_9k_max_average': 200,
                'latency_9k_max_latency': 200,
            },

            'net_enic': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'rx_bytes_fix': True,
                'no_vlan_even_in_software_mode': True,
            },

            'net_ntacc': {
                'rate_percent': 10,
                'rate_percent_soft': 1,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 170,
                'latency_9k_max_latency': 350,
                'no_vlan': True,
                'no_ipv6': True,
            },
        }

