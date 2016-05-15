#!/usr/bin/python
import os, sys, getpass
import tempfile
import argparse
import socket
from time import time, sleep
import subprocess, shlex, shutil, multiprocessing
from glob import glob
import logging
logging.basicConfig(level = logging.FATAL) # keep quiet

sys.path.append(os.path.join('external_libs', 'jsonrpclib-pelix-0.2.5'))
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer

sys.path.append(os.path.join('external_libs', 'termstyle'))
import termstyle


### Server functions ###

def check_connectivity():
    return True

def add(a, b): # for sanity checks
    return a + b

def get_trex_path():
    return args.trex_dir

def is_trex_daemon_running():
    ret_code, stdout, stderr = run_command('ps -u root --format comm')
    if ret_code:
        raise Exception('Failed to list running processes, stderr: %s' % stderr)
    if 'trex_daemon_ser' in stdout: # name is cut
        return True
    return False

def restart_trex_daemon():
    if is_trex_daemon_running:
        stop_trex_daemon()
    start_trex_daemon()
    return True

def stop_trex_daemon():
    if not is_trex_daemon_running():
        return False
    return_code, stdout, stderr = run_command('%s stop' % trex_daemon_path)
    if return_code:
        raise Exception('Could not stop trex_daemon_server, %s' % [return_code, stdout, stderr])
    for i in range(50):
        if not is_trex_daemon_running():
            return True
        sleep(0.1)
    raise Exception('Could not stop trex_daemon_server')

def start_trex_daemon():
    if is_trex_daemon_running():
        return False
    return_code, stdout, stderr = run_command('%s start' % trex_daemon_path)
    if return_code:
        raise Exception('Could not run trex_daemon_server, err: %s' % stderr)
    for i in range(50):
        if is_trex_daemon_running():
            return True
        sleep(0.1)
    raise Exception('Could not run trex_daemon_server')

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
        raise Exception('Could not get requested package.\nStdout: %s\nStderr: %s' % (stdout, stderr))
    # clean old unpacked dirs
    unpacked_dirs = glob(os.path.join(tmp_dir, 'v[0-9].[0-9][0-9]'))
    for unpacked_dir in unpacked_dirs:
        shutil.rmtree(unpacked_dir)
    # unpacking
    ret_code, stdout, stderr = run_command('tar -xzf %s -C %s' % (os.path.join(tmp_dir, file_name), tmp_dir))
    if ret_code:
        raise Exception('Could not untar the package.\nStdout: %s\nStderr: %s' % (stdout, stderr))
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


def unpack_client():
    if not args.allow_update:
        raise Exception('Unpacking of client not allowed')
    # determining client archive
    client_pkg_files = glob(os.path.join(args.trex_dir, 'trex_client*.tar.gz'))
    if not len(client_pkg_files):
        raise Exception('Could not find client package')
    if len(client_pkg_files) > 1:
        raise Exception('Found more than one client packages')
    client_pkg_name = os.path.basename(client_pkg_files[0])
    client_new_path = os.path.join(args.trex_dir, 'trex_client')
    if os.path.exists(client_new_path):
        if os.path.islink(client_new_path) or os.path.isfile(client_new_path):
            os.unlink(client_new_path)
        else:
            shutil.rmtree(client_new_path)
    # unpacking
    ret_code, stdout, stderr = run_command('tar -xzf %s -C %s' % (os.path.join(args.trex_dir, client_pkg_files[0]), args.trex_dir))
    if ret_code:
        raise Exception('Could not untar the client package: %s' % stderr)
    return True

### /Server functions ###

def fail(msg):
    print(msg)
    sys.exit(-1)

def run_command(command, timeout = 15):
    command = 'timeout %s %s' % (timeout, command)
    # pipes might stuck, even with timeout
    with tempfile.TemporaryFile() as stdout_file, tempfile.TemporaryFile() as stderr_file:
        proc = subprocess.Popen(shlex.split(command), stdout = stdout_file, stderr = stderr_file, cwd = args.trex_dir)
        proc.wait()
        stdout_file.seek(0)
        stderr_file.seek(0)
        return (proc.returncode, stdout_file.read().decode(errors = 'replace'), stderr_file.read().decode(errors = 'replace'))

def show_master_daemon_status():
    if get_master_daemon_pid():
        print(termstyle.red('Master daemon is running'))
    else:
        print(termstyle.red('Master daemon is NOT running'))

def start_master_daemon():
    if get_master_daemon_pid():
        print(termstyle.red('Master daemon is already running'))
        return
    server = multiprocessing.Process(target = start_master_daemon_func)
    server.daemon = True
    server.start()
    for i in range(50):
        if get_master_daemon_pid():
            print(termstyle.green('Master daemon is started'))
            os._exit(0)
        sleep(0.1)
    fail(termstyle.red('Master daemon failed to run'))

def restart_master_daemon():
    if get_master_daemon_pid():
        kill_master_daemon()
    start_master_daemon()

def start_master_daemon_func():
    server = SimpleJSONRPCServer(('0.0.0.0', args.daemon_port))
    print('Started master daemon (port %s)' % args.daemon_port)
    server.register_function(add)
    server.register_function(check_connectivity)
    server.register_function(get_trex_path)
    server.register_function(is_trex_daemon_running)
    server.register_function(restart_trex_daemon)
    server.register_function(start_trex_daemon)
    server.register_function(stop_trex_daemon)
    server.register_function(unpack_client)
    server.register_function(update_trex)
    server.serve_forever()


def get_master_daemon_pid():
    return_code, stdout, stderr = run_command('netstat -tlnp')
    if return_code:
        fail('Failed to determine which program holds port %s, netstat error: %s' % (args.daemon_port, stderr))
    for line in stdout.splitlines():
        if '0.0.0.0:%s' % args.daemon_port in line:
            line_arr = line.split()
            if len(line_arr) != 7:
                fail('Could not parse netstat line to determine which process holds port %s: %s'(args.daemon_port, line))
            if '/' not in line_arr[6]:
                fail('Expecting pid/program name in netstat line of using port %s, got: %s'(args.daemon_port, line_arr[6]))
            pid, program = line_arr[6].split('/')
            if 'python' not in program and 'master_daemon' not in program:
                fail('Some other program holds port %s, not our daemon: %s. Please verify.' % (args.daemon_port, program))
            return pid
    return None

def kill_master_daemon():
    pid = get_master_daemon_pid()
    if not pid:
        print(termstyle.red('Master daemon is NOT running'))
        return True
    return_code, stdout, stderr = run_command('kill %s' % pid) # usual kill
    if return_code:
        fail('Failed to kill master daemon, error: %s' % stderr)
    for i in range(50):
        if not get_master_daemon_pid():
            print(termstyle.green('Master daemon is killed'))
            return True
        sleep(0.1)
    return_code, stdout, stderr = run_command('kill -9 %s' % pid) # unconditional kill
    if return_code:
        fail('Failed to kill trex_daemon, error: %s' % stderr)
    for i in range(50):
        if not get_master_daemon_pid():
            print(termstyle.green('Master daemon is killed'))
            return True
        sleep(0.1)
    fail('Failed to kill master daemon, even with -9. Please review manually.') # should not happen

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

actions_help = '''Specify action command to be applied on master daemon.
    (*) start      : start the master daemon.
    (*) show       : prompt the status of master daemon process (running / not running).
    (*) stop       : exit the master daemon process.
    (*) restart    : stop, then start again the master daemon process
    '''
action_funcs = {'start': start_master_daemon,
                'show': show_master_daemon_status,
                'stop': kill_master_daemon,
                'restart': restart_master_daemon,
                }

parser = argparse.ArgumentParser(description = 'Runs master daemon that can start/stop TRex daemon or update TRex version.')
parser.add_argument('-p', '--daemon-port', type=int, default = 8091, dest='daemon_port', 
                    help = 'Select port on which the master_daemon runs.\nDefault is 8091.', action = 'store')
parser.add_argument('-d', '--trex-dir', type=str, default = os.getcwd(), dest='trex_dir',
                    help = 'Path of TRex, default is current dir', action = 'store')
parser.add_argument('--allow-update', default = False, dest='allow_update', action = 'store_true',
                    help = "Allow update of TRex via RPC command. WARNING: It's security hole! Use on your risk!")
parser.add_argument('action', choices=action_funcs.keys(),
                    action='store', help=actions_help)
parser.usage = None
args = parser.parse_args()

tmp_dir = '/tmp/trex-tmp'

if not _check_path_under_current_or_temp(args.trex_dir):
    raise Exception('Only allowed to use path under /tmp or current directory')
if os.path.isfile(args.trex_dir):
    raise Exception('Given path is a file')
if not os.path.exists(args.trex_dir):
    os.makedirs(args.trex_dir)
os.chmod(args.trex_dir, 0o777)
if not os.path.exists(tmp_dir):
    os.makedirs(tmp_dir)

trex_daemon_path = os.path.join(args.trex_dir, 'trex_daemon_server')
action_funcs[args.action]()


