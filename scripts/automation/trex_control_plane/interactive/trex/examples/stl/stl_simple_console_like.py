import stl_path
from trex.stl.api import *

import time

def simple ():

    # create client
    #verbose_level = 'high'
    c = STLClient(verbose_level = 'error')
    passed = True
    
    try:
        # connect to server
        c.connect()

        my_ports=[0,1]

        # prepare our ports
        c.reset(ports = my_ports)

        print((" is connected {0}".format(c.is_connected())))

        print((" number of ports {0}".format(c.get_port_count())))
        print((" acquired_ports {0}".format(c.get_acquired_ports())))
        # port stats
        print(c.get_stats(my_ports))
        # port info
        print(c.get_port_info(my_ports))

        #c.ping()
        profile_file = os.path.join(stl_path.STL_PROFILES_PATH, 'udp_1pkt_simple.py')

        print("start")
        c.start_line (" -f %s -m 10mpps --port 0 1 " % profile_file)
        time.sleep(2);
        c.pause_line("--port 0 1");
        time.sleep(2);
        c.resume_line("--port 0 1");
        time.sleep(2);
        c.update_line("--port 0 1 -m 5mpps");
        time.sleep(2);
        c.stop_line("--port 0 1");

    except STLError as e:
        passed = False
        print(e)

    finally:
        c.disconnect()

    if passed:
        print("\nTest has passed :-)\n")
    else:
        print("\nTest has failed :-(\n")


# run the tests
simple()

