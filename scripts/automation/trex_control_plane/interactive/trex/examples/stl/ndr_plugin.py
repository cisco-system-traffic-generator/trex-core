import stl_path

class MyNDRPlugin():
    def __init__(self):
        pass

    def pre_iteration(self, finding_max_rate, run_results=None, **kwargs):
        """ Function ran before each iteration.

            :parameters:
                finding_max_rate: boolean
                    Indicates whether we are running for the first time, trying to find the max rate. In this is the case, the run_results will be None.

                run_results: dict
                    A dictionary that contains the following keys:
                        queue_full_percentage: Percentage of packets that are queued.

                        drop_rate_percentage: Percentage of packets that were dropped.

                        rate_tx_bps: TX rate in bps.

                        rate_rx_bps: RX rate in bps.

                        tx_util: TX utilization percentage.

                        latency: Latency groups.

                        cpu_util: CPU utilization percentage.

                        tx_pps: TX in pps.

                        rx_pps: RX in pps.

                        tx_bps: TX in bps.

                        rx_bps: RX in bps.

                        bw_per_core: Bandwidth per core.

                        rate_p: Running rate in percentage out of max.

                        total_tx_L1: Total TX L1.

                        total_rx_L1: Total RX L1.

                        iteration: Description of iteration (not necessarily a number)

                    Pay attention: The rate is of the upcoming iteration. All the rest are of the previous iteration.

                kwargs: dict
                    List of tunables passed as parameters.

        """
        # Pre iteration function. This function will run before TRex transmits to the DUT.
        # Could use this to better prepare the DUT, for example define shapers, policers, increase buffers and queues.
        # You can receive tunables in the command line, through the kwargs argument.
        pass

    def post_iteration(self, finding_max_rate, run_results, **kwargs):
        """ Function ran after each iteration.

            :parameters:
                finding_max_rate: boolean
                    Indicates whether we are running for the first time, trying to find the max rate. If this is the case, some values of run_results (like iteration for example) are not relevant.

                run_results: dict
                    A dictionary that contains the following keys:
                        queue_full_percentage: Percentage of packets that are queued.

                        drop_rate_percentage: Percentage of packets that were dropped.

                        rate_tx_bps: TX rate in bps.

                        rate_rx_bps: RX rate in bps.

                        tx_util: TX utilization percentage.

                        latency: Latency groups.

                        cpu_util: CPU utilization percentage.

                        tx_pps: TX in pps.

                        rx_pps: RX in pps.

                        tx_bps: TX in bps.

                        rx_bps: RX in bps.

                        bw_per_core: Bandwidth per core.

                        rate_p: Running rate in percentage out of max.

                        total_tx_L1: Total TX L1.

                        total_rx_L1: Total RX L1.

                        iteration: Description of iteration (not necessarily a number)

                kwargs: dict
                    List of tunables passed as parameters.

            :returns:
                bool: should stop the benchmarking or not.

        """
        # Post iteration function. This function will run after TRex transmits to the DUT.
        # Could use this to decide if to continue the benchmark after querying the DUT post run. The DUT might be overheated or any other thing that might make you want to stop the run.
        # You can receive tunables in the command line, through the kwargs argument.
        should_stop = False
        return should_stop


# dynamic load of python module
def register():
    return MyNDRPlugin()