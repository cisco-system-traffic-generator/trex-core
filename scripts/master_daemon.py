#! /bin/bash
"source" "find_python.sh" "--local"
"exec" "$PYTHON" "$0" "$@"

import os
import sys
import shutil
import threading
import logging
import time
from collections import OrderedDict
from argparse import *
from glob import glob
import signal
from functools import partial

sys.path.append(os.path.join('automation', 'trex_control_plane', 'server'))
import CCustomLogger
import outer_packages
from tcp_daemon import TCPDaemon, run_command
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import termstyle

### Server functions ###

def check_connectivity():
    return True

def add(a, b): # for sanity checks
    return a + b

def get_trex_path():
    return args.trex_dir

def get_package_path():
    return CUpdate.info.get('path')

def get_package_sha1():
    return CUpdate.info.get('sha1')

def is_updating():
    return CUpdate.thread and CUpdate.thread.is_alive()

def _update_trex_process(package_path):
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)
    file_name = 'trex_package.tar.gz'

    # getting new package
    if package_path.startswith('http'):
        ret_code, stdout, stderr = run_command('wget %s -O %s' % (package_path, os.path.join(tmp_dir, file_name)), timeout = 600)
    else:
        ret_code, stdout, stderr = run_command('rsync -Lc %s %s' % (package_path, os.path.join(tmp_dir, file_name)), timeout = 300)
    if ret_code:
        raise Exception('Could not get requested package. Result: %s' % [ret_code, stdout, stderr])

    # calculating hash
    ret_code, stdout, stderr = run_command('sha1sum -b %s' % os.path.join(tmp_dir, file_name), timeout = 30)
    if ret_code:
        raise Exception('Could not calculate hash of package. Result: %s' % [ret_code, stdout, stderr])
    package_sha1 = stdout.strip().split()[0]

    # clean old unpacked dirs
    tmp_files = glob(os.path.join(tmp_dir, '*'))
    for tmp_file in tmp_files:
        if os.path.isdir(tmp_file) and not os.path.islink(tmp_file):
            shutil.rmtree(tmp_file)

    # unpacking
    ret_code, stdout, stderr = run_command('tar -xzf %s' % os.path.join(tmp_dir, file_name), timeout = 120, cwd = tmp_dir)
    if ret_code:
        raise Exception('Could not untar the package. %s' % [ret_code, stdout, stderr])
    tmp_files = glob(os.path.join(tmp_dir, '*'))
    unpacked_dirs = []
    for tmp_file in tmp_files:
        if os.path.isdir(tmp_file) and not os.path.islink(tmp_file):
            unpacked_dirs.append(tmp_file)
    if len(unpacked_dirs) != 1:
        raise Exception('Should be exactly one unpacked directory, got: %s' % unpacked_dirs)
    os.chmod(unpacked_dirs[0], 0o777) # allow core dumps to be written
    cur_dir = args.trex_dir
    if os.path.islink(cur_dir) or os.path.isfile(cur_dir):
        os.unlink(cur_dir)
    if not os.path.exists(cur_dir):
        os.makedirs(cur_dir)
        os.chmod(cur_dir, 0o777)
    bu_dir = '%s_BU%i' % (cur_dir, int(time.time()))
    try:
        # bu current dir
        shutil.move(cur_dir, bu_dir)
        shutil.move(unpacked_dirs[0], cur_dir)
        CUpdate.info = {'path': package_path, 'sha1': package_sha1}
        logging.info('Done updating, success')
    except BaseException as e: # something went wrong, return backup dir
        logging.error('Error while updating: %s' % e)
        if os.path.exists(cur_dir):
            shutil.rmtree(cur_dir)
        shutil.move(bu_dir, cur_dir)
        raise
    finally:
        if os.path.exists(bu_dir):
            shutil.rmtree(bu_dir)

# non blocking update
def update_trex(package_path = 'http://trex-tgn.cisco.com/trex/release/latest'):
    if not args.allow_update:
        raise Exception('Updating server not allowed')
    if CUpdate.thread and CUpdate.thread.is_alive():
        CUpdate.thread.terminate()
    CUpdate.thread = threading.Thread(target = _update_trex_process, args = [package_path])
    CUpdate.thread.daemon = True
    CUpdate.thread.start()


def save_coredump():
    latest_core_file = {
        'time': 0,
        'path': None}
    for core_file in glob(os.path.join(args.trex_dir, 'core*')):
        mod_time = os.path.getmtime(core_file)
        if latest_core_file['time'] < mod_time:
            latest_core_file['time'] = mod_time
            latest_core_file['path'] = core_file
    if latest_core_file['path']:
        shutil.copy(latest_core_file['path'], os.path.join(tmp_dir, 'coredump'))

### /Server functions ###

def fail(msg):
    print(msg)
    sys.exit(-1)


def set_logger():
    log_dir = os.path.dirname(logging_file)
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    if os.path.exists(logging_file):
        if os.path.exists(logging_file_bu):
            os.unlink(logging_file_bu)
        os.rename(logging_file, logging_file_bu)
    CCustomLogger.setup_daemon_logger('Master daemon', logging_file)

def log_usage(name, func, *args, **kwargs):
    log_string = name
    if args:
        log_string += ', args: ' + repr(args)
    if kwargs:
        log_string += ', kwargs: ' + repr(kwargs)
    logging.info(log_string)
    return func(*args, **kwargs)

def start_master_daemon():
    funcs_by_name = {}
    # master_daemon functions
    funcs_by_name['add'] = add
    funcs_by_name['check_connectivity'] = check_connectivity
    funcs_by_name['get_trex_path'] = get_trex_path
    funcs_by_name['get_package_path'] = get_package_path
    funcs_by_name['get_package_sha1'] = get_package_sha1
    funcs_by_name['is_updating'] = is_updating
    funcs_by_name['update_trex'] = update_trex
    funcs_by_name['save_coredump'] = save_coredump
    # trex_daemon_server
    funcs_by_name['is_trex_daemon_running'] = trex_daemon_server.is_running
    funcs_by_name['restart_trex_daemon'] = trex_daemon_server.restart
    funcs_by_name['start_trex_daemon'] = trex_daemon_server.start
    funcs_by_name['stop_trex_daemon'] = trex_daemon_server.stop
    # stl rpc proxy
    funcs_by_name['is_stl_rpc_proxy_running'] = stl_rpc_proxy.is_running
    funcs_by_name['restart_stl_rpc_proxy'] = stl_rpc_proxy.restart
    funcs_by_name['start_stl_rpc_proxy'] = stl_rpc_proxy.start
    funcs_by_name['stop_stl_rpc_proxy'] = stl_rpc_proxy.stop
    try:
        set_logger()
        server = SimpleJSONRPCServer(('0.0.0.0', master_daemon.port))
        logging.info('Started master daemon (port %s)' % master_daemon.port)
        for name, func in funcs_by_name.items():
            server.register_function(partial(log_usage, name, func), name)
        server.register_function(server.funcs.keys, 'get_methods') # should be last
        signal.signal(signal.SIGTSTP, stop_handler) # ctrl+z
        signal.signal(signal.SIGTERM, stop_handler) # kill
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info('Ctrl+C')
    except Exception as e:
        logging.error('Closing due to error: %s' % e)
    finally:
        if CUpdate.thread and CUpdate.thread.is_alive():
            CUpdate.thread.terminate()

def stop_handler(signalnum, *args, **kwargs):
    if CUpdate.thread and CUpdate.thread.pid == os.getpid():
        logging.info('Updating aborted.')
    else:
        logging.info('Got signal %s, exiting.' % signalnum)
    sys.exit(0)

# returns True if given path is under current dir or /tmp
def _check_path_under_current_or_temp(path):
    if not os.path.relpath(path, '/tmp').startswith(os.pardir):
        return True
    if not os.path.relpath(path, os.getcwd()).startswith(os.pardir):
        return True
    return False


### Main ###

if os.getuid() != 0:
    fail('Please run this program as root/with sudo')

pid = os.getpid()
ret, out, err = run_command('taskset -pc 0 %s' % pid)
if ret:
    fail('Could not set self affinity to core zero. Result: %s' % [ret, out, err])

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
args.trex_dir = os.path.abspath(args.trex_dir)
args.daemon_type = args.daemon_type or 'master_daemon'

stl_rpc_proxy_dir  = os.path.join(args.trex_dir, 'automation', 'trex_control_plane', 'server')
stl_rpc_proxy      = TCPDaemon('Stateless RPC proxy', args.stl_rpc_proxy_port, "su -s /bin/bash -c '%s rpc_proxy_server.py' nobody" % sys.executable, stl_rpc_proxy_dir)
trex_daemon_server = TCPDaemon('TRex daemon server', args.trex_daemon_port, '%s trex_daemon_server start' % sys.executable, args.trex_dir)
master_daemon      = TCPDaemon('Master daemon', args.master_port, start_master_daemon) # add ourself for easier check if running, kill etc.

tmp_dir = '/tmp/trex-tmp'
logging_file = '/var/log/trex/master_daemon.log'
logging_file_bu = '/var/log/trex/master_daemon.log_bu'
os.chdir('/')

class CUpdate:
    info = {}
    thread = None

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
    except:
        try: # give it another try
            time.sleep(1)
            func()
        except Exception as e:
            print(termstyle.red(e))
            sys.exit(1)

passive = {'start': 'started', 'restart': 'restarted', 'stop': 'stopped', 'show': 'running'}

if (args.action in ('show', 'start', 'restart')) ^ (not daemon.is_running()):
    print(termstyle.green('%s is %s' % (daemon.name, passive[args.action])))
    sys.exit(0)
else:
    print(termstyle.red('%s is NOT %s' % (daemon.name, passive[args.action])))
    sys.exit(-1)

