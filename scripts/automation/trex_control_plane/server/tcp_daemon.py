import errno
import os
import shlex
import socket
import signal
import tempfile
import types
from subprocess import Popen
from time import sleep
import outer_packages
import jsonrpclib
import netstat

# all daemons should use -p argument as listening tcp port and check_connectivity RPC method
class TCPDaemon(object):

    # run_cmd can be function of how to run daemon or a str to run at subprocess
    def __init__(self, name, port, run_cmd, dir = None):
        self.name    = name
        self.port    = port
        self.run_cmd = run_cmd
        self.dir     = dir
        self.stop    = self.kill # alias
        if dir and not os.path.exists(dir):
            print('Warning: path given for %s: %s, does not exist' % (name, dir))


    # returns True if daemon is running
    def is_running(self):
        for conn in netstat.netstat(with_pid = False, search_local_port = self.port):
            if conn[2] == '0.0.0.0' and conn[6] == 'LISTEN':
                return True
        return False


    def get_pid(self):
        pid = None
        for conn in netstat.netstat(with_pid = True, search_local_port = self.port):
            if conn[2] == '0.0.0.0' and conn[6] == 'LISTEN':
                pid = conn[7]
                if pid is None:
                    raise Exception('Found the connection, but could not determine pid: %s' % conn)
                break
        return pid


    def kill_by_signal(self, pid, signal_name, timeout):
        os.kill(pid, signal_name)
        poll_rate = 0.1
        for i in range(int(timeout / poll_rate)):
            if not self.is_running():
                return True
            sleep(poll_rate)


    # kill daemon, with verification
    def kill(self, timeout = 15):
        try:
            pid = self.get_pid()
        except:
            sleep(0.1)
            pid = self.get_pid()
        if not pid:
            raise Exception('%s is not running' % self.name)
        # try Ctrl+C, usual kill, kill -9
        for signal_name in [signal.SIGINT, signal.SIGTERM, signal.SIGKILL]:
            if self.kill_by_signal(int(pid), signal_name, timeout):
                return True
        raise Exception('Could not kill %s, even with -9' % self.name)


    # try connection as RPC client, return True upon success, False if fail
    def check_connectivity(self, timeout = 15):
        daemon = jsonrpclib.Server('http://127.0.0.1:%s/' % self.port, timeout = timeout)
        poll_rate = 0.1
        for i in range(int(timeout/poll_rate)):
            try:
                daemon.check_connectivity()
                return True
            except socket.error: # daemon is not up yet
                sleep(poll_rate)
        return False


    # start daemon
    # returns True if success, False if already running
    def start(self, timeout = 20):
        if self.is_running():
            raise Exception('%s is already running' % self.name)
        if not self.run_cmd:
            raise Exception('No starting command registered for %s' % self.name)
        if type(self.run_cmd) is types.FunctionType:
            # detach child with double fork trick
            pid = os.fork()
            if pid:
                for i in range(50):
                    if self.is_running():
                        return
                    sleep(0.1)
                return
            os.setsid()
            pid = os.fork()
            if pid:
                os._exit(0)
            self.run_cmd()
            os._exit(0)

        with tempfile.TemporaryFile() as stdout_file, tempfile.TemporaryFile() as stderr_file, open(os.devnull, 'w') as devnull:
            proc = Popen(shlex.split('%s -p %s' % (self.run_cmd, self.port)), cwd = self.dir, close_fds = True,
                         stdout = stdout_file, stderr = stderr_file, stdin = devnull)
            if timeout > 0:
                poll_rate = 0.1
                for i in range(int(timeout/poll_rate)):
                    if self.is_running():
                        break
                    sleep(poll_rate)
                    if bool(proc.poll()): # process ended with error
                        stdout_file.seek(0)
                        stderr_file.seek(0)
                        raise Exception('Run of %s ended unexpectfully: %s' % (self.name, [proc.returncode, stdout_file.read().decode(errors = 'replace'), stderr_file.read().decode(errors = 'replace')]))
                    elif proc.poll() == 0: # process runs other process, and ended
                        break
            if self.is_running():
                if self.check_connectivity():
                    return True
                raise Exception('Daemon process is running, but no connectivity')
            raise Exception('%s failed to run.' % self.name)

    # restart the daemon
    def restart(self, timeout = 15):
        if self.is_running():
            self.kill(timeout)
            sleep(0.5)
        return self.start(timeout)


# runs command
def run_command(command, timeout = 15, cwd = None):
    # pipes might stuck, even with timeout
    with tempfile.TemporaryFile() as stdout_file, tempfile.TemporaryFile() as stderr_file, open(os.devnull, 'w') as devnull:
        proc = Popen(shlex.split(command), stdout = stdout_file, stderr = stderr_file, stdin = devnull, cwd = cwd, close_fds = True)
        if timeout > 0:
            poll_rate = 0.1
            for i in range(int(timeout/poll_rate)):
                sleep(poll_rate)
                if proc.poll() is not None: # process stopped
                    break
            if proc.poll() is None:
                proc.kill() # timeout
                return (errno.ETIME, '', 'Timeout on running: %s' % command)
        else:
            proc.wait()
        stdout_file.seek(0)
        stderr_file.seek(0)
        return (proc.returncode, stdout_file.read().decode(errors = 'replace'), stderr_file.read().decode(errors = 'replace'))
