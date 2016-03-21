import stl_path
from trex_stl_lib.api import *

import time

def simple ():

    # create client
    #verbose_level = LoggerApi.VERBOSE_HIGH
    c = STLClient(verbose_level = LoggerApi.VERBOSE_REGULAR)
    passed = True
    
    try:
        # connect to server
        c.connect()

        my_ports=[0,1]

        # prepare our ports
        c.reset(ports = my_ports)

        profile_file =   "../../../../stl/udp_1pkt_simple.py"

        try:
            profile = STLProfile.load(profile_file)
        except STLError as e:
            print(format_text("\nError while loading profile '{0}'\n".format(opts.file[0]), 'bold'))
            print(e.brief() + "\n")
            return

        print(profile.dump_to_yaml())

        c.remove_all_streams(my_ports)


        c.add_streams(profile.get_streams(), ports = my_ports)

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        c.wait_on_traffic(ports = [0, 1])


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

