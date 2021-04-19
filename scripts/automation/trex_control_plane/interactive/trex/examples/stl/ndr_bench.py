# from __future__ import division
import stl_path
from trex.stl.api import *

import json
import argparse
import sys
import time
import math
import imp
from copy import deepcopy


class Rate:
    def __init__(self, rate):
        """
            A rate repressenting class.

            :parameters: 
                rate: float
                    Rate in bps
        """
        self.rate = float(rate)

    def convert_percent_to_rate(self, percent_of_rate):
        """
            Converts percent of the rate (defined in init) to an actual rate.

            :parameters:
                percent_of_rate: float
                    Percentage of rate that defined when class initialized.

            :returns:
                float,Rate in bps
        """
        return (float(percent_of_rate) / 100.00) * self.rate

    def convert_rate_to_percent_of_max_rate(self, rate_portion):
        """
            Converts a rate in bps to percentage of rate that was defined in init.

            :parameters: 
                rate_portion: float
                    Rate in bps

            :returns:
                float, percentage

        """
        return (float(rate_portion) / float(self.rate)) * 100.00

    def is_close(self, rate, rel_tol=0.05, abs_tol=1000000):
        """
            Returns if a rate is close to the rate that was defined in init.

            :parameters:
                rate: float
                    Rate to compare to the rate that was defined upon initialization.

                rel_tol: float
                    is a relative tolerance, it is multiplied by the greater of the magnitudes of the two arguments; 
                             as the values get larger, so does the allowed difference between them while still considering them equal.
                             Default Value = 5%.

                abs_tol: int
                    is an absolute tolerance that is applied as-is in all cases. 
                    If the difference is less than either of those tolerances, the values are considered equal.
                    Default value = 1 Mbit

            :returns:
                A boolean flag indicating it the given rate is close.
        """
        return abs(self.rate-rate) <= max(rel_tol * max(abs(self.rate), abs(rate)), abs_tol)


class NdrBenchConfig:
    def __init__(self, ports, title='Title', cores=1, iteration_duration=20.00,
                 q_full_resolution=2.00, first_run_duration=20.00, pdr=0.1, 
                 pdr_error=1.0, ndr_results=1, max_iterations=10,
                 max_latency=0, lat_tolerance=0, verbose=False, bi_dir=False,
                 plugin_file=None, tunables={}, opt_binary_search=False,
                 opt_binary_search_percentage=5, total_cores=1, **kwargs):
        """
            Configuration parameters for the benchmark.

            :parameters:
                ports: list
                    List of ports to transmit. Even ports will transmit and odd ones will receive, unless bi-directional traffic.

                title: string
                    Title of the benchmark.

                cores: int
                    Number of cores.

                iteration_duration: float
                    Duration of the iteration.

                q_full_resolution: float
                    Percent of queue full allowed.

                first_run_duration: float
                    Duration of the first run.

                pdr: float
                    Percentage of drop rate.

                pdr_error: float
                    Percentage of allowed error in pdr.

                ndr_results: int
                    Calculates the benchmark at each point scaled linearly under NDR [1-10]. Number of points

                max_iterations: int
                    Max number of iterations allowed.

                max_latency: int
                    Max value of latency allowed in msec.

                lat_tolerance: int
                    Percentage of latency packets allowed above max latency. Default value is 0 %. (Max Latency will be compared against total max)

                verbose: boolean
                    Verbose mode

                bi_dir: boolean
                    Bi-directional traffic if true, else uni-directional.

                plugin_file: string
                    Path to the plugin file.

                tunables: dict
                    Tunables for the plugin file.

                opt_binary_search: boolean
                    Flag to indicate if to search using the optimized binary mode or not.

                opt_binary_search_percentage: int
                    Percentage around assumed ndr allowed to search.

                kwargs: dict
        """

        self.bi_dir = bi_dir
        # The sleep call divides the duration by 2
        self.iteration_duration = (iteration_duration * 2)
        self.q_full_resolution = q_full_resolution
        self.first_run_duration = (first_run_duration * 2)
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.ndr_results = ndr_results
        self.max_iterations = max_iterations
        self.verbose = verbose
        self.title = title
        self.cores = cores
        self.total_cores = total_cores
        self.ports = list(ports)
        self.transmit_ports = [self.ports[i] for i in range(0, len(self.ports), 2)]
        self.receive_ports  = [self.ports[i] for i in range(1, len(self.ports), 2)]
        if self.bi_dir:
            self.transmit_ports = self.ports
            self.receive_ports  = self.ports
        self.transmit_core_masks = self.get_optimal_core_mask(num_of_cores=self.cores, num_of_ports=len(self.ports))
        self.plugin_file = plugin_file
        self.plugin_enabled = False
        if self.plugin_file is not None:
            self.plugin_enabled = True
            self.plugin = self.load_plugin(self.plugin_file)
        self.tunables = tunables
        self.opt_binary_search = opt_binary_search
        self.opt_binary_search_percentage = opt_binary_search_percentage
        self.latency = False
        self.max_latency = max_latency
        self.lat_tolerance = lat_tolerance
        self.max_latency_set = True if self.max_latency != 0 else False

    def get_optimal_core_mask(self, num_of_cores, num_of_ports):
        """
            Optimal core mask in case of bi directional traffic.

            :parameters:
                num_of_cores: int
                    Number of cores per dual port

                num_of_ports: int
                    Number of ports

            :returns:
                List of masks per each port to offer optimal traffic.
        """
        if not self.bi_dir:
            return None
        else:
            # Half of the cores are given to the first port and the second half to second port.
            # In case of an odd number of cores, one core (the middle one) is shared.
            half_of_cores = int(math.ceil(num_of_cores / 2.00))
            first_mask = (1 << num_of_cores) - (1 << (num_of_cores - half_of_cores))
            second_mask = (1 << half_of_cores) - 1
            return [first_mask, second_mask] * int(num_of_ports / 2)

    @classmethod
    def load_plugin(cls, plugin_file):
        """
            Load dynamically a plugin module so that we can provide the user with pre and post iteration API.

            :parameters:
                plugin_file: string
                    Path to the plugin file.
        """

        # check filename
        if not os.path.isfile(plugin_file):
            raise TRexError("File '{0}' does not exist".format(plugin_file))

        basedir = os.path.dirname(plugin_file)
        sys.path.insert(0, basedir)

        try:
            file    = os.path.basename(plugin_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module) # reload the update 

            plugin = module.register()

            return plugin

        except Exception as e:
            a, b, tb = sys.exc_info()
            x =''.join(traceback.format_list(traceback.extract_tb(tb)[1:])) + a.__name__ + ": " + str(b) + "\n"

            summary = "\nPython Traceback follows:\n\n" + x
            raise TRexError(summary)


        finally:
            sys.path.remove(basedir)

    def config_to_dict(self):
        """
            Create a dictionary of the configuration.

            :returns:
                Dictionary of configurations
        """
        config_dict = {'iteration_duration': self.iteration_duration, 'q_full_resolution': self.q_full_resolution,
                       'first_run_duration': self.first_run_duration, 'pdr': self.pdr, 'pdr_error': self.pdr_error,
                       'ndr_results': self.ndr_results, 'max_iterations': self.max_iterations,
                       'ports': self.ports, 'cores': self.cores, 'total_cores': self.total_cores, 'verbose': self.verbose,
                       'bi_dir' : self.bi_dir, 'plugin_file': self.plugin_file, 'tunables': self.tunables,
                       'opt_binary_search': self.opt_binary_search, 'title': self.title,
                       'opt_binary_search_percentage': self.opt_binary_search_percentage}
        return config_dict


class NdrBenchResults:
    def __init__(self, config=None, results={}):
        """
            NDR Bench Results

            :parameters:
                config: :class:`.NdrBenchConfig`
                    User configuration of parameters. Default value is none.

                results: dict
                    A dictionary containing partial or full results from a run.
        """
        self.stats = dict(results)
        self.init_time = float(time.time())
        self.config = config
        self.stats['total_iterations'] = 0

    def update(self, updated_dict):
        """
            Updates the elapsed time, and the stats of the class with the parameter

            :parameters:
                updated_dict: dict
                    Dictionary that we use as an reference to update our stats.
        """
        updated_dict['Elapsed Time'] = (float(time.time()) - self.init_time)
        self.stats.update(updated_dict)

    def convert_rate(self, rate_bps, packet=False):
        """
            Converts rate from bps or pps to a string.

            :parameters:
                rate_bps: float
                    Value of of rate in bps or pps based on the packet parameter.

                packet: boolean
                    If true then the rate is PPS, else bps.

            :returns:
                Rate as a formatted string.
        """
        converted = float(rate_bps)
        magnitude = 0
        while converted > 1000.00:
            magnitude += 1
            converted /= 1000.00
        converted = round(converted, 2)
        if packet:
            postfix = "PPS"
        else:
            postfix = "bps"
        if magnitude == 0:
            return str(converted) + postfix
        elif magnitude == 1:
            converted = round(converted)
            return str(converted) + " K" + postfix
        elif magnitude == 2:
            return str(converted) + " M" + postfix
        elif magnitude == 3:
            return str(converted) + " G" + postfix

    def print_latency(self):
        """
            Prints the latency stats in case there are any.
        """
        try:
            for k in self.stats['latency'].keys():
                print("Latency stats on port/pg_id       :%d" % k)
                print ("    Average                       :%0.2f" % self.stats['latency'][k]['average'])
                print ("    Jitter                        :%0.2f" % self.stats['latency'][k]['jitter'])
                print ("    Total Max                     :%0.2f" % self.stats['latency'][k]['total_max'])
                print ("    Total Min                     :%0.2f" % self.stats['latency'][k]['total_min'])
                print ("    Histogram                     :%s   " % self.stats['latency'][k]['histogram'])
        except TypeError:
            pass

    def print_run_stats(self):
        """
            Prints the T-REX stats after a run (transmission).
        """
        if self.config.bi_dir:
            traffic_dir = "bi-directional "
        else:
            traffic_dir = "uni-directional"
        print("Elapsed Time                      :%0.2f seconds" % self.stats['Elapsed Time'])
        print("BW Per Core                       :%0.2f Gbit/Sec @100%% per core" % float(self.stats['bw_per_core']))
        print("TX PPS %s            :%s" % (traffic_dir, self.convert_rate(float(self.stats['tx_pps']), True)))
        print("RX PPS %s            :%s" % (traffic_dir, self.convert_rate(float(self.stats['rx_pps']), True)))
        print("TX Utilization                    :%0.2f %%" % self.stats['tx_util'])
        print("TRex CPU                          :%0.2f %%" % self.stats['cpu_util'])
        print("Total TX L1 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['total_tx_L1']))))
        print("Total RX L1 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['total_rx_L1']))))
        print("Total TX L2 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['tx_bps']))))
        print("Total RX L2 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['rx_bps']))))
        if 'rate_difference' in self.stats:
            print("Distance from current Optimum     :%0.2f %%" % self.stats['rate_difference'])

        if self.config.latency:
            self.print_latency()

    def print_iteration_data(self):
        """
            Prints data regarding the current iteration.
        """
        if 'title' in self.stats:
            print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Iteration                         :{}".format(self.stats['iteration']))
        print("Running Rate                      :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("Running Rate (%% of max)           :%0.2f %%" % self.stats['rate_p'])
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Drop Rate                         :%0.5f %% of oPackets" % self.stats['drop_rate_percentage'])
        print("Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        if self.config.latency and self.config.max_latency_set:
            print("Valid Latency                     :%s" % self.stats['valid_latency'])
        self.print_run_stats()

    def print_final(self):
        """
            Prints the final data regarding where the NDR is found.
        """
        if self.config.bi_dir:
            traffic_dir = "bi-directional "
        else:
            traffic_dir = "uni-directional"
        print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Total Iterations                  :{}".format(self.stats['total_iterations']))
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Optimal P-Drop Rate               :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("P-Drop Rate (%% of max)            :%0.2f %%" % self.stats['rate_p'])
        print("Drop Rate at Optimal P-Drop Rate  :%0.5f %% of oPackets" % self.stats['drop_rate_percentage'])
        print("Queue Full at Optimal P-Drop Rate :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        if self.config.latency and self.config.max_latency_set:
            print("Valid Latency at Opt. P-Drop Rate :%s" % self.stats['valid_latency'])
        self.print_run_stats()
        for x in self.stats['ndr_points']:
            print("NDR(s) %s            :%s " % (traffic_dir, self.convert_rate(x)))

    def to_json(self):
        """
            Output the results to a json.
        """
        total_output = {'results': self.stats, 'config': self.config.config_to_dict()}
        return json.dumps(total_output)

    @staticmethod
    def print_state(state, high_bound, low_bound):
        print("\n\nStatus                            :%s" % state)
        if high_bound:
            print("Interval                          :[%d,%d]" % (low_bound, high_bound))

    def human_readable_dict(self):
        """
            Return a human readable dictionary of the results.
        """
        hu_dict = {'Queue Full [%]': str(round(self.stats['queue_full_percentage'], 2)) + "%",
                   'BW per core [Gbit/sec @100% per core]': str(
                       round(float(self.stats['bw_per_core']), 2)) + 'Gbit/Sec @100% per core',
                   'RX [MPPS]': self.convert_rate(float(self.stats['rx_pps']), True),
                   'TX [MPPS]': self.convert_rate(float(self.stats['tx_pps']), True),
                   'Line Utilization [%]': str(round(self.stats['tx_util'], 2)),
                   'CPU Utilization [%]': str(round(self.stats['cpu_util'],2)),
                   'Total TX L1': self.convert_rate(float(self.stats['total_tx_L1'])),
                   'Total RX L1': self.convert_rate(float(self.stats['total_rx_L1'])),
                   'TX [bps]': self.convert_rate(float(self.stats['tx_bps'])),
                   'RX [bps]': self.convert_rate(float(self.stats['rx_bps'])),
                   'OPT TX Rate [bps]': self.convert_rate(float(self.stats['rate_tx_bps'])),
                   'OPT RX Rate [bps]': self.convert_rate(float(self.stats['rate_rx_bps'])),
                   'OPT Rate (Multiplier) [%]': str(self.stats['rate_p']),
                   'Max Rate [bps]': self.convert_rate(float(self.stats['max_rate_bps'])),
                   'Drop Rate [%]': str(round(self.stats['drop_rate_percentage'], 2)),
                   'Elapsed Time [Sec]': str(round(self.stats['Elapsed Time'], 2)),
                   'NDR points': [self.convert_rate(float(x)) for x in self.stats['ndr_points']],
                   'Total Iterations': self.stats.get('iteration', None),
                   'Title': self.stats['title'],
                   'latency': dict(self.stats['latency']),
                   'valid_latency': self.stats['valid_latency']}

        return hu_dict


class NdrBench:
    def __init__(self, stl_client, config):
        """
            NDR Bench

            :parameters:
                stl_client: :class:`.STLClient`
                    STL Client

                config: :class:`.NdrBenchConfig`
                    Configurations and parameters for the benchmark
        """
        self.config = config
        self.results = NdrBenchResults(config)
        self.stl_client = stl_client
        self.results.update({'title': self.config.title})
        self.opt_run_stats = {}
        self.opt_run_stats['rate_p'] = 0 # percent

    def plugin_pre_iteration(self, finding_max_rate, run_results=None, **kwargs):
        """
            Plugin pre iteration wrapper in order to pass the plugin a deep copy of the run results,
            since the user might change the actual run results. Consult the Plugin API for more information.
        """
        self.config.plugin.pre_iteration(finding_max_rate, deepcopy(run_results), **kwargs)

    def plugin_post_iteration(self, finding_max_rate, run_results, **kwargs):
        """
            Plugin pre iteration wrapper in order to pass the plugin a deep copy of the run results,
            since the user might change the actual run results. Consult the Plugin API for more information.
        """
        return self.config.plugin.post_iteration(finding_max_rate, deepcopy(run_results), **kwargs)

    def max_iterations_reached(self, current_run_stats, high_bound, low_bound):
        if current_run_stats['iteration'] == self.config.max_iterations:
            self.opt_run_stats.update(current_run_stats)
            if self.config.verbose:
                self.results.print_state("Max Iterations reached. Results might not be fully accurate", high_bound,
                                         low_bound)
            return True
        return False

    def calculate_max_latency_received(self, latency_data):
        """
            Calculates the max latency of a run.
            Call this only if latency tolerance is 0%.

            :parameters:
                latency_data: dict
                    The latency field of get_stats() of the :class:`.STLClient`

            :returns:
                The max latency in that run.
        """
        max_latency = 0
        for pg_id in latency_data.keys():
            if type(pg_id) != int:
                continue
            max_latency = max(latency_data[pg_id]['latency']['total_max'], max_latency)
        return max_latency

    def calculate_latency_percentage(self, latency_data):
        """
            Calculates the percentage of latency packets beyond the max latency parameter.
            The percentage is calculated independently for each pg id and the maximal percentage is returned.
            Call this only if latency tolerance is more than 0%.

            :parameters:
                latency_data: dict
                    The latency field of get_stats() of the :class:`.STLClient`

            :returns:
                A float represeting the percentage of latency packets above max latency.
        """
        latency_percentage = 0
        for pg_id in latency_data.keys():
            if type(pg_id) != int:
                continue
            pg_id_histogram = latency_data[pg_id]['latency']['histogram']
            total_packets = sum(pg_id_histogram.values())
            packets_above_max_latency = sum(pg_id_histogram[index] for index in pg_id_histogram.keys() if index >= self.config.max_latency)
            packets_above_max_latency_percentage = float(packets_above_max_latency) / total_packets * 100
            latency_percentage = max(latency_percentage, packets_above_max_latency_percentage)
        return latency_percentage

    def is_valid_latency(self, latency_data):
        """
            Returns a boolean flag indicating if the latency of a run is valid.
            In case latency was not set then it returns True.

            :parameters:
                latency_data: dict
                    The latency field of get_stats() of the :class:`.STLClient`

            :returns:
                A boolean flag indiciating the latency was valid.
        """
        if self.config.latency and self.config.max_latency_set:
            if self.config.lat_tolerance == 0:
                return self.config.max_latency >= self.calculate_max_latency_received(latency_data)
            else:
                return self.config.lat_tolerance >= self.calculate_latency_percentage(latency_data)
        else:
            return True


    def calculate_ndr_points(self):
        """
            Calculates NDR points based on the ndr_results parameter in the :class:`.NdrBenchConfig` object.
        """
        ndr_res = [self.results.stats['tx_bps']]
        if self.config.ndr_results > 1:
            ndr_range = range(1, self.config.ndr_results + 1, 1)
            ndr_range.reverse()
            ndr_res = [float((self.results.stats['tx_bps'])) / float(t) for t in ndr_range]
        self.results.update({'ndr_points': ndr_res})

    def update_opt_stats(self, new_stats):
        """
            Updates the optimal stats if the new_stats are better.

            :parameters:
                new_stats: dict
                    Statistics of some run.
        """
        if new_stats['queue_full_percentage'] <= self.config.q_full_resolution and new_stats['drop_rate_percentage'] <= self.config.pdr \
                                        and new_stats['valid_latency']:
            if new_stats['rate_p'] > self.opt_run_stats['rate_p']:
                self.opt_run_stats.update(new_stats)

    def optimized_binary_search(self, lost_percentage, lost_allowed_percentage, stat_type):
        """
            Performs the Optimized Binary search algorithm.

            :parameters:
                lost_percentage: float
                    Percentage of drops/queue full in the previous run.

                lost_allowed_percentage: float
                    Percentage/Resolution of allowed drops/queue full.

                stat_type: string
                    Type of statistic that had drops. stat_type should be either "drop_rate_percentage", "queue_full_percentage" or "valid_latency".
                    Drops have priority over queue full which has priority over valid_latency,

            :returns:
                Dictionary of the optimal run stats found in the interval, based on the criterions defined in :class:`.NdrBenchConfig`,

        """
        max_rate = Rate(self.results.stats['max_rate_bps'])
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False

        current_run_stats = deepcopy(self.results.stats)

        assumed_ndr_rate = Rate(max_rate.convert_percent_to_rate(100 - lost_percentage))
        upper_bound = min(max_rate.rate, assumed_ndr_rate.convert_percent_to_rate(100 + self.config.opt_binary_search_percentage))
        upper_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(upper_bound)

        if not max_rate.is_close(upper_bound):
            # in case we are not close to the max rate, try with the upper bound of the assumed NDR
            current_run_stats['rate_p'] = upper_bound_percentage_of_max_rate
            current_run_stats['rate_tx_bps'] = upper_bound
            current_run_stats['iteration'] = "Upper bound of assumed NDR"
            if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            if self.config.verbose:
                self.results.print_state("Trying upper bound of assumed rate!", None, None)
            current_run_stats.update(self.perf_run(upper_bound_percentage_of_max_rate))
            self.results.update(current_run_stats)
            self.update_opt_stats(current_run_stats)
            if self.config.verbose:
                self.results.print_iteration_data()
            if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            if plugin_stop:
                if self.config.verbose:
                    self.results.print_state("Plugin decided to stop trying upper bound of assumed rate!", None, None)
                return current_run_stats

            if stat_type == "valid_latency":
                upper_bound_valid = current_run_stats[stat_type]
            else:
                upper_bound_valid = current_run_stats[stat_type] <= lost_allowed_percentage
            if upper_bound_valid:
                if self.config.verbose:
                    self.results.print_state("Upper bound of assumed NDR drops are below desired rate :)",\
                                             100, upper_bound_percentage_of_max_rate)
                return self.perf_run_interval(100, upper_bound_percentage_of_max_rate)

        # if you got here -> upper bound of assumed ndr has too many drops
        lower_bound = assumed_ndr_rate.convert_percent_to_rate(100 - self.config.opt_binary_search_percentage)
        lower_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(lower_bound)

        current_run_stats['rate_p'] = lower_bound_percentage_of_max_rate
        current_run_stats['rate_tx_bps'] = lower_bound
        current_run_stats['iteration'] = "Lower bound of assumed NDR"
        if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
        if self.config.verbose:
            self.results.print_state("Trying lower bound of assumed rate!", None, None)
        current_run_stats.update(self.perf_run(lower_bound_percentage_of_max_rate))
        self.results.update(current_run_stats)
        self.update_opt_stats(current_run_stats)
        if self.config.verbose:
                self.results.print_iteration_data()
        if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
        if plugin_stop:
            if self.config.verbose:
                self.results.print_state("Plugin decided to stop trying lower bound of assumed rate!", None, None)
            return current_run_stats


        if stat_type == "valid_latency":
            lower_bound_valid = current_run_stats[stat_type]
        else:
            lower_bound_valid = current_run_stats[stat_type] <= lost_allowed_percentage
        if lower_bound_valid:
            if self.config.verbose:
                self.results.print_state("Lower bound of assumed NDR drops are below desired rate :)",\
                                     upper_bound_percentage_of_max_rate, lower_bound_percentage_of_max_rate)
            return self.perf_run_interval(upper_bound_percentage_of_max_rate, lower_bound_percentage_of_max_rate)

        # if you got here -> lower bound of assumed ndr has too many drops
        else:
            if self.config.verbose:
                self.results.print_state("Lower bound of assumed NDR drops are beyond desired rate :(",\
                                     lower_bound_percentage_of_max_rate, 0)
            return self.perf_run_interval(lower_bound_percentage_of_max_rate, 0)

    def perf_run(self, rate_mb_percent, run_max=False):
        """
            Transmits traffic through the STL client object in the class. 

            :parameters:
                rate_mb_percent: float
                    Rate of transmit in Mbit.

                run_max: boolean
                    Flag indicating if we are transmitting the maximal rate.

            :returns:
                Dictionary with the results of the run.
        """
        self.stl_client.stop(ports=self.config.ports)
         # allow time for counters to settle from previous runs
        time.sleep(15)
        self.stl_client.clear_stats()
        duration = 0
        if run_max:
            duration = self.config.first_run_duration
            self.stl_client.start(ports=self.config.transmit_ports, mult="100%",
                                  duration=duration, core_mask=self.config.transmit_core_masks)
            rate_mb_percent = 100
        else:
            m_rate = Rate(self.results.stats['max_rate_bps'])
            if rate_mb_percent == 0:
                rate_mb_percent += 1
            run_rate = m_rate.convert_percent_to_rate(rate_mb_percent)
            duration = self.config.iteration_duration
            self.stl_client.start(ports=self.config.transmit_ports, mult=str(run_rate) + "bps",
                                  duration=duration, core_mask=self.config.transmit_core_masks)
        time.sleep(duration / 2)
        stats = self.stl_client.get_stats()
        self.stl_client.stop(ports=self.config.ports)
        opackets = stats['total']['opackets']
        ipackets = stats['total']['ipackets']
        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        q_full_packets = stats['global']['queue_full']
        q_full_percentage = float((q_full_packets / float(opackets)) * 100.000)
        latency_stats = stats['latency']
        if run_max and latency_stats:
            # first run & latency -> update that we have latency traffic
            self.config.latency = True
            self.results.latency = True
        latency_groups = {}
        if self.config.latency:
            for i in latency_stats.keys():
                if type(i) != int:
                    continue
                latency_dict = latency_stats[i]['latency']
                latency_groups[i] = latency_dict
        tx_bps = [stats[x]['tx_bps'] for x in self.config.transmit_ports]
        rx_bps = [stats[x]['rx_bps'] for x in self.config.receive_ports]
        tx_util_norm = sum([stats[x]['tx_util'] for x in self.config.transmit_ports]) / len(self.config.transmit_ports)
        self.results.stats['total_iterations'] = self.results.stats['total_iterations'] + 1 if not run_max else self.results.stats['total_iterations']
        run_results = {'queue_full_percentage': q_full_percentage, 'drop_rate_percentage': lost_p_percentage,
                       'valid_latency': self.is_valid_latency(latency_stats),
                       'rate_tx_bps': min(tx_bps),
                       'rate_rx_bps': min(rx_bps),
                       'tx_util': tx_util_norm, 'latency': latency_groups,
                       'cpu_util': stats['global']['cpu_util'], 'tx_pps': stats['total']['tx_pps'],
                       'bw_per_core': stats['global']['bw_per_core'], 'rx_pps': stats['total']['rx_pps'],
                       'rate_p': float(rate_mb_percent), 'total_tx_L1': stats['total']['tx_bps_L1'],
                       'total_rx_L1': stats['total']['rx_bps_L1'], 'tx_bps': stats['total']['tx_bps'],
                       'rx_bps': stats['total']['rx_bps'],
                       'total_iterations': self.results.stats['total_iterations']}
        return run_results

    def __find_max_rate(self):
        """
            Finds the maximal rate the hardware can transmit. This rate might have drops or queue full. 

            :returns:
                Dictionary with the results of the iteration, it also updates the :class:`.NdrBenchResults` object in the class.
        """
        if self.config.verbose:
            self.results.print_state("Calculation of max rate for DUT", None, None)
        run_results = self.perf_run(100, True)
        run_results['max_rate_bps'] = run_results['rate_tx_bps']
        self.results.update(run_results)
        if self.results.stats['drop_rate_percentage'] < 0:
            self.results.stats['drop_rate_percentage'] = 0
        if self.config.verbose:
            if run_results['rate_tx_bps'] < run_results['rate_rx_bps']:
                self.results.print_state("TX rate is slower than RX rate", None, None)
            self.results.print_iteration_data()
        return run_results

    def perf_run_interval(self, high_bound, low_bound):
        """
            Searches for NDR in an given interval bounded by the two parameters. Based on the number of iterations which is supplied in the :class:`.NdrBenchConfig`
            object of the class will perform multiple transmitting runs until one of the stopping conditions is met.

            :parameters:
                high_bound: float
                    In percents of maximal rate.

                low_bound: float
                    In percents of maximal rate

            :returns:
                Dictionary of the optimal run stats found in the interval, based on the criterions defined in :class:`.NdrBenchConfig`,
        """
        current_run_results = NdrBenchResults(self.config)
        current_run_stats = self.results.stats
        max_rate = Rate(self.results.stats['max_rate_bps'])
        current_run_stats['max_rate_bps'] = max_rate.rate
        current_run_stats['iteration'] = 0
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False
        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['rate_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['rate_tx_bps'] = max_rate.convert_percent_to_rate(current_run_stats['rate_p'])
            if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            current_run_stats.update(self.perf_run(str(current_run_stats['rate_p'])))
            if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            lost_p_percentage = current_run_stats['drop_rate_percentage']
            q_full_percentage = current_run_stats['queue_full_percentage']
            valid_latency = current_run_stats['valid_latency']
            current_run_stats['rate_difference'] = abs(current_run_stats['rate_p'] - self.opt_run_stats['rate_p'])
            current_run_results.update(current_run_stats)
            if self.config.verbose:
                if q_full_percentage > self.config.q_full_resolution:
                    current_run_results.print_state("Queue Full Occurred", high_bound, low_bound)
                elif lost_p_percentage > self.config.pdr:
                    current_run_results.print_state("Drops beyond Desired rate occurred", high_bound, low_bound)
                elif not valid_latency:
                    current_run_results.print_state("Invalid Latency", high_bound, low_bound)
                else:
                    current_run_results.print_state("Looking for NDR", high_bound, low_bound)
                current_run_results.print_iteration_data()
            if plugin_stop:
                if self.config.verbose:
                     current_run_results.print_state("Plugin decided to stop after the iteration!", high_bound, low_bound)
                self.update_opt_stats(current_run_stats)
                break

            if q_full_percentage <= self.config.q_full_resolution and lost_p_percentage <= self.config.pdr and valid_latency:
                if current_run_stats['rate_p'] > self.opt_run_stats['rate_p']:
                    self.opt_run_stats.update(current_run_stats)
                    if current_run_stats['rate_difference'] <= self.config.pdr_error:
                        break
                    low_bound = current_run_stats['rate_p']
                    current_run_stats['iteration'] += 1
                    if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                        break
                    else:
                        continue
                else:
                    break
            else:
                if current_run_stats['rate_difference'] <= self.config.pdr_error:
                    break
            high_bound = current_run_stats['rate_p']
            current_run_stats['iteration'] += 1
            if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                break

        self.opt_run_stats['iteration'] = current_run_stats['iteration']
        self.opt_run_stats['total_iterations'] = current_run_stats['total_iterations']
        self.opt_run_stats['rate_difference'] = 0
        return self.opt_run_stats

    def find_ndr(self):
        """
            Finds the NDR of the STL client that the class received. The function updates the :class:`.NdrBenchResults` object that 
            :class:`.NdrBench` contains. Decisions which algorithms to choose or if to apply a plugin are based on the object of type 
            :class:`.NdrBenchConfig` that this class contains.

            This is the top level function that finds the NDR and the user can use.
        """
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False
        if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=True, run_results=None, **self.config.tunables)
        first_run_results = self.__find_max_rate()
        self.update_opt_stats(first_run_results)
        if plugin_enabled:
            plugin_stop = self.plugin_post_iteration(finding_max_rate=True, run_results=first_run_results, **self.config.tunables)
        if plugin_stop:
            if self.config.verbose:
                self.results.print_state("Plugin decided to stop after trying to find the max rate!", None, None)
            self.calculate_ndr_points()
            return

        drop_percent = self.results.stats['drop_rate_percentage']
        q_full_percent = self.results.stats['queue_full_percentage']
        valid_latency = self.results.stats['valid_latency']

        if drop_percent > self.config.pdr:
            if self.config.opt_binary_search:
                if self.config.verbose:
                    self.results.print_state("Drops happened, searching for NDR with optimized binary search", None, None)
                self.results.update(self.optimized_binary_search(drop_percent, self.config.pdr, 'drop_rate_percentage'))
            else:
                if self.config.verbose:
                    self.results.print_state("Drops happened, searching for NDR", 100, 0)
                self.results.update(self.perf_run_interval(100, 0))

        elif q_full_percent >= self.config.q_full_resolution:
            if self.config.opt_binary_search:
                if self.config.verbose:
                    self.results.print_state("DUT Queue is Full, Looking for no queue full rate with optimized binary search", None, None)
                self.results.update(self.optimized_binary_search(q_full_percent, self.config.q_full_resolution, 'queue_full_percentage'))
            else:
                if self.config.verbose:
                    self.results.print_state("DUT Queue is Full, Looking for no queue full rate", 100, 0)
                self.results.update(self.perf_run_interval(100.00, 0.00))

        elif not valid_latency:
            if self.config.opt_binary_search:
                if self.config.verbose:
                    self.results.print_state("Invalid Latency, looking for NDR latency with optimized binary search", None, None)
                self.results.update(self.optimized_binary_search(q_full_percent, 0, 'valid_latency'))
            else:
                if self.config.verbose:
                    self.results.print_state("Invalid Latency, Looking for latency below tolerance", None, None)
                self.results.update(self.perf_run_interval(100.00, 0.00))

        else:
            if self.config.verbose:
                self.results.print_state("NDR found at max rate", None, None)

        self.calculate_ndr_points()


if __name__ == '__main__':
    print('Designed to be imported, not as stand-alone script.')
