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
import re
import tempfile
from .trex_astf_profile import *
from .cap_handling import CPcapFixTime, is_udp_pcap, pcap_cut_udp


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


def decode_tunables (tunable_str):
    tunables = {}

    # split by comma to tokens
    tokens = tunable_str.split(',')

    # each token is of form X=Y
    for token in tokens:
        m = re.search('(\S+)=(.+)', token)
        if not m:
            raise argparse.ArgumentTypeError("bad syntax for tunables: {0}".format(token))
        val = m.group(2)           # string
        if val.startswith(("'", '"')) and val.endswith(("'", '"')) and len(val) > 1: # need to remove the quotes from value
            val = val[1:-1]
        elif val.startswith('0x'): # hex
            val = int(val, 16)
        else:
            try:
                if '.' in val:     # float
                    val = float(val)
                else:              # int
                    val = int(val)
            except:
                pass
        tunables[m.group(1)] = val

    return tunables



def execute_bp_sim(opts):
    if opts.release:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64')
    else:
        exe = os.path.join(opts.bp_sim_path, 'bp-sim-64-debug')

        if not os.path.exists(exe):
            raise Exception("'{0}' does not exist, please build it before calling the simulation".format(exe))

    if opts.cmd:
        args = opts.cmd.split(",")
        #args = list(map(lambda x: "--"+x, args)) 
    else:
        args = []

    exe = [exe]
    if opts.valgrind:
        valgrind_str = get_valgrind() +' --leak-check=full --error-exitcode=1 --show-reachable=yes '
        valgrind = valgrind_str.split();
        exe = valgrind + exe

    if opts.emul_debug:
         exe += ["--astf-emul-debug"]

    if opts.pcap:
        exe += ["--pcap"]

    cmd = exe + ['--tcp_cfg', DEFAULT_OUT_JSON_FILE, '-o', opts.output_file]+args

    if opts.full:
        cmd = cmd + ['--full', '-d', str(opts.duration)]

    if opts.input_client_file:
        cmd = cmd + ['--client-cfg', str(opts.input_client_file)]

    if opts.verbose:
        print ("executing {0}".format(' '.join(cmd)))

    with tempfile.TemporaryFile('w+') as out:
        rc = subprocess.call(' '.join(cmd), stdout = out, stderr = subprocess.STDOUT, shell = True)

        out.seek(0)
        output = out.read()
        if rc != 0:
            raise Exception('Simulation has failed with error code %s\nOutput: %s' % (rc, output))
        if opts.verbose:
            print(output)


# when parsing paths, return an absolute path (for chdir)
def parse_path(p):
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
                        required=True,
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

    parser.add_argument('-e', '--emul-debug',
                        help="emulation debug",
                        action="store_true")

    parser.add_argument('-v', '--verbose',
                        action="store_true",
                        help="Print output to screen")

    parser.add_argument('--dev',
                        action="store_true",
                        help="Deveoper mode")

    parser.add_argument('--full',
                        action="store_true",
                        help="run in full simulation mode (with many clients and servers)")

    parser.add_argument('-d', '--duration',
                        type=float,
                        default=5.0,
                        help="duration in time for full mode")

    parser.add_argument("-c", "--cmd",
                        help="command to the simulator for example '--shaper-rate=12,--rtt=1' => spliter is ',' ",
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

    parser.add_argument('-t',
                        help = 'sets tunable for a profile',
                        dest = 'tunables',
                        default = None,
                        type = decode_tunables)

    fix_pcap = parser.add_argument_group(title = 'Processing input pcap (-f option is pcap, most of other options will be ignored)')

    fix_pcap.add_argument('--rtt',
                          help = 'Simulate network latency (msec). Recommended to use at least 5msec. Supported only for TCP',
                          type = int)

    fix_pcap.add_argument('--fix-timing',
                          help = 'Changes times as if the capture was done in intermediate device. Supported only for TCP',
                          action = 'store_true')

    fix_pcap.add_argument('--mss',
                          help = 'Size of data. TCP will be fragmented, UDP will be trimmed',
                          type = int)

    return parser


def profile_from_pcap(pcap, mss):
    ip_gen_c = ASTFIPGenDist(ip_range = ['16.0.0.0', '16.0.0.255'], distribution = 'seq')
    ip_gen_s = ASTFIPGenDist(ip_range = ['48.0.0.0', '48.0.255.255'], distribution = 'seq')
    ip_gen = ASTFIPGen(glob = ASTFIPGenGlobal(ip_offset = '1.0.0.0'),
                    dist_client = ip_gen_c,
                    dist_server = ip_gen_s)

    c_glob_info = ASTFGlobalInfo()
    if mss is not None:
        c_glob_info.tcp.mss = mss

    return ASTFProfile(default_ip_gen = ip_gen,
                       default_c_glob_info = c_glob_info,
                       default_s_glob_info = c_glob_info,
                       cap_list = [ASTFCapInfo(file = pcap,
                                               cps = 1)])

def fatal(msg):
    print(msg)
    sys.exit(1)

def main(args=None):

    parser = setParserOptions()
    if args is None:
        opts = parser.parse_args()
    else:
        opts = parser.parse_args(args)

    tun=opts.tunables if opts.tunables else {};

    profile = None
    if opts.rtt or opts.fix_timing or opts.mss:
        if opts.rtt is not None and opts.rtt <= 0:
            fatal('ERROR: RTT must be positive')
        if opts.mss is not None:
            if opts.mss <= 0:
                fatal('ERROR: MSS must be positive')
            elif opts.mss < 256:
                print('WARNING: MSS is too small - %s, continuing...' % opts.mss)

        if is_udp_pcap(opts.input_file):
            if opts.rtt or opts.fix_timing:
                fatal('Fix timing and/or rtt are supported only with TCP')
            pcap_cut_udp(opts.mss, opts.input_file, opts.output_file, verbose = opts.verbose)
            return
        else:
            if not opts.fix_timing:
                print('WARNING: Enabling fix timing implicitly')
                opts.fix_timing = True

            profile = profile_from_pcap(opts.input_file, opts.mss)
            opts.pcap = True
            if not opts.rtt:
                input_pcap = CPcapFixTime(opts.input_file)
                opts.rtt = input_pcap.calc_rtt() * 1000
            opts.cmd = '"--rtt=%s"' % (opts.rtt * 1000)

    else:
        if opts.dev:
            profile = ASTFProfile.load(opts.input_file, **tun);
        else:
            try:
                profile = ASTFProfile.load(opts.input_file, **tun);
            except Exception as e:
                fatal(e)

        if opts.json:
            print(profile.to_json_str())
            return

        if opts.stat:
            profile.print_stats()
            return

    f = open(DEFAULT_OUT_JSON_FILE, 'w')
    f.write(str(profile.to_json_str()).replace("'", "\""))
    f.close()

    # if the path is not the same - handle the switch
    if os.path.normpath(opts.bp_sim_path) == os.path.normpath(os.getcwd()):
        execute_inplace(opts)
    else:
        execute_with_chdir(opts)

    if opts.fix_timing:
        proc_file = opts.output_file
        if not opts.full:
            proc_file += '_c.pcap'
        try:
            pcap = CPcapFixTime(proc_file)
            pcap.fix_timing(opts.output_file)
        except Exception as e:
            fatal('Could not fix timing: %s ' % e)


def execute_inplace (opts):
    try:
        execute_bp_sim(opts)
    except Exception as e:
        fatal(e)
        
        
def execute_with_chdir (opts):
    
    
    cwd = os.getcwd()
    
    try:
        os.chdir(opts.bp_sim_path)
        execute_bp_sim(opts)
    except TypeError as e:
        fatal(e)
        
    finally:
        os.chdir(cwd)


if __name__ == '__main__':
    main()

