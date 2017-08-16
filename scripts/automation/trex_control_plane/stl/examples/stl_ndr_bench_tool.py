# from __future__ import division
import stl_path
from trex_stl_lib.api import *
import yaml as yml
from pprint import pprint
import argparse
import sys
import ndr_bench as ndr


def build_streams_for_bench(size, vm, src_start_ip, src_stop_ip, dest_start_ip, dest_stop_ip, direction):
    if vm:
        stl_bench = ndr.STLBench()
        if src_start_ip:
            stl_bench.ip_range['src']['start'] = src_start_ip
        if src_stop_ip:
            stl_bench.ip_range['src']['end'] = src_stop_ip
        if dest_start_ip:
            stl_bench.ip_range['dst']['start'] = dest_start_ip
        if dest_stop_ip:
            stl_bench.ip_range['dst']['end'] = dest_stop_ip

    streams = stl_bench.get_streams(size, vm, direction)
    return streams


# find NDR benchmark test
# it maps the ports to sides
# then it load a predefind profile 'IMIX'
# and attach it to both sides and inject
# then searches for NDR according to specified values
def ndr_benchmark_test(server='127.0.0.1', core_mask=None, pdr=0.1, iteration_duration=20.00, ndr_results=1,
                       title='Default Title', first_run_duration=20.00, verbose=False,
                       pdr_error=1.00, q_ful_resolution=2.00, latency=True, vm='cached', pkt_size=64,
                       fe_src_start_ip=None,
                       fe_src_stop_ip=None, fe_dst_start_ip=None, fe_dst_stop_ip=None, drop_rate_interval=10,
                       output=None, ports_list=[],
                       latency_rate=1000, max_iterations=10, yaml=None):
    configs = {'server': server, 'core_mask': core_mask, 'pdr': pdr, 'iteration_duration': iteration_duration,
               'ndr_results': ndr_results, 'first_run_duration': first_run_duration, 'verbose': verbose,
               'pdr_error': pdr_error, 'title': title,
               'q_ful_resolution': q_ful_resolution, 'latency': latency,
               'vm': vm, 'pkt_size': pkt_size,
               'ports': ports_list, 'latency_rate': latency_rate, 'max_iterations': max_iterations,
               'drop rate interval': drop_rate_interval, 'fe_src_start_ip': fe_src_start_ip,
               'fe_src_stop_ip': fe_src_stop_ip, 'fe_dst_start_ip': fe_dst_start_ip,
               'fe_dst_stop_ip': fe_dst_stop_ip}
    passed = True
    if yaml:
        try:
            f = open(yaml)
            yml_config_dict = yml.safe_load(f)
            configs.update(yml_config_dict)
            f.close()
        except IOError as e:
            print ("Error loading YAML file: %s \nExiting", e.message)
            return -1

    c = STLClient(server=configs['server'])
    # connect to server
    c.connect()
    trex_info = c.get_server_system_info()
    configs['cores'] = trex_info['dp_core_count_per_port']
    # pprint(c.get_util_stats())
    # take all the ports
    c.reset()

    # map ports - identify the routes
    table = stl_map_ports(c)
    ports_list = configs['ports']
    if ports_list:
        if len(ports_list) % 2 != 0:
            print("illegal ports list")
            return
        for i in range(0,len(ports_list), 2):
            if (ports_list[i],ports_list[i+1]) not in table['bi']:
                print("some given ports pairs are not configured properly ")
                return
    if ports_list:
        dir_0 = [ports_list[i] for i in range(0, len(ports_list), 2)]
        ports = ports_list
    else:
        dir_0 = [table['bi'][i][0] for i in range(0, len(table['bi']))]
        ports = []
        for port in table['bi']:
            ports.append(port[0])
            ports.append(port[1])
    configs['ports'] = ports
    streams = build_streams_for_bench(size=pkt_size, vm=vm, src_start_ip=fe_src_start_ip, src_stop_ip=fe_src_stop_ip,
                                      dest_start_ip=fe_dst_start_ip, dest_stop_ip=fe_dst_stop_ip, direction=0)
    if latency:
        burst_size = 1000
        pps = latency_rate
        pkt = STLPktBuilder(pkt=Ether() / IP(src="16.0.0.1", dst="48.0.0.1") / UDP(dport=12,
                                                                                   sport=1025) / 'at_least_16_bytes_payload_needed')
        total_pkts = burst_size
        for i in dir_0:
            all_streams = list(streams)
            latency_stream = STLStream(name='rx' + str(i),
                                       packet=pkt,
                                       flow_stats=STLFlowLatencyStats(pg_id=i),
                                       mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                             pps=pps))
            # print type(latency_stream)
            # print type(all_streams)
            all_streams.append(latency_stream)
            # print all_streams
            c.add_streams(all_streams, ports=[i])
    # add both streams to ports
    else:
        c.add_streams(streams, ports=dir_0)

    config = ndr.NdrBenchConfig(**configs)
    b = ndr.NdrBench(stl_client=c, config=config)

    try:
        b.find_ndr()
        if b.config.verbose:
            b.results.print_final(latency)
            # pprint(run_results)
    except STLError as e:
        passed = False
        print(e)
        sys.exit(1)

    finally:
        c.disconnect()

    if passed:
        print("\nBench Run has finished :-)\n")
    else:
        print("\nBench Run has failed :-(\n")

    if output == 'json':
        result = b.results.to_json()
        if verbose:
            pprint(result)
        return result

    result = {'results': b.results.stats, 'config': b.config.config_to_dict()}
    hu_dict = {'results': b.results.human_readable_dict(), 'config': b.config.config_to_dict()}
    return result, hu_dict


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="TRex NDR benchmark tool")
    parser.add_argument('-s', '--server',
                        dest='server',
                        help='Remote trex address',
                        default='127.0.0.1',
                        type=str)
    parser.add_argument('-c', '--core-mask',
                        dest='core_mask',
                        help='Determines the allocation of cores per port, see Stateless help for more info',
                        default=None,
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
                             'The results for ndr_results=2 are NDR and NDR/2.',
                        default=1,
                        type=int)
    parser.add_argument('-ti', '--title',
                        dest='title',
                        help='Title for this benchmark test',
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
    parser.add_argument('-ld', '--latency-disable',
                        dest='latency',
                        help='Specify this option to disable latency calculations.',
                        default=True,
                        action='store_false')
    parser.add_argument('-lr', '--latency-rate',
                        dest='latency_rate',
                        help='Specify the desired latency rate.',
                        default=100,
                        type=float)
    parser.add_argument('-fe',
                        dest='vm',
                        help='choose Field Engine Module: var1,var2,random,tuple,size,cached. default is none',
                        default='cached',
                        type=str)
    parser.add_argument('-size',
                        dest='size',
                        type=str,
                        help='choose packet size/imix. default is 64 bytes',
                        default=64)
    parser.add_argument('--fe-src-start-ip', dest='fe_src_start_ip',
                        help='when using FE you can define the start and stop ip addresses.'
                             'this is valid only when -fe flag is defined',
                        default=None)
    parser.add_argument('--fe-src-stop-ip', dest='fe_src_stop_ip',
                        help='when using FE you can define the start and stop ip addresses.'
                             'this is valid only when -fe flag is defined',
                        default=None)
    parser.add_argument('--fe-dst-start-ip', dest='fe_dst_start_ip',
                        help='when using FE you can define the start and stop ip addresses.'
                             'this is valid only when -fe flag is defined',
                        default=None)
    parser.add_argument('--fe-dst-stop-ip', dest='fe_dst_stop_ip',
                        help='when using FE you can define the start and stop ip addresses.'
                             'this is valid only when -fe flag is defined',
                        default=None)
    parser.add_argument('-d', '--drop-rate-interval', dest='drop_rate_interval',
                        help='The tool will search for NDR, when drop occur, the tool searches for ndr between an assumed'
                             'no drop rate within an interval defined by this parameter.[Percents]'
                             'Default value is 10 percent, the tool will search for ndr in an interval of '
                             '[assumed-rate - 10 percent, assumed rate + 10 percent]',
                        default=10)
    parser.add_argument('-o', '--output', dest='output',
                        help='Desired output format. specify json for JSON output.'
                             'Specify yaml for YAML output.'
                             'if this flag is unspecified, output will appear to console if the option -v is present',
                        default=None,
                        type=str)
    parser.add_argument('--ports', dest='ports_list', help='specify an even list of ports for running traffic on',
                        type=int, nargs='*', default=None)
    parser.add_argument('--yaml', dest='yaml', help='use YAML file for configurations, use --yaml PATH\TO\YAML.yaml',
                        type=str, default=None)
    args = parser.parse_args()
    # run the tests
    ndr_benchmark_test(args.server, args.core_mask, args.pdr, args.iteration_duration, args.ndr_results, args.title,
                       args.first_run_duration, args.verbose,
                       args.pdr_error, args.q_ful_resolution, args.latency, args.vm, args.size, args.fe_src_start_ip,
                       args.fe_src_stop_ip, args.fe_dst_start_ip, args.fe_dst_stop_ip, args.drop_rate_interval,
                       args.output,
                       args.ports_list, args.latency_rate, args.max_iterations, args.yaml)
