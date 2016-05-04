#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
from collections import deque
from time import time, sleep

class STLBenchmark_Test(CStlGeneral_Test):
    """Benchark stateless performance"""

    def test_CPU_benchmark(self):
        timeout   = 60 # max time to wait for stabilization
        stabilize = 5  # ensure stabilization over this period
        print('')

        for profile_bench in self.get_benchmark_param('profiles'):
            cpu_utils    = deque([0] * stabilize, maxlen = stabilize)
            bws_per_core = deque([0] * stabilize, maxlen = stabilize)
            kwargs = profile_bench.get('kwargs', {})
            print('Testing profile %s, kwargs: %s' % (profile_bench['name'], kwargs))
            profile = STLProfile.load(os.path.join(CTRexScenario.scripts_path, profile_bench['name']), **kwargs)

            self.stl_trex.reset()
            self.stl_trex.clear_stats()
            sleep(1)
            self.stl_trex.add_streams(profile)
            self.stl_trex.start(mult = '10%')
            start_time = time()

            for i in range(timeout + 1):
                stats = self.stl_trex.get_stats()
                cpu_utils.append(stats['global']['cpu_util'])
                bws_per_core.append(stats['global']['bw_per_core'])
                if i > stabilize and min(cpu_utils) > max(cpu_utils) * 0.95:
                    break
                sleep(0.5)

            agv_cpu_util    = sum(cpu_utils) / stabilize
            agv_bw_per_core = sum(bws_per_core) / stabilize

            if i == timeout and agv_cpu_util > 10:
                raise Exception('Timeout on waiting for stabilization, last CPU util values: %s' % list(cpu_utils))
            if stats[0]['opackets'] < 1000 or stats[1]['opackets'] < 1000:
                raise Exception('Too few opackets, port0: %s, port1: %s' % (stats[0]['opackets'], stats[1]['opackets']))
            if stats['global']['queue_full'] > 100000:
                raise Exception('Too much queue_full: %s' % stats['global']['queue_full'])
            if not cpu_utils[-1]:
                raise Exception('CPU util is zero, last values: %s' % list(cpu_utils))
            print('Done (%ss), CPU util: %4g, bw_per_core: %6sGb/core' % (int(time() - start_time), agv_cpu_util, round(agv_bw_per_core, 2)))
            # TODO: add check of benchmark based on results from regression

            # report benchmarks
            if self.GAManager:
                try:
                    profile_repr = '%s.%s %s' % (CTRexScenario.setup_name,
                                                os.path.basename(profile_bench['name']),
                                                repr(kwargs).replace("'", ''))
                    self.GAManager.gaAddAction(Event = 'stateless_test', action = profile_repr,
                                            label = 'bw_per_core', value = int(agv_bw_per_core))
                    # TODO: report expected once acquired
                    #self.GAManager.gaAddAction(Event = 'stateless_test', action = profile_repr,
                    #                           label = 'bw_per_core_exp', value = int(expected_norm_cpu))
                    self.GAManager.emptyAndReportQ()
                except Exception as e:
                    print('Sending GA failed: %s' % e)

    def tearDown(self):
        self.stl_trex.reset()
        self.stl_trex.clear_stats()
        sleep(1)
        CStlGeneral_Test.tearDown(self)

