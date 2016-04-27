#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
from collections import deque
from time import time, sleep

class STLBenchmark_Test(CStlGeneral_Test):
    """Benchark stateless performance"""

    def test_CPU_benchmark(self):
        timeout   = 30 # max time to wait for stabilization
        stabilize = 5  # ensure stabilization over this period

        for profile_bench in self.get_benchmark_param('profiles'):
            cpu_utils = deque([0] * stabilize, maxlen = stabilize)
            kwargs = profile_bench.get('kwargs', {})
            print('Testing profile %s, kwargs: %s' % (profile_bench['name'], kwargs))
            profile = STLProfile.load(os.path.join(CTRexScenario.scripts_path, profile_bench['name']), port_id = 0, **kwargs)

            self.stl_trex.reset()
            self.stl_trex.clear_stats()
            self.stl_trex.add_streams(profile, ports = [0, 1])
            self.stl_trex.start(ports = [0, 1], mult = '10%')
            start_time = time()

            for i in range(timeout + 1):
                stats = self.stl_trex.get_stats()
                cpu_utils.append(stats['global']['cpu_util'])
                print stats['global']['cpu_util']
                if i > stabilize and min(cpu_utils) > max(cpu_utils) * 0.98 - 0.1:
                    break
                sleep(0.5)

            if i == timeout:
                raise Exception('Timeout on waiting for stabilization, CPU util values: %s' % cpu_utils)
            if stats[0]['opackets'] < 10000 or stats[1]['opackets'] < 10000:
                raise Exception('Too few opackets, port0: %s, port1: %s' % (stats[0]['opackets'], stats[1]['opackets']))
            print stats['global']
            if stats['global']['queue_full'] > 100000:
                raise Exception('Too much queue_full: %s' % stats['global']['queue_full'])

            bw_per_core = 100 * stats['global']['tx_bps'] / cpu_utils[-1] / 1e6
            print('Done (%ss), CPU util: %4g, bw_per_core: %6sMb/core' % (int(time() - start_time), cpu_utils[-1], int(bw_per_core)))


