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


def get_valgrind():
    valgrind_loc = os.environ.get('VALGRIND_LOC')
    if not valgrind_loc:
        return("valgrind");

    os.environ['VALGRIND_LIB']=valgrind_loc+"/lib/valgrind"
    valgrind_exe=valgrind_loc+"/bin/valgrind";
    os.environ['VALGRIND_EXE']=valgrind_exe
    return(valgrind_exe);




def execute_bp_sim(opts):
    if opts.release:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64')
    else:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64-debug')

        if not os.path.exists(exe):
            raise Exception("'{0}' does not exist, please build it before calling the simulation".format(exe))

    if opts.cmd:
        args = opts.cmd.split(",")
    else:
        args = []

    exe = [exe]
    if opts.valgrind:
        valgrind_str = get_valgrind() +' --leak-check=full --error-exitcode=1 --show-reachable=yes '
        valgrind = valgrind_str.split();
        exe = valgrind + exe

    if opts.pcap:
        exe += ["--pcap"]

    cmd = exe + ['--tcp_cfg', DEFAULT_OUT_JSON_FILE, '-o', opts.output_file]+args

    if opts.full:
        cmd = cmd + ['--full', '-d', str(opts.duration)]

    if opts.input_client_file:
        cmd = cmd + ['--client-cfg', str(opts.input_client_file)]

    if opts.verbose:
        print ("executing {0}".format(' '.join(cmd)))

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


# when parsing paths, return an absolute path (for chdir)
def parse_path (p):
    return os.path.abspath(p)
    
    
def setParserOptions():
    parser = argparse.ArgumentParser(prog="astf_sim.py")

    parser.add_argument("-f",
                        dest="input_file",
                        help="New statefull profile file",
                        type=parse_path,
                        required=True)

    DEFAULT_PCAP_FILE_NAME = "astf_pcap"
    parser.add_argument("-o",
                        dest="output_file",
                        default=DEFAULT_PCAP_FILE_NAME,
                        type=parse_path,
                        help="File to which pcap output will be written. Default is {0}".format(DEFAULT_PCAP_FILE_NAME))

    parser.add_argument('-p', '--path',
                        help="BP sim path",
                        dest='bp_sim_path',
                        default=None,
                        type=parse_path)

    parser.add_argument("--cc",
                        dest="input_client_file",
                        default=None,
                        help="input client cluster file YAML",
                        type=parse_path,
                        required=False)

    parser.add_argument("--pcap",
                        help="Create output in pcap format (if not specified, will be in erf)",
                        action="store_true",
                        default=False)

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

    parser.add_argument('--full',
                        action="store_true",
                        help="run in full simulation mode (with many clients and servers)")

    parser.add_argument('-d', '--duration',
                        type=float,
                        default=5.0,
                        help="duration in time for full mode")

    parser.add_argument("-c", "--cmd",
                        help="command to the simulator",
                        dest='cmd',
                        default=None,
                        type=str)

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
        sys.exit(100)

    cl = prof.register()
    try:
        profile = cl.get_profile()
    except Exception as e:
        print (e)
        sys.exit(100)

    if opts.json:
        print(profile.to_json())
        sys.exit(0)

    if opts.stat:
        print_stats(profile)
        sys.exit(0)

    f = open(DEFAULT_OUT_JSON_FILE, 'w')
    f.write(str(profile.to_json()).replace("'", "\""))
    f.close()
    
    # if the path is not the same - handle the switch
    if os.path.normpath(opts.bp_sim_path) == os.path.normpath(os.getcwd()):
        execute_inplace(opts)
    else:
        execute_with_chdir(opts)
        
        
def execute_inplace (opts):
    try:
        execute_bp_sim(opts)
    except Exception as e:
        print(e)
        sys.exit(1)
        
        
def execute_with_chdir (opts):
    
    
    cwd = os.getcwd()
    
    try:
        os.chdir(opts.bp_sim_path)
        execute_bp_sim(opts)
    except TypeError as e:
        print (e)
        sys.exit(1)
        
    finally:
        os.chdir(cwd)

        
if __name__ == '__main__':
    main()
