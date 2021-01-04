# from __future__ import division
import stl_path
from trex.stl.api import *
import yaml as yml
from pprint import pprint
import argparse
import sys
import trex.examples.stl.ndr_bench as ndr


# find NDR benchmark test
def ndr_benchmark_test(server='127.0.0.1', pdr=0.1, iteration_duration=20.00, ndr_results=1,
                       title='Title', first_run_duration=20.00, verbose=False,
                       pdr_error=1.00, q_full_resolution=2.00, max_iterations=10, max_latency=0,
                       lat_tolerance=0, output=None, ports_list=[], yaml_file=None,
                       bi_dir=False, force_map_table=False, plugin_file=None, tunables={},
                       opt_binary_search=False, opt_binary_search_percentage=5,
                       profile='stl/imix.py', profile_tunables={}, print_final_results=True):


    if title == 'Title':
        title = profile.split('/')[-1].split(".")[0]
    configs = {'server': server, 'pdr': pdr, 'iteration_duration': iteration_duration,
               'ndr_results': ndr_results, 'first_run_duration': first_run_duration, 'verbose': verbose,
               'pdr_error': pdr_error, 'title': title, 'ports': ports_list,
               'q_full_resolution': q_full_resolution, 'max_iterations': max_iterations,
               'max_latency': max_latency, 'lat_tolerance': lat_tolerance,
               'bi_dir': bi_dir, 'force_map_table': force_map_table,
               'plugin_file': plugin_file, 'tunables': tunables,
               'opt_binary_search': opt_binary_search, 'opt_binary_search_percentage': opt_binary_search_percentage}

    passed = True
    if yaml_file:
        try:
            with open(yaml_file) as f:
                yml_config_dict = yml.safe_load(f)
            configs.update(yml_config_dict)
        except IOError as e:
            print ("Error loading YAML file: %s \nExiting", e.message)
            return -1

    c = STLClient(server=configs['server'])
    # connect to server
    c.connect()
    trex_info = c.get_server_system_info()
    configs['cores'] = trex_info['dp_core_count_per_port']
    configs['total_cores'] = trex_info["dp_core_count"]
    # take all the ports
    c.reset()

    # map ports - identify the routes
    table = stl_map_ports(c)
    # print table
    ports_list = configs['ports']
    if len(ports_list) % 2 != 0:
        print("Illegal ports list")
        return
    if not force_map_table:
        for i in range(0, len(ports_list), 2):
            if (ports_list[i],ports_list[i+1]) not in table['bi']:
                print("Some given port pairs are not configured properly ")
                print("Choose the ports according to this map")
                pprint(table)
                return
    dir_0 = [ports_list[i] for i in range(0, len(ports_list), 2)]
    dir_1 = [ports_list[i] for i in range(1, len(ports_list), 2)]

    # transmitting ports 
    for port_id in dir_0:
        streams = STLProfile.load_py(profile, direction=0, port_id=port_id, **profile_tunables).get_streams()
    c.add_streams(streams, ports=dir_0) 

    # if traffic is bi-directional -> add streams to second direction as well.
    if bi_dir:
        for port_id in dir_1:
            streams = STLProfile.load_py(profile, direction=1, port_id=port_id, **profile_tunables).get_streams()
        c.add_streams(streams, ports=dir_1)

    config = ndr.NdrBenchConfig(**configs)

    b = ndr.NdrBench(stl_client=c, config=config)

    try:
        b.find_ndr()
        if config.verbose:
            b.results.print_final()
    except STLError as e:
        passed = False
        print(e)
        sys.exit(1)

    finally:
        c.disconnect()

    result = {'results': b.results.stats, 'config': b.config.config_to_dict()}
    hu_dict = {'results': b.results.human_readable_dict(), 'config': b.config.config_to_dict()}

    if print_final_results:
        if passed:
            print("\nBench Run has finished :-)\n")
        else:
            print("\nBench Run has failed :-(\n")

        if output == 'json':
            pprint(b.results.to_json())
        elif output == 'hu':
            pprint(b.results.human_readable_dict())

    return result, hu_dict


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
    parser.add_argument('-p', '--pdr',
                        dest='pdr',
                        help='Allowed percentage of drops. (out of total traffic). [0(NDR)-100]',
                        default=0.1,
                        type=float)
    parser.add_argument('-t', '--iter-time',
                        dest='iteration_duration',
                        help='Duration of each run iteration during test. [seconds]',
                        default=20.00,
                        type=float)
    parser.add_argument('-n', '--ndr-results',
                        dest='ndr_results',
                        help='calculates the benchmark at each point scaled linearly under NDR [1-10].'
                             'The results for ndr_results=2 are NDR and NDR/2.',
                        default=1,
                        type=int)
    parser.add_argument('-ti', '--title',
                        dest='title',
                        help='Title for this benchmark test.',
                        default='Title',
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
                        help='When verbose is set, prints test results and iteration to stdout.',
                        default=False,
                        action='store_true')
    parser.add_argument('-x', '--max-iterations',
                        dest='max_iterations',
                        help='The bench stops when reaching result or max_iterations, the early of the two. [int]',
                        default=10,
                        type=int)
    parser.add_argument('-e', '--pdr-error',
                        dest='pdr_error',
                        help='The error around the actual result, in percent.\n'
                             '0%% error is not recommended due to precision issues. [percents 0-100]',
                        default=1.00,
                        type=float)
    parser.add_argument('-q', '--q-full',
                        dest='q_full_resolution',
                        help='Percent of traffic allowed to be queued when transmitting above DUT capability.\n'
                             '0%% q-full resolution is not recommended due to precision issues. [percents 0-100]',
                        default=2.00,
                        type=float)
    parser.add_argument('--max-latency', 
                        dest='max_latency',
                        help='Maximal latency allowed. If the percent of latency packets above this value pass the latency tolerance,\n'
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
                             ' Use json for JSON format or hu for human readable format.',
                        default=None,
                        type=str)
    parser.add_argument('--ports', dest='ports_list', help='Specify an even list of ports for running traffic on.',
                        type=int, nargs='*', default=None, required=True)
    parser.add_argument('--yaml', dest='yaml', help='use YAML file for configurations, use --yaml PATH\TO\YAML.yaml',
                        type=str, default=None)
    parser.add_argument('--force-map',
                        dest='force_map_table',
                        help='Ignore map table configuration and use the specified port list.',
                        default=False,
                        action='store_true')
    parser.add_argument('--bi-dir',
                        dest='bi_dir',
                        help='Specify bi-directional traffic.',
                        default=False,
                        action='store_true')
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
    parser.add_argument('--opt-bin-search',
                        dest='opt_binary_search',
                        help='Optimized binary search, makes use of the drop rate to optimize the first search interval. '
                             'When drop occurs, the tool tries to find a better (smaller) interval to start the search on.'
                             'If this parameter is true, the default value is 5 percent, the tool will search for ndr in an interval of '
                             '[assumed-rate - 5 percent, assumed rate + 5 percent]'
                             ' where assumed-rate is (100-drop_rate)%% of max_rate.',
                        action='store_true',
                        required='--opt-bin-search-percent' in sys.argv,
                        default=False,)
    parser.add_argument('--opt-bin-search-percent',
                        dest='opt_binary_search_percentage',
                        help='Use this to configure the percentage parameter for opt-bin-search.',
                        default=5,
                        type=is_percentage)
    parser.add_argument('--profile',
                        dest='profile',
                        help='Path to the profile we want to load. The profile defines the type of traffic we sent and the drop point differs depending on the traffic type.',
                        default='stl/imix.py',
                        type=str)
    parser.add_argument('--prof-tun',
                        dest='profile_tunables',
                        help='Tunables to forward to the profile if it exists. Use: --prof-tun a=1,b=2,c=3 (no spaces).',
                        default={},
                        type=decode_tunables)
    args = parser.parse_args()

    # run the tests
    ndr_benchmark_test(server=args.server, pdr=args.pdr, iteration_duration=args.iteration_duration, 
                       ndr_results=args.ndr_results, title=args.title, first_run_duration=args.first_run_duration,
                       verbose=args.verbose, max_iterations= args.max_iterations,pdr_error=args.pdr_error,
                       q_full_resolution=args.q_full_resolution, max_latency=args.max_latency, lat_tolerance=args.lat_tolerance,
                       output=args.output, ports_list=args.ports_list, yaml_file=args.yaml, bi_dir=args.bi_dir,
                       force_map_table=args.force_map_table, plugin_file=args.plugin_file, tunables=args.tunables,
                       opt_binary_search=args.opt_binary_search, opt_binary_search_percentage=args.opt_binary_search_percentage,
                       profile=args.profile, profile_tunables=args.profile_tunables)
