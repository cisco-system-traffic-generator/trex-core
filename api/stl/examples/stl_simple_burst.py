import sys
sys.path.insert(0, "../")

import trex_stl_api

from trex_stl_api import STLClient, STLError

import time

# define a simple burst test
def simple_burst ():

    passed = True

    try:
        with STLClient() as c:

            # activate this for some logging information
            #c.logger.set_verbose(c.logger.VERBOSE_REGULAR)

            # repeat for 5 times
            for i in xrange(1, 6):

                # read the stats before
                before_ipackets = c.get_stats()['total']['ipackets']

                # inject burst profile on two ports and block until done
                c.start(profiles = '../profiles/burst.yaml', ports = [0, 1], mult = "1gbps")
                c.wait_on_traffic(ports = [0, 1])

                after_ipackets  = c.get_stats()['total']['ipackets']

                print "Test iteration {0} - Packets Received: {1} ".format(i, (after_ipackets - before_ipackets))

                # we have 600 packets in the burst and two ports
                if (after_ipackets - before_ipackets) != (600 * 2):
                    passed = False

    # error handling
    except STLError as e:
        passed = False
        print e


  
    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"


simple_burst()

