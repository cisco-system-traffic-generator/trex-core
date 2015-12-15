#!/usr/bin/python


import os
import stat
import sys
import time
import outer_packages
import zmq
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import jsonrpclib
from jsonrpclib import Fault
import binascii
import socket
import errno
import signal
import binascii
from common.trex_status_e import TRexStatus
from common.trex_exceptions import *
import subprocess
from random import randrange
import logging
import threading
import CCustomLogger
from trex_launch_thread import AsynchronousTRexSession
from zmq_monitor_thread import ZmqMonitorSession
from argparse import ArgumentParser, RawTextHelpFormatter
from json import JSONEncoder
import re


# setup the logger
CCustomLogger.setup_custom_logger('TRexServer')
logger = logging.getLogger('TRexServer')

class CTRexServer(object):
    """This class defines the server side of the RESTfull interaction with TRex"""
    DEFAULT_TREX_PATH = '/auto/proj-pcube-b/apps/PL-b/tools/bp_sim2/v1.55/' #'/auto/proj-pcube-b/apps/PL-b/tools/nightly/trex_latest'
    TREX_START_CMD    = './t-rex-64'
    DEFAULT_FILE_PATH = '/tmp/trex_files/'

    def __init__(self, trex_path, trex_files_path, trex_host='0.0.0.0', trex_daemon_port=8090, trex_zmq_port=4500):
        """ 
        Parameters
        ----------
        trex_host : str
            a string of the t-rex ip address or hostname.
            default value: machine hostname as fetched from socket.gethostname()
        trex_daemon_port : int
            the port number on which the trex-daemon server can be reached
            default value: 8090
        trex_zmq_port : int
            the port number on which trex's zmq module will interact with daemon server
            default value: 4500

        Instantiate a TRex client object, and connecting it to listening daemon-server
        """
        self.TREX_PATH          = os.path.abspath(os.path.dirname(trex_path+'/'))
        self.trex_files_path    = os.path.abspath(os.path.dirname(trex_files_path+'/'))
        self.__check_trex_path_validity()
        self.__check_files_path_validity()
        self.trex               = CTRex()
        self.trex_version       = None
        self.trex_host          = trex_host
        self.trex_daemon_port   = trex_daemon_port
        self.trex_zmq_port      = trex_zmq_port
        self.trex_server_path   = "http://{hostname}:{port}".format( hostname = trex_host, port = trex_daemon_port )
        self.start_lock         = threading.Lock()
        self.__reservation      = None
        self.zmq_monitor        = ZmqMonitorSession(self.trex, self.trex_zmq_port)    # intiate single ZMQ monitor thread for server usage
    
    def add(self, x, y):
        print "server function add ",x,y
        logger.info("Processing add function. Parameters are: {0}, {1} ".format( x, y ))
        return x + y
        # return Fault(-10, "")

    def push_file (self, filename, bin_data):
        logger.info("Processing push_file() command.")
        try:
            filepath = os.path.abspath(os.path.join(self.trex_files_path, filename))
            with open(filepath, 'wb') as f:
                f.write(binascii.a2b_base64(bin_data))
            logger.info("push_file() command finished. `{name}` was saved at {fpath}".format( name = filename, fpath = self.trex_files_path))
            return True
        except IOError as inst:
            logger.error("push_file method failed. " + str(inst))
            return False

    def connectivity_check (self):
        logger.info("Processing connectivity_check function.")
        return True

    def start(self):
        """This method fires up the daemon server based on initialized parameters of the class"""
        # initialize the server instance with given resources
        try:
            print "Firing up TRex REST daemon @ port {trex_port} ...\n".format( trex_port = self.trex_daemon_port )
            logger.info("Firing up TRex REST daemon @ port {trex_port} ...".format( trex_port = self.trex_daemon_port ))
            logger.info("current working dir is: {0}".format(self.TREX_PATH) )
            logger.info("current files dir is  : {0}".format(self.trex_files_path) )
            logger.debug("Starting TRex server. Registering methods to process.")
            logger.info(self.get_trex_version(base64 = False))
            self.server = SimpleJSONRPCServer( (self.trex_host, self.trex_daemon_port) )
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                logger.error("TRex server requested address already in use. Aborting server launching.")
                print "TRex server requested address already in use. Aborting server launching."
                raise socket.error(errno.EADDRINUSE, "TRex daemon requested address already in use. "
                                                     "Server launch aborted. Please make sure no other process is "
                                                     "using the desired server properties.")
            elif isinstance(e, socket.gaierror) and e.errno == -3:
                # handling Temporary failure in name resolution exception
                raise socket.gaierror(-3, "Temporary failure in name resolution.\n"
                                          "Make sure provided hostname has DNS resolving.")
            else:
                raise

        # set further functionality and peripherals to server instance 
        try:
            self.server.register_function(self.add)
            self.server.register_function(self.get_trex_log)
            self.server.register_function(self.get_trex_daemon_log)
            self.server.register_function(self.get_trex_version)
            self.server.register_function(self.connectivity_check)
            self.server.register_function(self.start_trex)
            self.server.register_function(self.stop_trex)
            self.server.register_function(self.wait_until_kickoff_finish)
            self.server.register_function(self.get_running_status)
            self.server.register_function(self.is_running)
            self.server.register_function(self.get_running_info)
            self.server.register_function(self.is_reserved)
            self.server.register_function(self.get_files_path)
            self.server.register_function(self.push_file)
            self.server.register_function(self.reserve_trex)
            self.server.register_function(self.cancel_reservation)
            self.server.register_function(self.force_trex_kill)
            signal.signal(signal.SIGTSTP, self.stop_handler)
            signal.signal(signal.SIGTERM, self.stop_handler)
            self.zmq_monitor.start()
            self.server.serve_forever()
        except KeyboardInterrupt:
            logger.info("Daemon shutdown request detected." )
        finally:
            self.zmq_monitor.join()            # close ZMQ monitor thread resources
            self.server.shutdown()
            pass

    # get files from Trex server and return their content (mainly for logs)
    @staticmethod
    def _pull_file(filepath):
        try:
            with open(filepath, 'rb') as f:
                file_content = f.read()
                return binascii.b2a_base64(file_content)
        except Exception as e:
            err_str = "Can't get requested file: {0}, possibly due to TRex that did not run".format(filepath)
            logger.error('{0}, error: {1}'.format(err_str, e))
            return Fault(-33, err_str)

    # get Trex log /tmp/trex.txt
    def get_trex_log(self):
        logger.info("Processing get_trex_log() command.")
        return self._pull_file('/tmp/trex.txt')

    # get daemon log /var/log/trex/trex_daemon_server.log
    def get_trex_daemon_log (self):
        logger.info("Processing get_trex_daemon_log() command.")
        return self._pull_file('/var/log/trex/trex_daemon_server.log')
        
    # get Trex version from ./t-rex-64 --help (last lines starting with "Version : ...")
    def get_trex_version (self, base64 = True):
        try:
            logger.info("Processing get_trex_version() command.")
            if not self.trex_version:
                help_print = subprocess.Popen(['./t-rex-64', '--help'], cwd = self.TREX_PATH, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdout, stderr) = help_print.communicate()
                search_result = re.search('\n\s*(Version\s*:.+)', stdout, re.DOTALL)
                if not search_result:
                    raise Exception('Could not determine version from ./t-rex-64 --help')
                self.trex_version = binascii.b2a_base64(search_result.group(1))
            if base64:
                return self.trex_version
            else:
                return binascii.a2b_base64(self.trex_version)
        except Exception as e:
            err_str = "Can't get trex version, error: {0}".format(e)
            logger.error(err_str)
            return Fault(-33, err_str)

    def stop_handler (self, signum, frame):
        logger.info("Daemon STOP request detected.")
        if self.is_running():
            # in case TRex process is currently running, stop it before terminating server process
            self.stop_trex(self.trex.get_seq())
        sys.exit(0)

    def is_running (self):
        run_status = self.trex.get_status()
        logger.info("Processing is_running() command. Running status is: {stat}".format(stat = run_status) )
        if run_status==TRexStatus.Running:
            return True
        else:
            return False

    def is_reserved (self):
        logger.info("Processing is_reserved() command.")
        return bool(self.__reservation)

    def get_running_status (self):
        run_status = self.trex.get_status()
        logger.info("Processing get_running_status() command. Running status is: {stat}".format(stat = run_status) )
        return { 'state' : run_status.value, 'verbose' : self.trex.get_verbose_status() }

    def get_files_path (self):
        logger.info("Processing get_files_path() command." )
        return self.trex_files_path

    def reserve_trex (self, user):
        if user == "":
            logger.info("TRex reservation cannot apply to empty string user. Request denied.")
            return Fault(-33, "TRex reservation cannot apply to empty string user. Request denied.")

        with self.start_lock:
            logger.info("Processing reserve_trex() command.")
            if self.is_reserved():
                if user == self.__reservation['user']:  
                    # return True is the same user is asking and already has the resrvation
                    logger.info("the same user is asking and already has the resrvation. Re-reserving TRex.")
                    return True

                logger.info("TRex is already reserved to another user ({res_user}), cannot reserve to another user.".format( res_user = self.__reservation['user'] ))
                return Fault(-33, "TRex is already reserved to another user ({res_user}). Please make sure TRex is free before reserving it.".format(
                    res_user = self.__reservation['user']) )  # raise at client TRexInUseError
            elif self.trex.get_status() != TRexStatus.Idle:
                logger.info("TRex is currently running, cannot reserve TRex unless in Idle state.")
                return Fault(-13, 'TRex is currently running, cannot reserve TRex unless in Idle state. Please try again when TRex run finished.')  # raise at client TRexInUseError
            else:
                logger.info("TRex is now reserved for user ({res_user}).".format( res_user = user ))
                self.__reservation = {'user' : user, 'since' : time.ctime()}
                logger.debug("Reservation details: "+ str(self.__reservation))
                return True

    def cancel_reservation (self, user):
        with self.start_lock:
            logger.info("Processing cancel_reservation() command.")
            if self.is_reserved():
                if self.__reservation['user'] == user:
                    logger.info("TRex reservation to {res_user} has been canceled successfully.".format(res_user = self.__reservation['user']))
                    self.__reservation = None
                    return True
                else:
                    logger.warning("TRex is reserved to different user than the provided one. Reservation wasn't canceled.")
                    return Fault(-33, "Cancel reservation request is available to the user that holds the reservation. Request denied")  # raise at client TRexRequestDenied
            
            else:
                logger.info("TRex is not reserved to anyone. No need to cancel anything")
                assert(self.__reservation is None)
                return False

            
    def start_trex(self, trex_cmd_options, user, block_to_success = True, timeout = 40):
        with self.start_lock:
            logger.info("Processing start_trex() command.")
            if self.is_reserved():
                # check if this is not the user to which TRex is reserved
                if self.__reservation['user'] != user:  
                    logger.info("TRex is reserved to another user ({res_user}). Only that user is allowed to initiate new runs.".format(res_user = self.__reservation['user']))
                    return Fault(-33, "TRex is reserved to another user ({res_user}). Only that user is allowed to initiate new runs.".format(res_user = self.__reservation['user']))  # raise at client TRexRequestDenied
            elif self.trex.get_status() != TRexStatus.Idle:
                logger.info("TRex is already taken, cannot create another run until done.")
                return Fault(-13, '')  # raise at client TRexInUseError
            
            try: 
                server_cmd_data = self.generate_run_cmd(**trex_cmd_options)
                self.zmq_monitor.first_dump = True
                self.trex.start_trex(self.TREX_PATH, server_cmd_data)
                logger.info("TRex session has been successfully initiated.")
                if block_to_success:
                    # delay server response until TRex is at 'Running' state.
                    start_time = time.time()
                    trex_state = None
                    while (time.time() - start_time) < timeout :
                        trex_state = self.trex.get_status()
                        if trex_state != TRexStatus.Starting:
                            break
                        else:
                            time.sleep(0.5)

                    # check for TRex run started normally
                    if trex_state == TRexStatus.Starting:   # reached timeout
                        logger.warning("TimeoutError: TRex initiation outcome could not be obtained, since TRex stays at Starting state beyond defined timeout.")
                        return Fault(-12, 'TimeoutError: TRex initiation outcome could not be obtained, since TRex stays at Starting state beyond defined timeout.') # raise at client TRexWarning
                    elif trex_state == TRexStatus.Idle:
                        return Fault(-11, self.trex.get_verbose_status())   # raise at client TRexError
                
                # reach here only if TRex is at 'Running' state
                self.trex.gen_seq()
                return self.trex.get_seq()          # return unique seq number to client
                        
            except TypeError as e:
                logger.error("TRex command generation failed, probably because either -f (traffic generation .yaml file) and -c (num of cores) was not specified correctly.\nReceived params: {params}".format( params = trex_cmd_options) )
                raise TypeError('TRex -f (traffic generation .yaml file) and -c (num of cores) must be specified.')


    def stop_trex(self, seq):
        logger.info("Processing stop_trex() command.")
        if self.trex.get_seq()== seq:
            logger.debug("Abort request legit since seq# match")
            return self.trex.stop_trex()
        else:
            if self.trex.get_status() != TRexStatus.Idle:
                logger.warning("Abort request is only allowed to process initiated the run. Request denied.")

                return Fault(-33, 'Abort request is only allowed to process initiated the run. Request denied.')  # raise at client TRexRequestDenied
            else:
                return False

    def force_trex_kill (self):
        logger.info("Processing force_trex_kill() command. --> Killing TRex session indiscriminately.")
        return self.trex.stop_trex()

    def wait_until_kickoff_finish (self, timeout = 40):
        # block until TRex exits Starting state
        logger.info("Processing wait_until_kickoff_finish() command.")
        trex_state = None
        start_time = time.time()
        while (time.time() - start_time) < timeout :
            trex_state = self.trex.get_status()
            if trex_state != TRexStatus.Starting:
                return
        return Fault(-12, 'TimeoutError: TRex initiation outcome could not be obtained, since TRex stays at Starting state beyond defined timeout.') # raise at client TRexWarning

    def get_running_info (self):
        logger.info("Processing get_running_info() command.")
        return self.trex.get_running_info()

    def generate_run_cmd (self, f, d, iom = 0, export_path="/tmp/trex.txt", **kwargs):
        """ generate_run_cmd(self, trex_cmd_options, export_path) -> str

        Generates a custom running command for the kick-off of the TRex traffic generator.
        Returns a tuple of command (string) and export path (string) to be issued on the trex server

        Parameters
        ----------
        kwargs: dictionary
            Dictionary of parameters for trex. For example: (c=1, nc=True, l_pkt_mode=3).
            Notice that when sending command line parameters that has -, you need to replace it with _.
            for example, to have on command line "--l-pkt-mode 3", you need to send l_pkt_mode=3
        export_path : str
            Full system path to which the results of the trex-run will be logged.

        """
        if 'results_file_path' in kwargs:
            export_path = kwargs['results_file_path']
            del kwargs['results_file_path']


        # adding additional options to the command
        trex_cmd_options = ''
        for key, value in kwargs.iteritems():
            tmp_key = key.replace('_','-')
            dash = ' -' if (len(key)==1) else ' --'
            if (value == True) and (str(value) != '1'):       # checking also int(value) to excape from situation that 1 translates by python to 'True'
                trex_cmd_options += (dash + tmp_key)
            else:
                trex_cmd_options += (dash + '{k} {val}'.format( k = tmp_key, val =  value ))

        cmd = "{run_command} -f {gen_file} -d {duration} --iom {io} {cmd_options} --no-key > {export}".format( # -- iom 0 disables the periodic log to the screen (not needed)
            run_command = self.TREX_START_CMD,
            gen_file    = f,
            duration    = d,
            cmd_options = trex_cmd_options,
            io          = iom, 
            export = export_path )

        logger.info("TREX FULL COMMAND: {command}".format(command = cmd) )

        return (cmd, export_path, long(d))

    def __check_trex_path_validity(self):
        # check for executable existance
        if not os.path.exists(self.TREX_PATH+'/t-rex-64'):
            print "The provided TRex path do not contain an executable TRex file.\nPlease check the path and retry."
            logger.error("The provided TRex path do not contain an executable TRex file")
            exit(-1)
        # check for executable permissions
        st = os.stat(self.TREX_PATH+'/t-rex-64')
        if not bool(st.st_mode & (stat.S_IXUSR ) ):
            print "The provided TRex path do not contain an TRex file with execution privileges.\nPlease check the files permissions and retry."
            logger.error("The provided TRex path do not contain an TRex file with execution privileges")
            exit(-1)
        else:
            return

    def __check_files_path_validity(self):
        # first, check for path existance. otherwise, try creating it with appropriate credentials
        if not os.path.exists(self.trex_files_path):
            try:
                os.makedirs(self.trex_files_path, 0660)
                return
            except os.error as inst:
                print "The provided files path does not exist and cannot be created with needed access credentials using root user.\nPlease check the path's permissions and retry."
                logger.error("The provided files path does not exist and cannot be created with needed access credentials using root user.")
                exit(-1)
        elif os.access(self.trex_files_path, os.W_OK):
            return
        else:
            print "The provided files path has insufficient access credentials for root user.\nPlease check the path's permissions and retry."
            logger.error("The provided files path has insufficient access credentials for root user")
            exit(-1)

class CTRex(object):
    def __init__(self):
        self.status         = TRexStatus.Idle
        self.verbose_status = 'TRex is Idle'
        self.errcode        = None
        self.session        = None
        self.zmq_monitor    = None
        self.zmq_dump       = None
        self.seq            = None
        self.expect_trex    = threading.Event()
        self.encoder        = JSONEncoder()

    def get_status(self):
        return self.status

    def set_status(self, new_status):
        self.status = new_status

    def get_verbose_status(self):
        return self.verbose_status

    def set_verbose_status(self, new_status):
        self.verbose_status = new_status

    def gen_seq (self):
        self.seq = randrange(1,1000)

    def get_seq (self):
        return self.seq

    def get_running_info (self):
        if self.status == TRexStatus.Running:
            return self.encoder.encode(self.zmq_dump)
        else:
            logger.info("TRex isn't running. Running information isn't available.")
            if self.status == TRexStatus.Idle:
                if self.errcode is not None:    # some error occured
                    logger.info("TRex is in Idle state, with errors. returning fault")
                    return Fault(self.errcode, self.verbose_status)               # raise at client relevant exception, depending on the reason the error occured
                else:
                    logger.info("TRex is in Idle state, no errors. returning {}")
                    return u'{}'    
                
            return Fault(-12, self.verbose_status)                                # raise at client TRexWarning, indicating TRex is back to Idle state or still in Starting state

    def stop_trex(self):
        if self.status == TRexStatus.Idle:
            # t-rex isn't running, nothing to abort
            logger.info("TRex isn't running. No need to stop anything.")
            if self.errcode is not None:    # some error occurred, notify client despite TRex already stopped
                    return Fault(self.errcode, self.verbose_status)               # raise at client relevant exception, depending on the reason the error occured
            return False
        else:
            # handle stopping t-rex's run
            self.session.join()
            logger.info("TRex session has been successfully aborted.")
            return True

    def start_trex(self, trex_launch_path, trex_cmd):
        self.set_status(TRexStatus.Starting)
        logger.info("TRex running state changed to 'Starting'.")
        self.set_verbose_status('TRex is starting (data is not available yet)')

        self.errcode    = None
        self.session    = AsynchronousTRexSession(self, trex_launch_path, trex_cmd)      
        self.session.start()
        self.expect_trex.set()
#       self.zmq_monitor= ZmqMonitorSession(self, zmq_port)
#       self.zmq_monitor.start()



def generate_trex_parser ():
    default_path        = os.path.abspath(os.path.join(outer_packages.CURRENT_PATH, os.pardir, os.pardir, os.pardir))
    default_files_path  = os.path.abspath(CTRexServer.DEFAULT_FILE_PATH)

    parser = ArgumentParser(description = 'Run server application for TRex traffic generator',
        formatter_class = RawTextHelpFormatter,
        usage = """
trex_daemon_server [options]
""" )

    parser.add_argument('-v', '--version', action='version', version='%(prog)s 1.0')
    parser.add_argument("-p", "--daemon-port", type=int, default = 8090, metavar="PORT", dest="daemon_port", 
        help="Select port on which the daemon runs.\nDefault port is 8090.", action="store")
    parser.add_argument("-z", "--zmq-port", dest="zmq_port", type=int,
        action="store", help="Select port on which the ZMQ module listens to TRex.\nDefault port is 4500.", metavar="PORT",
        default = 4500)
    parser.add_argument("-t", "--trex-path", dest="trex_path",
        action="store", help="Specify the compiled TRex directory from which TRex would run.\nDefault path is: {def_path}.".format( def_path = default_path ),
        metavar="PATH", default = default_path )
    parser.add_argument("-f", "--files-path", dest="files_path",
        action="store", help="Specify a path to directory on which pushed files will be saved at.\nDefault path is: {def_path}.".format( def_path = default_files_path ), 
        metavar="PATH", default = default_files_path )
    parser.add_argument("--trex-host", dest="trex_host",
        action="store", help="Specify a hostname to be registered as the TRex server.\n"
                             "Default is to bind all IPs using '0.0.0.0'.",
        metavar="HOST", default = '0.0.0.0')
    return parser

trex_parser = generate_trex_parser()

def do_main_program ():

    args = trex_parser.parse_args()
    server = CTRexServer(trex_path = args.trex_path,  trex_files_path = args.files_path,
                         trex_host = args.trex_host, trex_daemon_port = args.daemon_port,
                         trex_zmq_port = args.zmq_port)
    server.start()


if __name__ == "__main__":
    do_main_program()
    
