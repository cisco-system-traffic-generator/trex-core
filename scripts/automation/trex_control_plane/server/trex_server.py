#!/usr/bin/python


import os
import stat
import sys
import time
import outer_packages
import zmq
import yaml
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import jsonrpclib
from jsonrpclib import Fault
import binascii
import socket
import errno
import signal
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
import json
import re
import shlex
import tempfile

try:
    from .tcp_daemon import run_command
except:
    from tcp_daemon import run_command


# setup the logger
CCustomLogger.setup_custom_logger('TRexServer')
logger = logging.getLogger('TRexServer')

class CTRexServer(object):
    """This class defines the server side of the RESTfull interaction with TRex"""
    TREX_START_CMD    = './t-rex-64'
    DEFAULT_FILE_PATH = '/tmp/trex_files/'

    def __init__(self, trex_path, trex_files_path, trex_host='0.0.0.0', trex_daemon_port=8090, trex_zmq_port=4500, trex_nice=-19):
        """ 
        Parameters
        ----------
        trex_host : str
            a string of the TRex ip address or hostname.
            default value: machine hostname as fetched from socket.gethostname()
        trex_daemon_port : int
            the port number on which the trex-daemon server can be reached
            default value: 8090
        trex_zmq_port : int
            the port number on which trex's zmq module will interact with daemon server
            default value: 4500
        nice: int
            priority of the TRex process

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
        self.trex_nice          = int(trex_nice)
        if self.trex_nice < -20 or self.trex_nice > 19:
            err = "Parameter 'nice' should be integer in range [-20, 19]"
            print(err)
            logger.error(err)
            raise Exception(err)
    
    def add(self, x, y):
        logger.info("Processing add function. Parameters are: {0}, {1} ".format( x, y ))
        return x + y
        # return Fault(-10, "")

    # Get information about available network interfaces
    def get_devices_info(self):
        logger.info("Processing get_devices_info() command.")
        try:
            args = [os.path.join(self.TREX_PATH, 'dpdk_nic_bind.py'), '-s', '--json']
            return subprocess.check_output(args, cwd=self.TREX_PATH, universal_newlines=True)
        except Exception as e:
            err_str = "Error processing get_devices_info(): %s" % e
            logger.error(e)
            return Fault(-33, err_str)

    def push_file (self, filename, bin_data):
        logger.info("Processing push_file() command.")
        try:
            filepath = os.path.join(self.trex_files_path, os.path.basename(filename))
            with open(filepath, 'wb') as f:
                f.write(binascii.a2b_base64(bin_data))
            logger.info("push_file() command finished. File is saved as %s" % filepath)
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
            print("Firing up TRex REST daemon @ port {trex_port} ...\n".format( trex_port = self.trex_daemon_port ))
            logger.info("Firing up TRex REST daemon @ port {trex_port} ...".format( trex_port = self.trex_daemon_port ))
            logger.info("current working dir is: {0}".format(self.TREX_PATH) )
            logger.info("current files dir is  : {0}".format(self.trex_files_path) )
            logger.debug("Starting TRex server. Registering methods to process.")
            logger.info(self.get_trex_version(base64 = False))
            self.server = SimpleJSONRPCServer( (self.trex_host, self.trex_daemon_port) )
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                logger.error("TRex server requested address already in use. Aborting server launching.")
                print("TRex server requested address already in use. Aborting server launching.")
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
        self.server.register_function(self.add)
        self.server.register_function(self.get_devices_info)
        self.server.register_function(self.cancel_reservation)
        self.server.register_function(self.connectivity_check)
        self.server.register_function(self.connectivity_check, 'check_connectivity') # alias
        self.server.register_function(self.force_trex_kill)
        self.server.register_function(self.get_file)
        self.server.register_function(self.get_files_list)
        self.server.register_function(self.get_files_path)
        self.server.register_function(self.get_latest_dump)
        self.server.register_function(self.get_running_info)
        self.server.register_function(self.get_running_status)
        self.server.register_function(self.get_trex_cmds)
        self.server.register_function(self.get_trex_config)
        self.server.register_function(self.get_trex_daemon_log)
        self.server.register_function(self.get_trex_log)
        self.server.register_function(self.get_trex_version)
        self.server.register_function(self.is_reserved)
        self.server.register_function(self.is_running)
        self.server.register_function(self.kill_all_trexes)
        self.server.register_function(self.push_file)
        self.server.register_function(self.reserve_trex)
        self.server.register_function(self.start_trex)
        self.server.register_function(self.stop_trex)
        self.server.register_function(self.wait_until_kickoff_finish)
        signal.signal(signal.SIGTSTP, self.stop_handler)
        signal.signal(signal.SIGTERM, self.stop_handler)
        try:
            self.zmq_monitor.start()
            self.server.serve_forever()
        except KeyboardInterrupt:
            logger.info("Daemon shutdown request detected." )
        finally:
            self.zmq_monitor.join()            # close ZMQ monitor thread resources
            self.server.shutdown()
            #self.server.server_close()


    # get files from Trex server and return their content (mainly for logs)
    @staticmethod
    def _pull_file(filepath):
        try:
            with open(filepath, 'rb') as f:
                file_content = f.read()
                return binascii.b2a_base64(file_content).decode(errors='replace')
        except Exception as e:
            err_str = "Can't get requested file %s: %s" % (filepath, e)
            logger.error(err_str)
            return Fault(-33, err_str)

    # returns True if given path is under TRex package or under /tmp/trex_files
    def _check_path_under_TRex_or_temp(self, path):
        if not os.path.relpath(path, self.trex_files_path).startswith(os.pardir):
            return True
        if not os.path.relpath(path, self.TREX_PATH).startswith(os.pardir):
            return True
        return False

    # gets the file content encoded base64 either from /tmp/trex_files or TRex server dir
    def get_file(self, filepath):
        try:
            logger.info("Processing get_file() command.")
            if not self._check_path_under_TRex_or_temp(filepath):
                raise Exception('Given path should be under current TRex package or /tmp/trex_files')
            return self._pull_file(filepath)
        except Exception as e:
            err_str = "Can't get requested file %s: %s" % (filepath, e)
            logger.error(err_str)
            return Fault(-33, err_str)

    # get tuple (dirs, files) with directories and files lists from given path (limited under TRex package or /tmp/trex_files)
    def get_files_list(self, path):
        try:
            logger.info("Processing get_files_list() command, given path: %s" % path)
            if not self._check_path_under_TRex_or_temp(path):
                raise Exception('Given path should be under current TRex package or /tmp/trex_files')
            return os.walk(path).next()[1:3]
        except Exception as e:
            err_str = "Error processing get_files_list(): %s" % e
            logger.error(err_str)
            return Fault(-33, err_str)

    # get Trex log /tmp/trex.txt
    def get_trex_log(self):
        logger.info("Processing get_trex_log() command.")
        return self._pull_file('/tmp/trex.txt')

    # get /etc/trex_cfg.yaml
    def get_trex_config(self):
        logger.info("Processing get_trex_config() command.")
        return self._pull_file('/etc/trex_cfg.yaml')

    # get daemon log /var/log/trex/trex_daemon_server.log
    def get_trex_daemon_log (self):
        logger.info("Processing get_trex_daemon_log() command.")
        return self._pull_file('/var/log/trex/trex_daemon_server.log')
        
    # get Trex version from ./t-rex-64 --help (last lines starting with "Version : ...")
    def get_trex_version (self, base64 = True):
        try:
            logger.info("Processing get_trex_version() command.")
            if not self.trex_version:
                ret_code, stdout, stderr = run_command('./t-rex-64 --help', cwd = self.TREX_PATH)
                search_result = re.search('\n\s*(Version\s*:.+)', stdout, re.DOTALL)
                if not search_result:
                    raise Exception('Could not determine version from ./t-rex-64 --help')
                self.trex_version = binascii.b2a_base64(search_result.group(1).encode(errors='replace'))
            if base64:
                return self.trex_version.decode(errors='replace')
            else:
                return binascii.a2b_base64(self.trex_version).decode(errors='replace')
        except Exception as e:
            err_str = "Can't get trex version, error: %s" % e
            logger.error(err_str)
            return Fault(-33, err_str)

    def stop_handler (self, *args, **kwargs):
        logger.info("Daemon STOP request detected.")
        if self.is_running():
            # in case TRex process is currently running, stop it before terminating server process
            self.stop_trex(self.trex.get_seq())
        sys.exit(0)

    def assert_zmq_ok(self):
        if self.trex.zmq_error:
            self.trex.zmq_error, err = None, self.trex.zmq_error
            raise Exception('ZMQ thread got error: %s' % err)
        if not self.zmq_monitor.is_alive():
            if self.trex.get_status() != TRexStatus.Idle:
                self.force_trex_kill()
            raise Exception('ZMQ thread is dead.')

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

    def start_trex(self, trex_cmd_options, user, block_to_success = True, timeout = 40, stateless = False, debug_image = False, trex_args = ''):
        self.trex.zmq_error = None
        self.assert_zmq_ok()
        with self.start_lock:
            logger.info("Processing start_trex() command.")
            if self.is_reserved():
                # check if this is not the user to which TRex is reserved
                if self.__reservation['user'] != user:  
                    logger.info("TRex is reserved to another user ({res_user}). Only that user is allowed to initiate new runs.".format(res_user = self.__reservation['user']))
                    return Fault(-33, "TRex is reserved to another user ({res_user}). Only that user is allowed to initiate new runs.".format(res_user = self.__reservation['user']))  # raise at client TRexRequestDenied
            elif self.trex.get_status() != TRexStatus.Idle:
                err = 'TRex is already taken, cannot create another run until done.'
                logger.info(err)
                return Fault(-13, err) # raise at client TRexInUseError

            try:
                server_cmd_data = self.generate_run_cmd(stateless = stateless, debug_image = debug_image, trex_args = trex_args, **trex_cmd_options)
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
                            self.assert_zmq_ok()

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
                raise TypeError('TRex -f (traffic generation .yaml file) and -c (num of cores) must be specified. %s' % e)


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

    # returns list of tuples (pid, command line) of running TRex(es)
    def get_trex_cmds(self):
        logger.info('Processing get_trex_cmds() command.')
        ret_code, stdout, stderr = run_command('ps -u root --format pid,comm,cmd')
        if ret_code:
            raise Exception('Failed to determine running processes, stderr: %s' % stderr)
        trex_cmds_list = []
        for line in stdout.splitlines():
            pid, proc_name, full_cmd = line.strip().split(' ', 2)
            pid = pid.strip()
            full_cmd = full_cmd.strip()
            if proc_name.find('_t-rex-64') >= 0:
                trex_cmds_list.append((pid, full_cmd))
        return trex_cmds_list


    # Silently tries to kill TRexes with given signal.
    # Responsibility of client to verify with get_trex_cmds.
    def kill_all_trexes(self, signal_name):
        logger.info('Processing kill_all_trexes() command.')
        trex_cmds_list = self.get_trex_cmds()
        for pid, cmd in trex_cmds_list:
            logger.info('Killing with signal %s process %s %s' % (signal_name, pid, cmd))
            try:
                os.kill(int(pid), signal_name)
            except OSError as e:
                if e.errno == errno.ESRCH:
                    logger.info('No such process, ignoring.')
                raise


    def wait_until_kickoff_finish (self, timeout = 40):
        # block until TRex exits Starting state
        logger.info("Processing wait_until_kickoff_finish() command.")
        start_time = time.time()
        while (time.time() - start_time) < timeout :
            self.assert_zmq_ok()
            trex_state = self.trex.get_status()
            if trex_state != TRexStatus.Starting:
                return
            time.sleep(0.1)
        return Fault(-12, 'TimeoutError: TRex initiation outcome could not be obtained, since TRex stays at Starting state beyond defined timeout.') # raise at client TRexWarning

    def get_running_info (self):
        self.assert_zmq_ok()
        logger.info("Processing get_running_info() command.")
        return self.trex.get_running_info()

    def get_latest_dump(self):
        logger.info("Processing get_latest_dump() command.")
        return self.trex.get_latest_dump()

    def generate_run_cmd (self, iom = 0, export_path="/tmp/trex.txt", stateless = False, debug_image = False, trex_args = '', **kwargs):
        """ generate_run_cmd(self, iom, export_path, kwargs) -> str

        Generates a custom running command for the kick-off of the TRex traffic generator.
        Returns a tuple of command (string) and export path (string) to be issued on the trex server

        Parameters
        ----------
        iom: int
            0 = don't print stats screen to log, 1 = print stats (can generate huge logs)
        stateless: boolean
            True = run as stateless, False = require -f and -d arguments
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
        if stateless:
            kwargs['i'] = True

        # adding additional options to the command
        trex_cmd_options = ''
        for key, value in kwargs.items():
            tmp_key = key.replace('_','-').lstrip('-')
            dash = ' -' if (len(key)==1) else ' --'
            if value is True:
                trex_cmd_options += (dash + tmp_key)
            elif value is False:
                continue
            else:
                trex_cmd_options += (dash + '{k} {val}'.format( k = tmp_key, val =  value ))
        if trex_args:
            trex_cmd_options += ' %s' % trex_args

        self._check_zmq_port(trex_cmd_options)

        if not stateless:
            if 'f' not in kwargs:
                raise Exception('Argument -f should be specified in stateful command')
            if 'd' not in kwargs:
                raise Exception('Argument -d should be specified in stateful command')

        cmd = "{nice}{run_command}{debug_image} --iom {io} {cmd_options} --no-key".format( # -- iom 0 disables the periodic log to the screen (not needed)
            nice = '' if self.trex_nice == 0 else 'nice -n %s ' % self.trex_nice,
            run_command = self.TREX_START_CMD,
            debug_image = '-debug' if debug_image else '',
            cmd_options = trex_cmd_options,
            io          = iom)

        logger.info("TREX FULL COMMAND: {command}".format(command = cmd) )

        return (cmd, export_path, kwargs.get('d', 0))


    def _check_zmq_port(self, trex_cmd_options):
        zmq_cfg_port = 4500 # default
        parser = ArgumentParser()
        parser.add_argument('--cfg', default = '/etc/trex_cfg.yaml')
        args, _ = parser.parse_known_args(shlex.split(trex_cmd_options))
        if not os.path.exists(args.cfg):
            raise Exception('Platform config file "%s" does not exist!' % args.cfg)
        with open(args.cfg) as f:
            trex_cfg = yaml.safe_load(f.read())
        if type(trex_cfg) is not list:
            raise Exception('Platform config file "%s" content should be array.' % args.cfg)
        if not len(trex_cfg):
            raise Exception('Platform config file "%s" content should be array with one element.' % args.cfg)
        trex_cfg = trex_cfg[0]
        if 'enable_zmq_pub' in trex_cfg and trex_cfg['enable_zmq_pub'] == False:
            raise Exception('TRex daemon expects ZMQ publisher to be enabled. Please change "enable_zmq_pub" to true.')
        if 'zmq_pub_port' in trex_cfg:
            zmq_cfg_port = trex_cfg['zmq_pub_port']
        if zmq_cfg_port != self.trex_zmq_port:
            raise Exception('ZMQ port does not match: platform config file is configured to: %s, daemon server to: %s' % (zmq_cfg_port, self.trex_zmq_port))


    def __check_trex_path_validity(self):
        # check for executable existance
        if not os.path.exists(self.TREX_PATH+'/t-rex-64'):
            print("The provided TRex path do not contain an executable TRex file.\nPlease check the path and retry.")
            logger.error("The provided TRex path do not contain an executable TRex file")
            exit(-1)
        # check for executable permissions
        st = os.stat(self.TREX_PATH+'/t-rex-64')
        if not bool(st.st_mode & (stat.S_IXUSR ) ):
            print("The provided TRex path do not contain an TRex file with execution privileges.\nPlease check the files permissions and retry.")
            logger.error("The provided TRex path do not contain an TRex file with execution privileges")
            exit(-1)
        else:
            return

    def __check_files_path_validity(self):
        # first, check for path existance. otherwise, try creating it with appropriate credentials
        if not os.path.exists(self.trex_files_path):
            try:
                os.makedirs(self.trex_files_path, 0o660)
                return
            except os.error as inst:
                print("The provided files path does not exist and cannot be created with needed access credentials using root user.\nPlease check the path's permissions and retry.")
                logger.error("The provided files path does not exist and cannot be created with needed access credentials using root user.")
                exit(-1)
        elif os.access(self.trex_files_path, os.W_OK):
            return
        else:
            print("The provided files path has insufficient access credentials for root user.\nPlease check the path's permissions and retry.")
            logger.error("The provided files path has insufficient access credentials for root user")
            exit(-1)

class CTRex(object):
    def __init__(self):
        self.status         = TRexStatus.Idle
        self.verbose_status = 'TRex is Idle'
        self.errcode        = None
        self.session        = None
        self.zmq_monitor    = None
        self.__zmq_dump     = {}
        self.zmq_dump_lock  = threading.Lock()
        self.zmq_error      = None
        self.seq            = None
        self.expect_trex    = threading.Event()

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

    def get_latest_dump(self):
        with self.zmq_dump_lock:
            return json.dumps(self.__zmq_dump)

    def update_zmq_dump_key(self, key, val):
        with self.zmq_dump_lock:
            self.__zmq_dump[key] = val

    def clear_zmq_dump(self):
        with self.zmq_dump_lock:
            self.__zmq_dump = {}

    def get_running_info (self):
        if self.status == TRexStatus.Running:
            return self.get_latest_dump()
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
            # TRex isn't running, nothing to abort
            logger.info("TRex isn't running. No need to stop anything.")
            if self.errcode is not None:    # some error occurred, notify client despite TRex already stopped
                    return Fault(self.errcode, self.verbose_status)               # raise at client relevant exception, depending on the reason the error occured
            return False
        else:
            # handle stopping TRex's run
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
    cur = os.path.dirname(__file__)
    default_path        = os.path.abspath(os.path.join(cur, os.pardir, os.pardir, os.pardir))
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
    parser.add_argument('-n', '--nice', dest='nice', action="store", default = -19, type = int,
        help="Determine the priority TRex process [-20, 19] (lower = higher priority)\nDefault is -19.")
    return parser

trex_parser = generate_trex_parser()

def do_main_program ():

    args = trex_parser.parse_args()
    server = CTRexServer(trex_path = args.trex_path,  trex_files_path = args.files_path,
                         trex_host = args.trex_host, trex_daemon_port = args.daemon_port,
                         trex_zmq_port = args.zmq_port, trex_nice = args.nice)
    server.start()


if __name__ == "__main__":
    do_main_program()
    
