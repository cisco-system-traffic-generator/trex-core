# -*- coding: utf-8 -*-

"""
Copyright (c) 2017-2017 Cisco Systems, Inc.
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
import argparse
import os
import sys
import subprocess

DEFAULT_OUT_JSON_FILE = "/tmp/astf.json"


def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename


def unsigned_int(x):
    x = int(x)
    if x < 0:
        raise argparse.ArgumentTypeError("argument must be >= 0")

    return x


def execute_bp_sim(opts):
    if opts.release:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64')
    else:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64-debug')

        if not os.path.exists(exe):
            raise Exception("'{0}' does not exist, please build it before calling the simulation".format(exe))

    exe = [exe]
    if opts.valgrind:
        valgrind = 'valgrind --leak-check=full --error-exitcode=1 --show-reachable=yes '.split()
        exe = valgrind + exe

    cmd = exe + ['--tcp_cfg', DEFAULT_OUT_JSON_FILE, '-o', opts.output_file]

    if opts.verbose:
        print ("executing {0}".format(''.join(cmd)))

    if opts.verbose:
        rc = subprocess.call(cmd)
    else:
        FNULL = open(os.devnull, 'wb')
        rc = subprocess.call(cmd, stdout=FNULL)
        if rc != 0:
            raise Exception('simulation has failed with error code {0}'.format(rc))


def print_stats(prof):
    # num programs, buffers, cps, bps client/server ip range
    prof.print_stats()

def setParserOptions():
    parser = argparse.ArgumentParser(prog="astf_sim.py")

    parser.add_argument("-f",
                        dest="input_file",
                        help="New statefull profile file",
                        required=True)

    DEFAULT_PCAP_FILE_NAME = "astf_pcap"
    parser.add_argument("-o",
                        dest="output_file",
                        default=DEFAULT_PCAP_FILE_NAME,
                        help="File to which pcap output will be written. Default is {0}".format(DEFAULT_PCAP_FILE_NAME))

    parser.add_argument('-p', '--path',
                        help="BP sim path",
                        dest='bp_sim_path',
                        default=None,
                        type=str)

    parser.add_argument("-r", "--release",
                        help="runs on release image instead of debug [default is False]",
                        action="store_true",
                        default=False)

    parser.add_argument('-s', '--sim',
                        help="Run simulator with json result",
                        action="store_true")

    parser.add_argument('--stat',
                        help="Print expected usage statistics on TRex server (memory, bps,...) if this file will be used.",
                        action="store_true")

    parser.add_argument('-v', '--verbose',
                        action="store_true",
                        help="Print output to screen")

    group = parser.add_mutually_exclusive_group()

    group.add_argument("-g", "--gdb",
                       help="run under GDB [default is False]",
                       action="store_true",
                       default=False)

    group.add_argument("--json",
                       help="Print JSON output to stdout and exit",
                       action="store_true",
                       default=False)

    group.add_argument("-x", "--valgrind",
                       help="run under valgrind [default is False]",
                       action="store_true",
                       default=False)

    return parser


def main(args=None):
    parser = setParserOptions()
    if args is None:
        opts = parser.parse_args()
    else:
        opts = parser.parse_args(args)

    basedir = os.path.dirname(opts.input_file)
    sys.path.insert(0, basedir)

    try:
        file = os.path.basename(opts.input_file).split('.')[0]
        prof = __import__(file, globals(), locals(), [], 0)
    except Exception as e:
        print("Failed importing {0}".format(opts.input_file))
        print(e)
        sys.exit(1)

    cl = prof.register()

    try:
        profile = cl.get_profile()
    except Exception as e:
        print (e)
        sys.exit(1)

    if opts.json:
        print(profile.to_json())
        sys.exit(0)

    if opts.stat:
        print_stats(profile)
        sys.exit(0)

    f = open(DEFAULT_OUT_JSON_FILE, 'w')
    f.write(str(profile.to_json()).replace("'", "\""))
    f.close()

    try:
        execute_bp_sim(opts)
    except Exception as e:
        print (e)
        sys.exit(1)

if __name__ == '__main__':
    main()
