#!/router/bin/python

import outer_packages
from client.trex_hltapi import CTRexHltApi
import traceback
import sys, time
from pprint import pprint

def check_response(res):
    if res['status'] == 0:
        print 'Encountered error:\n%s' % res['log']
        sys.exit(1)

if __name__ == "__main__":
    try:
        is_verbose = 2 if '--verbose' in sys.argv else 1
        hlt_client = CTRexHltApi(verbose = is_verbose)
       
        print 'Connecting...'
        res = hlt_client.connect(device = 'csi-trex-04', port_list = [0], username = 'danklei', break_locks = True, reset = True)
        check_response(res)
        port_handle = res['port_handle']
        print 'Connected.'

        res = hlt_client.traffic_config('reset', port_handle = port_handle[:], ip_src_addr='1.1.1.1')
        print res
        check_response(res)

        res = hlt_client.traffic_config('create', port_handle = port_handle[:], ip_src_addr='1.1.1.1')
        print res
        check_response(res)


        res = hlt_client.traffic_config('create', port_handle = port_handle[0], ip_src_addr='2.2.2.2')
        check_response(res)


        res = hlt_client.get_stats()
        res = hlt_client.traffic_config('modify', port_handle = port_handle[0], stream_id = 1, ip_src_addr='6.6.6.6')
        check_response(res)
        #for stream_id, stream_data in hlt_client.get_port_streams(0).iteritems():
        #    print '>>>>>>>>> %s: %s' % (stream_id, stream_data)


        check_response(hlt_client.traffic_config('create', port_handle = port_handle[1], ip_src_addr='3.3.3.3'))
        check_response(hlt_client.traffic_config('create', port_handle = port_handle[1], ip_src_addr='4.4.4.4'))
        print '2'
        check_response(htl_client.traffic_config('create', port_handle = port_handle[:], stream_id = 999, ip_src_addr='5.5.5.5'))
        check_response(hlt_client.traffic_config('modify', port_handle = port_handle[0], stream_id = 1, ip_src_addr='6.6.6.6'))
        print '3'
        check_response(hlt_client.traffic_config('modify', port_handle = port_handle[1], stream_id = 1))

#        res = hlt_client.traffic_config('create', port_handle = port_handle[1])#, ip_src_addr='2000.2.2')
#        if res['status'] == 0:
#            fail(res['log'])

        print 'got to running!'
        check_response(hlt_client.traffic_control('run', port_handle = port_handle[1], mul = {'type': 'raw', 'op': 'abs', 'value': 10}, duration = 15))
        for i in range(0, 15):
            print '.',
            time.sleep(1)

        print 'stop the traffic!'
        check_response(hlt_client.traffic_control('stop', port_handle = port_handle[1]))


    except Exception as e:
        print traceback.print_exc()
        print e
        raise
    finally:
        print 'Done.'
        #if hlt_client.trex_client:
        #    res = hlt_client.teardown()
