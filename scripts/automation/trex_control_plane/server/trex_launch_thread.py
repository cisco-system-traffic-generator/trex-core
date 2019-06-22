#!/router/bin/python


import os
import signal
import socket
from common.trex_status_e import TRexStatus
import subprocess
import shlex
import time
import threading
import logging
import CCustomLogger
import traceback

# setup the logger
CCustomLogger.setup_custom_logger('TRexServer')
logger = logging.getLogger('TRexServer')

def bt_process(tree, level, pid, cmd):
    logger.info('TRex subprocess level: %s, pid: %s, cmd: %s' % (level, pid, cmd))

    if level > 4:
        logger.warn('Process tree is too deep (level %s)' % level)
        return

    gdb_cmd = 'gdb --pid %s --eval bt --batch' % pid
    output = subprocess.check_output(shlex.split(gdb_cmd), stderr = subprocess.STDOUT, universal_newlines = True)
    logger.info('BT of subprocess:\n%s' % output)

    for child_pid, child_cmd in tree.get(pid, []):
        bt_process(tree, level + 1, child_pid, child_cmd)


def bt_process_tree(root_pid):
    root_cmd = None
    output = subprocess.check_output(shlex.split('ps -u root --format=pid,ppid,command'), universal_newlines = True)

    parents_dict = {}
    for line in output.splitlines():
        pid, ppid, cmd = line.strip().split(None, 2)
        if pid == 'PID':
            continue
        pid, ppid = int(pid), int(ppid)
        if pid == root_pid:
            root_cmd = cmd
        if ppid in parents_dict:
            parents_dict[ppid].append((pid, cmd))
        else:
            parents_dict[ppid] = [(pid, cmd)]
    if not root_cmd:
        logger.error('Could not find TRex process')
        return
    bt_process(parents_dict, 0, root_pid, root_cmd)


class AsynchronousTRexSession(threading.Thread):
    def __init__(self, trexObj , trex_launch_path, trex_cmd_data):
        super(AsynchronousTRexSession, self).__init__()
        self.stoprequest                            = threading.Event()
        self.launch_path                            = trex_launch_path
        self.cmd, self.export_path, self.duration   = trex_cmd_data
        self.session                                = None
        self.trexObj                                = trexObj
        self.time_stamps                            = {'start' : None, 'run_time' : None}
        self.trexObj.clear_zmq_dump()

    def run (self):
        try:
            with open(self.export_path, 'w') as output_file, open(os.devnull, 'w') as devnull:
                self.time_stamps['start'] = self.time_stamps['run_time'] = time.time()
                self.session   = subprocess.Popen(shlex.split(self.cmd), cwd = self.launch_path, stdout = output_file, stdin = devnull,
                                                  stderr = subprocess.STDOUT, preexec_fn=os.setsid, close_fds = True)
                logger.info("TRex session initialized successfully, Parent process pid is {pid}.".format( pid = self.session.pid ))
                while self.session.poll() is None:  # subprocess is NOT finished
                    time.sleep(0.5)
                    if self.stoprequest.is_set():
                        logger.debug("Abort request received by handling thread. Terminating TRex session." )
                        os.killpg(self.session.pid, signal.SIGUSR1)
                        self.trexObj.set_status(TRexStatus.Idle)
                        self.trexObj.set_verbose_status("TRex is Idle")
                        break
        except Exception as e:
            logger.error(e)

        self.time_stamps['run_time'] = time.time() - self.time_stamps['start']

        try:
            if self.time_stamps['run_time'] < 5:
                logger.error("TRex run failed due to wrong input parameters, or due to readability issues.")
                self.trexObj.set_verbose_status("TRex run failed due to wrong input parameters, or due to readability issues.\n\nTRex command: {cmd}\n\nRun output:\n{output}".format(
                    cmd = self.cmd, output = self.load_trex_output(self.export_path)))
                self.trexObj.errcode = -11
            elif (self.session.returncode is not None and self.session.returncode != 0) or ( (self.time_stamps['run_time'] < self.duration) and (not self.stoprequest.is_set()) ):
                if (self.session.returncode is not None and self.session.returncode != 0):
                    logger.debug("Failed TRex run due to session return code ({ret_code})".format( ret_code = self.session.returncode ) )
                elif ( (self.time_stamps['run_time'] < self.duration) and not self.stoprequest.is_set()):
                    logger.debug("Failed TRex run due to running time ({runtime}) combined with no-stopping request.".format( runtime = self.time_stamps['run_time'] ) )

                logger.warning("TRex run was terminated unexpectedly by outer process or by the hosting OS")
                self.trexObj.set_verbose_status("TRex run was terminated unexpectedly by outer process or by the hosting OS.\n\nRun output:\n{output}".format(
                    output = self.load_trex_output(self.export_path)))
                self.trexObj.errcode = -15
            else:
                time.sleep(0.5)
                logger.info("TRex run session finished.")
                self.trexObj.set_verbose_status('TRex finished.')
                self.trexObj.errcode = None

        finally:
            self.trexObj.set_status(TRexStatus.Idle)
            logger.info("TRex running state changed to 'Idle'.")
            self.trexObj.expect_trex.clear()
            logger.debug("Finished handling a single run of TRex.")
            self.trexObj.zmq_dump   = None

    def join(self, timeout = 5, with_tb = False):
        if with_tb and self.session.pid:
            try:
                bt_process_tree(self.session.pid)
            except:
                logger.error('Could not get traceback of TRex processes, error: %s' % traceback.format_exc())
        self.stoprequest.set()
        super(AsynchronousTRexSession, self).join(timeout)

    def load_trex_output (self, export_path):
        with open(export_path) as f:
            return f.read()





if __name__ == "__main__":
    pass
    
