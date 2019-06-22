#!/bin/python

import astf_path
from trex.astf.api import *

from pprint import pprint
import argparse
import os
import sys

def astf_test(server, mult, duration, profile_path):

    # create client
    c = ASTFClient(server = server)

    # connect to server
    c.connect()

    passed = True

    try:
        # take all the ports
        c.reset()

        # load ASTF profile
        if not profile_path:
            profile_path = os.path.join(astf_path.get_profiles_path(), 'http_simple.py')

        c.load_profile(profile_path)

        # clear the stats before injecting
        c.clear_stats()

        print("Injecting with multiplier of '%s' for %s seconds" % (mult, duration))
        c.start(mult = mult, duration = duration)

        # block until done
        c.wait_on_traffic()

        # read the stats after the test
        stats = c.get_stats()

        # use this for debug info on all the stats
        pprint(stats)

        if c.get_warnings():
            print('\n\n*** test had warnings ****\n\n')
            for w in c.get_warnings():
                print(w)


        client_stats = stats['traffic']['client']
        server_stats = stats['traffic']['server']

        tcp_client_sent, tcp_server_recv = client_stats.get('tcps_sndbyte', 0), server_stats.get('tcps_rcvbyte', 0)
        tcp_server_sent, tcp_client_recv = server_stats.get('tcps_sndbyte', 0), client_stats.get('tcps_rcvbyte', 0)

        udp_client_sent, udp_server_recv = client_stats.get('udps_sndbyte', 0), server_stats.get('udps_rcvbyte', 0)
        udp_server_sent, udp_client_recv = server_stats.get('udps_sndbyte', 0), client_stats.get('udps_rcvbyte', 0)

        assert (tcp_client_sent == tcp_server_recv), 'Too much TCP drops - clients sent: %s, servers received: %s' % (tcp_client_sent, tcp_server_recv)
        assert (tcp_server_sent == tcp_client_recv), 'Too much TCP drops - servers sent: %s, clients received: %s' % (tcp_server_sent, tcp_client_recv)

        assert (udp_client_sent == udp_server_recv), 'Too much UDP drops - clients sent: %s, servers received: %s' % (udp_client_sent, udp_server_recv)
        assert (udp_server_sent == udp_client_recv), 'Too much UDP drops - servers sent: %s, clients received: %s' % (udp_server_sent, udp_client_recv)


    except TRexError as e:
        passed = False
        print(e)

    except AssertionError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if passed:
        print('\nTest has passed :-)\n')
    else:
        print('\nTest has failed :-(\n')
        sys.exit(1)


def parse_args():
    parser = argparse.ArgumentParser(description = 'Example for TRex ASTF, sending http_simple.py')
    parser.add_argument('-s',
                        dest = 'server',
                        help='remote TRex address',
                        default='127.0.0.1',
                        type = str)
    parser.add_argument('-m',
                        dest = 'mult',
                        help='multiplier of traffic, see ASTF help for more info',
                        default = 100,
                        type = int)
    parser.add_argument('-f',
                        dest = 'file',
                        help='profile path to send, default will astf/http_simple.py',
                        type = str)
    parser.add_argument('-d',
                        default = 10,
                        dest = 'duration',
                        help='duration of traffic, default is 10 sec',
                        type = float)

    return parser.parse_args()


args = parse_args()
astf_test(args.server, args.mult, args.duration, args.file)

