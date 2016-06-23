#!/usr/bin/python
import os
import sys
from time import time, sleep
import shlex
import threading
import subprocess
import multiprocessing
import tempfile
import fnmatch


sys.path.append('automation/trex_control_plane/stl')
from trex_stl_lib.api import *

def run_server(command):
    return subprocess.Popen(shlex.split(command), stdout = subprocess.PIPE, stderr = subprocess.PIPE, close_fds = True)


def run_command(command, timeout = 15, cwd = None):
    # pipes might stuck, even with timeout
    with tempfile.TemporaryFile() as stdout_file, tempfile.TemporaryFile() as stderr_file:
        proc = subprocess.Popen(shlex.split(command), stdout = stdout_file, stderr = stderr_file, cwd = cwd, close_fds = True)
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

def get_trex_cmds():
    ret_code, stdout, stderr = run_command('ps -u root --format pid,comm,cmd')
    if ret_code:
        raise Exception('Failed to determine running processes, stderr: %s' % stderr)
    trex_cmds_list = []
    for line in stdout.splitlines():
        pid, proc_name, full_cmd = line.strip().split(' ', 2)
        pid = pid.strip()
        full_cmd = full_cmd.strip()
        if proc_name.find('t-rex-64') >= 0:
            trex_cmds_list.append((pid, full_cmd))
        else:
            if full_cmd.find('t-rex-64') >= 0:
                trex_cmds_list.append((pid, full_cmd))

    return trex_cmds_list

def is_any_core ():
    ret_code, stdout, stderr = run_command('ls')
    assert(ret_code==0);
    l= stdout.split()
    for file in l:
         if fnmatch.fnmatch(file, 'core.*'):
            return True
    return False


def kill_all_trexes():
    trex_cmds_list = get_trex_cmds()
    if not trex_cmds_list:
        return False
    for pid, cmd in trex_cmds_list:
        run_command('kill %s' % pid)
        ret_code_ps, _, _ = run_command('ps -p %s' % pid)
        if not ret_code_ps:
            run_command('kill -9 %s' % pid)
            ret_code_ps, _, _ = run_command('ps -p %s' % pid)
            if not ret_code_ps:
                pass;
    return True

def term_all_trexes():
    trex_cmds_list = get_trex_cmds()
    if not trex_cmds_list:
        return False
    for pid, cmd in trex_cmds_list:
        print pid
        run_command('kill -INT %s' % pid)
    return True



def run_one_iter ():
    try:
       server = run_server('./t-rex-64-debug-gdb-bt  -i -c 4 --iom 0')
    
       print "sleep 1 sec"
       time.sleep(1);
       crash=True;
    
       if True:
           c = STLClient()
           print 'Connecting to server'
           c.connect()
           print 'Connected'
            
           print 'Mapping'
           print 'Map: %s' % stl_map_ports(c)
           c.disconnect()
           crash=False;

    except Exception as e:
       print(e)
    finally :
        if crash:
            print "Crash seen, wait for the info"
            # wait the process to make the core file 
            loop=0;
            while True:
                if server.poll() is not None: # server ended
                      print 'Server stopped.\nReturn code: %s\nStderr: %s\nStdout: %s' % (server.returncode, server.stdout.read().decode(errors = 'replace'), server.stderr.read().decode(errors = 'replace'))
                      break;
                time.sleep(1);
                loop=loop+1;
                if loop >600:
                    print "Timeout on crash!!"
                    break;
            return 1
        else:
           print "kill process ",server.pid
           term_all_trexes();
           kill_all_trexes();
           return 0


def loop_inter ():
    kill_all_trexes()
    cnt=0;
    while True:

        print (time.strftime("%H:%M:%S")),
        print "Iter",cnt
        ret=run_one_iter ()
        if ret==1:
            break;
        cnt=cnt+1;
        if is_any_core ():
            print "stop due to core file"
            break;

loop_inter ()

