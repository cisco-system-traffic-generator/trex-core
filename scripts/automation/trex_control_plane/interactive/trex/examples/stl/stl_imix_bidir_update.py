import stl_path
from trex.stl.api import *

import time
from pprint import pprint
import argparse

import os

# IMIX test
# it maps the ports to sides
# then it load a predefind profile 'IMIX'
# and attach it to both sides and inject
# at a certain rate for some time
# finally it checks that all packets arrived
def imix_test (server):


    # create client
    c = STLClient(server = server)
    passed = True


    try:

        # connect to server
        c.connect()

        # take all the ports
        c.reset()

        dir_0 = [0]
        dir_1 = [1]

        print "Mapped ports to sides {0} <--> {1}".format(dir_0, dir_1)

        # load IMIX profile
        profile_file = os.path.join(stl_path.STL_PROFILES_PATH, 'imix.py')
        profile1 = STLProfile.load_py(profile_file, direction=0)
        profile2 = STLProfile.load_py(profile_file, direction=1)
        stream1 = profile1.get_streams()
        stream2 = profile2.get_streams()

        # add both streams to ports
        c.add_streams(stream1, ports = dir_0)
        c.add_streams(stream2, ports = dir_1)


        # clear the stats before injecting
        c.clear_stats()

        c.start(ports = (dir_0 + dir_1), mult = "100kpps", total = True)

        for rate in range(200,3100,10):

            # choose rate and start traffic for 10 seconds on 5 mpps
            #mult = "30%"
            my_mult = ("%dkpps"%rate)
            print "Injecting {0} <--> {1} on total rate of '{2}' ".format(dir_0, dir_1, my_mult)
            c.clear_stats()


            c.update(ports = (dir_0 + dir_1), mult = my_mult)

            #time.sleep(1);

            # block until done
            #c.wait_on_traffic(ports = (dir_0 + dir_1))

            # read the stats after the test
            stats = c.get_stats()

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

            print "\nPackets injected from {0}: {1:,}".format(dir_0, dir_0_opackets)
            print "Packets injected from {0}: {1:,}".format(dir_1, dir_1_opackets)

            print "\npackets lost from {0} --> {1}:   {2:,} pkts".format(dir_0, dir_1, lost_0)
            print "packets lost from {0} --> {1}:   {2:,} pkts".format(dir_1, dir_0, lost_1)

            if (lost_0 <= 0) and (lost_1 <= 0): # less or equal because we might have incoming arps etc.
                passed = True
            else:
                passed = False
                break


    except STLError as e:
        passed = False
        print e

    finally:
        c.disconnect()

    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"

parser = argparse.ArgumentParser(description="Example for TRex Stateless, sending IMIX traffic")
parser.add_argument('-s', '--server',
                    dest='server',
                    help='Remote trex address',
                    default='127.0.0.1',
                    type = str)
args = parser.parse_args()

# run the tests
imix_test(args.server)

