import stl_ndr_bench_tool as ndrtool
from pprint import pprint

import pandas as pd

config = {"server": 'csi-trex-08', "vm": "cached", "latency": True, "drop_rate_interval": 10,
          "pkt_size": "128", "max_iterations": 1, "iteration_duration": 20.0, "q_ful_resolution": 2.0,
          "first_run_duration": 20.0, "ports_list": [0, 3], "pdr": 0.1, "pdr_error": 1.0,
          "ndr_results": 1, "verbose": True}

print __file__
results = ndrtool.ndr_benchmark_test(**config)

print("this is results:\n")
pprint(results)

s = pd.Series(results['results'])
results_dframe = pd.DataFrame({results['results']['title']: s})
print results_dframe
results_dframe.to_csv("myfile.csv")
