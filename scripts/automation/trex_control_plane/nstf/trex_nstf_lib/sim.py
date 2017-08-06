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
from pprint import pprint

DEFAULT_OUT_JSON_FILE = "/tmp/nstf.json"


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

    cmd = [exe, '--tcp_cfg', DEFAULT_OUT_JSON_FILE, '-o', opts.output_file]

    if opts.verbose:
        print ("executing {0}".format(cmd))

    if opts.verbose:
        rc = subprocess.call(cmd)
    else:
        FNULL = open(os.devnull, 'wb')
        rc = subprocess.call(cmd, stdout=FNULL)
        if rc != 0:
            raise Exception('simulation has failed with error code {0}'.format(rc))


def setParserOptions():
    parser = argparse.ArgumentParser(prog="nstf_sim.py")

    parser.add_argument("-f",
                        dest="input_file",
                        help="New statefull profile file",
                        required=True)

    DEFAULT_PCAP_FILE_NAME = "nstf_pcap"
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


def process_options():
    parser = setParserOptions()
    return parser.parse_args()


def main(args=None):
    opts = process_options()

    basedir = os.path.dirname(opts.input_file)
    sys.path.insert(0, basedir)

    try:
        file = os.path.basename(opts.input_file).split('.')[0]
        prof = __import__(file, globals(), locals(), [], 0)
    except ImportError as e:
        print("Failed importing {0}".format(opts.input_file))
        print(e)
        sys.exit(1)

    cl = prof.register()

    profile = cl.get_profile()

    if opts.json:
        pprint(profile.to_json())
        sys.exit(0)

    f = open(DEFAULT_OUT_JSON_FILE, 'w')
    f.write(str(profile.to_json()).replace("'", "\""))
    f.close()

    execute_bp_sim(opts)

if __name__ == '__main__':
    main()
