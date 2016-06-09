#!/usr/bin/python
import os
import sys
import getpass
import shutil
import multiprocessing
import logging
from collections import OrderedDict
from argparse import *
from time import time, sleep
from glob import glob
import signal

sys.path.append(os.path.join('automation', 'trex_control_plane', 'server'))
import CCustomLogger
import outer_packages
from singleton_daemon import SingletonDaemon, register_socket, run_command
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import termstyle

### Server functions ###

def check_connectivity():
    return True

def add(a, b): # for sanity checks
    return a + b

def get_trex_path():
    return args.trex_dir

def update_trex(package_path = 'http://trex-tgn.cisco.com/trex/release/latest'):
    if not args.allow_update:
        raise Exception('Updading server not allowed')
    # getting new package
    if package_path.startswith('http://'):
        file_name = package_path.split('/')[-1]
        ret_code, stdout, stderr = run_command('wget %s -O %s' % (package_path, os.path.join(tmp_dir, file_name)), timeout = 600)
    else:
        file_name = os.path.basename(package_path)
        ret_code, stdout, stderr = run_command('rsync -Lc %s %s' % (package_path, os.path.join(tmp_dir, file_name)), timeout = 300)
    if ret_code:
        raise Exception('Could not get requested package. Result: %s' % [ret_code, stdout, stderr])
    # clean old unpacked dirs
    unpacked_dirs = glob(os.path.join(tmp_dir, 'v[0-9].[0-9][0-9]'))
    for unpacked_dir in unpacked_dirs:
        shutil.rmtree(unpacked_dir)
    # unpacking
    ret_code, stdout, stderr = run_command('tar -xzf %s' % os.path.join(tmp_dir, file_name), timeout = 60, cwd = tmp_dir)
    if ret_code:
        raise Exception('Could not untar the package. %s' % [ret_code, stdout, stderr])
    unpacked_dirs = glob(os.path.join(tmp_dir, 'v[0-9].[0-9][0-9]'))
    if not len(unpacked_dirs) or len(unpacked_dirs) > 1:
        raise Exception('Should be exactly one unpacked directory, got: %s' % unpacked_dirs)
    try:
        # bu current dir
        cur_dir = args.trex_dir
        bu_dir = '%s_BU%i' % (cur_dir, int(time()))
        shutil.move(cur_dir, bu_dir)
        shutil.move(unpacked_dirs[0], cur_dir)
        # no errors, remove BU dir
        if os.path.exists(bu_dir):
            shutil.rmtree(bu_dir)
        return True
    except: # something went wrong, return backup dir
        if os.path.exists(cur_dir):
            shutil.rmtree(cur_dir)
        shutil.move(bu_dir, cur_dir)
        raise

### /Server functions ###

def fail(msg):
    print(msg)
    sys.exit(-1)


def start_master_daemon():
    if master_daemon.is_running():
        raise Exception('Master daemon is already running')
    proc = multiprocessing.Process(target = start_master_daemon_func)
    proc.daemon = True
    proc.start()
    for i in range(50):
        if master_daemon.is_running():
            print(termstyle.green('Master daemon is started.'))
            os._exit(0)
        sleep(0.1)
    fail(termstyle.red('Master daemon failed to run. Please look in log: %s' % logging_file))

def set_logger():
    log_dir = os.path.dirname(logging_file)
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    if os.path.exists(logging_file):
        if os.path.exists(logging_file_bu):
            os.unlink(logging_file_bu)
        os.rename(logging_file, logging_file_bu)
    CCustomLogger.setup_daemon_logger('Master daemon', logging_file)

def start_master_daemon_func():
    try:
        set_logger()
        register_socket(master_daemon.tag)
        server = SimpleJSONRPCServer(('0.0.0.0', master_daemon.port))
        logging.info('Started master daemon (port %s)' % master_daemon.port)
        server.register_function(add)
        server.register_function(check_connectivity)
        server.register_function(get_trex_path)
        server.register_function(update_trex)
        # trex_daemon_server
        server.register_function(trex_daemon_server.is_running, 'is_trex_daemon_running')
        server.register_function(trex_daemon_server.restart, 'restart_trex_daemon')
        server.register_function(trex_daemon_server.start, 'start_trex_daemon')
        server.register_function(trex_daemon_server.stop, 'stop_trex_daemon')
        # stl rpc proxy
        server.register_function(stl_rpc_proxy.is_running, 'is_stl_rpc_proxy_running')
        server.register_function(stl_rpc_proxy.restart, 'restart_stl_rpc_proxy')
        server.register_function(stl_rpc_proxy.start, 'start_stl_rpc_proxy')
        server.register_function(stl_rpc_proxy.stop, 'stop_stl_rpc_proxy')
        server.register_function(server.funcs.keys, 'get_methods') # should be last
        signal.signal(signal.SIGTSTP, stop_handler)
        signal.signal(signal.SIGTERM, stop_handler)
        server.serve_forever()
    except Exception as e:
        logging.error('Closing due to error: %s' % e)

def stop_handler(*args, **kwargs):
    logging.info('Got killed explicitly.')
    sys.exit(0)

# returns True if given path is under current dir or /tmp
def _check_path_under_current_or_temp(path):
    if not os.path.relpath(path, '/tmp').startswith(os.pardir):
        return True
    if not os.path.relpath(path, os.getcwd()).startswith(os.pardir):
        return True
    return False


### Main ###

if getpass.getuser() != 'root':
    fail('Please run this program as root/with sudo')

daemon_actions = OrderedDict([('start', 'start the daemon'),
                              ('stop', 'exit the daemon process'),
                              ('show', 'prompt the status of daemon process (running / not running)'),
                              ('restart', 'stop, then start again the daemon process')])

actions_help = 'Specify action command to be applied on master daemon.\n' +\
               '\n'.join(['    (*) %-11s: %s' % (key, val) for key, val in daemon_actions.items()])

daemons = {}.fromkeys(['master_daemon', 'trex_daemon_server', 'stl_rpc_proxy'])

# show -p --master_port METAVAR instead of -p METAVAR --master_port METAVAR
class MyFormatter(RawTextHelpFormatter):
    def _format_action_invocation(self, action):
        if not action.option_strings or action.nargs == 0:
            return super(MyFormatter, self)._format_action_invocation(action)
        default = action.dest.upper()
        args_string = self._format_args(action, default)
        return ', '.join(action.option_strings) + ' ' + args_string

parser = ArgumentParser(description = 'Runs master daemon that can start/stop TRex daemon or update TRex version.',
                        formatter_class = MyFormatter)
parser.add_argument('-p', '--master-port', type=int, default = 8091, dest='master_port',
                    help = 'Select port to which the Master daemon will listen.\nDefault is 8091.', action = 'store')
parser.add_argument('--trex-daemon-port', type=int, default = 8090, dest='trex_daemon_port',
                    help = 'Select port to which the TRex daemon server will listen.\nDefault is 8090.', action = 'store')
parser.add_argument('--stl-rpc-proxy-port', type=int, default = 8095, dest='stl_rpc_proxy_port',
                    help = 'Select port to which the Stateless RPC proxy will listen.\nDefault is 8095.', action = 'store')
parser.add_argument('-d', '--trex-dir', type=str, default = os.getcwd(), dest='trex_dir',
                    help = 'Path of TRex, default is current dir', action = 'store')
parser.add_argument('--allow-update', default = False, dest='allow_update', action = 'store_true',
                    help = "Allow update of TRex via RPC command. WARNING: It's security hole! Use on your risk!")
parser.add_argument('action', choices = daemon_actions,
                    action = 'store', help = actions_help)
parser.add_argument('--type', '--daemon-type', '--daemon_type', choices = daemons.keys(), dest = 'daemon_type',
                    action = 'store', help = 'Specify daemon type to start/stop etc.\nDefault is master_daemon.')

args = parser.parse_args()
args.trex_dir = os.path.normpath(args.trex_dir)
args.daemon_type = args.daemon_type or 'master_daemon'

stl_rpc_proxy_dir  = os.path.join(args.trex_dir, 'automation', 'trex_control_plane', 'stl', 'examples')
stl_rpc_proxy      = SingletonDaemon('Stateless RPC proxy', 'trex_stl_rpc_proxy', args.stl_rpc_proxy_port, './rpc_proxy_server.py', stl_rpc_proxy_dir)
trex_daemon_server = SingletonDaemon('TRex daemon server', 'trex_daemon_server', args.trex_daemon_port, './trex_daemon_server start', args.trex_dir)
master_daemon      = SingletonDaemon('Master daemon', 'trex_master_daemon', args.master_port, start_master_daemon) # add ourself for easier check if running, kill etc.

tmp_dir = '/tmp/trex-tmp'
logging_file = '/var/log/trex/master_daemon.log'
logging_file_bu = '/var/log/trex/master_daemon.log_bu'

if not _check_path_under_current_or_temp(args.trex_dir):
    raise Exception('Only allowed to use path under /tmp or current directory')
if os.path.isfile(args.trex_dir):
    raise Exception('Given path is a file')
if not os.path.exists(args.trex_dir):
    print('Path %s does not exist, creating new assuming TRex will be unpacked there.' % args.trex_dir)
    os.makedirs(args.trex_dir)
    os.chmod(args.trex_dir, 0o777)
elif args.allow_update:
    print('Due to allow updates flag, setting mode 777 on given directory')
    os.chmod(args.trex_dir, 0o777)

if not os.path.exists(tmp_dir):
    os.makedirs(tmp_dir)

if args.daemon_type not in daemons.keys(): # not supposed to happen
    raise Exception('Error in daemon type , should be one of following: %s' % daemon.keys())
daemon = vars().get(args.daemon_type)
if not daemon:
    raise Exception('Daemon %s does not exist' % args.daemon_type)

if args.action != 'show':
    func = getattr(daemon, args.action)
    if not func:
        raise Exception('%s does not have function %s' % (daemon.name, args.action))
    try:
        func()
    except Exception as e:
        print(termstyle.red(e))
        sys.exit(1)

# prints running status
if daemon.is_running():
    print(termstyle.green('%s is running' % daemon.name))
else:
    print(termstyle.red('%s is NOT running' % daemon.name))

