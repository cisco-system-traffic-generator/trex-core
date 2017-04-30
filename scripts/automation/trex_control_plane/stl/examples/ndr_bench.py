# from __future__ import division
import stl_path
from trex_stl_lib.api import *

import time
import json
from pprint import pprint
import argparse
import sys
import numpy as np

"""
IN:
max_rate: the maximal rate measured for the dut (in mbps)
rate_interval [high_bound,low_bound] : array of the rate at which the iterations will start from and end at (both in percentage)
iteration_precision: the size, in percents, of the interval between iterations
resolution: how far (in percentage) you allow the actual result to be from the desired result

OUT:
the rate in which the desired result was calculated (percentage)

"""


class Rate:
    def __init__(self, rate):
        self.rate = float(rate)

    def convert_percent_to_rate(self, percent_of_rate):
        return (float(percent_of_rate) / 100.00) * self.rate

    def convert_rate_to_percent_of_max_rate(self, rate_portion):
        return (float(rate_portion) / float(self.rate)) * 100.00


class NdrBench:
    def __init__(self, server, iteration_duration, q_ful_resolution,
                 first_run_duration, pdr, pdr_error, ndr_results, max_iterations=10, latency=True, verbose=True):
        self.verbose = verbose
        self.server = server
        self.max_rate = 0  # in mbps
        self.iteration_duration = iteration_duration
        self.q_ful_resolution = q_ful_resolution
        self.first_run_duration = first_run_duration
        self.no_q_ful_point_percent = 0  # percent of max rate
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.ndr_results = ndr_results
        self.max_iterations = max_iterations
        self.latency = latency
        self.stl_client = STLClient(server=server)
        # connect to server
        self.stl_client.connect()

        # take all the ports
        self.stl_client.reset()

        # map ports - identify the routes
        table = stl_map_ports(self.stl_client)
        # pprint(table)
        dir_0 = [table['bi'][0][0]]
        profile_file = os.path.join(stl_path.STL_PROFILES_PATH, 'imix.py')  # load IMIX profile
        profile = STLProfile.load_py(profile_file)
        streams = profile.get_streams()
        # print("Mapped ports to sides {0} <--> {1}".format(dir_0, dir_1))
        if self.latency:
            burst_size = 1000
            pps = 1000
            pkt = STLPktBuilder(pkt=Ether() / IP(src="16.0.0.1", dst="48.0.0.1") / UDP(dport=12,
                                                                                       sport=1025) / 'at_least_16_bytes_payload_needed')
            total_pkts = burst_size
            s1 = STLStream(name='rx',
                           packet=pkt,
                           flow_stats=STLFlowLatencyStats(pg_id=5),
                           mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                 pps=pps))
            streams.append(s1)
        # add both streams to ports
        self.stl_client.add_streams(streams, ports=dir_0)
        # self.stl_client.add_streams(streams, ports=dir_1)
        self.ports = dir_0

    def disconnect(self):
        self.stl_client.disconnect()

    def perf_run(self, rate_mb):
        self.stl_client.clear_stats()
        self.stl_client.start(ports=self.ports, mult=(str(rate_mb)), duration=self.iteration_duration,
                              total=True)
        time.sleep(self.iteration_duration / 2)
        stats = self.stl_client.get_stats()
        # pprint(stats)
        self.stl_client.stop(ports=self.ports)
        opackets = stats[self.ports[0]]['opackets']
        ipackets = stats[1]['ipackets']
        rate = stats['total']['tx_bps_L1']
        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        q_ful_packets = stats['global']['queue_full']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        latency = []
        if self.latency:
            latency_dict = stats['latency'][5]['latency']
            latency = {'average': latency_dict['average'], 'total_max': latency_dict['total_max'],
                       'jitter': latency_dict['jitter'], 'total_min': latency_dict['total_min']}
        tx_util = stats[self.ports[0]]['tx_util']
        run_results = {'q_ful_percentage': q_ful_percentage, 'lost_p_percentage': lost_p_percentage, 'rate': rate,
                       'tx_util': tx_util, 'latency': latency}
        return run_results

    def __find_max_rate(self):
        run_results = self.perf_run("100%")
        lost_p_percentage = run_results['lost_p_percentage']
        q_ful_percentage = run_results['q_ful_percentage']
        max_rate_mb = run_results['rate'] / 1000000.00
        self.max_rate = float(max_rate_mb)
        if self.verbose:
            print "max rate in Mbps: %0.4f \n" % max_rate_mb
            print "drop rate is: %0.4f %% of opackets" % lost_p_percentage
            print "q ful is %0.4f %% of opackets" % q_ful_percentage
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        return q_ful_percentage, lost_p_percentage

    def perf_run_interval(self, high_bound, low_bound):
        """

        Args:
            high_bound: in percents of rate
            low_bound: in percents of rate

        Returns: ndr in interval between low and high bounds (opt), q_ful(%) at opt and drop_rate(%) at opt

        """
        max_rate = Rate(self.max_rate)
        opt_rate_p = 0  # percent
        q_ful_opt = 0
        drop_rate_opt = 0
        iteration = 0
        while iteration <= self.max_iterations:
            running_rate_percent = float((high_bound + low_bound)) / 2.00
            running_rate = max_rate.convert_percent_to_rate(running_rate_percent)
            run_results = self.perf_run((str(running_rate) + "mbps"))
            lost_p_percentage = run_results['lost_p_percentage']
            q_ful_percentage = run_results['q_ful_percentage']
            latency = run_results['latency']
            tx_util = run_results['tx_util']
            if self.verbose:
                print "\n*** iteration %d***\n" % iteration
                print("running at %0.4f %% of line rate which is: %0.4f mbps\n" % (running_rate_percent, running_rate))
                print(
                    "drop rate is: %0.4f %% and q ful is %0.4f %% of traffic\n" % (lost_p_percentage, q_ful_percentage))
                print "latency is:"
                pprint(latency)

            rate_diffrential = abs(running_rate_percent - opt_rate_p)
            if q_ful_percentage <= self.q_ful_resolution and lost_p_percentage <= self.pdr:
                if running_rate_percent > opt_rate_p:
                    opt_rate_p = running_rate_percent
                    q_ful_opt = q_ful_percentage
                    drop_rate_opt = lost_p_percentage
                    tx_util_opt = tx_util
                    latency_opt = latency
                    if rate_diffrential <= self.pdr_error:
                        break
                    low_bound = running_rate_percent
                    iteration += 1
                    continue
                else:
                    break
            else:
                if rate_diffrential <= self.pdr_error:
                    break
            high_bound = running_rate_percent
            iteration += 1
        rate = max_rate.convert_percent_to_rate(opt_rate_p)
        run_results_opt = {'q_ful_percentage': q_ful_opt, 'lost_p_percentage': drop_rate_opt, 'rate_p': opt_rate_p,
                           'tx_util': tx_util_opt, 'rate': rate, 'latency': latency_opt}
        return run_results_opt

    def find_ndr(self):
        q_ful_percent, drop_percent = self.__find_max_rate()
        max_rate = Rate(self.max_rate)
        if drop_percent > self.pdr:
            assumed_rate_percent = 100 - drop_percent  # percent calculation
            if self.verbose:
                assumed_rate_mb = max_rate.convert_percent_to_rate(assumed_rate_percent)
                print "assumed rate percent: %0.4f %% of max rate" % assumed_rate_percent
                print "assumed rate mbps: %0.4f " % assumed_rate_mb
            run_results = self.perf_run_interval(assumed_rate_percent, assumed_rate_percent - self.pdr_error)
        if q_ful_percent >= self.q_ful_resolution:
            run_results = self.perf_run_interval(100.00, 0.00)
        if self.ndr_results > 1:
            ndr_res = np.linspace(start=run_results['rate'] / self.ndr_results, stop=run_results['rate'],
                                  num=self.ndr_results, endpoint=True)
            run_results['ndr_points'] = ndr_res
        return run_results
