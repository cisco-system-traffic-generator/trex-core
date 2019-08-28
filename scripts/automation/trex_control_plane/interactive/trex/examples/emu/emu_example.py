#!/usr/bin/python

from itertools import chain, combinations
from trex.emu.api import *

import argparse
import pprint

EMU_SERVER = "localhost"

c = EMUClient(server=EMU_SERVER,
                sync_port=4510,
                verbose_level= "debug",
                logger=None,
                sync_timeout=None)

# load profile
parser = argparse.ArgumentParser(description='Simple script to run EMU profile.')
parser.add_argument("-f", "--file", required = True, dest="file", help="Python file with a valid EMU profile.")
args = parser.parse_args()

c.connect()

print("loading profile from: %s" % args.file)
profile = EMUProfile.load_py(args.file)

# start the emu profile
c.load_profile('emu/simple_emu.py')

# print tables of namespaces and clients
c.print_all_ns_clients()

# print all the ctx counters
c.print_counters()
