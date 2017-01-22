#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
from collections import deque
from time import time, sleep
import pprint

class STLBenchmark_Test(CStlGeneral_Test):
    """Benchark stateless performance"""

    def test_CPU_benchmark(self):
        critical_test = CTRexScenario.setup_name in ('kiwi02', 'trex08', 'trex09') # temporary patch, this test needs to be fixed
        timeout       = 60 # max time to wait for stabilization
        stabilize     = 5  # ensure stabilization over this period
        print('')

        #self.get_benchmark_param('profiles')
        #profiles=[{'bw_per_core': 1,
        #  'cpu_util': 1,
        #  'kwargs': {'packet_len': 64},
        #  'name': 'stl/udp_for_benchmarks.py'}]

        profiles = self.get_benchmark_param('profiles')
        dp_cores     = self.stl_trex.system_info.get('dp_core_count', 0)

        for profile_bench in profiles:

            cpu_utils    = deque([0] * stabilize, maxlen = stabilize)
            bps          = deque([0] * stabilize, maxlen = stabilize)
            pps          = deque([0] * stabilize, maxlen = stabilize)

            kwargs = profile_bench.get('kwargs', {})
            print('Testing profile %s, kwargs: %s' % (profile_bench['name'], kwargs))
            profile = STLProfile.load(os.path.join(CTRexScenario.scripts_path, profile_bench['name']), **kwargs)

            self.stl_trex.reset()
            self.stl_trex.clear_stats()
            sleep(1)
            self.stl_trex.add_streams(profile)
            mult = '1%' if self.is_virt_nics else '10%'
            self.stl_trex.start(mult = mult)
            start_time = time()

            for i in range(timeout + 1):
                stats = self.stl_trex.get_stats()
                cpu_utils.append(stats['global']['cpu_util'])
                bps.append(stats['global']['tx_bps'])
                pps.append(stats['global']['tx_pps'])

                if i > stabilize and min(cpu_utils) > max(cpu_utils) * 0.95:
                    break
                sleep(0.5)

            agv_cpu_util    = sum(cpu_utils) / stabilize
            agv_pps         = sum(pps) / stabilize
            agv_bps         = sum(bps) / stabilize

            if agv_cpu_util == 0.0:
                agv_cpu_util=1.0;

            agv_mpps         = (agv_pps/1e6);
            agv_gbps         = (agv_bps/1e9)


            agv_gbps_norm    = agv_gbps * 100.0/agv_cpu_util;
            agv_mpps_norm    = agv_mpps * 100.0/agv_cpu_util;

            agv_gbps_norm_pc    = agv_gbps_norm/dp_cores;
            agv_mpps_norm_pc    = agv_mpps_norm/dp_cores;


            if critical_test and i == timeout and agv_cpu_util > 10:
                raise Exception('Timeout on waiting for stabilization, last CPU util values: %s' % list(cpu_utils))
            if stats[0]['opackets'] < 300 or stats[1]['opackets'] < 300:
                raise Exception('Too few opackets, port0: %s, port1: %s' % (stats[0]['opackets'], stats[1]['opackets']))
            if stats['global']['queue_full'] > 100000:
                raise Exception('Too much queue_full: %s' % stats['global']['queue_full'])
            if not cpu_utils[-1]:
                raise Exception('CPU util is zero, last values: %s' % list(cpu_utils))
            print('Done (%ss), CPU util: %4g, norm_pps_per_core:%6smpps norm_bw_per_core: %6sGb/core' % (int(time() - start_time), agv_cpu_util, round(agv_mpps_norm_pc,2), round(agv_gbps_norm_pc, 2)))

            # report benchmarks to elk
            if self.elk:
                streams=kwargs.get('stream_count',1)
                elk_obj = self.get_elk_obj()
                print("\n* Reporting to elk *\n")
                name=profile_bench['name']
                elk_obj['test']={ "name" : name,
                            "type"  : "stateless-range",
                            "cores" : dp_cores,
                            "cpu%"  : agv_cpu_util,
                            "mpps" :  (agv_mpps),
                            "streams_count" :streams,
                            "mpps_pc" :  (agv_mpps_norm_pc),
                            "gbps_pc" :  (agv_gbps_norm_pc),
                            "gbps" :  (agv_gbps),
                            "avg-pktsize" : round((1000.0*agv_gbps/(8.0*agv_mpps))),
                            "latecny" : { "min" : -1.0,
                                          "max" : -1.0,
                                          "avr" : -1.0
                                         }
                    };
                #pprint.pprint(elk_obj);
                self.elk.perf.push_data(elk_obj)


    def tearDown(self):
        self.stl_trex.reset()
        self.stl_trex.clear_stats()
        sleep(1)
        CStlGeneral_Test.tearDown(self)

