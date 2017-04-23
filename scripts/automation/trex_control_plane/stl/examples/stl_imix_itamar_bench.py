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
                 first_run_duration, pdr, verbose=True):
        self.verbose = verbose
        self.stl_client = stl_client
        self.ports = ports
        self.max_rate = 0  # in mb
        self.iteration_duration = iteration_duration
        self.q_ful_iteration_precision = iteration_precision
        self.q_ful_resolution = q_ful_resolution
        self.first_run_duration = first_run_duration
        self.no_q_ful_point_percent = 0  # percent of max rate
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate

    def __find_max_rate(self):
        c = self.stl_client
        ports = self.ports
        c.clear_stats()
        duration = self.first_run_duration
        c.start(ports=ports, mult="100%", duration=duration, total=True)
        time.sleep(float(duration) / 2.00)
        stats = c.get_stats()
        max_rate = stats['total']['tx_bps_L1']
        q_ful_packets = stats['global']['queue_full']
        opackets = stats[ports[0]]['opackets']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        if q_ful_percentage <= self.q_ful_resolution:
            self.no_q_ful_point_percent = 100.00
            if self.verbose:
                print 'queue full percentage is: %0.4f' % q_ful_percentage
        max_rate_mb = float(max_rate) / 1000000.00
        self.max_rate = max_rate_mb
        if self.verbose:
            print "max rate in Mb: %0.4f \n" % max_rate_mb
        c.clear_stats()
        c.stop(ports=ports)

    # should find max_rate first
    def q_ful_perf_iterations(self, rate_interval, iteration_precision):
        max_rate = Rate(self.max_rate)
        high_bound = max_rate.convert_percent_to_rate(rate_interval[0])
        low_bound = max_rate.convert_percent_to_rate(rate_interval[1])
        rate_delta = max_rate.convert_percent_to_rate(iteration_precision)
        q_ful_percentage = 100
        iterations = 0
        print "max rate is: %0.4f " % float(max_rate.rate)
        print "starting rate is: %0.4f " % (high_bound - rate_delta)
        rate = high_bound
        rate_percentage_opt = rate_interval[1]  # low_bound
        while float(q_ful_percentage) > float(self.q_ful_resolution):
            iterations += 1
            rate -= rate_delta
            print("rate %0.4f low bound %0.4f" % (rate, low_bound))
            if float(rate) <= float(low_bound):
                break
            self.stl_client.clear_stats()
            rate_percentage = max_rate.convert_rate_to_percent_of_max_rate(rate)
            test_duration = (100 / float(rate_percentage)) * self.iteration_duration
            print "***** iteration %d *****" % iterations
            print("starting traffic at %0.4f mbps which is %0.4f %% of max rate for %0.1f seconds" % (
                rate, rate_percentage, test_duration))
            self.stl_client.start(ports=self.ports, mult=(str(rate) + "mbps"), duration=test_duration, total=True)
            self.stl_client.wait_on_traffic(ports=self.ports)
            stats = self.stl_client.get_stats()
            q_ful_packets = stats['global']['queue_full']
            opackets = stats[self.ports[0]]['opackets']
            print('queue full is: %d' % q_ful_packets)
            q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
            print (
                'queue full percentage is: %0.4f, resolution is: %0.4f \n' % (q_ful_percentage, self.q_ful_resolution))
            print("rate_percentage %0.4f  rate_percentage_opt %0.4f\n\n" % (rate_percentage, rate_percentage_opt))
            if float(q_ful_percentage) <= float(self.q_ful_resolution):
                if rate_percentage > rate_percentage_opt:
                    rate_percentage_opt = rate_percentage
            time.sleep(2)
        return rate_percentage_opt

    def find_ndr(self):
        # duration should be at least 10 seconds
        lost_p_percentage = 100
        print "no q ful point percent %0.4f : " % self.no_q_ful_point_percent
        no_q_ful_rate_temp = Rate(self.max_rate).convert_percent_to_rate(self.no_q_ful_point_percent)
        no_q_ful_rate = Rate(no_q_ful_rate_temp)
        running_rate = no_q_ful_rate.rate
        while self.pdr < lost_p_percentage:
            rate_percentage = no_q_ful_rate.convert_rate_to_percent_of_max_rate(running_rate)
            print("starting traffic at %0.4f mbps which is %0.4f %% of max rate for %0.1f seconds" % (
                running_rate, rate_percentage, self.iteration_duration))
            self.stl_client.start(ports=self.ports, mult=(str(running_rate) + "mbps"), duration=self.iteration_duration,
                                  total=True)
            self.stl_client.clear_stats()
            time.sleep(5)
            stats = self.stl_client.get_stats()
            self.stl_client.stop(ports=self.ports)
            # print "******* NDR printing"
            # print ports
            # pprint(stats)
            opackets = stats[self.ports[0]]['opackets']
            ipackets = stats[1]['ipackets']

            lost_p = opackets - ipackets

            lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
            print(
                "opackets is: %0.4f, ipackets is: %0.4f, lost_percenage is: %0.4f\n" % (
                    opackets, ipackets, lost_p_percentage))
            if round(self.pdr, 4) >= round(float(lost_p_percentage), 4):
                print ("pdr found \n pdr: %0.4f, drop percentage: %0.4f \n" % (self.pdr, lost_p_percentage))
                break
            else:
                print("pdr not found yet \npdr: %0.4f, drop percentage: %0.4f \n" % (self.pdr, lost_p_percentage))
            rate_delta = no_q_ful_rate.convert_percent_to_rate(lost_p_percentage)
            running_rate -= rate_delta
        return  running_rate

    def find_no_q_full_rate(self):
        self.__find_max_rate()
        print "this is max rate i found %0.4f " % self.max_rate
        if not self.no_q_ful_point_percent:
            if self.q_ful_iteration_precision <= 10:
                self.no_q_ful_point_percent = self.q_ful_perf_iterations([100.00, 0.0], 10)
            if self.q_ful_iteration_precision <= 5:
                q_ful_low_bound = self.no_q_ful_point_percent
                q_ful_high_bound = q_ful_low_bound + 10
                self.no_q_ful_point_percent = self.q_ful_perf_iterations([q_ful_high_bound, q_ful_low_bound], 5)
                q_ful_low_bound = self.no_q_ful_point_percent
                q_ful_high_bound = self.no_q_ful_point_percent + 5
                self.no_q_ful_point_percent = self.q_ful_perf_iterations([q_ful_high_bound, q_ful_low_bound],
                                                                         self.q_ful_iteration_precision)












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

        b = NdrBench(c,dir_0,iteration_duration=20,iteration_precision=1,q_ful_resolution=0.5,first_run_duration=20,pdr=0.1,verbose=True)
        b.find_no_q_full_rate()
        ndr_rate = b.find_ndr()

        print("PDR of %0.4f is at rate: %0.4f" % (b.pdr,ndr_rate))
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
