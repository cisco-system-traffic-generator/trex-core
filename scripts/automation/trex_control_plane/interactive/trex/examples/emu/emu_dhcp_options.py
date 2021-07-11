#!/usr/bin/python
import emu_path
import time

from trex.emu.api import *


PROFILE_PATH = os.path.join(emu_path.EMU_PROFILES_PATH, "dhcp_json_options.py")
HOSTNAME_DHCP_OPTION = 12


c = EMUClient(server='localhost',
                sync_port=4510,
                verbose_level= "error",
                logger=None,
                sync_timeout=None)

c.connect()

print("Loading profile from: {}".format(PROFILE_PATH))

options = {
    "discoverDhcpClassIdOption": "MSFT 5.0",
    "requestDhcpClassIdOption": "MSFT 6.0",
    "dis": [
        [HOSTNAME_DHCP_OPTION, ord('j'), ord('s'), ord('o'), ord('n')]
    ],
    "req":[
        [HOSTNAME_DHCP_OPTION, ord('e'), ord('n'), ord('c'), ord('o'), ord('d'), ord('e')]
    ],
}

# start the emu profile using tunables
c.load_profile(profile = PROFILE_PATH, tunables=['--options', json.dumps(options)])

# print tables of namespaces and clients
print("Current ns and clients:")

c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

print("Capture traffic and verify the options :-)")

# removing profile
print("Removing profile..")
c.remove_all_clients_and_ns()

# print tables of namespaces and clients after removal
print("After removal ns and clients:")

c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)
