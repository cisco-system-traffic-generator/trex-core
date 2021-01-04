#!/bin/router/python
import stl_path
from trex.stl.api import *
import yaml as yml
from pprint import pprint
import argparse
import sys
import trex.examples.stl.ndr_bench as ndr
import trex.examples.stl.stl_ndr_bench_tool as ndr_tool
from texttable import Texttable


def print_table(final_results, sizes, scale):
    traffic_unit = scale["traffic_unit"]
    pkt_unit = scale["pkt_unit"]

    table = Texttable(max_width=0)
    table.set_chars(['', '|', '', ''])
    table.set_precision(2)
    table.set_deco(Texttable.VLINES)
    rows = [["| Packet size",
                "Line Utilization (%)",
                "Total L1 {}".format(traffic_unit),
                "Total L2 {}".format(traffic_unit),
                "CPU Util (%)",
                "Total {}".format(pkt_unit),
                "BW per core {} <1>".format(traffic_unit),
                "{} per core <2>".format(pkt_unit),
                "Multiplier"
            ]]
    for size in sizes:
        pkt_result = final_results[size]
        pkt_str = "| " + str(pkt_result["Packet size"])
        row = [pkt_str,
                pkt_result["Line Utilization (%)"],
                pkt_result["Total L1 {}".format(traffic_unit)],
                pkt_result["Total L2 {}".format(traffic_unit)],
                pkt_result["CPU Util (%)"],
                pkt_result["Total {}".format(pkt_unit)],
                pkt_result["BW per core {}".format(traffic_unit)],
                pkt_result["{} per core".format(pkt_unit)],
                pkt_result["Multiplier"]
                ]
        rows.append(row)

    table.add_rows(rows)

    print (table.draw())


def convert_rate(rate_bps, packet=False, scale=None):
    if scale is not None:
        if packet:
            return rate_bps / scale["pkt_value"], scale["pkt_unit"]
        return rate_bps / scale["traffic_value"], scale["traffic_unit"]

    converted = float(rate_bps)
    magnitude = 0
    while converted > 1000.00:
        magnitude += 1
        converted /= 1000.00
    converted = round(converted, 2)
    if packet:
        postfix = "PPS"
    else:
        postfix = "b/s"

    if magnitude == 1:
        postfix = "K" + postfix

    elif magnitude == 2:
        postfix = "M" + postfix

    elif magnitude == 3:
        postfix = "G" + postfix

    if not packet:
        postfix = "(" + postfix + ")"

    return 1000 ** magnitude, postfix


def get_scale(rate_bps, rate_pps):
    scale = {}
    traffic_value, traffic_unit =  convert_rate(rate_bps)
    pkt_value, pkt_unit = convert_rate(rate_pps, packet=True)

    scale["traffic_value"] = traffic_value
    scale["pkt_value"] = pkt_value
    scale["traffic_unit"] = traffic_unit
    scale["pkt_unit"] = pkt_unit

    return scale


def get_info(result, size, num_total_cores, fsl, scale):
    reduced_result = {}

    if scale is None:
        scale = get_scale(result.get("total_tx_L1", 0), result.get("tx_pps", 0))

    reduced_result["Line Utilization (%)"] = round(result.get("tx_util", 0), 2)

    total_tx_l1 ,traffic_unit = convert_rate(result.get("total_tx_L1", 0), scale=scale)
    reduced_result["Total L1 {}".format(traffic_unit)] = total_tx_l1

    tx_bps = (convert_rate(result.get("tx_bps", 0), scale=scale))[0]
    reduced_result["Total L2 {}".format(traffic_unit)] = tx_bps

    tx_pps, pkt_unit = convert_rate(result.get("tx_pps", 0), packet=True, scale=scale)
    reduced_result["Total {}".format(pkt_unit)] = tx_pps

    reduced_result["CPU Util (%)"] = round(result.get("cpu_util", 0), 2)

    reduced_result["Multiplier"] = str(round(result.get("rate_p", 0), 2)) + '%'

    if fsl:
        cpu_util = result["cpu_util"]
    else:
        cpu_util = reduced_result["CPU Util (%)"]

    if cpu_util != 0:
        reduced_result["{} per core".format(pkt_unit)] = round((reduced_result["Total {}".format(pkt_unit)] * 100) / (num_total_cores * cpu_util), 2)
        reduced_result["BW per core {}".format(traffic_unit)] = round((reduced_result["Total L1 {}".format(traffic_unit)] * 100) / (num_total_cores * cpu_util), 2)
    else:
        reduced_result["{} per core".format(pkt_unit)] = "-"
        reduced_result["BW per core {}".format(traffic_unit)] = "-"
        print("Can't calculate {} per core because of cpu_util=0".format(pkt_unit))
        print("Can't calculate BW per core because of cpu_util=0")

    reduced_result["Packet size"] = size

    return reduced_result, scale


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="TRex Stateless Bench FS Latency using NDR tool")
    parser.add_argument('-o', '--output', dest='output',
                        help='If you specify this flag, after the test is finished, the final results will be printed in the requested format.'
                             ' Use json for JSON format or hu for human readable format.'
                             'The default value is hu.',
                        default='hu',
                        type=str)
    parser.add_argument('--prof-tun',
                        dest='profile_tunables',
                        help='Tunables to forward to the profile if it exists. Use: --prof-tun a=1,b=2,c=3 (no spaces).'
                             'The default value for vm is "cached".',
                        default={'vm' : 'cached'},
                        type=ndr_tool.decode_tunables)
    parser.add_argument('--ports', dest='ports_list', help='Specify an even list of ports for running traffic on.',
                        type=int, nargs='*', default=None, required=True)
    parser.add_argument('-p', '--pdr',
                        dest='pdr',
                        help='Allowed percentage of drops. (out of total traffic). [0(NDR)-100].'
                             'The default value is 0.1.',
                        default=0.1,
                        type=float)
    parser.add_argument('-t', '--iter-time',
                        dest='iteration_duration',
                        help='Duration of each run iteration during test. [seconds].'
                             'The default value is 20.0.',
                        default=20.00,
                        type=float)
    parser.add_argument('-v', '--verbose',
                        dest='verbose',
                        help='When verbose is set, prints test results and iteration to stdout.'
                             'The default value is False',
                        default=False,
                        action='store_true')
    parser.add_argument('-x', '--max-iterations',
                        dest='max_iterations',
                        help='The bench stops when reaching result or max_iterations, the early of the two. [int]'
                             'The default value is 10.',
                        default=10,
                        type=int)
    parser.add_argument('-q', '--q-full',
                        dest='q_full_resolution',
                        help='Percent of traffic allowed to be queued when transmitting above DUT capability.\n'
                             '0%% q-full resolution is not recommended due to precision issues. [percents 0-100]'
                             'The default value is 100.',
                        default=100,
                        type=float)
    parser.add_argument('--max-latency', 
                        dest='max_latency',
                        help='Maximal latency allowed. If the percent of latency packets above this value pass the latency tolerance,\n'
                             ' then the rate is considered too high. If the value is 0, then we consider this as unset. Default=0. [usec].'
                             'The default value is 0.',
                        default=0,
                        required='--lat-tolerance' in sys.argv,
                        type=int)
    parser.add_argument('--lat-tolerance',
                        dest='lat_tolerance',
                        help='Percentage of latency packets allowed beyond max-latency. Default is 0%%. In this case we compare max-latency\n'
                             'to the maximal latency in a run. [percents 0-100].'
                             'The default value is 0.',
                        default=0,
                        type=ndr_tool.is_percentage)

    args = parser.parse_args()

    profile = os.path.join(stl_path.STL_PROFILES_PATH, 'bench.py')

    title = 'TRex Stateless Bench FS Latency using NDR tool'

    fsl = False
    if "flow" in args.profile_tunables:
        if args.profile_tunables["flow"] == "fsl":
            fsl = True

    final_results = {}
    scale = None
    success = True

    if fsl:
        sizes = [1514, 590, 128, 64]
    else:
        sizes = ['imix', 1514, 590, 128, 64]

    for idx, size in enumerate(sizes):
        args.profile_tunables["size"] = size
        # run the tests
        ret_value = ndr_tool.ndr_benchmark_test(profile_tunables=args.profile_tunables, output=args.output, ports_list=args.ports_list,
                                                       opt_binary_search=True, bi_dir=True, profile=profile, print_final_results=False,
                                                       pdr=args.pdr, iteration_duration=args.iteration_duration, max_latency=args.max_latency,
                                                       max_iterations= args.max_iterations, title=title, verbose=args.verbose,
                                                       q_full_resolution=args.q_full_resolution, lat_tolerance=args.lat_tolerance)
        if ret_value == -1 or ret_value == None:
            success = False
            break

        results = ret_value[0]

        print("Bench run has finished for size {} ({}\{}).\n".format(size, idx+1, len(sizes)))

        final_results[size], scale = get_info(results["results"], size, results["config"]["total_cores"], fsl, scale)

    if success:
        if args.output == "json":
            pprint(final_results)
        else:
            print_table(final_results, sizes, scale)
