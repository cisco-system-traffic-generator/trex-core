import stl_ndr_bench_tool as ndrtool
from pprint import pprint
import yaml as yml
import pandas as pd
import os
import argparse


def exclude_from_report(hu_dict):
    del (hu_dict['results']['Title'])
    del (hu_dict['results']['Total Iterations'])
    del (hu_dict['results']['Elapsed Time [Sec]'])
    del (hu_dict['results']['Drop Rate [%]'])
    hu_dict['results']['Total MPPS'] = hu_dict['results']['TX [MPPS]']
    del (hu_dict['results']['TX [MPPS]'])
    del hu_dict['results']['RX [MPPS]']
    hu_dict['results']['Total L1'] = hu_dict['results']['Total TX L1']
    del (hu_dict['results']['Total RX L1'])
    del (hu_dict['results']['Total TX L1'])
    hu_dict['results']['Total L2'] = hu_dict['results']['TX [bps]']
    del(hu_dict['results']['TX [bps]'])
    del (hu_dict['results']['RX [bps]'])
    del (hu_dict['results']['OPT TX Rate [bps]'])
    del (hu_dict['results']['OPT RX Rate [bps]'])
    del (hu_dict['results']['NDR points'])
    return hu_dict


def run_and_export(server_name):
    pkt_sizes = ["imix", "1514", "590", "128", "64"]

    config = {"server": server_name, "vm": "cached", "latency": False, "drop_rate_interval": 10,
              "pkt_size": "128", "max_iterations": 1, "iteration_duration": 20.0, "q_ful_resolution": 2.0,
              "first_run_duration": 20.0, "pdr": 0.1, "pdr_error": 1.0,
              "ndr_results": 1, "verbose": True, "title": "kiwi-02", 'core_mask': 0xffffffffffffffff}

    cfg_file = 'yamls/config.yaml'
    if os.path.isfile(cfg_file):
        try:
            with open(cfg_file) as f:
                yml_config_dict = yml.safe_load(f)
            config.update(yml_config_dict)
        except IOError as e:
            print "loading yaml failed with error: " + e.message

    total_results = {}
    results_series = {}
    latency_series = {}
    # port = ports_list[0]

    # pprint(config)
    for pkt_size in pkt_sizes:
        config['pkt_size'] = pkt_size
        results, hu_dict = ndrtool.ndr_benchmark_test(**config)
        total_results[pkt_size] = dict(hu_dict)
        hu_dict = exclude_from_report(hu_dict)
        # if config['latency']:
        #     latency_results = hu_dict['results']['latency']
        #     latency_histogram = latency_results[port]['histogram']
        #     del (hu_dict['results']['latency'][port]['histogram'])
        #     latency_results[port]['histogram'] = [(k, latency_histogram[k]) for k in latency_histogram]
        #     l = pd.Series(latency_results[port])
        #     latency_series[pkt_size] = l
        del (hu_dict['results']['latency'])
        s = pd.Series(hu_dict['results'])
        results_series[pkt_size] = s

    res_dframe = pd.DataFrame(results_series)
    res_dframe.index.name = 'Packet Size'
    res_dframe.transpose().to_csv("NDR_benchmark_"+config["server"]+".csv")
    if config['latency']:
        latency_dframe = pd.DataFrame(latency_series)
        latency_dframe.transpose().to_csv("NDR_benchmark_latency_"+config["server"]+".csv")
    # print("this is total_results:\n")
    # pprint(total_results)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="TRex NDR benchmark tool")
    parser.add_argument('-s', '--server',
                        dest='server',
                        help='Remote trex address',
                        default='127.0.0.1',
                        type=str)
    args = parser.parse_args()
    run_and_export(args.server)






