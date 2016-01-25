import sys
sys.path.insert(0, "../")

import trex_stl_api

from trex_stl_api import STLClient, STLError

import time

# define a simple burst test
def simple_burst ():

    passed = True

    c = STLClient()

    try:
        # activate this for some logging information
        #c.logger.set_verbose(c.logger.VERBOSE_REGULAR)

        # connect to server
        c.connect()

        # prepare port 0,1
        c.reset(ports = [0, 1])

        # load profile to both ports
        c.load_profile('../profiles/burst.yaml', ports = [0, 1])

        # repeat for 5 times
        for i in xrange(1, 6):

            c.clear_stats()

            # start traffic and block until done
            c.start(ports = [0, 1], mult = "1gbps", duration = 5)
            c.wait_on_traffic(ports = [0, 1])

            # read the stats
            stats = c.get_stats()
            ipackets  = stats['total']['ipackets']

            print "Test iteration {0} - Packets Received: {1} ".format(i, ipackets)

            # we have 600 packets in the burst and two ports
            if (ipackets != (600 * 2)):
                passed = False


        # error handling
    except STLError as e:
        passed = False
        print e

    # cleanup
    finally:
        c.disconnect()
  
    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"


simple_burst()

