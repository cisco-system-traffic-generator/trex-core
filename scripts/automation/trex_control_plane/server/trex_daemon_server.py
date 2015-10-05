#!/usr/bin/python

import outer_packages
import daemon
from trex_server import do_main_program, trex_parser
import CCustomLogger

import logging
import time
import sys
import os, errno
import grp
import signal
from daemon import runner
from extended_daemon_runner import ExtendedDaemonRunner
import lockfile
import errno

class TRexServerApp(object):
    def __init__(self):
        TRexServerApp.create_working_dirs()
        self.stdin_path      = '/dev/null'
        self.stdout_path     = '/dev/tty'                               # All standard prints will come up from this source.
        self.stderr_path     = "/var/log/trex/trex_daemon_server.log"   # All log messages will come up from this source
        self.pidfile_path    = '/var/run/trex/trex_daemon_server.pid'
        self.pidfile_timeout = 5    # timeout in seconds
            
    def run(self):
        do_main_program()


    @staticmethod
    def create_working_dirs():
        if not os.path.exists('/var/log/trex'):
            os.mkdir('/var/log/trex')
        if not os.path.exists('/var/run/trex'):
            os.mkdir('/var/run/trex')



def main ():

    trex_app = TRexServerApp()

    # setup the logger
    default_log_path = '/var/log/trex/trex_daemon_server.log'

    try:
        CCustomLogger.setup_daemon_logger('TRexServer', default_log_path)
        logger = logging.getLogger('TRexServer')
        logger.setLevel(logging.INFO)
        formatter = logging.Formatter("%(asctime)s %(name)-10s %(module)-20s %(levelname)-8s %(message)s")
        handler = logging.FileHandler("/var/log/trex/trex_daemon_server.log")
        logger.addHandler(handler)
    except EnvironmentError, e:
            if e.errno == errno.EACCES: # catching permission denied error
                print "Launching user must have sudo privileges in order to run TRex daemon.\nTerminating daemon process."
            exit(-1)

    try:
        daemon_runner = ExtendedDaemonRunner(trex_app, trex_parser)
    except IOError as err:
        # catch 'tty' error when launching server from remote location
        if err.errno == errno.ENXIO:
            trex_app.stdout_path = "/dev/null"
            daemon_runner = ExtendedDaemonRunner(trex_app, trex_parser)
        else:
            raise

    #This ensures that the logger file handle does not get closed during daemonization
    daemon_runner.daemon_context.files_preserve=[handler.stream]

    try:
        if not set(['start', 'stop']).isdisjoint(set(sys.argv)):
            print "Logs are saved at: {log_path}".format( log_path = default_log_path )
        daemon_runner.do_action()
        
    except lockfile.LockTimeout as inst:
        logger.error(inst)
        print inst
        print """
        Please try again once the timeout has been reached.
        If this error continues, consider killing the process manually and restart the daemon."""


if __name__ == "__main__":
    main()
