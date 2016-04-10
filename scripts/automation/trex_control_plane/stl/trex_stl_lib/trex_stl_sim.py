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
from . import trex_stl_ext
from .trex_stl_exceptions import *
from .trex_stl_streams import *
from .utils import parsing_opts
from .trex_stl_client import STLClient
from .utils import pcap
from trex_stl_lib.trex_stl_packet_builder_scapy import RawPcapReader, RawPcapWriter, hexdump

from yaml import YAMLError

import re
import json
import argparse
import tempfile
import subprocess
import os
from operator import itemgetter

class BpSimException(Exception):
    pass


# stateless simulation
class STLSim(object):
    def __init__ (self, bp_sim_path = None, handler = 0, port_id = 0, api_h = "dummy"):

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
        self.api_h   = api_h
        self.port_id = port_id


    def generate_start_cmd (self, mult = "1", force = True, duration = -1):
        return  {"id":1,
                 "jsonrpc": "2.0",
                 "method": "start_traffic",
                 "params": {"handler": self.handler,
                            "api_h" : self.api_h,
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
             silent = False,
             tunables = None):

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
        if tunables == None:
            tunables = {}
        else:
            tunables = tunables[0]

        for input_file in input_files:
            try:
                if not 'direction' in tunables:
                    tunables['direction'] = self.port_id % 2

                profile = STLProfile.load(input_file, **tunables)

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
                              "api_h": self.api_h,
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
            print(json.dumps(cmds_json, indent = 4, separators=(',', ': '), sort_keys = True))
            return
        elif mode == 'yaml':
            print(STLProfile(stream_list).dump_to_yaml())
            return
        elif mode == 'pkt':
            print(STLProfile(stream_list).dump_as_pkt())
            return
        elif mode == 'native':
            print(STLProfile(stream_list).dump_to_code())
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

        msg = json.dumps(cmds_json).encode()

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

        print("executing command: '{0}'".format(" ".join(cmd)))

        if self.silent:
            FNULL = open(os.devnull, 'wb')
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


        if not self.silent:
            print("Mering cores output to a single pcap file...\n")
        inputs = ["{0}-{1}".format(self.outfile, index) for index in range(0, self.dp_core_count)]
        pcap.merge_cap_files(inputs, self.outfile, delete_src = True)



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
                        choices = list(range(1, 9)))

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


    parser.add_argument('-t',
                        help = 'sets tunable for a profile',
                        dest = 'tunables',
                        default = None,
                        type = parsing_opts.decode_tunables)

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

    group.add_argument("--test_multi_core",
                       help = "runs the profile with c=1-8",
                       action = "store_true",
                       default = False)

    return parser


def validate_args (parser, options):

    if options.dp_core_index:
        if not options.dp_core_index in range(0, options.dp_core_count):
            parser.error("DP core index valid range is 0 to {0}".format(options.dp_core_count - 1))

    # zero is ok - no limit, but other values must be at least as the number of cores
    if (options.limit != 0) and options.limit < options.dp_core_count:
        parser.error("limit cannot be lower than number of DP cores")


# a more flexible check
def compare_caps (cap1, cap2, max_diff_sec = (5 * 1e-6)):
    pkts1 = list(RawPcapReader(cap1))
    pkts2 = list(RawPcapReader(cap2))

    if len(pkts1) != len(pkts2):
        print('{0} contains {1} packets vs. {1} contains {2} packets'.format(cap1, len(pkts1), cap2, len(pkts2)))
        return False

    # to be less strict we define equality if all packets from cap1 exists and in cap2
    # and vice versa
    # 'exists' means the same packet with abs(TS1-TS2) < 5nsec
    # its O(n^2) but who cares, right ?
    for i, pkt1 in enumerate(pkts1):
        ts1 = float(pkt1[1][0]) + (float(pkt1[1][1]) / 1e6)
        found = None
        for j, pkt2 in enumerate(pkts2):
            ts2 = float(pkt2[1][0]) + (float(pkt2[1][1]) / 1e6)
            
            if abs(ts1-ts2) > max_diff_sec:
                break

            if pkt1[0] == pkt2[0]:
                found = j
                break

        
        if found is None:
            print(format_text("cannot find packet #{0} from {1} in {2}\n".format(i, cap1, cap2), 'bold'))
            return False
        else:
            del pkts2[found]

    return True


  

# a more strict comparsion 1 <--> 1
def compare_caps_strict (cap1, cap2, max_diff_sec = (5 * 1e-6)):
    pkts1 = list(RawPcapReader(cap1))
    pkts2 = list(RawPcapReader(cap2))

    if len(pkts1) != len(pkts2):
        print('{0} contains {1} packets vs. {1} contains {2} packets'.format(cap1, len(pkts1), cap2, len(pkts2)))
        return False

    # a strict check
    for pkt1, pkt2, i in zip(pkts1, pkts2, range(1, len(pkts1))):
        ts1 = float(pkt1[1][0]) + (float(pkt1[1][1]) / 1e6)
        ts2 = float(pkt2[1][0]) + (float(pkt2[1][1]) / 1e6)

        if abs(ts1-ts2) > 0.000005: # 5 nsec
            print(format_text("TS error: cap files '{0}', '{1}' differ in cap #{2} - '{3}' vs. '{4}'\n".format(cap1, cap2, i, ts1, ts2), 'bold'))
            return False

        if pkt1[0] != pkt2[0]:
            print(format_text("RAW error: cap files '{0}', '{1}' differ in cap #{2}\n".format(cap1, cap2, i), 'bold'))
            print(hexdump(pkt1[0]))
            print("")
            print(hexdump(pkt2[0]))
            print("")
            return False

    return True

#
def test_multi_core (r, options):

    for core_count in [1, 2, 4, 6, 8]:
        r.run(input_list = options.input_file,
              outfile = '{0}.cap'.format(core_count),
              dp_core_count = core_count,
              is_debug = (not options.release),
              pkt_limit = options.limit,
              mult = options.mult,
              duration = options.duration,
              mode = 'none',
              silent = True,
              tunables = options.tunables)

    print("")

    print(format_text("comparing 2 cores to 1 core:\n", 'underline'))
    rc = compare_caps('1.cap', '2.cap')
    if rc:
        print("[Passed]\n")

    print(format_text("comparing 4 cores to 1 core:\n", 'underline'))
    rc = compare_caps('1.cap', '4.cap')
    if rc:
        print("[Passed]\n")

    print(format_text("comparing 6 cores to 1 core:\n", 'underline'))
    rc = compare_caps('1.cap', '6.cap')
    if rc:
        print("[Passed]\n")

    print(format_text("comparing 8 cores to 1 core:\n", 'underline'))
    rc = compare_caps('1.cap', '8.cap')
    if rc:
        print("[Passed]\n")


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
    elif options.test_multi_core:
        mode = 'test_multi_core'
    else:
        mode = 'none'

    try:
        r = STLSim(bp_sim_path = options.bp_sim_path, port_id = options.port_id)

        if mode == 'test_multi_core':
            test_multi_core(r, options)
        else:
            r.run(input_list = options.input_file,
                  outfile = options.output_file,
                  dp_core_count = options.dp_core_count,
                  dp_core_index = options.dp_core_index,
                  is_debug = (not options.release),
                  pkt_limit = options.limit,
                  mult = options.mult,
                  duration = options.duration,
                  mode = mode,
                  silent = options.silent,
                  tunables = options.tunables)

    except KeyboardInterrupt as e:
        print("\n\n*** Caught Ctrl + C... Exiting...\n\n")
        return (-1)

    except STLError as e:
        print(e)
        return (-1)

    return (0)


if __name__ == '__main__':
    main()


