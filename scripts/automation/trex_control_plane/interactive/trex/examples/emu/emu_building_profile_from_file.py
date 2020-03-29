#!/usr/bin/python
import emu_path

from trex.emu.api import *

import argparse
import pprint

EMU_SERVER = "10.56.199.218"

c = EMUClient(server=EMU_SERVER,
                sync_port=4510,
                verbose_level= "error",
                logger=None,
                sync_timeout=None)

# load profile
parser = argparse.ArgumentParser(description='Simple script to run EMU profile.')
parser.add_argument("-f", "--file", required = True, dest="file", help="Python file with a valid EMU profile.")
args = parser.parse_args()

c.connect()

print("loading profile from: %s" % args.file)

# start the emu profile using tunables
c.load_profile(profile = args.file)

# print tables of namespaces and clients
print("Current ns and clients:")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

# removing profile
print("Removing profile..")
c.remove_all_clients_and_ns()

# print tables of namespaces and clients after removal
print("After removal ns and clients:")
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)