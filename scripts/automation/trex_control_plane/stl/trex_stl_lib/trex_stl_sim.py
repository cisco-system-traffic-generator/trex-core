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
# simulator can be run as a standalone
import trex_stl_ext

from trex_stl_exceptions import *
from yaml import YAMLError
from trex_stl_streams import *
from utils import parsing_opts
from trex_stl_client import STLClient

import re
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
    if not all([os.path.exists(f) for f in pcap_file_list]):
        print "failed to merge cap file list...\nnot all files exist\n"
        return

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



# stateless simulation
class STLSim(object):
    def __init__ (self, bp_sim_path = None, handler = 0, port_id = 0):

        if not bp_sim_path:
            # auto find scripts
            m = re.match(".*/trex-core", os.getcwd())
            if not m:
                raise STLError('cannot find BP sim path, please provide it')

            self.bp_sim_path = os.path.join(m.group(0), 'scripts')

        else:
            self.bp_sim_path = bp_sim_path

        # dummies
        self.handler = handler
        self.port_id = port_id


    def generate_start_cmd (self, mult = "1", force = True, duration = -1):
        return  {"id":1,
                 "jsonrpc": "2.0",
                 "method": "start_traffic",
                 "params": {"handler": self.handler,
                            "force":  force,
                            "port_id": self.port_id,
                            "mul": parsing_opts.decode_multiplier(mult),
                            "duration": duration}
                 }



    # run command
    # input_list - a list of streams or YAML files
    # outfile - pcap file to save output, if None its a dry run
    # dp_core_count - how many DP cores to use
    # dp_core_index - simulate only specific dp core without merging
    # is_debug - debug or release image
    # pkt_limit - how many packets to simulate
    # mult - multiplier
    # mode - can be 'valgrind, 'gdb', 'json' or 'none'
    def run (self,
             input_list,
             outfile = None,
             dp_core_count = 1,
             dp_core_index = None,
             is_debug = True,
             pkt_limit = 5000,
             mult = "1",
             duration = -1,
             mode = 'none',
             silent = False):

        if not mode in ['none', 'gdb', 'valgrind', 'json', 'yaml','pkt','native']:
            raise STLArgumentError('mode', mode)

        # listify
        input_list = input_list if isinstance(input_list, list) else [input_list]

        # check streams arguments
        if not all([isinstance(i, (STLStream, str)) for i in input_list]):
            raise STLArgumentError('input_list', input_list)

        # split to two type
        input_files  = [x for x in input_list if isinstance(x, str)]
        stream_list = [x for x in input_list if isinstance(x, STLStream)]

        # handle YAMLs
        for input_file in input_files:
            try:
                profile = STLProfile.load(input_file, direction = (self.port_id % 2), port = self.port_id)
            except STLError as e:
                s = format_text("\nError while loading profile '{0}'\n".format(input_file), 'bold')
                s += "\n" + e.brief()
                raise STLError(s)

            stream_list += profile.get_streams()


        # load streams
        cmds_json = []

        id_counter = 1

        lookup = {}

        # allocate IDs
        for stream in stream_list:
            if stream.get_id() is not None:
                stream_id = stream.get_id()
            else:
                stream_id = id_counter
                id_counter += 1

            name = stream.get_name() if stream.get_name() is not None else id(stream)
            if name in lookup:
                raise STLError("multiple streams with name: '{0}'".format(name))
            lookup[name] = stream_id

        # resolve names
        for stream in stream_list:

            name = stream.get_name() if stream.get_name() is not None else id(stream)
            stream_id = lookup[name]

            next_id = -1
            next = stream.get_next()
            if next:
                if not next in lookup:
                    raise STLError("stream dependency error - unable to find '{0}'".format(next))
                next_id = lookup[next]


            stream_json = stream.to_json()
            stream_json['next_stream_id'] = next_id

            cmd = {"id":1,
                   "jsonrpc": "2.0",
                   "method": "add_stream",
                   "params": {"handler": self.handler,
                              "port_id": self.port_id,
                              "stream_id": stream_id,
                              "stream": stream_json}
                   }

            cmds_json.append(cmd)

        # generate start command
        cmds_json.append(self.generate_start_cmd(mult = mult,
                                                 force = True,
                                                 duration = duration))

        if mode == 'json':
            print json.dumps(cmds_json, indent = 4, separators=(',', ': '), sort_keys = True)
            return
        elif mode == 'yaml':
            print STLProfile(stream_list).dump_to_yaml()
            return
        elif mode == 'pkt':
            print STLProfile(stream_list).dump_as_pkt();
            return
        elif mode == 'native':
            print STLProfile(stream_list).dump_to_code()
            return


        # start simulation
        self.outfile = outfile
        self.dp_core_count = dp_core_count
        self.dp_core_index = dp_core_index
        self.is_debug = is_debug
        self.pkt_limit = pkt_limit
        self.mult = mult
        self.duration = duration,
        self.mode = mode
        self.silent = silent

        self.__run(cmds_json)


    # internal run
    def __run (self, cmds_json):

        # write to temp file
        f = tempfile.NamedTemporaryFile(delete = False)

        msg = json.dumps(cmds_json)

        f.write(msg)
        f.close()

        # launch bp-sim
        try:
            self.execute_bp_sim(f.name)
        finally:
            os.unlink(f.name)



    def execute_bp_sim (self, json_filename):
        if self.is_debug:
            exe = os.path.join(self.bp_sim_path, 'bp-sim-64-debug')
        else:
            exe = os.path.join(self.bp_sim_path, 'bp-sim-64')

        if not os.path.exists(exe):
            raise STLError("'{0}' does not exists, please build it before calling the simulation".format(exe))
            

        cmd = [exe,
               '--pcap',
               '--sl',
               '--cores',
               str(self.dp_core_count),
               '--limit',
               str(self.pkt_limit),
               '-f',
               json_filename]

        # out or dry
        if not self.outfile:
            cmd += ['--dry']
            cmd += ['-o', '/dev/null']
        else:
            cmd += ['-o', self.outfile]

        if self.dp_core_index != None:
            cmd += ['--core_index', str(self.dp_core_index)]

        if self.mode == 'valgrind':
            cmd = ['valgrind', '--leak-check=full', '--error-exitcode=1'] + cmd

        elif self.mode == 'gdb':
            cmd = ['/usr/bin/gdb', '--args'] + cmd

        print "executing command: '{0}'".format(" ".join(cmd))

        if self.silent:
            FNULL = open(os.devnull, 'w')
            rc = subprocess.call(cmd, stdout=FNULL)
        else:
            rc = subprocess.call(cmd)

        if rc != 0:
            raise STLError('simulation has failed with error code {0}'.format(rc))

        self.merge_results()


    def merge_results (self):
        if not self.outfile:
            return

        if self.dp_core_count == 1:
            return

        if self.dp_core_index != None:
            return


        print "Mering cores output to a single pcap file...\n"
        inputs = ["{0}-{1}".format(self.outfile, index) for index in xrange(0, self.dp_core_count)]
        merge_cap_files(inputs, self.outfile, delete_src = True)




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

    parser.add_argument("-f",
                        dest ="input_file",
                        help = "input file in YAML or Python format",
                        type = is_valid_file,
                        required=True)

    parser.add_argument("-o",
                        dest = "output_file",
                        default = None,
                        help = "output file in ERF format")


    parser.add_argument("-c", "--cores",
                        help = "DP core count [default is 1]",
                        dest = "dp_core_count",
                        default = 1,
                        type = int,
                        choices = xrange(1, 9))

    parser.add_argument("-n", "--core_index",
                        help = "Record only a specific core",
                        dest = "dp_core_index",
                        default = None,
                        type = int)

    parser.add_argument("-i", "--port",
                        help = "Simulate a specific port ID [default is 0]",
                        dest = "port_id",
                        default = 0,
                        type = int)


    parser.add_argument("-r", "--release",
                        help = "runs on release image instead of debug [default is False]",
                        action = "store_true",
                        default = False)


    parser.add_argument("-s", "--silent",
                        help = "runs on silent mode (no stdout) [default is False]",
                        action = "store_true",
                        default = False)

    parser.add_argument("-l", "--limit",
                        help = "limit test total packet count [default is 5000]",
                        default = 5000,
                        type = unsigned_int)

    parser.add_argument('-m', '--multiplier',
                        help = parsing_opts.match_multiplier_help,
                        dest = 'mult',
                        default = "1",
                        type = parsing_opts.match_multiplier_strict)

    parser.add_argument('-d', '--duration',
                        help = "run duration",
                        dest = 'duration',
                        default = -1,
                        type = float)

    parser.add_argument('-p', '--path',
                        help = "BP sim path",
                        dest = 'bp_sim_path',
                        default = None,
                        type = str)


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

    group.add_argument("--pkt",
                       help = "Parse the packet and show it as hex",
                       action = "store_true",
                       default = False)

    group.add_argument("--yaml",
                       help = "generate YAML from input file [default is False]",
                       action = "store_true",
                       default = False)

    group.add_argument("--native",
                       help = "generate Python code with stateless profile from input file [default is False]",
                       action = "store_true",
                       default = False)

    return parser


def validate_args (parser, options):

    if options.dp_core_index:
        if not options.dp_core_index in xrange(0, options.dp_core_count):
            parser.error("DP core index valid range is 0 to {0}".format(options.dp_core_count - 1))

    # zero is ok - no limit, but other values must be at least as the number of cores
    if (options.limit != 0) and options.limit < options.dp_core_count:
        parser.error("limit cannot be lower than number of DP cores")


def main (args = None):
    parser = setParserOptions()
    options = parser.parse_args(args = args)

    validate_args(parser, options)

    

    if options.valgrind:
        mode = 'valgrind'
    elif options.gdb:
        mode = 'gdb'
    elif options.json:
        mode = 'json'
    elif options.yaml:
        mode = 'yaml'
    elif options.native:
        mode = 'native'
    elif options.pkt:
        mode = 'pkt'
    else:
        mode = 'none'

    try:
        r = STLSim(bp_sim_path = options.bp_sim_path, port_id = options.port_id)

        r.run(input_list = options.input_file,
              outfile = options.output_file,
              dp_core_count = options.dp_core_count,
              dp_core_index = options.dp_core_index,
              is_debug = (not options.release),
              pkt_limit = options.limit,
              mult = options.mult,
              duration = options.duration,
              mode = mode,
              silent = options.silent)

    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        return (-1)

    except STLError as e:
        print e
        return (-1)

    return (0)


if __name__ == '__main__':
    main()


