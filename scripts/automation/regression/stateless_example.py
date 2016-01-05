#!/router/bin/python

import outer_packages
from client.trex_hltapi import CTRexHltApi
import traceback
import sys, time

def fail(reason):
    print 'Encountered error:\n%s' % reason
    sys.exit(1)

if __name__ == "__main__":
    port_list = [0, 1]
    #port_list = 1
    try:
        print 'init'
        hlt_client = CTRexHltApi()
        
        print 'connecting'
        con = hlt_client.connect("localhost", port_list, "danklei", sync_port = 4501, async_port = 4500, break_locks=True, reset=True)#, port=6666)
        print 'connected?', hlt_client.connected
        if not hlt_client.trex_client or not hlt_client.connected:
            fail(con['log'])
        print 'connect result:', con

        res = hlt_client.traffic_config("create", 0)#, ip_src_addr="2000.2.2")
        print 'traffic_config result:', res

        res = hlt_client.traffic_config("create", 1)#, ip_src_addr="2000.2.2")
        print res
        print 'got to running!'
        #sys.exit(0)
        res = hlt_client.traffic_control("run", 1, mul = {'type': 'raw', 'op': 'abs', 'value': 1}, duration = 15)#, ip_src_addr="2000.2.2")
        print res
        time.sleep(2)
        res = hlt_client.traffic_control("stop", 1)#, ip_src_addr="2000.2.2")
        print res



    except Exception as e:
        raise
    finally:
        #pass
        if hlt_client.trex_client:
            res = hlt_client.cleanup_session(port_list)
            print res
