Non Drop Rate
=======================

The NDR's purpose is to identify the maximal point in terms of packet and bandwidth throughput at which the Packet Loss Ratio (PLR) stands at 0%.
T-Rex offers a benchmark to find the NDR point.
The benchmarker is meant to test different DUTs. Each DUT has its own capabilities, specifications and API.
We clearly can't support all the DUTs in the world.
Hence, we offer the user an API which can help him integrate his DUT with the TRex NDR Benchmarker. 
The user can control his device before each iteration (pre) and optimize his DUT for maximum performance.
He also can get different data from the DUT after (post) the iteration, and decide whether to continue to the next iteration or stop.


`Complete Documentation <https://trex-tgn.cisco.com/trex/doc/trex_ndr_bench_doc.html>`_


The following snippet shows a simple yet general implementation of the API ::

    import stl_path

    class DUT():
        def __init__(self, name):
            self.counter = 0
            self.name = name

        def QoS(self, rate_tx_bps, rate_rx_bps):
            pass

        def prepare(self, rate_tx_bps, rate_rx_bps):
            self.QoS(rate_tx_bps, rate_rx_bps)

        def should_stop(self, drop_percentage, queue_full_percentage):
            self.counter += 1
            if self.counter > 2 and drop_percentage < 3 and queue_full_percentage < 1:
                return True
            return False

    class DeviceUnderTestNDRPlugin():
        def __init__(self):
            self.dut = DUT(name="Cisco Catalyst 3850x")

        def pre_iteration(self, finding_max_rate, run_results=None, **kwargs):
            if run_results is not None:
                self.dut.prepare(run_results['rate_tx_bps'], run_results['rate_rx_bps'])

        def post_iteration(self, finding_max_rate, run_results, **kwargs):
            return self.dut.should_stop(run_results['drop_rate_percentage'], run_results['queue_full_percentage'])


    # dynamic load of python module
    def register():
        return DeviceUnderTestNDRPlugin()


From MyNDRPlugin class
----------------------

.. autoclass:: trex.examples.stl.ndr_plugin.MyNDRPlugin
    :members: 
    :inherited-members:
    :member-order: bysource
