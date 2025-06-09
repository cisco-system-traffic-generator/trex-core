import time
import sys
from .astf_profile_test import ASTFProfile_Test, CTRexScenario
from common.performance_common import PerformanceReport
from pprint import pprint

def avg (values):
    return (sum(values) / float(len(values)))

class ASTFPerformanceTests(ASTFProfile_Test):
    """Tests for astf performance"""
    
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.c = cls.astf_trex
        cls.setup = CTRexScenario.setup_name
        astf_performance_setups = ["trex08", "trex09", "trex19"]
        if cls.setup not in astf_performance_setups:
            cls.skip("%s not part of astf performance setups: %s" % (cls.setup, astf_performance_setups))

    def get_scenario(self):
        name = self.get_tst_name()
        if name.startswith("test_performance_"):
            return name[len("test_performance_"):]
        return name

    def run_performance(self, profile_name, m, is_udp, is_tcp, mpps_per_core_golden, ipv6 =False, check_counters=True, nc = False):
        c = self.c

        if ipv6 and self.driver_params.get('no_ipv6', False):
            return

        c.reset()

        print('starting performance test with profile %s' % (profile_name))
        c.load_profile(self.get_profile_by_name(profile_name))
        c.clear_stats()
        c.start(nc= nc,mult = m,ipv6 = ipv6, duration=90)

        # stabilize
        print("Step 1 - waiting for stabilization... (10 seconds)")
        for _ in range(10):
            time.sleep(1)
            sys.stdout.write('.')
            sys.stdout.flush()

        print("\n")

        samples = {'cpu' : [], 'bps': [], 'pps': []}
        cores_count = self.c.system_info.get('dp_core_count', 0)

        # let the server gather samples
        print("Step 2 - Waiting for samples... (60 seconds)")

        port = 0

        for _ in range(0, 3):

            # sample bps/pps
            for _ in range(0, 20):
                stats = self.c.get_stats()
                max_queue_full = 100000 if self.is_VM else 10000 
                    # CPU results are not valid cannot use them 
                
                if stats['global'][ 'queue_full'] > max_queue_full:
                    assert 0, "Queue is full need to tune the multiplier"

                pps = 0
                bps = 0
                for port in range(self.get_port_count()):
                    bps += stats[port]['tx_bps']
                    pps += stats[port]['tx_pps']

                samples['bps'].append(bps)
                samples['pps'].append(pps)

                time.sleep(1)
                sys.stdout.write('.')
                sys.stdout.flush()

            # sample CPU per core
            rc = self.c._transmit('get_utilization')
            if not rc:
                raise Exception(rc)

            data = rc.data()['cpu']
            self.assertEqual(len(data), cores_count, "sampling info does not match core count")

            for s in data:
                samples['cpu'] += s['history']

        print("")

        self.c.wait_on_traffic()
        stats = self.c.get_stats()

        avg_values = {k:avg(v) for k, v in samples.items()}
        avg_cpu  = avg_values['cpu'] * cores_count
        avg_gbps = avg_values['bps'] / 1e9
        avg_mpps = avg_values['pps'] / 1e6

        avg_gbps_per_core = avg_gbps * (100.0 / avg_cpu)
        avg_mpps_per_core = avg_mpps * (100.0 / avg_cpu)

        report = PerformanceReport(scenario            =  self.get_scenario(),
                                   machine_name        =  CTRexScenario.setup_name,
                                   core_count          =  cores_count,
                                   avg_cpu             =  avg_cpu,
                                   avg_gbps            =  avg_gbps,
                                   avg_mpps            =  avg_mpps,
                                   avg_gbps_per_core   =  avg_gbps_per_core,
                                   avg_mpps_per_core   =  avg_mpps_per_core)


        report.show()
        print("")

        golden = mpps_per_core_golden
        print("golden Mpps per core (at 100% CPU):      min: {0}, max {1}".format(golden['min'], golden['max']))

        if check_counters:
            self.check_counters(stats,is_udp,is_tcp)
            if c.get_warnings():
                print('\n\n*** test had warnings ****\n\n')
                for w in c.get_warnings():
                    print(w)

        #report to elk
        if self.elk:
            elk_obj = self.get_elk_obj()
            report.report_to_elk(self.elk, elk_obj, test_type="advanced-stateful")

        rc = report.check_golden(golden)

        if rc == PerformanceReport.GOLDEN_NORMAL or rc == PerformanceReport.GOLDEN_BETTER:
            return

        print("\n*** Measured Mpps per core '{0}' is lower than expected golden '{1}'".format(report.avg_mpps_per_core, golden))

        assert 0, "performance failure"


    def test_performance_http_simple(self):
        mult  = self.get_benchmark_param('multiplier', test_name='test_tcp_http')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_tcp_http')
        profile = "http_simple.py"
        self.run_performance(profile, mult, is_udp=False, is_tcp=True, mpps_per_core_golden=mpps_per_core_golden)


    def test_performance_udp_pcap(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_udp_pcap')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_udp_pcap')
        profile = "udp_pcap.py"
        self.run_performance(profile, mult, is_udp=True, is_tcp=False, mpps_per_core_golden=mpps_per_core_golden)


    def test_performance_http_simple_ipv6(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_ipv6_tcp_http')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_ipv6_tcp_http')
        profile = "http_simple.py"
        self.run_performance(profile, mult, is_udp=False, is_tcp=True, mpps_per_core_golden=mpps_per_core_golden, ipv6=True)


    def test_performance_udp_pcap_ipv6(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_ipv6_udp_pcap')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_ipv6_udp_pcap')
        profile = "udp_pcap.py"
        self.run_performance(profile, mult, is_udp=True, is_tcp=False, mpps_per_core_golden=mpps_per_core_golden, ipv6=True)


    def test_performance_prof_sfr_tcp(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_sfr')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_tcp_sfr')
        profile = "sfr.py"
        self.run_performance(profile, mult, is_udp=False, is_tcp=True, mpps_per_core_golden=mpps_per_core_golden)


    def test_performance_prof_sfr_tcp_udp(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_udp_sfr')
        mpps_per_core_golden = self.get_benchmark_param('mpps_per_core_golden', test_name='test_tcp_udp_sfr')
        profile = "sfr_full.py"
        self.run_performance(profile, mult, is_udp=True, is_tcp=True, mpps_per_core_golden=mpps_per_core_golden)
