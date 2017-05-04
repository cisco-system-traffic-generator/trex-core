# from __future__ import division
import stl_path
from trex_stl_lib.api import *

from pprint import pprint
import argparse
import sys
import ndr_bench as ndr


# find NDR benchmark test
# it maps the ports to sides
# then it load a predefind profile 'IMIX'
# and attach it to both sides and inject
# then searches for NDR according to specified values

def ndr_benchmark_test(server, core_mask, pdr, iteration_duration, ndr_results, setup_name, first_run_duration, verbose,
                       pdr_error, q_ful_resolution, latency):
    passed = True
    b = ndr.NdrBench(server=server, iteration_duration=iteration_duration, q_ful_resolution=q_ful_resolution,
                     first_run_duration=first_run_duration, pdr=pdr, pdr_error=pdr_error, ndr_results=ndr_results,
                     latency=latency,
                     verbose=verbose)

    try:
        run_results = b.find_ndr()
        opt_rate_p = run_results['rate_p']
        q_ful_opt = run_results['q_ful_percentage']
        drop_rate_opt = run_results['lost_p_percentage']
        tx_util = run_results['tx_util']
        if b.verbose:
            print('\n*******SUMMARY*******')
            print("drop rate is: %0.4f %% " % drop_rate_opt)
            print (
                'queue full percentage is: %0.4f, q_ful_resolution is: %0.4f' % (q_ful_opt, b.q_ful_resolution))
            print("max rate %0.4f mbps" % b.max_rate)
            print("tx util is: %0.2f%%" % tx_util)
            print("rate_percentage %0.4f%% of max rate \n" % opt_rate_p)
            print("PDR of %0.4f is at rate: %0.4f%% of max rate\n" % (drop_rate_opt, opt_rate_p))
        pprint(run_results)
    except STLError as e:
        passed = False
        print(e)
        sys.exit(1)

    finally:
        b.disconnect()

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
parser.add_argument('-c', '--core-mask',
                    dest='core_mask',
                    help='Determines the allocation of cores per port, see Stateless help for more info',
                    default=1,
                    type=int)
parser.add_argument('-p', '--pdr',
                    dest='pdr',
                    help='Allowed percentage of drops. (out of total traffic) [0(NDR)-100]',
                    default=0.1,
                    type=float)
parser.add_argument('-t', '--iter-time',
                    dest='iteration_duration',
                    help='Duration of each run iteration during test. [seconds]',
                    default=20.00,
                    type=float)
parser.add_argument('-n', '--ndr-results',
                    dest='ndr_results',
                    help='calculates the benchmark at each point scaled linearly under NDR [1-10]. '
                         'The results for ndr_results=2 are NDR and NDR/2.'
                         'NumPy lib should be installed to support this',
                    default=1,
                    type=int)
parser.add_argument('-na', '--setup-name',
                    dest='setup_name',
                    help='Name of the setup the benchmark tests',
                    default='trex',
                    type=str)
parser.add_argument('-ft', '--first_run_duration_time',
                    dest='first_run_duration',
                    help='The first run tests for the capability of the device.\n'
                         '100%% operation will be tested after half of the duration.\n'
                         'Shorter times may lead to inaccurate measurement [seconds]',
                    default=20.00,
                    type=float)
parser.add_argument('-v', '--verbose',
                    dest='verbose',
                    help='When verbose is set, prints test results and iteration to stdout',
                    default=False,
                    action='store_true')
parser.add_argument('-x', '--max-iterations',
                    dest='max_iterations',
                    help='The bench stops when reaching result or max_iterations, the early of the two [int]',
                    default=10,
                    type=int)
parser.add_argument('-e', '--pdr-error',
                    dest='pdr_error',
                    help='The error around the actual result, in percent.\n'
                         '0%% error is not recommended due to precision issues. [percents 0-100]',
                    default=1.00,
                    type=float)
parser.add_argument('-q', '--q-full',
                    dest='q_ful_resolution',
                    help='percent of traffic allowed to be queued when transmitting above dut capability.\n'
                         '0%% q-full resolution is not recommended due to precision issues. [percents 0-100]',
                    default=2.00,
                    type=float)
parser.add_argument('-l', '--latency',
                    dest='latency',
                    help='Specify this option to disable latency calculations.',
                    default=True,
                    action='store_false')
args = parser.parse_args()

# run the tests
ndr_benchmark_test(args.server, args.core_mask, args.pdr, args.iteration_duration, args.ndr_results, args.setup_name,
                   args.first_run_duration, args.verbose,
                   args.pdr_error, args.q_ful_resolution, args.latency)
