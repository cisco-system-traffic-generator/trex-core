import os
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *

def avg (values):
    return (sum(values) / float(len(values)))

# performance report object
class PerformanceReport(object):
    GOLDEN_NORMAL  = 1
    GOLDEN_FAIL    = 2
    GOLDEN_BETTER  = 3

    def __init__ (self,
                  scenario,
                  machine_name,
                  core_count,
                  avg_cpu,
                  avg_gbps,
                  avg_mpps,
                  avg_gbps_per_core,
                  avg_mpps_per_core,
                  ):

        self.scenario                = scenario
        self.machine_name            = machine_name
        self.core_count              = core_count
        self.avg_cpu                 = avg_cpu
        self.avg_gbps                = avg_gbps
        self.avg_mpps                = avg_mpps
        self.avg_gbps_per_core       = avg_gbps_per_core
        self.avg_mpps_per_core       = avg_mpps_per_core

    def show (self):

        print("\n")
        print("scenario:                                {0}".format(self.scenario))
        print("machine name:                            {0}".format(self.machine_name))
        print("DP core count:                           {0}".format(self.core_count))
        print("average CPU:                             {0}".format(self.avg_cpu))
        print("average Gbps:                            {0}".format(self.avg_gbps))
        print("average Mpps:                            {0}".format(self.avg_mpps))
        print("average pkt size (bytes):                {0}".format( (self.avg_gbps * 1000 / 8) / self.avg_mpps))
        print("average Gbps per core (at 100% CPU):     {0}".format(self.avg_gbps_per_core))
        print("average Mpps per core (at 100% CPU):     {0}".format(self.avg_mpps_per_core))


    def check_golden (self, golden_mpps):
        if self.avg_mpps_per_core < golden_mpps['min']:
            return self.GOLDEN_FAIL

        if self.avg_mpps_per_core > golden_mpps['max']:
            return self.GOLDEN_BETTER

        return self.GOLDEN_NORMAL

    def report_to_analytics(self, ga, golden_mpps):
        print("\n* Reporting to GA *\n")
        ga.gaAddTestQuery(TestName = self.scenario,
                          TRexMode = 'stl',
                          SetupName = self.machine_name,
                          TestType = 'performance',
                          Mppspc = self.avg_mpps_per_core,
                          ActionNumber = os.getenv("BUILD_ID","n/a"),
                          GoldenMin = golden_mpps['min'],
                          GoldenMax = golden_mpps['max'])

        ga.emptyAndReportQ()


class STLPerformance_Test(CStlGeneral_Test):
    """Tests for stateless client"""

    def setUp(self):

        CStlGeneral_Test.setUp(self)

        self.c = CTRexScenario.stl_trex
        self.c.connect()
        self.c.reset()

        

    def tearDown (self):
        CStlGeneral_Test.tearDown(self)


    def build_perf_profile_vm (self, pkt_size, cache_size = None):
        size = pkt_size - 4; # HW will add 4 bytes ethernet FCS
        src_ip = '16.0.0.1'
        dst_ip = '48.0.0.1'

        base_pkt = Ether()/IP(src=src_ip,dst=dst_ip)/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = STLScVmRaw( [   STLVmFlowVar ( "ip_src",  min_value="10.0.0.1", max_value="10.0.0.255", size=4, step=1,op="inc"),
                             STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src" ),
                             STLVmFixIpv4(offset = "IP")
                         ],
                         cache_size = cache_size
                        );

        pkt = STLPktBuilder(pkt = base_pkt/pad, vm = vm)
        return STLStream(packet = pkt, mode = STLTXCont())


    def build_perf_profile_syn_attack (self, pkt_size):
        size = pkt_size - 4; # HW will add 4 bytes ethernet FCS

        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")
        pad = max(0, size - len(base_pkt)) * 'x'

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                            STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),

                           STLVmFixIpv4(offset = "IP"), # fix checksum

                           STLVmWrFlowVar(fv_name="src_port", 
                                                pkt_offset= "TCP.sport") # fix udp len  

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())



    # single CPU, VM, no cache, 64 bytes
    def test_performance_vm_single_cpu (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "VM - 64 bytes, single CPU"
        scenario_cfg['streams']     = self.build_perf_profile_vm(64)
        scenario_cfg['core_count']  = 1

        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden'] 
        
        

        self.execute_single_scenario(scenario_cfg)


    # single CPU, VM, cached, 64 bytes
    def test_performance_vm_single_cpu_cached (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "VM - 64 bytes, single CPU, cache size 1024"
        scenario_cfg['streams']     = self.build_perf_profile_vm(64, cache_size = 1024)
        scenario_cfg['core_count']  = 1

        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden']

        self.execute_single_scenario(scenario_cfg)


    # single CPU, syn attack, 64 bytes
    def test_performance_syn_attack_single_cpu (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "syn attack - 64 bytes, single CPU"
        scenario_cfg['streams']     = self.build_perf_profile_syn_attack(64)
        scenario_cfg['core_count']  = 1

        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden']

        self.execute_single_scenario(scenario_cfg)


    # two CPUs, VM, no cache, 64 bytes
    def test_performance_vm_multi_cpus (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "VM - 64 bytes, multi CPUs"
        scenario_cfg['streams']     = self.build_perf_profile_vm(64)

        scenario_cfg['core_count']           = setup_cfg['core_count']
        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden']

        self.execute_single_scenario(scenario_cfg)



    # multi CPUs, VM, cached, 64 bytes
    def test_performance_vm_multi_cpus_cached (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "VM - 64 bytes, multi CPU, cache size 1024"
        scenario_cfg['streams']     = self.build_perf_profile_vm(64, cache_size = 1024)


        scenario_cfg['core_count']           = setup_cfg['core_count']
        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden']

        self.execute_single_scenario(scenario_cfg)


    # multi CPUs, syn attack, 64 bytes
    def test_performance_syn_attack_multi_cpus (self):
        setup_cfg = self.get_benchmark_param('cfg')
        scenario_cfg = {}

        scenario_cfg['name']        = "syn attack - 64 bytes, multi CPUs"
        scenario_cfg['streams']     = self.build_perf_profile_syn_attack(64)

        scenario_cfg['core_count']           = setup_cfg['core_count']
        scenario_cfg['mult']                 = setup_cfg['mult']
        scenario_cfg['mpps_per_core_golden'] = setup_cfg['mpps_per_core_golden']

        self.execute_single_scenario(scenario_cfg)



############################################# test's infra functions ###########################################

    def execute_single_scenario (self, scenario_cfg, iterations = 4):
        golden = scenario_cfg['mpps_per_core_golden']
        

        for i in range(iterations, -1, -1):
            report = self.execute_single_scenario_iteration(scenario_cfg)
            rc = report.check_golden(golden)

            if (rc == PerformanceReport.GOLDEN_NORMAL) or (rc == PerformanceReport.GOLDEN_BETTER):
                if self.GAManager:
                    report.report_to_analytics(self.GAManager, golden)

                return

            if rc == PerformanceReport.GOLDEN_BETTER:
                return

            print("\n*** Measured Mpps per core '{0}' is lower than expected golden '{1} - re-running scenario...{2} attempts left".format(report.avg_mpps_per_core, scenario_cfg['mpps_per_core_golden'], i))

        assert 0, "performance failure"




    def execute_single_scenario_iteration (self, scenario_cfg):

        print("\nExecuting performance scenario: '{0}'\n".format(scenario_cfg['name']))

        self.c.reset(ports = [0])
        self.c.add_streams(ports = [0], streams = scenario_cfg['streams'])

        # use one core
        cores_per_port = self.c.system_info.get('dp_core_count_per_port', 0)
        if cores_per_port < scenario_cfg['core_count']:
            assert 0, "test configuration requires {0} cores but only {1} per port are available".format(scenario_cfg['core_count'], cores_per_port)

        core_mask = (2 ** scenario_cfg['core_count']) - 1
        self.c.start(ports = [0], mult = scenario_cfg['mult'], core_mask = [core_mask])

        # stablize
        print("Step 1 - waiting for stabilization... (10 seconds)")
        for _ in range(10):
            time.sleep(1)
            sys.stdout.write('.')
            sys.stdout.flush()

        print("\n")

        samples = {'cpu' : [], 'bps': [], 'pps': []}

        # let the server gather samples
        print("Step 2 - Waiting for samples... (60 seconds)")

        for i in range(0, 3):

            # sample bps/pps
            for _ in range(0, 20):
                stats = self.c.get_stats(ports = 0)
                if stats['global'][ 'queue_full']>10000:
                    assert 0, "Queue is full need to tune the multiplier"

                    # CPU results are not valid cannot use them 
                samples['bps'].append(stats[0]['tx_bps'])
                samples['pps'].append(stats[0]['tx_pps'])
                time.sleep(1)
                sys.stdout.write('.')
                sys.stdout.flush()

            # sample CPU per core
            rc = self.c._transmit('get_utilization')
            if not rc:
                raise Exception(rc)

            data = rc.data()['cpu']
            # filter
            data = [s for s in data if s['ports'][0] == 0]

            assert len(data) == scenario_cfg['core_count'] , "sampling info does not match core count"

            for s in data:
                samples['cpu'] += s['history']
            

        stats = self.c.get_stats(ports = 0)
        self.c.stop(ports = [0])


        
        avg_values = {k:avg(v) for k, v in samples.items()}
        avg_cpu  = avg_values['cpu'] * scenario_cfg['core_count']
        avg_gbps = avg_values['bps'] / 1e9
        avg_mpps = avg_values['pps'] / 1e6

        avg_gbps_per_core = avg_gbps * (100.0 / avg_cpu)
        avg_mpps_per_core = avg_mpps * (100.0 / avg_cpu)

        report = PerformanceReport(scenario            =  scenario_cfg['name'],
                                   machine_name        =  CTRexScenario.setup_name,
                                   core_count          =  scenario_cfg['core_count'],
                                   avg_cpu             =  avg_cpu,
                                   avg_gbps            =  avg_gbps,
                                   avg_mpps            =  avg_mpps,
                                   avg_gbps_per_core   =  avg_gbps_per_core,
                                   avg_mpps_per_core   =  avg_mpps_per_core)


        report.show()

        print("")
        golden = scenario_cfg['mpps_per_core_golden']
        print("golden Mpps per core (at 100% CPU):      min: {0}, max {1}".format(golden['min'], golden['max']))
        

        return report

