import argparse
import stf_path
from trex_stf_lib.trex_client import CTRexClient
from pprint import pprint
import csv 
import math

# sample TRex stateful to chnage active-flows and get results 

def minimal_stateful_test(server,csv_file,a_active_flows):

    trex_client = CTRexClient(server)

    trex_client.start_trex(
            c = 7,
            m = 30000,
#            f = 'cap2/cur_flow_single.yaml',
            f = 'cap2/cur_flow.yaml',
            d = 30,
            l = 1000,
            p=True,
            cfg = "cfg/trex_08_5mflows.yaml",
            active_flows=a_active_flows,
            nc=True
            )

    result = trex_client.sample_to_run_finish()


    active_flows = result.get_value_list('trex-global.data.m_active_flows')

    cpu_utl = result.get_value_list('trex-global.data.m_cpu_util')

    pps = result.get_value_list('trex-global.data.m_tx_pps')

    queue_full = result.get_value_list('trex-global.data.m_total_queue_full')

    if queue_full[-1]>10000:
        print("WARNING QUEU WAS FULL");

    tuple=(active_flows[-5],cpu_utl[-5],pps[-5],queue_full[-1])
    file_writer = csv.writer(test_file)
    file_writer.writerow(tuple);
    


if __name__ == '__main__':
    test_file = open('tw_2_layers.csv', 'wb');
    parser = argparse.ArgumentParser(description="active-flow example")

    parser.add_argument('-s', '--server',
                        dest='server',
                        help='Remote trex address',
                        default='127.0.0.1',
                        type = str)
    args = parser.parse_args()

    max_flows = 8000000;
    min_flows = 100;
    active_flow = min_flows;
    num_point = 10
    factor = math.exp(math.log(max_flows/min_flows,math.e)/num_point);
    for i in range(num_point+1):
        print("<<=====================>>",i,math.floor(active_flow))
        minimal_stateful_test(args.server,test_file,math.floor(active_flow))
        active_flow=active_flow*factor

    test_file.close();

