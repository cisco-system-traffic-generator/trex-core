#!/router/bin/python

import sys
import os

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages
import jsonrpclib
from jsonrpclib import ProtocolError, AppError
from common.trex_status_e import TRexStatus
from common.trex_exceptions import *
from common.trex_exceptions import exception_handler
from client_utils.general_utils import *
from enum import Enum
import socket
import errno
import time
import re
import copy
import binascii
from collections import deque, OrderedDict
from json import JSONDecoder
from distutils.util import strtobool



class CTRexClient(object):
    """
    This class defines the client side of the RESTfull interaction with TRex
    """

    def __init__(self, trex_host, max_history_size = 100, trex_daemon_port = 8090, trex_zmq_port = 4500, verbose = False):
        """ 
        Instantiate a TRex client object, and connecting it to listening daemon-server

        :parameters:
             trex_host : str
                a string of the TRex ip address or hostname.
             max_history_size : int
                a number to set the maximum history size of a single TRex run. Each sampling adds a new item to history.

                default value : **100**
             trex_daemon_port : int
                the port number on which the trex-daemon server can be reached

                default value: **8090**
             trex_zmq_port : int
                the port number on which trex's zmq module will interact with daemon server

                default value: **4500**
             verbose : bool
                sets a verbose output on supported class method.

                default value : **False**

        :raises:
            socket errors, in case server could not be reached.

        """
        self.trex_host          = trex_host
        self.trex_daemon_port   = trex_daemon_port
        self.trex_zmq_port      = trex_zmq_port
        self.seq                = None
        self.verbose            = verbose
        self.result_obj         = CTRexResult(max_history_size)
        self.decoder            = JSONDecoder()
        self.trex_server_path   = "http://{hostname}:{port}/".format( hostname = trex_host, port = trex_daemon_port )
        self.__verbose_print("Connecting to TRex @ {trex_path} ...".format( trex_path = self.trex_server_path ) )
        self.history            = jsonrpclib.history.History()
        self.server             = jsonrpclib.Server(self.trex_server_path, history = self.history)
        self.check_server_connectivity()
        self.__verbose_print("Connection established successfully!")
        self._last_sample       = time.time()
        self.__default_user     = get_current_user()


    def add (self, x, y):
        try:
            return self.server.add(x,y)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def start_trex (self, f, d, block_to_success = True, timeout = 30, user = None, **trex_cmd_options):
        """
        Request to start a TRex run on server.
                
        :parameters:  
            f : str
                a path (on server) for the injected traffic data (.yaml file)
            d : int
                the desired duration of the test. must be at least 30 seconds long.
            block_to_success : bool
                determine if this method blocks until TRex changes state from 'Starting' to either 'Idle' or 'Running'

                default value : **True**
            timeout : int
                maximum time (in seconds) to wait in blocking state until TRex changes state from 'Starting' to either 'Idle' or 'Running'

                default value: **30**
            user : str
                the identity of the the run issuer.
            trex_cmd_options : key, val
                sets desired TRex options using key=val syntax, separated by comma.
                for keys with no value, state key=True

        :return: 
            **True** on success

        :raises:
            + :exc:`ValueError`, in case 'd' parameter inserted with wrong value.
            + :exc:`trex_exceptions.TRexError`, in case one of the trex_cmd_options raised an exception at server.
            + :exc:`trex_exceptions.TRexInUseError`, in case TRex is already taken.
            + :exc:`trex_exceptions.TRexRequestDenied`, in case TRex is reserved for another user than the one trying start TRex.
            + ProtocolError, in case of error in JSON-RPC protocol.
        
        """
        user = user or self.__default_user
        try:
            d = int(d)
            if d < 30:  # specify a test should take at least 30 seconds long.
                raise ValueError
        except ValueError:
            raise ValueError('d parameter must be integer, specifying how long TRex run, and must be larger than 30 secs.')

        trex_cmd_options.update( {'f' : f, 'd' : d} )
        if not trex_cmd_options.get('l'):
            self.result_obj.latency_checked = False

        self.result_obj.clear_results()
        try:
            issue_time = time.time()
            retval = self.server.start_trex(trex_cmd_options, user, block_to_success, timeout)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

        if retval!=0:   
            self.seq = retval   # update seq num only on successful submission
            return True
        else:   # TRex is has been started by another user
            raise TRexInUseError('TRex is already being used by another user or process. Try again once TRex is back in IDLE state.')

    def stop_trex (self):
        """
        Request to stop a TRex run on server.

        The request is only valid if the stop initiator is the same client as the TRex run initiator.
                
        :parameters:        
            None

        :return: 
            + **True** on successful termination 
            + **False** if request issued but TRex wasn't running.

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case TRex ir running but started by another user.
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            return self.server.stop_trex(self.seq)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def force_kill (self, confirm = True):
        """
        Force killing of running TRex process (if exists) on the server.

        .. tip:: This method is a safety method and **overrides any running or reserved resources**, and as such isn't designed to be used on a regular basis. 
                 Always consider using :func:`trex_client.CTRexClient.stop_trex` instead.

        In the end of this method, TRex will return to IDLE state with no reservation.
        
        :parameters:        
            confirm : bool
                Prompt a user confirmation before continue terminating TRex session

        :return: 
            + **True** on successful termination 
            + **False** otherwise.

        :raises:
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        if confirm:
            prompt = "WARNING: This will terminate active TRex session indiscriminately.\nAre you sure? "
            sys.stdout.write('%s [y/n]\n' % prompt)
            while True:
                try:
                    if strtobool(user_input().lower()):
                        break
                    else:
                        return
                except ValueError:
                    sys.stdout.write('Please respond with \'y\' or \'n\'.\n')
        try:
            return self.server.force_trex_kill()
        except AppError as err:
            # Silence any kind of application errors- by design
            return False
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def wait_until_kickoff_finish(self, timeout = 40):
        """
        Block the client application until TRex changes state from 'Starting' to either 'Idle' or 'Running'

        The request is only valid if the stop initiator is the same client as the TRex run initiator.

        :parameters:        
            timeout : int
                maximum time (in seconds) to wait in blocking state until TRex changes state from 'Starting' to either 'Idle' or 'Running'

        :return: 
            + **True** on successful termination 
            + **False** if request issued but TRex wasn't running.

        :raises:
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + ProtocolError, in case of error in JSON-RPC protocol.

            .. note::  Exceptions are throws only when start_trex did not block in the first place, i.e. `block_to_success` parameter was set to `False`

        """

        try:
            return self.server.wait_until_kickoff_finish(timeout)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def is_running (self, dump_out = False):
        """
        Poll for TRex running status.

        If TRex is running, a history item will be added into result_obj and processed.

        .. tip:: This method is especially useful for iterating until TRex run is finished.

        :parameters:        
            dump_out : dict
                if passed, the pointer object is cleared and the latest dump stored in it.

        :return: 
            + **True** if TRex is running.
            + **False** if TRex is not running.

        :raises:
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + :exc:`TypeError`, in case JSON stream decoding error.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            res = self.get_running_info()
            if res == {}:
                return False
            if (dump_out != False) and (isinstance(dump_out, dict)):        # save received dump to given 'dump_out' pointer
                dump_out.clear()
                dump_out.update(res)
            return True
        except TRexWarning as err:
            if err.code == -12:      # TRex is either still at 'Starting' state or in Idle state, however NO error occured
                return False
        except TRexException:
            raise
        except ProtocolError as err:
            raise
        finally:
            self.prompt_verbose_data()

    def get_trex_files_path (self):
        """
        Fetches the local path in which files are stored when pushed to TRex server from client.

        :parameters:        
            None

        :return: 
            string representation of the desired path

            .. note::  The returned path represents a path on the TRex server **local machine**

        :raises:
            ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            return (self.server.get_files_path() + '/')
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def get_running_status (self):
        """
        Fetches the current TRex status.

        If available, a verbose data will accompany the state itself.

        :parameters:        
            None

        :return: 
            dictionary with 'state' and 'verbose' keys.

        :raises:
            ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            res = self.server.get_running_status()
            res['state'] = TRexStatus(res['state'])
            return res
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def get_running_info (self):
        """
        Performs single poll of TRex running data and process it into the result object (named `result_obj`).

        .. tip:: This method will throw an exception if TRex isn't running. Always consider using :func:`trex_client.CTRexClient.is_running` which handles a single poll operation in safer manner.

        :parameters:        
            None

        :return: 
            dictionary containing the most updated data dump from TRex.

        :raises:
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + :exc:`TypeError`, in case JSON stream decoding error.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        if not self.is_query_relevance():
            # if requested in timeframe smaller than the original sample rate, return the last known data without interacting with server
            return self.result_obj.get_latest_dump()
        else:
            try: 
                latest_dump = self.decoder.decode( self.server.get_running_info() ) # latest dump is not a dict, but json string. decode it.
                self.result_obj.update_result_data(latest_dump)
                return latest_dump
            except TypeError as inst:
                raise TypeError('JSON-RPC data decoding failed. Check out incoming JSON stream.')
            except AppError as err:
                self._handle_AppError_exception(err.args[0])
            except ProtocolError:
                raise
            finally:
                self.prompt_verbose_data()

    def sample_until_condition (self, condition_func, time_between_samples = 5):
        """
        Automatically sets ongoing sampling of TRex data, with sampling rate described by time_between_samples.

        On each fetched dump, the condition_func is applied on the result objects, and if returns True, the sampling will stop.

        :parameters:        
            condition_func : function
                function that operates on result_obj and checks if a condition has been met

                .. note:: `condition_finc` is applied on `CTRexResult` object. Make sure to design a relevant method.
            time_between_samples : int
                determines the time between each sample of the server

                default value : **5**

        :return: 
            the first result object (see :class:`CTRexResult` for further details) of the TRex run on which the condition has been met.

        :raises:
            + :exc:`UserWarning`, in case the condition_func method condition hasn't been met
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + :exc:`TypeError`, in case JSON stream decoding error.
            + ProtocolError, in case of error in JSON-RPC protocol.
            + :exc:`Exception`, in case the condition_func suffered from any kind of exception

        """
        # make sure TRex is running. raise exceptions here if any
        self.wait_until_kickoff_finish()    
        try:
            while self.is_running():
                results = self.get_result_obj()
                if condition_func(results):
                    # if condition satisfied, stop TRex and return result object
                    self.stop_trex()
                    return results
                time.sleep(time_between_samples)
        except TRexWarning:
            # means we're back to Idle state, and didn't meet our condition
            raise UserWarning("TRex results condition wasn't met during TRex run.")
        except Exception:
            # this could come from provided method 'condition_func'
            raise

    def sample_to_run_finish (self, time_between_samples = 5):
        """
        Automatically sets automatically sampling of TRex data with sampling rate described by time_between_samples until TRex run finished.

        :parameters:        
            time_between_samples : int
                determines the time between each sample of the server

                default value : **5**

        :return: 
            the latest result object (see :class:`CTRexResult` for further details) with sampled data.

        :raises:
            + :exc:`UserWarning`, in case the condition_func method condition hasn't been met
            + :exc:`trex_exceptions.TRexIncompleteRunError`, in case one of failed TRex run (unexpected termination).
            + :exc:`TypeError`, in case JSON stream decoding error.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        self.wait_until_kickoff_finish()    
        
        try: 
            while self.is_running():
                time.sleep(time_between_samples)
        except TRexWarning:
            pass
        results = self.get_result_obj()
        return results
            

    def get_result_obj (self, copy_obj = True):
        """
        Returns the result object of the trex_client's instance. 

        By default, returns a **copy** of the objects (so that changes to the original object are masked).

        :parameters:        
            copy_obj : bool
                False means that a reference to the original (possibly changing) object are passed 

                defaul value : **True**

        :return: 
            the latest result object (see :class:`CTRexResult` for further details) with sampled data.

        """
        if copy_obj:
            return copy.deepcopy(self.result_obj)
        else:
            return self.result_obj

    def is_reserved (self):
        """
        Checks if TRex is currently reserved to any user or not.

        :parameters:        
            None

        :return: 
            + **True** if TRex is reserved.
            + **False** otherwise.

        :raises:
            ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            return self.server.is_reserved()
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def get_trex_daemon_log (self):
        """
        Get Trex daemon log.

        :return: 
            String representation of TRex daemon log

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case file could not be read.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            return binascii.a2b_base64(self.server.get_trex_daemon_log())
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def get_trex_log (self):
        """
        Get TRex CLI output log

        :return: 
            String representation of TRex log

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case file could not be fetched at server side.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        try:
            return binascii.a2b_base64(self.server.get_trex_log())
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def get_trex_version (self):
        """
        Get TRex version details.

        :return: 
            Trex details (Version, User, Date, Uuid, Git SHA) as ordered dictionary

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case TRex version could not be determined.
            + ProtocolError, in case of error in JSON-RPC protocol.
            + General Exception is case one of the keys is missing in response
        """

        try:
            version_dict = OrderedDict()
            result_lines = binascii.a2b_base64(self.server.get_trex_version()).split('\n')
            for line in result_lines:
                if not line:
                    continue
                key, value = line.strip().split(':', 1)
                version_dict[key.strip()] = value.strip()
            for key in ('Version', 'User', 'Date', 'Uuid', 'Git SHA'):
                if key not in version_dict:
                    raise Exception('get_trex_version: got server response without key: {0}'.format(key))
            return version_dict
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def reserve_trex (self, user = None):
        """
        Reserves the usage of TRex to a certain user.

        When TRex is reserved, it can't be reserved.
                
        :parameters:        
            user : str
                a username of the desired owner of TRex

                default: current logged user

        :return: 
            **True** if reservation made successfully

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case TRex is reserved for another user than the one trying to make the reservation.
            + :exc:`trex_exceptions.TRexInUseError`, in case TRex is currently running.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        username = user or self.__default_user
        try:
            return self.server.reserve_trex(user = username)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def cancel_reservation (self, user = None):
        """
        Cancels a current reservation of TRex to a certain user.

        When TRex is reserved, no other user can start new TRex runs.

                
        :parameters:        
            user : str
                a username of the desired owner of TRex

                default: current logged user

        :return: 
            + **True** if reservation canceled successfully, 
            + **False** if there was no reservation at all.

        :raises:
            + :exc:`trex_exceptions.TRexRequestDenied`, in case TRex is reserved for another user than the one trying to cancel the reservation.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        
        username = user or self.__default_user
        try:
            return self.server.cancel_reservation(user = username)
        except AppError as err:
            self._handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()
    
    def push_files (self, filepaths):
        """
        Pushes a file (or a list of files) to store locally on server. 
                
        :parameters:        
            filepaths : str or list
                a path to a file to be pushed to server.
                if a list of paths is passed, all of those will be pushed to server

        :return: 
            + **True** if file(s) copied successfully.
            + **False** otherwise.

        :raises:
            + :exc:`IOError`, in case specified file wasn't found or could not be accessed.
            + ProtocolError, in case of error in JSON-RPC protocol.

        """
        paths_list = None
        if isinstance(filepaths, str):
            paths_list = [filepaths]
        elif isinstance(filepaths, list):
            paths_list = filepaths
        else:
            raise TypeError("filepaths argument must be of type str or list")
        
        for filepath in paths_list:
            try:
                if not os.path.exists(filepath):
                    raise IOError(errno.ENOENT, "The requested `{fname}` file wasn't found. Operation aborted.".format(
                        fname = filepath) )
                else:
                    filename = os.path.basename(filepath)
                    with open(filepath, 'rb') as f:
                        file_content = f.read()
                        self.server.push_file(filename, binascii.b2a_base64(file_content))
            finally:
                self.prompt_verbose_data()
        return True

    def is_query_relevance(self):
        """
        Checks if time between any two consecutive server queries (asking for live running data) passed.

        .. note:: The allowed minimum time between each two consecutive samples is 0.5 seconds.
                
        :parameters:        
            None

        :return: 
            + **True** if more than 0.5 seconds has been past from last server query.
            + **False** otherwise.

        """
        cur_time = time.time()
        if cur_time-self._last_sample < 0.5:
            return False
        else:
            self._last_sample = cur_time
            return True

    def call_server_mathod_safely (self, method_to_call):
        try:
            return method_to_call()
        except socket.error as e:
            if e.errno == errno.ECONNREFUSED:
                raise SocketError(errno.ECONNREFUSED, "Connection from TRex server was refused. Please make sure the server is up.")

    def check_server_connectivity (self):
        """
        Checks for server valid connectivity.
        """
        try:
            socket.gethostbyname(self.trex_host)
            return self.server.connectivity_check()
        except socket.gaierror as e:
            raise socket.gaierror(e.errno, "Could not resolve server hostname. Please make sure hostname entered correctly.")    
        except socket.error as e:
            if e.errno == errno.ECONNREFUSED:
                raise socket.error(errno.ECONNREFUSED, "Connection from TRex server was refused. Please make sure the server is up.")
        finally:
            self.prompt_verbose_data()
        
    def prompt_verbose_data(self):
        """
        This method prompts any verbose data available, only if `verbose` option has been turned on.
        """
        if self.verbose:
            print ('\n')
            print ("(*) JSON-RPC request:", self.history.request)
            print ("(*) JSON-RPC response:", self.history.response)

    def __verbose_print(self, print_str):
        """
        This private method prints the `print_str` string only in case self.verbose flag is turned on.

        :parameters:        
            print_str : str
                a string to be printed

        :returns:
            None
        """
        if self.verbose:
            print (print_str)


    
    def _handle_AppError_exception(self, err):
        """
        This private method triggres the TRex dedicated exception generation in case a general ProtocolError has been raised.
        """
        # handle known exceptions based on known error codes.
        # if error code is not known, raise ProtocolError
        raise exception_handler.gen_exception(err)


class CTRexResult(object):
    """
    A class containing all results received from TRex.

    Ontop to containing the results, this class offers easier data access and extended results processing options
    """
    def __init__(self, max_history_size):
        """ 
        Instatiate a TRex result object

        :parameters:
             max_history_size : int
                a number to set the maximum history size of a single TRex run. Each sampling adds a new item to history.

        """
        self._history = deque(maxlen = max_history_size)
        self.clear_results()
        self.latency_checked = True

    def __repr__(self):
        return ("Is valid history?       {arg}\n".format( arg = self.is_valid_hist() ) +
                "Done warmup?            {arg}\n".format( arg =  self.is_done_warmup() ) +
                "Expected tx rate:       {arg}\n".format( arg = self.get_expected_tx_rate() ) +
                "Current tx rate:        {arg}\n".format( arg = self.get_current_tx_rate() ) +
                "Maximum latency:        {arg}\n".format( arg = self.get_max_latency() ) +
                "Average latency:        {arg}\n".format( arg = self.get_avg_latency() ) +
                "Average window latency: {arg}\n".format( arg = self.get_avg_window_latency() ) +
                "Total drops:            {arg}\n".format( arg = self.get_total_drops() ) +
                "Drop rate:              {arg}\n".format( arg = self.get_drop_rate() ) +
                "History size so far:    {arg}\n".format( arg = len(self._history) ) )

    def get_expected_tx_rate (self):
        """
        Fetches the expected TX rate in various units representation

        :parameters:        
            None

        :return: 
            dictionary containing the expected TX rate, where the key is the measurement units, and the value is the measurement value.

        """
        return self._expected_tx_rate

    def get_current_tx_rate (self):
        """
        Fetches the current TX rate in various units representation

        :parameters:        
            None

        :return: 
            dictionary containing the current TX rate, where the key is the measurement units, and the value is the measurement value.

        """
        return self._current_tx_rate

    def get_max_latency (self):
        """
        Fetches the maximum latency measured on each of the interfaces

        :parameters:        
            None

        :return: 
            dictionary containing the maximum latency, where the key is the measurement interface (`c` indicates client), and the value is the measurement value.

        """
        return self._max_latency

    def get_avg_latency (self):
        """
        Fetches the average latency measured on each of the interfaces from the start of TRex run

        :parameters:        
            None

        :return: 
            dictionary containing the average latency, where the key is the measurement interface (`c` indicates client), and the value is the measurement value.

            The `all` key represents the average of all interfaces' average

        """
        return self._avg_latency

    def get_avg_window_latency (self):
        """
        Fetches the average latency measured on each of the interfaces from all the sampled currently stored in window.

        :parameters:        
            None

        :return: 
            dictionary containing the average latency, where the key is the measurement interface (`c` indicates client), and the value is the measurement value.

            The `all` key represents the average of all interfaces' average

        """
        return self._avg_window_latency

    def get_total_drops (self):
        """
        Fetches the total number of drops identified from the moment TRex run began.

        :parameters:        
            None

        :return: 
            total drops count (as int)

        """
        return self._total_drops
    
    def get_drop_rate (self):
        """
        Fetches the most recent drop rate in pkts/sec units.

        :parameters:        
            None

        :return: 
            current drop rate (as float)

        """
        return self._drop_rate

    def is_valid_hist (self):
        """
        Checks if result obejct contains valid data.

        :parameters:        
            None

        :return: 
            + **True** if history is valid.
            + **False** otherwise.

        """
        return self.valid

    def set_valid_hist (self, valid_stat = True):
        """
        Sets result obejct validity status.

        :parameters:        
            valid_stat : bool
                defines the validity status

                dafault value : **True**

        :return: 
            None

        """
        self.valid = valid_stat

    def is_done_warmup (self):
        """
        Checks if TRex latest results TX-rate indicates that TRex has reached its expected TX-rate.

        :parameters:        
            None

        :return: 
            + **True** if expected TX-rate has been reached.
            + **False** otherwise.

        """
        return self._done_warmup

    def get_last_value (self, tree_path_to_key, regex = None):
        """
        A dynamic getter from the latest sampled data item stored in the result object.

        :parameters:        
            tree_path_to_key : str
                defines a path to desired data. 

                .. tip:: | Use '.' to enter one level deeper in dictionary hierarchy. 
                         | Use '[i]' to access the i'th indexed object of an array.

            tree_path_to_key : regex
                apply a regex to filter results out from a multiple results set.

                Filter applies only on keys of dictionary type.

                dafault value : **None**

        :return: 
            + a list of values relevant to the specified path
            + None if no results were fetched or the history isn't valid.

        """
        if not self.is_valid_hist():
            return None
        else:
            return CTRexResult.__get_value_by_path(self._history[len(self._history)-1], tree_path_to_key, regex)

    def get_value_list (self, tree_path_to_key, regex = None, filter_none = True):
        """
        A dynamic getter from all sampled data items stored in the result object.

        :parameters:        
            tree_path_to_key : str
                defines a path to desired data. 

                .. tip:: | Use '.' to enter one level deeper in dictionary hierarchy. 
                         | Use '[i]' to access the i'th indexed object of an array.

            tree_path_to_key : regex
                apply a regex to filter results out from a multiple results set.

                Filter applies only on keys of dictionary type.

                dafault value : **None**

            filter_none : bool
                specify if None results should be filtered out or not.

                dafault value : **True**

        :return: 
            + a list of values relevant to the specified path. Each item on the list refers to a single server sample.
            + None if no results were fetched or the history isn't valid.
        """

        if not self.is_valid_hist():
            return None
        else:
            raw_list = list( map(lambda x: CTRexResult.__get_value_by_path(x, tree_path_to_key, regex), self._history) )
            if filter_none:
                return list (filter(lambda x: x!=None, raw_list) )
            else:
                return raw_list

    def get_latest_dump(self):
        """
        A  getter to the latest sampled data item stored in the result object.

        :parameters:        
            None

        :return: 
            + a dictionary of the latest data item
            + an empty dictionary if history is empty.

        """
        history_size = len(self._history)
        if history_size != 0:
            return self._history[len(self._history) - 1]
        else:
            return {}

    def update_result_data (self, latest_dump):
        """
        Integrates a `latest_dump` dictionary into the CTRexResult object.

        :parameters:        
            latest_dump : dict
                a dictionary with the items desired to be integrated into the object history and stats

        :return: 
            None

        """
        # add latest dump to history
        if latest_dump != {}:
            self._history.append(latest_dump)
            if not self.valid:
                self.valid = True 

            # parse important fields and calculate averages and others
            if self._expected_tx_rate is None:
                # get the expected data only once since it doesn't change
                self._expected_tx_rate = CTRexResult.__get_value_by_path(latest_dump, "trex-global.data", "m_tx_expected_\w+")

            self._current_tx_rate = CTRexResult.__get_value_by_path(latest_dump, "trex-global.data", "m_tx_(?!expected_)\w+")
            if not self._done_warmup and self._expected_tx_rate is not None:
                # check for up to 2% change between expected and actual
                if (self._current_tx_rate['m_tx_bps']/self._expected_tx_rate['m_tx_expected_bps'] > 0.98):
                    self._done_warmup = True
            
            # handle latency data
            if self.latency_checked:
                latency_pre = "trex-latency"
                self._max_latency = self.get_last_value("{latency}.data".format(latency = latency_pre), ".*max-")#None # TBC
                # support old typo
                if self._max_latency is None:
                    latency_pre = "trex-latecny"
                    self._max_latency = self.get_last_value("{latency}.data".format(latency = latency_pre), ".*max-")

                self._avg_latency = self.get_last_value("{latency}.data".format(latency = latency_pre), "avg-")#None # TBC
                self._avg_latency = CTRexResult.__avg_all_and_rename_keys(self._avg_latency)

                avg_win_latency_list     = self.get_value_list("{latency}.data".format(latency = latency_pre), "avg-")
                self._avg_window_latency = CTRexResult.__calc_latency_win_stats(avg_win_latency_list)

            tx_pkts = CTRexResult.__get_value_by_path(latest_dump, "trex-global.data.m_total_tx_pkts")
            rx_pkts = CTRexResult.__get_value_by_path(latest_dump, "trex-global.data.m_total_rx_pkts")
            if tx_pkts is not None and rx_pkts is not None:
                self._total_drops = tx_pkts - rx_pkts
            self._drop_rate   = CTRexResult.__get_value_by_path(latest_dump, "trex-global.data.m_rx_drop_bps")

    def clear_results (self):
        """
        Clears all results and sets the history's validity to `False`

        :parameters:        
            None

        :return: 
            None

        """
        self.valid               = False
        self._done_warmup        = False
        self._expected_tx_rate   = None
        self._current_tx_rate    = None
        self._max_latency        = None
        self._avg_latency        = None
        self._avg_window_latency = None
        self._total_drops        = None
        self._drop_rate          = None
        self._history.clear()

    @staticmethod
    def __get_value_by_path (dct, tree_path, regex = None):
        try:
            for i, p in re.findall(r'(\d+)|([\w|-]+)', tree_path):
                dct = dct[p or int(i)]
            if regex is not None and isinstance(dct, dict):
            	res = {}
            	for key,val in dct.items():
            		match = re.match(regex, key)
            		if match:
            			res[key]=val
            	return res
            else:
               return dct
        except (KeyError, TypeError):
            return None

    @staticmethod
    def __calc_latency_win_stats (latency_win_list):
        res = {'all' : None }
        port_dict = {'all' : []}
        list( map(lambda x: CTRexResult.__update_port_dict(x, port_dict), latency_win_list) )

        # finally, calculate everages for each list
        res['all'] = float("%.3f" % (sum(port_dict['all'])/float(len(port_dict['all']))) )
        port_dict.pop('all')
        for port, avg_list in port_dict.items():
            res[port] = float("%.3f" % (sum(avg_list)/float(len(avg_list))) )

        return res

    @staticmethod
    def __update_port_dict (src_avg_dict, dest_port_dict):
        all_list = src_avg_dict.values()
        dest_port_dict['all'].extend(all_list)
        for key, val in src_avg_dict.items():
            reg_res = re.match("avg-(\d+)", key)
            if reg_res:
                tmp_key = "port"+reg_res.group(1)
                if tmp_key in dest_port_dict:
                    dest_port_dict[tmp_key].append(val)
                else:
                    dest_port_dict[tmp_key] = [val]

    @staticmethod
    def __avg_all_and_rename_keys (src_dict):
        res       = {}
        all_list  = src_dict.values()
        res['all'] = float("%.3f" % (sum(all_list)/float(len(all_list))) )
        for key, val in src_dict.items():
            reg_res = re.match("avg-(\d+)", key)
            if reg_res:
                tmp_key = "port"+reg_res.group(1)
                res[tmp_key] = val  # don't touch original fields values
        return res



if __name__ == "__main__":
    pass

