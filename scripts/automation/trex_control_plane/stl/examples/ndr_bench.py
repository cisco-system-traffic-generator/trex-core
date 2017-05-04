# from __future__ import division
import stl_path
from trex_stl_lib.api import *

import time
import json
from pprint import pprint
import argparse
import sys


class Rate:
    def __init__(self, rate):
        self.rate = float(rate)

    def convert_percent_to_rate(self, percent_of_rate):
        return (float(percent_of_rate) / 100.00) * self.rate

    def convert_rate_to_percent_of_max_rate(self, rate_portion):
        return (float(rate_portion) / float(self.rate)) * 100.00


class NdrBenchConfig:
    def __init__(self, ports, iteration_duration=20.00, q_ful_resolution=2.00,
                 first_run_duration=20.00, pdr=0.1, pdr_error=1.0, ndr_results=1, max_iterations=10, latency=True,
                 verbose=False):
        self.iteration_duration = iteration_duration
        self.q_ful_resolution = q_ful_resolution
        self.first_run_duration = first_run_duration
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.ndr_results = ndr_results
        self.max_iterations = max_iterations
        self.latencyCalculation = latency
        self.verbose = verbose
        self.ports = list(ports)


class NdrBenchResults:
    def __init__(self, results={}):
        self.stats = dict(results)

    def update(self, updated_dict):
        self.stats.update(updated_dict)

    def print_max_rate_stats(self):
        print"\n\nMax Rate Measured:            %0.2f Mbps" % self.stats['max_rate']
        print"Drop Rate:                    %0.2f %% (of Opackets)" % self.stats['drop_rate_percentage']
        print"Queue Full:                   %0.2f %% (of Opackets)" % self.stats['queue_full_percentage']

    def print_iteration_data(self):
        print"\nIteration                         :%d" % self.stats['iteration']
        print"Running Rate                      :%0.2f Mbps" % self.stats['rate_L1']
        print"Running Rate (%% of max)           :%0.2f %%" % self.stats['rate_p']
        print"Drop Rate                         :%0.2f %% of oPackets" % self.stats['drop_rate_percentage']
        print"Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage']
        print"BW Per Core                       :%0.2f %%" % self.stats['bw_per_core']
        print"TRex CPU                          :%0.2f %%" % self.stats['cpu_util']
        print"Distance from current Optimum     :%0.2f %%" % self.stats['rate_diffrential']

        # pprint(self.stats)
    def print_final(self):
        print"\nTotal Iterations                 :%d" % self.stats['iteration']
        print"Running Rate                      :%0.2f Mbps" % self.stats['rate_L1']
        print"Running Rate (%% of max)           :%0.2f %%" % self.stats['rate_p']
        print"Drop Rate                         :%0.2f %% of oPackets" % self.stats['drop_rate_percentage']
        print"Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage']
        print"BW Per Core                       :%0.2f %%" % self.stats['bw_per_core']
        print"TRex CPU                          :%0.2f %%" % self.stats['cpu_util']
        print"Distance from current Optimum     :%0.2f %%" % self.stats['rate_diffrential']
        for x in self.stats['ndr_points']:
            print"NDR(s)                            :%0.2f " % x


class NdrBench:
    def __init__(self, stl_client, config):
        self.config = config
        self.results = NdrBenchResults()
        self.stl_client = stl_client

    def perf_run(self, rate_mb_percent):
        self.stl_client.clear_stats()
        self.stl_client.start(ports=self.config.ports[0], mult=(str(rate_mb_percent) + "%"),
                              duration=self.config.iteration_duration,
                              total=True)
        time.sleep(self.config.iteration_duration / 2)
        stats = self.stl_client.get_stats()
        # pprint(stats)
        self.stl_client.stop(ports=self.config.ports)
        opackets = stats[self.config.ports[0]]['opackets']
        ipackets = stats[self.config.ports[1]]['ipackets']
        rate = stats['total']['tx_bps_L1']
        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        q_ful_packets = stats['global']['queue_full']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        latency = []
        if self.config.latencyCalculation:
            latency_dict = stats['latency'][5]['latency']
            latency = {'average': latency_dict['average'], 'total_max': latency_dict['total_max'],
                       'jitter': latency_dict['jitter'], 'total_min': latency_dict['total_min']}
        # tx_util = stats[self.ports[0]]['tx_util']
        # cpu_util = stats['global']['cpu_util']
        run_results = {'queue_full_percentage': q_ful_percentage, 'drop_rate_percentage': lost_p_percentage,
                       'rate_L1': rate,
                       'tx_util': stats['total']['tx_util'], 'latency': latency,
                       'cpu_util': stats['global']['cpu_util'], 'tx_pps': stats['total']['tx_pps'],
                       'bw_per_core': stats['global']['bw_per_core']}
        return run_results

    def __find_max_rate(self):
        run_results = self.perf_run("100")
        run_results['max_rate'] = float(run_results['rate_L1'] / 1000000.00)
        self.results.update(run_results)
        if self.results.stats['drop_rate_percentage'] < 0:
            self.results.stats['drop_rate_percentage'] = 0
        if self.config.verbose:
            self.results.print_max_rate_stats()

    def perf_run_interval(self, high_bound, low_bound):
        """

        Args:
            high_bound: in percents of rate
            low_bound: in percents of rate

        Returns: ndr in interval between low and high bounds (opt), q_ful(%) at opt and drop_rate(%) at opt

        """
        current_run_results = NdrBenchResults()
        current_run_stats = {}
        max_rate = Rate(self.results.stats['max_rate'])
        current_run_stats['max_rate'] = max_rate.rate
        opt_run_stats = {}
        opt_run_stats['rate_p'] = 0  # percent
        current_run_stats['iteration'] = 0
        while current_run_stats['iteration'] <= self.config.max_iterations:
            # running_rate_percent = float((high_bound + low_bound)) / 2.00
            current_run_stats['rate_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['rate_L1'] = max_rate.convert_percent_to_rate(current_run_stats['rate_p'])
            current_run_stats.update(self.perf_run(str(current_run_stats['rate_p'])))
            lost_p_percentage = current_run_stats['drop_rate_percentage']
            q_ful_percentage = current_run_stats['queue_full_percentage']
            current_run_stats['rate_diffrential'] = abs(current_run_stats['rate_p'] - opt_run_stats['rate_p'])
            if self.config.verbose:
                current_run_results.update(current_run_stats)
                current_run_results.print_iteration_data()
            if q_ful_percentage <= self.config.q_ful_resolution and lost_p_percentage <= self.config.pdr:
                if current_run_stats['rate_p'] > opt_run_stats['rate_p']:
                    opt_run_stats.update(current_run_stats)
                    if current_run_stats['rate_diffrential'] <= self.config.pdr_error:
                        break
                    low_bound = current_run_stats['rate_p']
                    current_run_stats['iteration'] += 1
                    continue
                else:
                    break
            else:
                if current_run_stats['rate_diffrential'] <= self.config.pdr_error:
                    break
            high_bound = current_run_stats['rate_p']
            current_run_stats['iteration'] += 1
        if current_run_stats['iteration'] == self.config.max_iterations + 1:
            opt_run_stats.update(current_run_stats)
        return opt_run_stats

    def find_ndr(self):
        self.__find_max_rate()
        drop_percent = self.results.stats['drop_rate_percentage']
        q_ful_percent = self.results.stats['queue_full_percentage']
        max_rate = Rate(self.results.stats['max_rate'])
        if drop_percent > self.config.pdr:
            assumed_rate_percent = 100 - drop_percent  # percent calculation
            if self.config.verbose:
                assumed_rate_mb = max_rate.convert_percent_to_rate(assumed_rate_percent)
                print "assumed rate percent: %0.4f %% of max rate" % assumed_rate_percent
                print "assumed rate mbps: %0.4f " % assumed_rate_mb
            self.results.update(
                self.perf_run_interval(assumed_rate_percent, assumed_rate_percent - self.config.pdr_error))
        if q_ful_percent >= self.config.q_ful_resolution:
            self.results.update(self.perf_run_interval(100.00, 0.00))
        else:
            self.results.update(self.perf_run_interval(100.00, 99.00))
            ndr_res = [self.results.stats['rate_p']]
            if self.config.ndr_results > 1:
                ndr_res = range(start=self.results.stats['rate_L1'] / self.config.ndr_results,
                                stop=self.results.stats['rate_L1'],
                                step=(self.results.stats['rate_L1'] / self.config.ndr_results))
            self.results.stats['ndr_points'] = ndr_res
