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
