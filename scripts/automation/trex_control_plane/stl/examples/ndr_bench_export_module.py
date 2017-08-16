import stl_ndr_bench_tool as ndrtool
from pprint import pprint

import pandas as pd

ports_list = [0, 1]

pkt_sizes = ["imix", "1514", "590", "128", "64"]

config = {"server": 'csi-kiwi-02', "vm": "cached", "latency": False, "drop_rate_interval": 10,
          "pkt_size": "128", "max_iterations": 1, "iteration_duration": 20.0, "q_ful_resolution": 2.0,
          "first_run_duration": 20.0, "ports_list": ports_list, "pdr": 0.1, "pdr_error": 1.0,
          "ndr_results": 1, "verbose": True, "title": "kiwi-02", 'core_mask': None}

print __file__

total_results = {}
results_series = {}
latency_series = {}
port = ports_list[0]

for pkt_size in pkt_sizes:
    config['pkt_size'] = pkt_size
    results, hu_dict = ndrtool.ndr_benchmark_test(**config)
    total_results[pkt_size] = dict(hu_dict)
    del (hu_dict['results']['Title'])
    if config['latency']:
        latency_results = hu_dict['results']['latency']
        latency_histogram = latency_results[port]['histogram']
        del (hu_dict['results']['latency'][port]['histogram'])
        latency_results[port]['histogram'] = [(k, latency_histogram[k]) for k in latency_histogram]
        del (hu_dict['results']['latency'])
        l = pd.Series(latency_results[port])
        latency_series[pkt_size] = l
    s = pd.Series(hu_dict['results'])
    results_series[pkt_size] = s


res_dframe = pd.DataFrame(results_series)
res_dframe.transpose().to_csv("NDR_benchmark.csv")
if config['latency']:
    latency_dframe = pd.DataFrame(latency_series)
    latency_dframe.transpose().to_csv("NDR_benchmark_latency.csv")
print("this is total_results:\n")
pprint(total_results)

