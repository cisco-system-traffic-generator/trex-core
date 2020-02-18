# from __future__ import division
import astf_path
from trex.astf.api import *

import json
import argparse
import sys
import time
import math
import imp
from copy import deepcopy

class MultiplierDomain:
    def __init__(self, low_bound, high_bound):
        """
            A class that represents the multiplier domain received by the user.

            :parameters: 
                high_bound:
                    Higher bound of the interval in which the ndr point is found.

                low_bound:
                    Lower bound of the interval in which the ndr point is found.

        """
        self.high_bound = high_bound
        self.low_bound = low_bound
        self.domain_size = high_bound - low_bound

    def convert_percent_to_mult(self, percent):
        """
            Converts percent in the domain to an actual multiplier.

            :parameters:
                percent: float
                    Percentage in between high and low bound. Low is 0% and High is 100%/

            :returns:
                Multiplier of cps
        """
        return int(self.low_bound + (percent * self.domain_size / 100))

    def convert_mult_to_percent(self, mult):
        """
            Converts a multiplier to percentage of the domain that was defined in init.

            :parameters: 
                mult: int
                    CPS multiplier.

            :returns:
                float, percentage

        """
        assert(mult >= self.low_bound and mult <= self.high_bound), "Invalid multiplier"
        return (mult - self.lower_bound) * 100 / self.domain_size


class ASTFNdrBenchConfig:
    def __init__(self, high_mult, low_mult, title='Title', iteration_duration=20.00,
                 q_full_resolution=2.00, allowed_error=1.0, max_iterations=10,
                 latency_pps=0, max_latency=0, lat_tolerance=0, verbose=False,
                 plugin_file=None, tunables={}, **kwargs):
        """
            Configuration parameters for the benchmark.

            :parameters:
                high_mult: int
                    Higher bound of the interval in which the ndr point is found.

                low_mult: int
                    Lower bound of the interval in which the ndr point is found.

                title: string
                    Title of the benchmark.

                iteration_duration: float
                    Duration of the iteration.

                q_full_resolution: float
                    Percent of queue full allowed.

                allowed_error: float
                    Percentage of allowed error.

                max_iterations: int
                    Max number of iterations allowed.

                latency_pps: int (uint32_t)
                    Rate of latency packets. Zero value means disable.

                max_latency: int
                    Max value of latency allowed in msec.

                lat_tolerance: int
                    Percentage of latency packets allowed above max latency. Default value is 0 %. (Max Latency will be compared against total max)

                verbose: boolean
                    Verbose mode

                plugin_file: string
                    Path to the plugin file.

                tunables: dict
                    Tunables for the plugin file.

                kwargs: dict
        """
        self.high_mult = high_mult
        self.low_mult = low_mult
        self.iteration_duration = iteration_duration
        self.q_full_resolution = q_full_resolution
        self.allowed_error = allowed_error
        self.max_iterations = max_iterations
        self.verbose = verbose
        self.title = title
        self.plugin_file = plugin_file
        self.plugin_enabled = False
        if self.plugin_file is not None:
            self.plugin_enabled = True
            self.plugin = self.load_plugin(self.plugin_file)
        self.tunables = tunables
        self.latency_pps = latency_pps
        self.max_latency = max_latency
        self.lat_tolerance = lat_tolerance
        self.max_latency_set = True if self.max_latency != 0 else False

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
        config_dict = {'high_mult': self.high_mult, 'low_mult': self.low_mult,
                       'iteration_duration': self.iteration_duration, 'q_full_resolution': self.q_full_resolution,
                       'allowed_error': self.allowed_error, 'max_iterations': self.max_iterations,
                       'verbose': self.verbose, 'title': self.title,
                       'plugin_file': self.plugin_file, 'tunables': self.tunables,
                       'latency_pps': self.latency_pps, 'max_latency':self.max_latency,
                       'lat_tolerance': self.lat_tolerance}
        return config_dict


class ASTFNdrBenchResults:
    def __init__(self, config=None, results={}):
        """
            NDR Bench Results

            :parameters:
                config: :class:`.ASTFNdrBenchConfig`
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
                print("Latency stats on port             :%d" % k)
                print ("    Average                       :%0.2f" % self.stats['latency'][k]['s_avg'])
                print ("    Total Max (usec)              :%d" % self.stats['latency'][k]['max_usec'])
                print ("    Min Delta (usec)              :%d" % self.stats['latency'][k]['min_usec'])
                print ("    Packets >  threash hold       :%d" % self.stats['latency'][k]['high_cnt'])
                print ("    Histogram                     :%s   " % self.stats['latency'][k]['histogram'])
        except TypeError:
            pass

    def print_run_stats(self):
        """
            Prints the TRex stats after a run (transmission).
        """
        print("Elapsed Time                      :%0.2f seconds" % self.stats['Elapsed Time'])
        print("BW Per Core                       :%0.2f Gbit/Sec @100%% per core" % float(self.stats['bw_per_core']))
        print("TX PPS                            :%s" % (self.convert_rate(float(self.stats['tx_pps']), True)))
        print("RX PPS                            :%s" % (self.convert_rate(float(self.stats['rx_pps']), True)))
        print("TX Utilization                    :%0.2f %%" % self.stats['tx_util'])
        print("TRex CPU                          :%0.2f %%" % self.stats['cpu_util'])
        print("Total TX L1                       :%s     " % (self.convert_rate(float(self.stats['total_tx_L1']))))
        print("Total RX L1                       :%s     " % (self.convert_rate(float(self.stats['total_rx_L1']))))
        print("Total TX L2                       :%s     " % (self.convert_rate(float(self.stats['tx_bps']))))
        print("Total RX L2                       :%s     " % (self.convert_rate(float(self.stats['rx_bps']))))
        if 'mult_difference' in self.stats:
            print("Distance from current Optimum     :%0.2f %%" % self.stats['mult_difference'])

        if self.config.latency_pps > 0:
            self.print_latency()

    def print_iteration_data(self):
        """
            Prints data regarding the current iteration.
        """
        if 'title' in self.stats:
            print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Iteration                         :%s" % self.stats['iteration'])
        print("Multiplier                        :%s" % self.stats['mult'])
        print("Multiplier Percentage             :%0.2f %%" % self.stats['mult_p'])
        print("Max Multiplier                    :%s" % self.config.high_mult)
        print("Min Multiplier                    :%s" % self.config.low_mult)
        print("Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        print("Errors                            :%s" % ("Yes" if self.stats['error_flag'] else "No"))
        if self.config.max_latency_set:
            print("Valid Latency                     :%s" % self.stats['valid_latency'])
        self.print_run_stats()

    def print_final(self):
        """
            Prints the final data regarding where the NDR is found.
        """
        print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Total Iterations                  :%s " % self.stats['total_iterations'])
        print("Max Multiplier                    :%s" % self.config.high_mult)
        print("Min Multiplier                    :%s" % self.config.low_mult)
        print("Optimal P-Drop Mult               :%s" % self.stats['mult'])
        print("P-Drop Percentage                 :%0.2f %%" % self.stats['mult_p'])
        print("Queue Full at Optimal P-Drop Mult :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        print("Errors at Optimal P-Drop Mult     :%s" % ("Yes" if self.stats['error_flag'] else "No"))
        if self.config.max_latency_set:
            print("Valid Latency at Opt. P-Drop Mult :%s" % self.stats['valid_latency'])
        self.print_run_stats()

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
                   'OPT Multiplier [%]': self.stats['mult_p'],
                   'Max Multiplier ': self.config.high_mult,
                   'Elapsed Time [Sec]': str(round(self.stats['Elapsed Time'], 2)),
                   'Total Iterations': self.stats.get('total_iterations', None),
                   'Title': self.stats['title'],
                   'Latency': dict(self.stats['latency']),
                   'Valid Latency': "Yes" if self.stats['valid_latency'] else "No",
                   "Error Status": ("Yes" if self.stats['error_flag'] else "No"), 
                   'Errors': self.stats["errors"]}

        return hu_dict


class ASTFNdrBench:
    def __init__(self, astf_client, config):
        """
            ASTFNDRBench class object

            :parameters:
                astf_client: :class:`.ASTFClient`
                    ASTF Client

                config: :class:`.ASTFNdrBenchConfig`
                    Configurations and parameters for the benchmark
        """
        self.config = config
        self.results = ASTFNdrBenchResults(config)
        self.astf_client = astf_client
        self.results.update({'title': self.config.title})
        self.mult_domain = MultiplierDomain(self.config.low_mult, self.config.high_mult)
        self.opt_run_stats = {}
        self.opt_run_stats['mult_p'] = 0 # percent

    def plugin_pre_iteration(self, run_results=None, **kwargs):
        """
            Plugin pre iteration wrapper in order to pass the plugin a deep copy of the run results,
            since the user might change the actual run results. Consult the Plugin API for more information.
        """
        self.config.plugin.pre_iteration(deepcopy(run_results), **kwargs)

    def plugin_post_iteration(self, run_results, **kwargs):
        """
            Plugin pre iteration wrapper in order to pass the plugin a deep copy of the run results,
            since the user might change the actual run results. Consult the Plugin API for more information.
        """
        return self.config.plugin.post_iteration(deepcopy(run_results), **kwargs)

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
                    The latency field of get_stats() of the :class:`.ASTFClient`

            :returns:
                The max latency in that run.
        """
        max_latency = 0
        for port in latency_data.keys():
            if type(port) != int:
                continue
            max_key = 0
            for entry in latency_data[port]['hist']['histogram']:
                max_key = max(entry['key'], max_key)
            max_latency = max(latency_data[port]['hist']['max_usec'], max_latency, max_key)
        return max_latency

    def calculate_latency_percentage(self, latency_data):
        """
            Calculates the percentage of latency packets beyond the max latency parameter.
            The percentage is calculated independently for each port and the maximal percentage is returned.
            Call this only if latency tolerance is more than 0%.

            :parameters:
                latency_data: dict
                    The latency field of get_stats() of the :class:`.ASTFClient`

            :returns:
                A float represeting the percentage of latency packets above max latency.
        """
        latency_percentage = 0
        for port in latency_data.keys():
            total_packets = 0
            packets_above_max_latency = 0
            if type(port) != int:
                continue
            port_histogram = latency_data[port]['hist']['histogram']
            for entry in port_histogram:
                total_packets += entry['val']
                if entry['key'] >= self.config.max_latency:
                    packets_above_max_latency += entry['val']
            total_packets = 1 if total_packets == 0 else total_packets
            packets_above_max_latency_percentage = float(packets_above_max_latency) / total_packets * 100
            latency_percentage = max(latency_percentage, packets_above_max_latency_percentage)
        return latency_percentage

    def is_valid_latency(self, latency_data):
        """
            Returns a boolean flag indicating if the latency of a run is valid.
            In case latency was not set then it returns True.

            :parameters:
                latency_data: dict
                    The latency field of get_stats() of the :class:`.ASTFClient`

            :returns:
                A boolean flag indiciating the latency was valid.
        """
        if self.config.latency_pps > 0 and self.config.max_latency_set:
            if self.config.lat_tolerance == 0:
                return self.config.max_latency >= self.calculate_max_latency_received(latency_data)
            else:
                return self.config.lat_tolerance >= self.calculate_latency_percentage(latency_data)
        else:
            return True


    def update_opt_stats(self, new_stats):
        """
            Updates the optimal stats if the new_stats are better.

            :parameters:
                new_stats: dict
                    Statistics of some run.
        """
        if new_stats['queue_full_percentage'] <= self.config.q_full_resolution and new_stats['valid_latency'] and not new_stats['error_flag']:
            if new_stats['mult_p'] > self.opt_run_stats['mult_p']:
                self.opt_run_stats.update(new_stats)


    def perf_run(self, mult):
        """
            Transmits traffic through the ASTF client object in the class. 

            :parameters:
                mult: int
                    Multiply total CPS of profile by this value.

            :returns:
                Dictionary with the results of the run.
        """
        self.astf_client.stop()
         # allow time for counters to settle from previous runs
        time.sleep(10)
        self.astf_client.clear_stats()
        self.astf_client.start(mult=mult, nc=True, latency_pps=self.config.latency_pps)
        time_slept = 0
        sleep_interval = 1 # in seconds
        error_flag = False
        while time_slept < self.config.iteration_duration:
            time.sleep(sleep_interval)
            time_slept += sleep_interval
            stats = self.astf_client.get_stats()
            error_flag, errors = self.astf_client.is_traffic_stats_error(stats['traffic'])
            if error_flag:
                break
        self.astf_client.stop()
        opackets = stats['total']['opackets']
        ipackets = stats['total']['ipackets']
        q_full_packets = stats['global']['queue_full']
        q_full_percentage = float((q_full_packets / float(opackets)) * 100.000)
        latency_stats = stats['latency']
        latency_groups = {}
        if latency_stats:
            for i in latency_stats.keys():
                if type(i) != int:
                    continue
                latency_dict = latency_stats[i]['hist']
                latency_groups[i] = latency_dict
        tx_bps = stats['total']['tx_bps']
        rx_bps = stats['total']['rx_bps']
        tx_util_norm = stats['total']['tx_util'] / self.astf_client.get_port_count()
        self.results.stats['total_iterations'] += 1 
        run_results = {'error_flag': error_flag, 'errors': errors, 'queue_full_percentage': q_full_percentage,
                       'valid_latency': self.is_valid_latency(latency_stats),
                       'rate_tx_bps': tx_bps,
                       'rate_rx_bps': rx_bps,
                       'tx_util': tx_util_norm, 'latency': latency_groups,
                       'cpu_util': stats['global']['cpu_util'], 'tx_pps': stats['total']['tx_pps'],
                       'bw_per_core': stats['global']['bw_per_core'], 'rx_pps': stats['total']['rx_pps'],
                       'total_tx_L1': stats['total']['tx_bps_L1'],
                       'total_rx_L1': stats['total']['rx_bps_L1'], 'tx_bps': stats['total']['tx_bps'],
                       'rx_bps': stats['total']['rx_bps'],
                       'total_iterations': self.results.stats['total_iterations']}
        return run_results

    def perf_run_interval(self, high_bound, low_bound):
        """
            Searches for NDR in an given interval bounded by the two parameters. Based on the number of iterations which is supplied in the :class:`.ASTFNdrBenchConfig`
            object of the class will perform multiple transmitting runs until one of the stopping conditions is met.

            :parameters:
                high_bound: float
                    In percents of :class:`.MultiplierDomain`

                low_bound: float
                    In percents of :class:`.MultiplierDomain`

            :returns:
                Dictionary of the optimal run stats found in the interval, based on the criterions defined in :class:`.ASTFNdrBenchConfig`,
        """
        current_run_results = ASTFNdrBenchResults(self.config)
        current_run_stats = self.results.stats
        current_run_stats['iteration'] = 0
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False
        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['mult_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['mult'] = self.mult_domain.convert_percent_to_mult(current_run_stats['mult_p'])
            if plugin_enabled:
                self.plugin_pre_iteration(run_results=current_run_stats, **self.config.tunables)
            current_run_stats.update(self.perf_run(current_run_stats['mult']))
            error_flag = current_run_stats['error_flag']
            q_full_percentage = current_run_stats['queue_full_percentage']
            valid_latency = current_run_stats['valid_latency']
            current_run_stats['mult_difference'] = abs(current_run_stats['mult_p'] - self.opt_run_stats['mult_p'])
            current_run_results.update(current_run_stats)
            if plugin_enabled:
                plugin_stop, error_flag = self.plugin_post_iteration(run_results=current_run_stats, **self.config.tunables)
            if self.config.verbose:
                if error_flag:
                    current_run_results.print_state("Errors Occurred", high_bound, low_bound)
                elif q_full_percentage > self.config.q_full_resolution:
                    current_run_results.print_state("Queue Full Occurred", high_bound, low_bound)
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

            if q_full_percentage <= self.config.q_full_resolution and valid_latency and not error_flag:
                if current_run_stats['mult_p'] > self.opt_run_stats['mult_p']:
                    self.opt_run_stats.update(current_run_stats)
                    if current_run_stats['mult_difference'] <= self.config.allowed_error:
                        break
                    low_bound = current_run_stats['mult_p']
                    current_run_stats['iteration'] += 1
                    if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                        break
                    else:
                        continue
                else:
                    break
            else:
                if current_run_stats['mult_difference'] <= self.config.allowed_error:
                    break
            high_bound = current_run_stats['mult_p']
            current_run_stats['iteration'] += 1
            if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                break

        self.opt_run_stats['iteration'] = current_run_stats['iteration']
        self.opt_run_stats['total_iterations'] = current_run_stats['total_iterations']
        self.opt_run_stats['rate_difference'] = 0
        return self.opt_run_stats

    def find_ndr(self):
        """
            Wrapper function, runs the binary search between 0% and 100% of the :class:`.MultiplierDomain`
        """
        self.perf_run_interval(high_bound=100, low_bound=0)


if __name__ == '__main__':
    print('Designed to be imported, not as stand-alone script.')
