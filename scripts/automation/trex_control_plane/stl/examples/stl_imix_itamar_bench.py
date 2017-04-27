# from __future__ import division
import stl_path
from trex_stl_lib.api import *

import time
import json
from pprint import pprint
import argparse
import sys

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
    def __init__(self, stl_client, ports, iteration_duration, iteration_precision, q_ful_resolution,
                 first_run_duration, pdr, pdr_error=1, max_iterations=10, verbose=True):
        self.verbose = verbose
        self.stl_client = stl_client
        self.ports = ports
        self.max_rate = 0  # in mb
        self.iteration_duration = iteration_duration
        self.iteration_precision = iteration_precision
        self.q_ful_resolution = q_ful_resolution
        self.first_run_duration = first_run_duration
        self.no_q_ful_point_percent = 0  # percent of max rate
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.max_iterations = max_iterations

    def __find_max_rate(self):
        c = self.stl_client
        ports = self.ports
        c.clear_stats()
        duration = self.first_run_duration
        c.start(ports=ports, mult="100%", duration=duration, total=True)
        time.sleep(float(duration) / 2.00)
        stats = c.get_stats()
        c.clear_stats()
        c.stop(ports=ports)
        max_rate = stats['total']['tx_bps_L1']
        q_ful_packets = stats['global']['queue_full']
        opackets = stats[ports[0]]['opackets']
        ipackets = stats[1]['ipackets']
        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        max_rate_mb = float(max_rate) / 1000000.00
        self.max_rate = max_rate_mb
        if self.verbose:
            print "max rate in Mb: %0.4f \n" % max_rate_mb
            print "drop rate is: %0.4f %% of opackets" % lost_p_percentage
            print "q ful is %0.4f %% of opackets" % q_ful_percentage
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        return q_ful_percentage, lost_p_percentage

    def perfRun(self, rate_mb):
        self.stl_client.clear_stats()
        self.stl_client.start(ports=self.ports, mult=(str(rate_mb) + "mbps"), duration=self.iteration_duration,
                              total=True)
        time.sleep(self.iteration_duration / 2)
        stats = self.stl_client.get_stats()
        self.stl_client.stop(ports=self.ports)
        opackets = stats[self.ports[0]]['opackets']
        ipackets = stats[1]['ipackets']

        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        q_ful_packets = stats['global']['queue_full']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)

        return q_ful_percentage, lost_p_percentage

    def perf_run_interval(self, high_bound, low_bound):
        """

        Args:
            high_bound: in percents of rate
            low_bound: in percents of rate

        Returns: ndr in interval between low and high bounds (opt), q_ful(%) at opt and drop_rate(%) at opt

        """
        max_rate = Rate(self.max_rate)
        # rate_delta = max_rate.convert_percent_to_rate(self.iteration_precision)
        opt = 0  # percent
        q_ful_opt = 0
        drop_rate_opt = 0
        iteration = 0
        while iteration <= self.max_iterations:
            running_rate_percent = float((high_bound + low_bound)) / 2.00
            running_rate = max_rate.convert_percent_to_rate(running_rate_percent)
            q_ful_percentage, lost_p_percentage = self.perfRun(running_rate)
            if self.verbose:
                print "*** iteration %d***\n" % iteration
                print("running at %0.4f %% of line rate which is: %0.4f\n" % (running_rate_percent, running_rate))
                print("drop rate is: %0.4f %% and q ful is %0.4f %%\n" % (lost_p_percentage, q_ful_percentage))
            if q_ful_percentage <= self.q_ful_resolution and lost_p_percentage <= self.pdr:
                if running_rate_percent > opt:
                    if abs(running_rate_percent - opt) <= self.pdr_error:
                        break
                    opt = running_rate_percent
                    q_ful_opt = q_ful_percentage
                    drop_rate_opt = lost_p_percentage
                    low_bound = running_rate_percent
                    iteration += 1
                    continue
                else:
                    break
            high_bound = running_rate_percent
            iteration += 1
        return opt, q_ful_opt, drop_rate_opt

    def find_ndr(self):
        q_ful_percent, drop_percent = self.__find_max_rate()
        max_rate = Rate(self.max_rate)
        if drop_percent > self.pdr:
            assumed_rate_percent = 100 - drop_percent  # percent calculation
            if self.verbose:
                assumed_rate_mb = max_rate.convert_percent_to_rate(assumed_rate_percent)
                print "assumed rate percent: %0.4f " % assumed_rate_percent
                print "assumed rate mb: %0.4f " % assumed_rate_mb
            opt, q_ful_opt, drop_rate_opt = self.perf_run_interval(assumed_rate_percent,
                                                                   assumed_rate_percent - self.pdr_error)
        if q_ful_percent >= self.q_ful_resolution:
            opt, q_ful_opt, drop_rate_opt = self.perf_run_interval(100.00, 0.00)

        print("drop rate is: %0.4f %%" % drop_rate_opt)
        print (
            'queue full percentage is: %0.4f, resolution is: %0.4f \n' % (q_ful_opt, self.q_ful_resolution))
        print("max rate %0.4f \n" % self.max_rate)
        print("rate_percentage %0.4f \n" % opt)
        return drop_rate_opt, opt


# IMIX test
# it maps the ports to sides
# then it load a predefind profile 'IMIX'
# and attach it to both sides and inject
# at a certain rate for some time
# finally it checks that all packets arrived
def imix_test(server, mult):
    # mult = "100%"
    # create client
    c = STLClient(server=server)

    passed = True

    try:

        # connect to server
        c.connect()

        # take all the ports
        c.reset()

        # map ports - identify the routes
        table = stl_map_ports(c)
        pprint(table)
        # retur
        dir_0 = [table['bi'][0][0]]
        dir_1 = [table['bi'][0][1]]

        print("Mapped ports to sides {0} <--> {1}".format(dir_0, dir_1))

        # load IMIX profile
        profile_file = os.path.join(stl_path.STL_PROFILES_PATH, 'imix.py')
        profile = STLProfile.load_py(profile_file)
        streams = profile.get_streams()

        # add both streams to ports
        c.add_streams(streams, ports=dir_0)
        c.add_streams(streams, ports=dir_1)

        b = NdrBench(c, dir_0, iteration_duration=20, iteration_precision=1, q_ful_resolution=2,
                     first_run_duration=20, pdr=0.1, verbose=True)
        pdr, ndr_rate = b.find_ndr()
        print("PDR of %0.4f is at rate: %0.4f" % (pdr, ndr_rate))
    except STLError as e:
        passed = False
        print(e)
        sys.exit(1)

    finally:
        c.disconnect()

    if passed:
        print("\nTest has passed :-)\n")
    else:
        print("\nTest has failed :-(\n")


parser = argparse.ArgumentParser(description="Example for TRex Stateless, sending IMIX traffic")
parser.add_argument('-s', '--server',
                    dest='server',
                    help='Remote trex address',
                    default='127.0.0.1',
                    type=str)
parser.add_argument('-m', '--mult',
                    dest='mult',
                    help='Multiplier of traffic, see Stateless help for more info',
                    default='30%',
                    type=str)
args = parser.parse_args()

# run the tests
imix_test(args.server, args.mult)
