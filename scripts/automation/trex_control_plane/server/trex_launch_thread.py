#!/router/bin/python


import os
import signal
import socket
from common.trex_status_e import TRexStatus
import subprocess
import time
import threading
import logging
import CCustomLogger

# setup the logger
CCustomLogger.setup_custom_logger('TRexServer')
logger = logging.getLogger('TRexServer')


class AsynchronousTRexSession(threading.Thread):
    def __init__(self, trexObj , trex_launch_path, trex_cmd_data):
        super(AsynchronousTRexSession, self).__init__()
        self.stoprequest                            = threading.Event()
        self.terminateFlag                          = False
        self.launch_path                            = trex_launch_path
        self.cmd, self.export_path, self.duration   = trex_cmd_data
        self.session                                = None
        self.trexObj                                = trexObj
        self.time_stamps                            = {'start' : None, 'run_time' : None}
        self.trexObj.zmq_dump                       = {}

    def run (self):
            
        with open(os.devnull, 'w') as DEVNULL:
            self.time_stamps['start'] = self.time_stamps['run_time'] = time.time()
            self.session   = subprocess.Popen("exec "+self.cmd, cwd = self.launch_path, shell=True, stdin = DEVNULL, stderr = subprocess.PIPE, preexec_fn=os.setsid)
            logger.info("T-Rex session initialized successfully, Parent process pid is {pid}.".format( pid = self.session.pid ))
            while self.session.poll() is None:  # subprocess is NOT finished
                time.sleep(0.5)
                if self.stoprequest.is_set():
                    logger.debug("Abort request received by handling thread. Terminating T-Rex session." )
                    os.killpg(self.session.pid, signal.SIGUSR1)
                    self.trexObj.set_status(TRexStatus.Idle)
                    self.trexObj.set_verbose_status("T-Rex is Idle")
                    break

            self.time_stamps['run_time'] = time.time() - self.time_stamps['start']

            try:
                if self.time_stamps['run_time'] < 5:
                    logger.error("T-Rex run failed due to wrong input parameters, or due to reachability issues.")
                    self.trexObj.set_verbose_status("T-Rex run failed due to wrong input parameters, or due to reachability issues.\n\nT-Rex command: {cmd}\n\nRun output:\n{output}".format(
                        cmd = self.cmd, output = self.load_trex_output(self.export_path)))
                    self.trexObj.errcode = -11
                elif (self.session.returncode is not None and self.session.returncode < 0) or ( (self.time_stamps['run_time'] < self.duration) and (not self.stoprequest.is_set()) ):
                    if (self.session.returncode is not None and self.session.returncode < 0):
                        logger.debug("Failed T-Rex run due to session return code ({ret_code})".format( ret_code = self.session.returncode ) )
                    elif ( (self.time_stamps['run_time'] < self.duration) and not self.stoprequest.is_set()):
                        logger.debug("Failed T-Rex run due to running time ({runtime}) combined with no-stopping request.".format( runtime = self.time_stamps['run_time'] ) )

                    logger.warning("T-Rex run was terminated unexpectedly by outer process or by the hosting OS")
                    self.trexObj.set_verbose_status("T-Rex run was terminated unexpectedly by outer process or by the hosting OS.\n\nRun output:\n{output}".format(
                        output = self.load_trex_output(self.export_path)))
                    self.trexObj.errcode = -15
                else:
                    logger.info("T-Rex run session finished.")
                    self.trexObj.set_verbose_status('T-Rex finished.')
                    self.trexObj.errcode = None
                    
            finally:
                self.trexObj.set_status(TRexStatus.Idle)
                logger.info("TRex running state changed to 'Idle'.")
                self.trexObj.expect_trex.clear()
                logger.debug("Finished handling a single run of T-Rex.")
                self.trexObj.zmq_dump   = None

    def join (self, timeout = None):
        self.stoprequest.set()
        super(AsynchronousTRexSession, self).join(timeout)

    def load_trex_output (self, export_path):
        output = None
        with open(export_path, 'r') as f:
            output = f.read()
        return output





if __name__ == "__main__":
    pass
    
