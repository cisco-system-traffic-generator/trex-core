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



class SimRun(object):
    def __init__ (self, yaml_file, dp_core_count, core_index, packet_limit, output_filename):

        self.yaml_file = yaml_file
        self.output_filename = output_filename
        self.dp_core_count = dp_core_count
        self.core_index = core_index
        self.packet_limit = packet_limit

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
            subprocess.call(['bp-sim-64-debug', '--sl', '-f', f.name, '-o', self.output_filename])
        finally:
            os.unlink(f.name)


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
                        help = "DP core index to examine [default is 0]",
                        default = 0,
                        type = int)

    parser.add_argument("-j", "--join",
                        help = "run and join output from 0..core_count [default is False]",
                        default = False,
                        type = bool)

    parser.add_argument("-l", "--limit",
                        help = "limit test total packet count [default is 5000]",
                        default = 5000,
                        type = unsigned_int)



    return parser


def validate_args (parser, options):
    if options.core_index < 0 or options.core_index >= options.cores:
        parser.error("DP core index valid range is 0 to {0}".format(options.cores - 1))



def main ():
    parser = setParserOptions()
    options = parser.parse_args()

    validate_args(parser, options)

    r = SimRun(options.input_file, options.cores, options.core_index, options.limit, options.output_file)

    r.run()


if __name__ == '__main__':
    main()


