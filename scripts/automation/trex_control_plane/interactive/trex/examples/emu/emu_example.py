#!/usr/bin/python
import emu_path

from itertools import chain, combinations
from trex.emu.api import *

import argparse
import pprint

EMU_SERVER = "localhost"

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

# start the emu profile
c.load_profile(filename = args.file, max_rate = 2048, tunables = '')

# print tables of namespaces and clients
c.print_all_ns_clients(max_ns_show = 1, max_c_show = 10)

# print arp counters
pprint.pprint(c.arp.get_counters(port = 1))
