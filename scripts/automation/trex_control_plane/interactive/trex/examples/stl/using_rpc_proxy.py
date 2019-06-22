#!/router/bin/python

import argparse
import sys
import os
from time import sleep
from pprint import pprint

# ext libs
import stl_path
sys.path.append(os.path.join(stl_path.EXT_LIBS_PATH, 'jsonrpclib-pelix-0.2.5'))
import jsonrpclib

def fail(msg):
    print(msg)
    sys.exit(1)

def verify(res):
    if not res[0]:
        fail(res[1])
    return res

def verify_hlt(res):
    if res['status'] == 0:
        fail(res['log'])
    return res

### Main ###

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Use of Stateless through rpc_proxy. (Can be implemented in any language)')
    parser.add_argument('-s', '--server', type=str, default = 'localhost', dest='server', action = 'store',
                        help = 'Address of rpc proxy.')
    parser.add_argument('-p', '--port', type=int, default = 8095, dest='port', action = 'store',
                        help = 'Port of rpc proxy.\nDefault is 8095.')
    parser.add_argument('--master_port', type=int, default = 8091, dest='master_port', action = 'store',
                        help = 'Port of Master daemon.\nDefault is 8091.')
    args = parser.parse_args()

    server = jsonrpclib.Server('http://%s:%s' % (args.server, args.port), timeout = 15)
    master = jsonrpclib.Server('http://%s:%s' % (args.server, args.master_port), timeout = 15)

# Connecting

    try:
        print('Connecting to STL RPC proxy server')
        server.check_connectivity()
        print('Connected')
    except Exception as e:
        print('Could not connect to STL RPC proxy server: %s\nTrying to start it from Master daemon.' % e)
        try:
            master.check_connectivity()
            master.start_stl_rpc_proxy()
            print('Started')
        except Exception as e:
            print('Could not start it from Master daemon. Error: %s' % e)
            sys.exit(-1)


# Native API

    print('Initializing Native Client')
    verify(server.native_proxy_init(server = args.server, force = True))

    print('Connecting to TRex server')
    verify(server.connect())

    print('Resetting all ports')
    verify(server.reset())

    print('Getting ports info')
    res = verify(server.native_method(func_name = 'get_port_info'))
    print('Ports info is: %s' % res[1])
    ports = [port['index'] for port in res[1]]

    print('Sending pcap to ports %s' % ports)
    verify(server.push_remote(pcap_filename = 'stl/sample.pcap'))
    sleep(3)

    print('Getting stats')
    res = verify(server.get_stats())
    pprint(res[1])

    print('Resetting all ports')
    verify(server.reset())

    imix_path_1 = '../../../../stl/imix.py'
    imix_path_2 = '../../stl/imix.py'
    if os.path.exists(imix_path_1):
        imix_path = imix_path_1
    elif os.path.exists(imix_path_2):
        imix_path = imix_path_2
    else:
        print('Could not find path of imix profile, skipping')
        imix_path = None

    if imix_path:
        print('Adding profile %s' % imix_path)
        verify(server.native_method(func_name = 'add_profile', filename = imix_path))

        print('Start traffic for 5 sec')
        verify(server.native_method('start'))
        sleep(5)

        print('Getting stats')
        res = verify(server.get_stats())
        pprint(res[1])

        print('Resetting all ports')
        verify(server.reset())

    print('Deleting Native Client instance')
    verify(server.native_proxy_del())

# HLTAPI

    print('Initializing HLTAPI Client')
    verify_hlt(server.hltapi_proxy_init(force = True))
    print('HLTAPI Client initiated')

    print('HLTAPI connect')
    verify_hlt(server.hlt_connect(device = args.server, port_list = ports, reset = True, break_locks = True))

    print('Creating traffic')
    verify_hlt(server.traffic_config(
            mode = 'create', bidirectional = True,
            port_handle = ports[0], port_handle2 = ports[1],
            frame_size = 100,
            l3_protocol = 'ipv4',
            ip_src_addr = '10.0.0.1', ip_src_mode = 'increment', ip_src_count = 254,
            ip_dst_addr = '8.0.0.1', ip_dst_mode = 'increment', ip_dst_count = 254,
            l4_protocol = 'udp',
            udp_dst_port = 12, udp_src_port = 1025,
            rate_percent = 10, ignore_macs = True,
            ))

    print('Starting traffic for 5 sec')
    verify_hlt(server.traffic_control(action = 'run', port_handle = ports[:2]))

    sleep(5)
    print('Stopping traffic')
    verify_hlt(server.traffic_control(action = 'stop', port_handle = ports[:2]))

    print('Getting stats')
    res = verify_hlt(server.traffic_stats(mode = 'aggregate', port_handle = ports[:2]))
    pprint(res)

    print('Deleting HLTAPI Client instance')
    verify_hlt(server.hltapi_proxy_del())
