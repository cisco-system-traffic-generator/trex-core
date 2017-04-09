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
rate_interval [low_bound,high_bound] : array of the rate at which the iterations will start from and end at (both in percentage)
iteration_precision: the size, in percents, of the interval between iterations
resolution: how far (in percentage) you allow the actual result to be from the desired result

OUT:
the rate in which the desired result was calculated (percentage)

"""


def q_ful_perf_iterations(stl_client, ports, max_rate, rate_interval, iteration_precision, resolution, duration):
    high_bound = (rate_interval[0])/100.00 * max_rate
    low_bound = (rate_interval[1])/100.00 * max_rate
    rate_delta = max_rate * (float(iteration_precision) / 100.000)
    q_ful_percentage = 100
    iterations = 0
    print "max rate is: %0.4f " % max_rate
    print "starting rate is: %0.4f " % (high_bound - rate_delta)
    rate = high_bound
    while float(q_ful_percentage) > float(resolution):
        iterations += 1
        rate -= rate_delta
        if rate <= low_bound:
            break
        stl_client.clear_stats()
        rate_percentage = (float(rate) / float(max_rate)) * 100.00
        test_duration = (100 / float(rate_percentage)) * duration
        print "***** iteration %d *****" % iterations
        print("starting traffic at %0.4f mbps which is %0.4f %% of max rate for %0.1f seconds" % (
            rate, rate_percentage, test_duration))
        stl_client.start(ports=ports, mult=(str(rate) + "mbps"), duration=test_duration, total=True)
        stl_client.wait_on_traffic(ports=ports)
        stats = stl_client.get_stats()
        q_ful_packets = stats['global']['queue_full']
        opackets = stats[ports[0]]['opackets']
        print('queue full is: %d' % q_ful_packets)
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        print ('queue full percentage is: %0.4f \n\n' % q_ful_percentage)
        stl_client.stop(ports=ports)
        time.sleep(2)

    stl_client.wait_on_traffic(ports=ports)
    return rate_percentage


def find_ndr(stl_client,ports,pdr,no_q_ful_rate,duration):






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

        # clear the stats before injecting
        c.clear_stats()

        duration = 20
        c.start(ports=dir_0, mult="100%", duration=duration, total=True)
        time.sleep(10)
        stats = c.get_stats()
        max_rate = stats['total']['tx_bps_L1']
        q_ful_packets = stats['global']['queue_full']
        opackets = stats[dir_0[0]]['opackets']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        max_rate_mb = float(max_rate) / 1000000
        print "max rate in Mb: %0.4f \n" % max_rate_mb

        c.clear_stats()
        c.stop(ports=dir_0)

        no_q_ful_rate = q_ful_perf_iterations(c, dir_0, max_rate_mb, [100.0, 0.0], 10, 0.5, 20)
        no_q_ful_rate2 = q_ful_perf_iterations(c, dir_0, max_rate_mb, [100.0, no_q_ful_rate], 5, 0.5, 20)
        result = no_q_ful_rate2

        print "\n\n **** done iterating - mult is: %0.4f %% line rate ****" % result
        # use this for debug info on all the stats
        pprint(stats)

        # sum dir 0
        dir_0_opackets = sum([stats[i]["opackets"] for i in dir_0])
        dir_0_ipackets = sum([stats[i]["ipackets"] for i in dir_0])

        # sum dir 1
        dir_1_opackets = sum([stats[i]["opackets"] for i in dir_1])
        dir_1_ipackets = sum([stats[i]["ipackets"] for i in dir_1])

        lost_0 = dir_0_opackets - dir_1_ipackets
        lost_1 = dir_1_opackets - dir_0_ipackets

        print("\nPackets injected from {0}: {1:,}".format(dir_0, dir_0_opackets))
        print("Packets injected from {0}: {1:,}".format(dir_1, dir_1_opackets))

        print("\npackets lost from {0} --> {1}:   {2:,} pkts".format(dir_0, dir_0, lost_0))
        print("packets lost from {0} --> {1}:   {2:,} pkts".format(dir_1, dir_1, lost_1))

        print('queue full is: %d' % stats['global']['queue_full'])

        if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)

        if (lost_0 <= 0) and (
                    lost_1 <= 0) and not c.get_warnings():  # less or equal because we might have incoming arps etc.
            passed = True
        else:
            passed = False


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
