import astf_path

class MyNDRPlugin():
    def __init__(self):
        pass

    def pre_iteration(self, run_results=None, **kwargs):
        """ Function ran before each iteration.

            :parameters:
                run_results: dict
                    A dictionary that contains the following keys (might be empty in case of first run):
                        queue_full_percentage: Percentage of packets that are queued.

                        error_flag: Whether there were errors in the previous run.

                        errors: The errors in the previous run.

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

                        mult_p: Percentage of multiplier in the domain.

                        mult: Multiplier of CPS.

                        total_tx_L1: Total TX L1.

                        total_rx_L1: Total RX L1.

                        iteration: Iteration number.

                    Pay attention: The multiplier related fields are of the upcoming iteration. All the rest are of the previous iteration.

                kwargs: dict
                    List of tunables passed as parameters.

        """
        # Pre iteration function. This function will run before TRex transmits to the DUT.
        # Could use this to better prepare the DUT, for example define shapers, policers, increase buffers and queues.
        # You can receive tunables in the command line, through the kwargs argument.
        pass

    def post_iteration(self, run_results, **kwargs):
        """ Function ran after each iteration.

            :parameters:
                run_results: dict
                    A dictionary that contains the following keys:
                        queue_full_percentage: Percentage of packets that are queued.

                        error_flag: Whether there were errors in the previous run.

                        errors: The errors in the previous run.

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

                        mult_p: Percentage of multiplier in the domain.

                        mult: Multiplier of CPS.

                        total_tx_L1: Total TX L1.

                        total_rx_L1: Total RX L1.

                        iteration: Iteration number.

                kwargs: dict
                    List of tunables passed as parameters.

            :returns:
                tuple of booleans: (should stop the benchmarking or not, invalid errors)


        """
        # Post iteration function. This function will run after TRex transmits to the DUT.
        # Could use this to decide if to continue the benchmark after querying the DUT post run.
        # The DUT might be overheated or any other thing that might make you want to stop the run.
        # Also can use this to allow specific errors, this will override the normal check that TRex performs on errors.
        # You can receive tunables in the command line, through the kwargs argument.
        should_stop  = False 
        invalid_errors == True if run_results['errors'] else False
        return should_stop, invalid_errors


# dynamic load of python module
def register():
    return MyNDRPlugin()