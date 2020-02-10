# from __future__ import division
import astf_path
from trex.astf.api import *
import yaml as yml
from pprint import pprint
import argparse
import sys
import trex.examples.astf.ndr_bench as ndr


# find NDR benchmark test
def ndr_benchmark_test(high_mult, low_mult, server='127.0.0.1', iteration_duration=20.00,
                       title='Title', verbose=False, allowed_error=1.00, q_full_resolution=2.00,
                       max_iterations=10, latency_pps=0, max_latency=0,
                       lat_tolerance=0, output=None, yaml_file=None, plugin_file=None,
                       tunables={}, profile='astf/udp_mix.py', profile_tunables={}):


    if title == 'Title':
        title = profile.split('/')[-1].split(".")[0]
    configs = {'high_mult': high_mult, 'low_mult': low_mult, 'server': server,
               'iteration_duration': iteration_duration, 'verbose': verbose,
               'allowed_error': allowed_error, 'title': title, 'q_full_resolution': q_full_resolution,
               'max_iterations': max_iterations, 'latency_pps': latency_pps, 
               'max_latency': max_latency, 'lat_tolerance': lat_tolerance,
               'plugin_file': plugin_file, 'tunables': tunables}

    passed = True
    if yaml_file:
        try:
            with open(yaml_file) as f:
                yml_config_dict = yml.safe_load(f)
            configs.update(yml_config_dict)
        except IOError as e:
            print ("Error loading YAML file: %s \nExiting", e.message)
            return -1

    c = ASTFClient(server=configs['server'])
    # connect to server
    c.connect()
    # take all the ports
    c.reset()

    c.load_profile(profile=profile, tunables=profile_tunables)

    config = ndr.ASTFNdrBenchConfig(**configs)

    b = ndr.ASTFNdrBench(astf_client=c, config=config)

    try:
        b.find_ndr()
        if config.verbose:
            b.results.print_final()
    except ASTFError as e:
        passed = False
        print(e)
        sys.exit(1)

    finally:
        c.disconnect()

    if passed:
        print("\nBench Run has finished :-)\n")
    else:
        print("\nBench Run has failed :-(\n")

    result = {'results': b.results.stats, 'config': b.config.config_to_dict()}
    hu_dict = {'results': b.results.human_readable_dict(), 'config': b.config.config_to_dict()}

    if output == 'json':
        pprint(b.results.to_json())
    elif output == 'hu':
        pprint(b.results.human_readable_dict())

    return result, hu_dict


def verify_uint_32(value):
    ivalue = int(value)
    if ivalue < 0 or ivalue > 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("%s is an invalid uint32_t" % value)
    return ivalue


def decode_tunables(tunable_str):

    tunables = {}

    # split by comma to tokens
    tokens = tunable_str.split(',')

    # each token is of form X=Y
    for token in tokens:
        m = re.search('(\S+)=(.+)', token)
        if not m:
            raise argparse.ArgumentTypeError("bad syntax for tunables: {0}".format(token))
        val = m.group(2)           # string
        if val.startswith(("'", '"')) and val.endswith(("'", '"')) and len(val) > 1: # need to remove the quotes from value
            val = val[1:-1]
        elif val.startswith('0x'): # hex
            val = int(val, 16)
        else:
            try:
                if '.' in val:     # float
                    val = float(val)
                else:              # int
                    val = int(val)
            except:
                pass
        tunables[m.group(1)] = val

    return tunables


def is_percentage(str):
    try:
        num = int(str)
    except ValueError:
        raise argparse.ArgumentTypeError('Integer value between 0 and 100 expected.')
    if not (0 < num <=100):
        raise argparse.ArgumentTypeError('Integer value between 0 and 100 (exclusive) expected.')
    return num


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="TRex NDR benchmark tool")
    parser.add_argument('-s', '--server',
                        dest='server',
                        help='TRex server address. Default is local.',
                        default='127.0.0.1',
                        type=str)
    parser.add_argument('-t', '--iter-time',
                        dest='iteration_duration',
                        help='Duration of each run iteration during test. [seconds]',
                        default=20.00,
                        type=float)
    parser.add_argument('-ti', '--title',
                        dest='title',
                        help='Title for this benchmark test',
                        default='Title',
                        type=str)
    parser.add_argument('-v', '--verbose',
                        dest='verbose',
                        help='When verbose is set, prints test results and iteration to stdout.',
                        default=False,
                        action='store_true')
    parser.add_argument('-x', '--max-iterations',
                        dest='max_iterations',
                        help='The bench stops when reaching result or max_iterations, the early of the two. [int]',
                        default=10,
                        type=int)
    parser.add_argument('-e', '--allowed-error',
                        dest='allowed_error',
                        help='The error around the actual result, in percent.\n'
                             '0%% error is not recommended due to precision issues. [percents 0-100]',
                        default=1.00,
                        type=float)
    parser.add_argument('-q', '--q-full',
                        dest='q_full_resolution',
                        help='Percent of traffic allowed to be queued when transmitting above dut capability.\n'
                             '0%% q-full resolution is not recommended due to precision issues. [percents 0-100]',
                        default=2.00,
                        type=float)
    parser.add_argument('-lpps', '--latency-pps',
                        dest='latency_pps',
                        help='Rate of latency in packets per second. Default is 0, meaning no latency packets.',
                        default=0,
                        type=verify_uint_32,
                        required='--max-latency' in sys.argv),
    parser.add_argument('--max-latency', 
                        dest='max_latency',
                        help='Maximal latency allowed. If the percent of latency packets above this value passes the latency tolerance,\n'
                             ' then the rate is considered too high. If the value is 0, then we consider this as unset. Default=0. [usec]',
                        default=0,
                        required='--lat-tolerance' in sys.argv,
                        type=int)
    parser.add_argument('--lat-tolerance',
                        dest='lat_tolerance',
                        help='Percentage of latency packets allowed beyond max-latency. Default is 0%%. In this case we compare max-latency\n'
                                'to the maximal latency in a run. [percents 0-100]',
                        default=0,
                        type=is_percentage)
    parser.add_argument('-o', '--output', dest='output',
                        help='If you specify this flag, after the test is finished, the final results will be printed in the requested format.'
                             'Use json for JSON format or hu for human readable format.',
                        default=None,
                        type=str)
    parser.add_argument('--yaml', dest='yaml', help='use YAML file for configurations, use --yaml PATH\TO\YAML.yaml',
                        type=str, default=None)
    parser.add_argument('-f', '--plugin-file',
                        dest='plugin_file',
                        help='Provide the plugin file for the plugin that implements the pre and post iteration functions.',
                        required='--tunables' in sys.argv,
                        type=str,
                        default=None)
    parser.add_argument('--tunables',
                        dest='tunables',
                        help='Tunables to forward to the plugin if it exists. Use: --tunables a=1,b=2,c=3 (no spaces).',
                        default={},
                        type=decode_tunables)
    parser.add_argument('--profile',
                        dest='profile',
                        help='Path to the profile we want to load. The profile defines the type of traffic we send and the NDR differs depending on the traffic type.',
                        default='astf/udp_mix.py',
                        type=str)
    parser.add_argument('--prof-tun',
                        dest='profile_tunables',
                        help='Tunables to forward to the profile if it exists. Use: --prof-tun a=1,b=2,c=3 (no spaces).',
                        default={},
                        type=decode_tunables)
    parser.add_argument('--high-mult',
                        dest='high_mult',
                        help='Higher bound of the interval in which the NDR point is found.',
                        type=int,
                        required=True)
    parser.add_argument('--low-mult',
                        dest='low_mult',
                        help='Lower bound of the interval in which the NDR point is found.',
                        type=int,
                        required=True)
    args = parser.parse_args()

    # run the tests
    ndr_benchmark_test(server=args.server, iteration_duration=args.iteration_duration, 
                       title=args.title, verbose=args.verbose, max_iterations= args.max_iterations,
                       allowed_error=args.allowed_error, q_full_resolution=args.q_full_resolution,
                       latency_pps=args.latency_pps, max_latency=args.max_latency, lat_tolerance=args.lat_tolerance,
                       output=args.output, yaml_file=args.yaml, plugin_file=args.plugin_file,
                       tunables=args.tunables, profile=args.profile, profile_tunables=args.profile_tunables,
                       high_mult=args.high_mult, low_mult=args.low_mult)
