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

from common.trex_streams import *
from client_utils import parsing_opts

import json



import argparse
import tempfile
import subprocess
import os
from dpkt import pcap
from operator import itemgetter

class BpSimException(Exception):
    pass

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
    def __init__ (self, options):

        self.options = options

        # dummies
        self.handler = 0
        self.port_id = 0

        self.mul = options.mult

        self.duration = -1

    def load_yaml_file (self):
        streams_db = CStreamsDB()
        stream_list = streams_db.load_yaml_file(self.options.input_file)

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
                            "force":  False,
                            "port_id": self.port_id,
                            "mul": parsing_opts.decode_multiplier(self.mul),
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
            if self.options.json:
                with open(f.name) as file:
                    data = "\n".join(file.readlines())
                    print json.dumps(json.loads(data), indent = 4, separators=(',', ': '), sort_keys = True)
            else:
                self.execute_bp_sim(f.name)
        finally:
            os.unlink(f.name)



    def execute_bp_sim (self, json_filename):
        exe = './bp-sim-64' if self.options.release else './bp-sim-64-debug'
        if not os.path.exists(exe):
            print "cannot find executable '{0}'".format(exe)
            exit(-1)

        cmd = [exe,
               '--pcap',
               '--sl',
               '--cores',
               str(self.options.cores),
               '--limit',
               str(self.options.limit),
               '-f',
               json_filename,
               '-o',
               self.options.output_file]

        if self.options.dry:
            cmd += ['--dry']

        if self.options.core_index != None:
            cmd += ['--core_index', str(self.options.core_index)]

        if self.options.valgrind:
            cmd = ['valgrind', '--leak-check=full', '--error-exitcode=1'] + cmd

        elif self.options.gdb:
            cmd = ['gdb', '--args'] + cmd

        print "executing command: '{0}'".format(" ".join(cmd))
        rc = subprocess.call(cmd)
        if rc != 0:
            raise BpSimException()

        self.merge_results()


    def merge_results (self):
        if self.options.dry:
            return

        if self.options.cores == 1:
            return

        if self.options.core_index != None:
            return


        print "Mering cores output to a single pcap file...\n"
        inputs = ["{0}-{1}".format(self.options.output_file, index) for index in xrange(0, self.options.cores)]
        merge_cap_files(inputs, self.options.output_file, delete_src = True)




def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename


def unsigned_int (x):
    x = int(x)
    if x < 0:
        raise argparse.ArgumentTypeError("argument must be >= 0")

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

    parser.add_argument("-r", "--release",
                        help = "runs on release image instead of debug [default is False]",
                        action = "store_true",
                        default = False)

    parser.add_argument("-s", "--dry",
                        help = "dry run only (nothing will be written to the file) [default is False]",
                        action = "store_true",
                        default = False)

    parser.add_argument("-l", "--limit",
                        help = "limit test total packet count [default is 5000]",
                        default = 5000,
                        type = unsigned_int)

    parser.add_argument('-m', '--multiplier',
                        help = parsing_opts.match_multiplier_help,
                        dest = 'mult',
                        default = {'type':'raw', 'value':1, 'op': 'abs'},
                        type = parsing_opts.match_multiplier_strict)

    group = parser.add_mutually_exclusive_group()

    group.add_argument("-x", "--valgrind",
                       help = "run under valgrind [default is False]",
                       action = "store_true",
                       default = False)

    group.add_argument("-g", "--gdb",
                       help = "run under GDB [default is False]",
                       action = "store_true",
                       default = False)

    group.add_argument("--json",
                       help = "generate JSON output only to stdout [default is False]",
                       action = "store_true",
                       default = False)

    return parser


def validate_args (parser, options):

    if options.core_index:
        if not options.core_index in xrange(0, options.cores):
            parser.error("DP core index valid range is 0 to {0}".format(options.cores - 1))

    # zero is ok - no limit, but other values must be at least as the number of cores
    if (options.limit != 0) and options.limit < options.cores:
        parser.error("limit cannot be lower than number of DP cores")


def main ():
    parser = setParserOptions()
    options = parser.parse_args()

    validate_args(parser, options)

    r = SimRun(options)

    try:
        r.run()
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        exit(1)

    except BpSimException as e:
        print "\n\n*** BP sim exit code was non zero\n\n"
        exit(1)

    exit(0)

if __name__ == '__main__':
    main()


