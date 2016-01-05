#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Itay Marom
Cisco Systems, Inc.

Copyright (c) 2015-2015 Cisco Systems, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages

from client_utils.jsonrpc_client import JsonRpcClient, BatchMessage
from client_utils.packet_builder import CTRexPktBuilder
import json

from common.trex_streams import *

import argparse
import tempfile
import subprocess
import os
from dpkt import pcap
from operator import itemgetter


def merge_cap_files (pcap_file_list, out_filename, delete_src = False):

    out_pkts = []

    # read all packets to a list
    for src in pcap_file_list:
        f = open(src, 'r')
        reader = pcap.Reader(f)
        pkts = reader.readpkts()
        out_pkts += pkts
        f.close()
        if delete_src:
            os.unlink(src)

    # sort by the timestamp
    out_pkts = sorted(out_pkts, key=itemgetter(0))


    out = open(out_filename, 'w')
    out_writer = pcap.Writer(out)

    for ts, pkt in out_pkts:
        out_writer.writepkt(pkt, ts)

    out.close()




class SimRun(object):
    def __init__ (self, yaml_file, dp_core_count, core_index, packet_limit, output_filename, is_valgrind, is_gdb, limit):

        self.yaml_file = yaml_file
        self.output_filename = output_filename
        self.dp_core_count = dp_core_count
        self.core_index = core_index
        self.packet_limit = packet_limit
        self.is_valgrind = is_valgrind
        self.is_gdb = is_gdb
        self.limit = limit

        # dummies
        self.handler = 0
        self.port_id = 0
        self.mul = {"op": "abs",
                    "type": "raw",
                    "value": 1}

        self.duration = -1

    def load_yaml_file (self):
        streams_db = CStreamsDB()
        stream_list = streams_db.load_yaml_file(self.yaml_file)

        streams_json = []
        for stream in stream_list.compiled:
            stream_json = {"id":1,
                           "jsonrpc": "2.0",
                           "method": "add_stream",
                           "params": {"handler": self.handler,
                                      "port_id": self.port_id,
                                      "stream_id": stream.stream_id,
                                      "stream": stream.stream}
                           }

            streams_json.append(stream_json)

        return streams_json


    def generate_start_cmd (self):
        return  {"id":1,
                 "jsonrpc": "2.0",
                 "method": "start_traffic",
                 "params": {"handler": self.handler,
                            "port_id": self.port_id,
                            "mul": self.mul,
                            "duration": self.duration}
                 }


    def run (self):

        # load the streams
        cmds_json = (self.load_yaml_file())
        cmds_json.append(self.generate_start_cmd())

        f = tempfile.NamedTemporaryFile(delete = False)
        f.write(json.dumps(cmds_json))
        f.close()

        try:
            cmd = ['bp-sim-64-debug',
                   '--pcap',
                   '--sl',
                   '--cores',
                   str(self.dp_core_count),
                   '--limit',
                   str(self.limit),
                   '-f',
                   f.name,
                   '-o',
                   self.output_filename]

            if self.core_index != None:
                cmd += ['--core_index', str(self.core_index)]

            if self.is_valgrind:
                cmd = ['valgrind', '--leak-check=full'] + cmd
            elif self.is_gdb:
                cmd = ['gdb', '--args'] + cmd

            print "executing command: '{0}'".format(" ".join(cmd))
            subprocess.call(cmd)

            # core index 
            if (self.dp_core_count > 1) and (self.core_index == None):
                self.merge_results()

        finally:
            os.unlink(f.name)


    def merge_results (self):
        if (self.core_index != None) or (self.dp_core_count == 1):
            # nothing to do
            return

        inputs = ["{0}-{1}".format(self.output_filename, index) for index in xrange(0, self.dp_core_count)]
        merge_cap_files(inputs, self.output_filename, delete_src = True)



def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename


def unsigned_int (x):
    x = int(x)
    if x <= 0:
        raise argparse.ArgumentTypeError("argument must be >= 1")

    return x

def setParserOptions():
    parser = argparse.ArgumentParser(prog="stl_sim.py")

    parser.add_argument("input_file",
                        help = "input file in YAML or Python format",
                        type = is_valid_file)

    parser.add_argument("output_file",
                        help = "output file in ERF format")


    parser.add_argument("-c", "--cores",
                        help = "DP core count [default is 1]",
                        default = 1,
                        type = int,
                        choices = xrange(1, 9))

    parser.add_argument("-n", "--core_index",
                        help = "Record only a specific core",
                        default = None,
                        type = int)

    parser.add_argument("-j", "--join",
                        help = "run and join output from 0..core_count [default is False]",
                        default = False,
                        type = bool)

    parser.add_argument("-l", "--limit",
                        help = "limit test total packet count [default is 5000]",
                        default = 5000,
                        type = unsigned_int)


    group = parser.add_mutually_exclusive_group()

    group.add_argument("-x", "--valgrind",
                       help = "run under valgrind [default is False]",
                       action = "store_true",
                       default = False)

    group.add_argument("-g", "--gdb",
                       help = "run under GDB [default is False]",
                       action = "store_true",
                       default = False)

    return parser


def validate_args (parser, options):

    if options.core_index:
        if not options.core_index in xrange(0, options.cores):
            parser.error("DP core index valid range is 0 to {0}".format(options.cores - 1))



def main ():
    parser = setParserOptions()
    options = parser.parse_args()

    validate_args(parser, options)

    r = SimRun(options.input_file,
               options.cores,
               options.core_index,
               options.limit,
               options.output_file,
               options.valgrind,
               options.gdb,
               options.limit)

    r.run()


if __name__ == '__main__':
    main()


