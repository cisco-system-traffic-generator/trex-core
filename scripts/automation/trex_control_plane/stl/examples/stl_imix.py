import stl_path
from trex_stl_lib.api import *

import time
import json
from pprint import pprint

# IMIX test
# it maps the ports to sides
# then it load a predefind profile 'IMIX'
# and attach it to both sides and inject
# at a certain rate for some time
# finally it checks that all packets arrived
def imix_test ():
    

    # create client
    c = STLClient()
    passed = True


    try:

        # connect to server
        c.connect()

        # take all the ports
        c.reset()

        # map ports - identify the routes
        table = stl_map_ports(c)

        print "Mapped ports to sides {0} <--> {1}".format(table['dir'][0], table['dir'][1])
        dir_0 = table['dir'][0]
        dir_1 = table['dir'][1]

        # load IMIX profile
        streams = c.load_profile('../../../../stl/profiles/imix.py')

        # add both streams to ports
        c.add_streams(streams, ports = dir_0)
        c.add_streams(streams, ports = dir_1)
        
        # clear the stats before injecting
        c.clear_stats()

        # choose rate and start traffic for 10 seconds on 5 mpps
        duration = 10
        mult = "5mpps"
        print "Injecting {0} <--> {1} on total rate of '{2}' for {3} seconds".format(dir_0, dir_1, mult, duration)

        c.start(ports = (dir_0 + dir_1), mult = mult, duration = duration, total = True)
        
        # block until done
        c.wait_on_traffic(ports = (dir_0 + dir_1))

        # read the stats after the test
        stats = c.get_stats()

        # use this for debug info on all the stats
        #pprint(stats)

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

        print "\npackets lost from {0} --> {1}:   {2:,} pkts".format(dir_0, dir_0, lost_0)
        print "packets lost from {0} --> {1}:   {2:,} pkts".format(dir_0, dir_0, lost_0)

        if (lost_0 == 0) and (lost_0 == 0):
            passed = True
        else:
            passed = False


    except STLError as e:
        passed = False
        print e

    finally:
        c.disconnect()

    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"


# run the tests
imix_test()

