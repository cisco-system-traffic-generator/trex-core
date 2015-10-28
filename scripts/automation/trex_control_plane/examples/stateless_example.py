#!/router/bin/python

import trex_root_path
from client.trex_hltapi import CTRexHltApi

if __name__ == "__main__":
    port_list = [1,2]
    try:
        hlt_client = CTRexHltApi()
        con = hlt_client.connect("localhost", port_list, "danklei", break_locks=True, reset=True)#, port=6666)
        print con

        res = hlt_client.traffic_config("create", 1)#, ip_src_addr="2000.2.2")
        print res
        res = hlt_client.traffic_config("create", 2)#, ip_src_addr="2000.2.2")
        print res

        res = hlt_client.traffic_control("run", [1, 2])#, ip_src_addr="2000.2.2")
        print res

        res = hlt_client.traffic_control("stop", [1, 2])#, ip_src_addr="2000.2.2")
        print res



    except Exception as e:
        raise
    finally:
        res = hlt_client.cleanup_session(port_list)
        print res