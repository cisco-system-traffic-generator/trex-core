#!/router/bin/python

import outer_packages
from client.trex_hltapi import CTRexHltApi
import traceback
import sys, time
from pprint import pprint
import argparse

def check_res(res):
    if res['status'] == 0:
        print('Encountered error:\n%s' % res['log'])
        sys.exit(1)
    return res

def save_streams_id(res, streams_id_arr):
    stream_id = res.get('stream_id')
    if type(stream_id) in (int, long):
        streams_id_arr.append(stream_id)
    elif type(stream_id) is list:
        streams_id_arr.extend(stream_id)

def print_brief_stats(res):
    title_str = ' '*3
    tx_str = 'TX:'
    rx_str = 'RX:'
    for port_id, stat in res.iteritems():
        if type(port_id) is not int:
            continue
        title_str += ' '*10 + 'Port%s' % port_id
        tx_str    += '%15s' % res[port_id]['aggregate']['tx']['total_pkts']
        rx_str    += '%15s' % res[port_id]['aggregate']['rx']['total_pkts']
    print(title_str)
    print(tx_str)
    print(rx_str)

def wait_with_progress(seconds):
    for i in range(0, seconds):
        time.sleep(1)
        sys.stdout.write('.')
        sys.stdout.flush()
    print('')

if __name__ == "__main__":
    try:
        parser = argparse.ArgumentParser(description='Example of using stateless TRex via HLT API.', formatter_class=argparse.RawTextHelpFormatter)
        parser.add_argument('-v', dest = 'verbose', default = 0, help='Stateless API verbosity:\n0: No prints\n1: Commands and their status\n2: Same as 1 + ZMQ in&out')
        parser.add_argument('--device', dest = 'device', default = 'localhost', help='Address of TRex server')
        args = parser.parse_args()
        hlt_client = CTRexHltApi(verbose = int(args.verbose))
        streams_id_arr = []
       
        print('Connecting to %s...' % args.device)
        res = check_res(hlt_client.connect(device = args.device, port_list = [0, 1], username = 'danklei', break_locks = True, reset = True))
        port_handle = res['port_handle']
        print('Connected.')

        print('Create single_burst 100 packets rate_pps=100 on port 0')
        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[0], transmit_mode = 'single_burst', pkts_per_burst = 100, rate_pps = 100))
        #save_streams_id(res, streams_id_arr)

        # playground - creating various streams on port 1
        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[1], save_to_yaml = '/tmp/hlt2.yaml',
                        tcp_src_port_mode = 'decrement',
                        tcp_src_port_count = 10, tcp_dst_port_count = 10, tcp_dst_port_mode = 'random'))

        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[1], save_to_yaml = '/tmp/hlt3.yaml',
                        l4_protocol = 'udp',
                        udp_src_port_mode = 'decrement',
                        udp_src_port_count = 10, udp_dst_port_count = 10, udp_dst_port_mode = 'random'))

        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[1], save_to_yaml = '/tmp/hlt4.yaml',
                        length_mode = 'increment',
                        #ip_src_addr = '192.168.1.1', ip_src_mode = 'increment', ip_src_count = 5,
                        ip_dst_addr = '5.5.5.5', ip_dst_mode = 'random', ip_dst_count = 2))

        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[1], save_to_yaml = '/tmp/hlt5.yaml',
                        length_mode = 'decrement', frame_size_min = 100, frame_size_max = 3000,
                        #ip_src_addr = '192.168.1.1', ip_src_mode = 'increment', ip_src_count = 5,
                        #ip_dst_addr = '5.5.5.5', ip_dst_mode = 'random', ip_dst_count = 2
                        ))
        # remove the playground
        check_res(hlt_client.traffic_config(mode = 'reset', port_handle = port_handle[1]))

        print('Create continuous stream for port 1, rate_pps = 1')
        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[1], save_to_yaml = '/tmp/hlt1.yaml',
                        #length_mode = 'increment', l3_length_min = 200,
                        ip_src_addr = '192.168.1.1', ip_src_mode = 'increment', ip_src_count = 5,
                        ip_dst_addr = '5.5.5.5', ip_dst_mode = 'random', ip_dst_count = 2))
        
        check_res(hlt_client.traffic_control(action = 'run', port_handle = port_handle))
        wait_with_progress(1)
        print('Sample after 1 seconds (only packets count)')
        res = check_res(hlt_client.traffic_stats(mode = 'all', port_handle = port_handle))
        print_brief_stats(res)
        print ''

        print('Port 0 has finished the burst, put continuous instead with rate 1000. No stopping of other ports.')
        check_res(hlt_client.traffic_control(action = 'stop', port_handle = port_handle[0]))
        check_res(hlt_client.traffic_config(mode = 'reset', port_handle = port_handle[0]))
        res = check_res(hlt_client.traffic_config(mode = 'create', port_handle = port_handle[0], rate_pps = 1000))
        save_streams_id(res, streams_id_arr)
        check_res(hlt_client.traffic_control(action = 'run', port_handle = port_handle[0]))
        wait_with_progress(5)
        print('Sample after another 5 seconds (only packets count)')
        res = check_res(hlt_client.traffic_stats(mode = 'aggregate', port_handle = port_handle))
        print_brief_stats(res)
        print ''

        print('Stop traffic at port 1')
        res = check_res(hlt_client.traffic_control(action = 'stop', port_handle = port_handle[1]))
        wait_with_progress(5)
        print('Sample after another %s seconds (only packets count)' % 5)
        res = check_res(hlt_client.traffic_stats(mode = 'aggregate', port_handle = port_handle))
        print_brief_stats(res)
        print ''
        print('Full HLT stats:')
        pprint(res)

        check_res(hlt_client.cleanup_session())
    except Exception as e:
        print(traceback.print_exc())
        print(e)
        raise
    finally:
        print('Done.')
