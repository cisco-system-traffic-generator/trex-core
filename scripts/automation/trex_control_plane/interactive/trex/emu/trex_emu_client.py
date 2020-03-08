from ..astf.arg_verify import ArgVerify
from ..common.trex_api_annotators import client_api, plugin_api
from ..common.trex_ctx import TRexCtx
from ..common.trex_exceptions import *
from ..common.trex_logger import ScreenLogger
from ..common.trex_types import *
from ..utils import parsing_opts, text_tables
from ..utils.common import *
from ..utils.text_opts import format_text, format_num
from ..utils.text_tables import *

from .trex_emu_conn import RRConnection
from collections import OrderedDict, defaultdict
from contextlib import contextmanager
from .emu_plugins.emu_plugin_base import *
from .trex_emu_counters import *
from .trex_emu_profile import *
from .trex_emu_conversions import *

import glob
import imp
import inspect
import os
import math
import pprint
import sys
import time
import yaml


NO_ITEM_SYM = '-'

class SimplePolicer:

    def __init__(self, cir):
        self.cir = cir
        self.last_time = 0

    def update(self, size):
        now_sec = time.time()

        if self.last_time == 0:
            self.last_time = now_sec
            return True

        if self.cir == 0:
            return False  # No rate

        sent_clients = (now_sec - self.last_time) * self.cir
        if sent_clients > size:
            self.last_time = now_sec
            return True
        else:
            return False

CHUNK_SIZE = 256  # size of each data chunk sending to emu server

############################     helper     #############################
############################     funcs      #############################
############################                #############################

def divide_into_chunks(lst, n):
    """
        Yield successive n-sized chunks from lst.
        :parameters:
            lst: list
                List of elements.
            n: int
                Length of each chunk to yield.
    """
    if n < len(lst):
        for i in range(0, len(lst), n):
            yield lst[i:i + n]
    else:
        yield lst

def dump_json_yaml(data, to_json, to_yaml, ident_size = 4):
    """
        Prints data into json or yaml format. 

            :parameters:
                data: list
                    List of data.
                to_json: bool
                    Print in Json format.
                to_yaml: bool
                    Print in Yaml format.
                ident_size: int
                    Number of indentation, defaults to 4.
            :raises:
                None
            :returns:
                bool: True
    """
    if to_json:
        print(json.dumps(data, indent = ident_size))
        return True
    elif to_yaml:
        print(yaml.safe_dump(data, allow_unicode = True, default_flow_style = False, indent = ident_size))
        return True

class EMUClient(object):
    """
        Request Response abstract client. Does not support Async events like TRex Client 

        contains common code , only Emulation client will use it EMUClient
    """
    
    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4510,
                 verbose_level = "error",
                 logger = None,
                 sync_timeout = None):

        api_ver = {'name': 'EMU', 'major': 0, 'minor': 1}

        # logger
        logger = logger if logger is not None else ScreenLogger()
        logger.set_verbose(verbose_level)

        # first create a TRex context
        self.ctx = TRexCtx(api_ver,
                           username,
                           server,
                           sync_port,
                           None,
                           logger,
                           sync_timeout,
                           None)

        # init objects
        self.ports           = {}

        # connection state object
        self.conn = RRConnection(self.ctx)

        # used for converting from bytes to str and vice versa
        self.client_keys_map = {'ipv4': {'delim': '.', 'pad_zeros': False, 'format_type': 'd'},
                            'ipv4_dg': {'delim': '.', 'pad_zeros': False, 'format_type': 'd'},
                            'mac': {'delim': ':', 'pad_zeros': True},
                            'ipv6': {'delim': ':', 'pad_zeros': True, 'group_bytes': 2}
                        }
        
        # used for ctx counter meta data
        self.meta_data = None

        # emu client holds every emu plugin
        self.general_data_c = DataCounter(self.conn, 'ctx_cnt')

    def __del__(self):
        self.disconnect()

    def get_mode (self):
        """
            return the mode/type for the client
        """
        return 'emulation'
############################    abstract   #############################
############################    functions  #############################
############################               #############################

    def _on_connect (self):
        """
            for deriving objects

            actions performed when connecting to the server

        """



############################     private     #############################
############################     functions   #############################
############################                 #############################

    # transmit request on the RPC link
    def _transmit(self, method_name, params = None):
        return self.conn.rpc.transmit(method_name, params)

    # execute 'method' for a port list
    def _for_each_port (self, method, port_list = None, *args, **kwargs):

        # specific port params
        pargs = {}

        if 'pargs' in kwargs:
            pargs = kwargs['pargs']
            del kwargs['pargs']


        # handle single port case
        if port_list is not None:
            port_list = listify(port_list)

        # none means all
        port_list = port_list if port_list is not None else self.get_all_ports()
        
        rc = RC()

        for port in port_list:

            port_id = int(port)
            profile_id = None
            if isinstance(port, PortProfileID):
                profile_id = port.profile_id

            # get port specific
            pkwargs = pargs.get(port_id, {})
            if pkwargs:
                pkwargs.update(kwargs)
            else:
                pkwargs = kwargs

            if profile_id:
                pkwargs.update({'profile_id': profile_id})

            func = getattr(self.ports[port_id], method)
            rc.add(func(*args, **pkwargs))


        return rc


    # connect to server
    def _connect(self):

        # connect to the server
        rc = self.conn.connect()
        if not rc:
            return rc

        # version
        rc = self._transmit("get_version")
        if not rc:
            return rc

        self.ctx.server_version = rc.data()

        return RC_OK()

    # disconenct from server
    def _disconnect(self, release_ports = True):
        # release any previous acquired ports
        if self.conn.is_connected() and release_ports:
            with self.ctx.logger.suppress():
                try:
                    self.release()
                except TRexError:
                    pass

        # disconnect the link to the server
        self.conn.disconnect()

        return RC_OK()

    def _send_chunks(self, cmd, data_key, data, chunk_size = CHUNK_SIZE, max_data_rate = None, add_data = None, track_progress = False):
        """
            Send `cmd` to EMU server with `data` over ZMQ separated to chunks using `max_data_rate`.
            i.e: self._send_chunks('ctx_client_add', data_key = 'clients', data = clients_data, max_data_rate = 100, add_data = namespace)
            will send add client request, with a rate of 100 clients
            
            :parameters:
                cmd: string
                    Name of command.
                
                data_key: string
                    Key for data, i.e: `tun` for namespace sending.

                data: list
                    List with data to send, i.e: [{key: value..}, ... ,{key: value}]
                    This will be separated into chunks according to max_data_rate.
                
                chunk_size: int
                    Size of every chunk to send.

                max_data_rate: int
                    Rate as an upper bound of data information per second.

                add_data: dictionary
                    Dictionary representing additional data to send in each separate chunk.
                    i.e: {'tun': {'vport': 10, ...}}   

                track_progress: bool
                    True will show a progress bar of the sending, defaults to False.
        
            :raises:
                + :exc: `TRexError`
        """        
        def _create_params(data):
            if add_data is not None:
                params = dict(add_data)
            else:
                params = {}
            params.update({data_key: data})
            return params

        # unlimited rate send everyting
        if max_data_rate is None:
            chunk_size = len(data)
            max_data_rate = chunk_size

        # policer = SimplePolicer(cir = max_data_rate)
        total_chunks = int(math.ceil(len(data) / chunk_size)) if chunk_size != 0 else 1
        total_chunks = total_chunks if total_chunks != 0 else 1
        if track_progress:
            print('')

        c = 0
        for chunk_i, chunked_data in enumerate(divide_into_chunks(data, chunk_size)):
            c += 1
            params = _create_params(chunked_data)

            # TODO remove policer for now
            # while not policer.update(len(chunked_data)):
            #     # don't send more requests, let the server process the old ones
            #     time.sleep(0.1)

            rc = self._transmit(method_name = cmd, params = params)

            if not rc:
                self.err(rc.err())
            
            if track_progress:
                sys.stdout.write((' ' * 50) + '\r')  # clear line
                dots = '.' * (chunk_i % 5)
                text = "Sending data: %s out of %s chunks %s\r" % (chunk_i + 1, total_chunks, dots)
                sys.stdout.write(format_text(text, 'yellow'))
                sys.stdout.flush()
        
        if track_progress:
            print('')

        return RC_OK()

    def _iter_ns(self, cmd, count, reset = False, **kwargs):
        """
            Iterate namespace on EMU server with a given cmd.
        
            :parameters:
                cmd: string
                    Name of the command.

                count: int
                    Number of namespaces to iterate in this request.

                reset: bool
                    True if iterator should start from the beggining, defaults to False.
                
                kwargs: dictionary
                    Additional arguments to send with the command.
        
            :raises:
                + :'TRexError'
        
            :returns:
                Returns the data of server response.
        """
        ver_args = {'types':
            [{'name': 'count', 'arg': count, 't': int},
            {'name': 'reset', 'arg': reset, 't': bool}]
            }
        params = {'count': count, 'reset': reset}
        params.update(kwargs)
        ArgVerify.verify(self.__class__.__name__, ver_args)

        rc = self._transmit(cmd, params)

        if not rc:
            raise TRexError(rc.err())

        return rc.data()

    def _client_to_bytes(self, c):
        """
        Convert client into bytes, ready for sending to emu server. 
        
            :parameters:
                c: dictionary
                    Valid client dictionary.
        """        
        for key in c:
            if c[key] is None:
                continue
            try:
                c[key] = conv_to_bytes(c[key], key)
            except Exception as e:
                print("Cannot convert client's key: %s: %s\nException:\n%s" % (key, c[key], str(e)))

    def _transmit_and_get_data(self, method_name, params):
        """
        Trassmit method name with params and returns the data. 
        
            :parameters:
                method_name: string
                    Name of method to send.
                params: dictionary
                    Parameters for method.
        
            :returns:
                Data recived from emu server.
        """
        rc = self._transmit(method_name, params)

        if not rc:
            self.err(rc.err())
        return rc.data()
    
    def _wait_for_pressed_key(self, msg):
        """
        Wait for enter key. Works with Python 2 & 3.
        
            :parameters:
                msg: string
                    Message to show until user will press enter.
        
        """
        try:
            raw_input(msg)
        except NameError:
            input(msg)

    # Print table
    def print_dict_as_table(self, data, title = None):
        """
        Print dictionary as a human readable table. 
        
            :parameters:
                data: dictionary
                    Data to show.
                title: string
                    Title to show above data, defaults to None means no title.
        """
        unconverted_keys = get_val_types()

        for k in unconverted_keys:
            if k in data:
                data[k] = conv_to_str(data[k], k)

        headers = list(data.keys())
        table = text_tables.TRexTextTable('' if title is None else title)
        table.header(headers)

        table_data = []
        for h in headers:
            h_value = data.get(h, '-')
            table_data.append(str(h_value))

        table.add_row(table_data)

        table.set_cols_align(['c'] * len(headers))
        table.set_cols_width([max(len(h), len(str(d))) for h, d in zip(headers, table_data)])
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)


    def err(self, msg):
        """
        Uses TRexError object as an error. 
        
            :parameters:
                msg: string
                    Message to show with Error.
        
            :raises:
                + :exe:'TRexError'
        """        
        raise TRexError(msg)

############################   Getters   #############################
############################             #############################
############################             #############################
    
    @client_api('getter', False)
    def probe_server (self):
        """
        Probe the server for the version / mode

        Can be used to determine stateless or advanced statefull mode

        :parameters:
          none

        :return:
          dictionary describing server version and configuration

        :raises:
          None

        """

        rc = self.conn.probe_server()
        if not rc:
            raise TRexError(rc.err())
        
        return rc.data()


    @property
    def logger (self):
        """
        Get the associated logger

        :parameters:
          none

        :return:
            Logger object

        :raises:
          None
        """

        return self.ctx.logger


    @client_api('getter', False)
    def get_verbose (self):
        """ 
        Get the verbose mode  

        :parameters:
          none

        :return:
            Get the verbose mode as Bool

        :raises:
          None

        """

        return self.ctx.logger.get_verbose()

    @client_api('getter', True)
    def get_server_supported_cmds(self):
        """ 

        :parameters:
          None

        :return:
            List of commands supported by server RPC

        :raises:
          None

        """

        return self.supported_cmds

    @client_api('getter', True)
    def get_server_version(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.ctx.server_version

    @client_api('getter', True)
    def get_server_system_info(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.ctx.system_info

    @property
    def system_info (self):
        # provided for backward compatability
        return self.ctx.system_info

    @client_api('getter', True)
    def get_port_count(self):
        """ 

        :parameters:
          None

        :return:
            Number of ports

        :raises:
          None

        """

        return len(self.ports)

    @client_api('getter', True)
    def get_port (self, port_id):
        """

        :parameters:
          port_id - int

        :return:
            Port object

        :raises:
          None

        """
        port = self.ports.get(port_id, None)
        if (port != None):
            return port
        else:
            raise TRexArgumentError('port id', port_id, valid_values = self.get_all_ports())

    @client_api('command', False)
    def set_timeout(self, timeout_sec):
        '''
            Set timeout for connectivity to TRex server. Must be not connected.

            :parameters:
                timeout_sec : int or float
                    | Timeout in seconds for sync operations.
                    | If async data does not arrive for this period, disconnect.

            :raises:
                + :exc:`TRexError` - in case client is already connected.
        '''

        validate_type('timeout_sec', timeout_sec, (int, float))
        if timeout_sec <= 0:
            raise TRexError('timeout_sec must be positive')
        if self.is_connected():
            raise TRexError('Can set timeout only when not connected')
        self.conn.rpc.set_timeout_sec(timeout_sec)

    @client_api('command', False)
    def connect (self):
        """

            Connects to the Emulation server

            :parameters:
                None

            :raises:
                + :exc:`TRexError`

        """
        
        # cleanup from previous connection
        self._disconnect()
        
        rc = self._connect()
        if not rc:
            self._disconnect()
            raise TRexError(rc.err())
        
    @client_api('command', True)
    def acquire(self, force = False):
        pass

    @client_api('command', True)
    def release(self, force = False):
        pass

    @client_api('command', False)
    def disconnect (self, release_ports = True):
        """
            Disconnects from the server

            :parameters:
                release_ports : bool
                    Attempts to release all the acquired ports.

        """

        self.ctx.logger.pre_cmd("Disconnecting from server at '{0}':'{1}'".format(self.ctx.server,
                                                                                  self.ctx.sync_port))
        rc = self._disconnect(release_ports)
        self.ctx.logger.post_cmd(rc)

    @client_api('command', True)
    def ping_rpc_server(self):
        """
            Pings the RPC server

            :parameters:
                 None

            :return:
                 Timestamp of server

            :raises:
                + :exc:`TRexError`

        """
        self.ctx.logger.pre_cmd("Pinging the server on '{0}' port '{1}': ".format(self.ctx.server, self.ctx.sync_port))
        rc = self._transmit("ping")

        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc.err())

        return rc.data()

    @client_api('getter', True)
    def yield_n_items(self, cmd, amount = None, retry_num = 1, **kwargs):
        """
            Gets n items by calling `cmd`, iterating items from EMU server.

            :parameters:
                cmd: string
                    name of the iterator command

                amount: int
                    Amount of data to fetch, None is default means all.
                
                retry_num: int
                    Number of retries in case of failure, Default is 1.

            :return:
                yield list of amount namespaces keys i.e:[{u'tci': [1, 0], u'tpid': [33024, 0], u'vport': 1},
                                                    {u'tci': [2, 0], u'tpid': [33024, 0], u'vport': 1}]

            :raises:
                + :exc:`TRexError`
        """
        CHUNK_SIZE = 255  # limited by EMU server
        done, failed_num = False, 0

        while not done:

            reset = True

            while True:
                res = self._iter_ns(cmd, amount if amount is not None else CHUNK_SIZE, reset = reset, **kwargs)
                if 'error' in res.keys():
                    failed_num += 1
                    break
                if res['stopped'] or res['empty']:
                    done = True
                    break
                yield res['data']
                reset = False  # reset only at start
            
            if failed_num == retry_num:
                self.err("Reached max retry number, operation failed")

    def get_n_items(self, cmd, amount = None, retry_num = 1, **kwargs):
        """
            Gets requested number of elements from emu server using a given command. 
        
            :parameters:
                cmd: string
                    Command to send.
                amount: int
                    Amount of data we want to receive, defaults to None means everything.
                retry_num: int
                    Number of retries before throwing error, defaults to 1.
        
            :raises:
                None

            :returns:
                list: All the requested data. 
        """        
        res = []
        gen = self.yield_n_items(cmd, amount, retry_num, **kwargs)
        for item in gen:
            res.extend(item)
        return res

    def get_n_ns(self, amount = None):
        """
        Gets n namespaces. 
        
            :parameters:
                amount: int
                    Amount of namespaces to request from server, defaults to None means everything.
        
            :raises:
                + :exc:`TRexError`

            :returns:
                list: List of namespaces.
        """        
        return self.yield_n_items('ctx_iter', amount = amount)

    def get_all_ns_and_clients(self):
        """
        Gets all the namespaces and clients from emu server. 
        
            :returns:
                list: List of all namespaces and clients.
        """        
        ns_list = self.get_all_ns()
        for ns in ns_list:
            macs_in_ns = self.get_all_clients_for_ns(ns)
            ns['clients'] = self.get_info_client(ns, macs_in_ns)
            ns['tpid'] = [hex(t) for t in ns['tpid']]
        return ns_list

    def get_all_ns(self):
        """
        Gets all the namespaces. 
                
            :returns:
                List: List of all namespaces.
        """        
        res = []
        for ns in self.get_n_ns(amount = None):
            res.extend(ns)
        return res

    @client_api('getter', True)    
    def get_info_ns(self, namespaces):
        """
            Return information about a given namespace.
        
            :parameters:
                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
        
            :raises:
                + :exe:'TRexError': [description]
        
            :return:
                List of all namespaces with additional information i.e:[{'tci': [1, 0], 'tpid': [33024, 0], 'vport': 1,
                                                                        'number of plugins': 2, 'number of clients': 5}]
        """
        ver_args = {'types':
                [{'name': 'namespaces', 'arg': namespaces, 't': dict, 'allow_list': True},]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        
        namespaces = listify(namespaces)
        params = {'tunnels': namespaces}

        rc = self._transmit('ctx_get_info', params)

        if not rc:
            raise TRexError(rc.err())
        return rc.data()

    def get_all_clients_for_ns(self, namespace):
        """
        Gets all clients for a given namespace. 
        
            :parameters:
                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
        
            :returns:
                list: List of all clients in namespace.
        """        
        res = []
        for c in self.yield_n_items(cmd = 'ctx_client_iter', amount = None, tun = namespace):
            res.extend(c)
        return res

    @client_api('getter', True)    
    def get_info_client(self, namespace, macs):
        """
            Get information for a list of clients from a specific namespace.

            :parameters:
                namespaces: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
                
                macs: list
                    list of dictionaries with the form: {'mac': '00:01:02:03:04:05', 'ipv4': '1.1.1.3', 'ipv4_dg':'1.1.1.2', 'ipv6': '00:00:01:02:03:04'}
                    `mac` is the only required field.
            
            :return:
                list of dictionaries with data for each client, the order is the same order of macs.
        """
        
        ver_args = {'types':
                [{'name': 'namespace', 'arg': namespace, 't': dict},
                {'name': 'macs', 'arg': macs, 't': list}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        
        # convert mac address to bytes if needed
        if len(macs) > 0 and ':' in macs[0]: 
            macs = [conv_to_bytes(m, 'mac') for m in macs]

        params = {'tun': namespace, 'macs': macs}

        rc = self._transmit('ctx_client_get_info', params)

        if not rc:
            raise TRexError(rc.err())

        # converting data from bytes to str
        for d in rc.data():
            for key, value in d.items():
                d[key] = conv_to_str(value, key)
        return rc.data()

    # CTX Counters
    @client_api('getter', True)
    def get_counters(self, meta = False, zero = False, mask = None, clear = False):
        """
        Get global counters of emu server. 
        
            :parameters:
                meta: bool
                    True will get the meta data, defaults to False.
                zero: bool
                    True will send also zero values in request, defaults to False.
                mask: list of strings
                    List of wanted tables, defaults to None means all.
                clear: bool
                    Clear the counters and exit, defaults to False.
        
            :returns:
                List: List of all wanted counters.
        """        
        return self.general_data_c._get_counters(meta, zero, mask, clear)

    # Ns & Client Table Printing
    @client_api('getter', True)
    def print_all_ns_clients(self, max_ns_show, max_c_show , to_json = False, to_yaml = False, ipv6 = False):
        """
            Prints all the namespaces and clients in tables.

            :parameters:

                max_ns_show: int
                    Max namespace to show each time.

                max_c_show: int
                    Max clients to show each time.

                to_json: bool
                    Print the data in a json format.
                
                to_yaml: bool
                    Print the data in a yaml format.

            :return:
                None
        """

        if to_json or to_yaml:
            # gets all data, no pauses
            all_data = self.get_all_ns_and_clients()
            return dump_json_yaml(all_data, to_json, to_yaml)

        all_ns_gen = self.get_n_ns(amount = max_ns_show)

        glob_ns_num = 0
        for ns_chunk_i, ns_list in enumerate(all_ns_gen):
            ns_infos = self.get_info_ns(ns_list)

            if ns_chunk_i != 0:
                self._wait_for_pressed_key('Press Enter to see more namespaces')
            
            for ns_i, ns in enumerate(ns_list):
                glob_ns_num += 1

                # add ns info to current namespace
                ns_info = ns_infos[ns_i]
                ns.update(ns_info)

                # print namespace table
                self._print_ns_table(ns, ns_num = glob_ns_num)

                for client_i, clients_mac in enumerate(self.yield_n_items(cmd = 'ctx_client_iter', amount = max_c_show, tun = ns)):                    
                    is_first_time = client_i == 0
                    if not is_first_time != 0:
                        self._wait_for_pressed_key("Press Enter to see more clients")
                    ns_clients = self.get_info_client(ns, clients_mac)
                    self._print_clients_table(ns_clients, print_header = is_first_time, ipv6 = ipv6)
        
        if not glob_ns_num:
            text_tables.print_colored_line('There are no namespaces', 'yellow', buffer = sys.stdout)      

    def _print_ns_table(self, ns, ns_num = None):
        """
        Print namespace table. 
        
            :parameters:
                ns: dictionary
                    Namespace as a dictionary.
                ns_num: int
                    Number of namespace to print as a title, defaults to None.
                
            :returns:
                None.
        """        
        headers = ['Port', 'Vlan tags', 'Tpids', '#Plugins' , '#Clients']
        table = text_tables.TRexTextTable('Namespace information' if ns_num is None else 'Namespace #%s information' % ns_num)
        table.header(headers)

        def check_zeros_val(key):
            if key not in ns or all([t == 0 for t in ns[key]]):
                return NO_ITEM_SYM
            else:
                return [hex(t) for t in ns[key]]

        correct_tpids = check_zeros_val('tpid')
        correct_vlan = check_zeros_val('tci')

        data = [
                ns.get('vport', NO_ITEM_SYM),
                correct_vlan,
                correct_tpids,
                ns.get('plugins_count', NO_ITEM_SYM),
                ns.get('active_clients', NO_ITEM_SYM),
        ]
        table.add_row(data)

        table.set_cols_align(['c'] * len(headers))
        table.set_cols_width([max(len(h), len(str(d))) for h, d in zip(headers, data)])
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)

    def _print_clients_table(self, clients, print_header = True, ipv6 = False):
        """
        Print clients table. 
        
            :parameters:
                clients: list
                    List with clients data.
                print_header: bool
                    True will print title, defaults to True.
                ipv6: bool
                    [description], defaults to False.
        
            :raises:
                + :exe:'TRexError'
        
            :returns:
                None
        """
        def _is_header_empty(key, empty_val):
            return all([c.get(key) == empty_val for c in clients])

        def _remove_empty_headers(values):
            for val in values:
                key, header, empty_val = val['key'], val['header'], val['empty_val'] 
                if not _is_header_empty(key, empty_val):
                    headers.append(header)
                    client_keys.append(key)

        if len(clients) == 0:
            text_tables.print_colored_line("There are not clients for this namespace", 'yellow', buffer = sys.stdout)      
            return

        table = text_tables.TRexTextTable('Clients information')
        headers = ['MAC']
        client_keys = ['mac']

        values = [{'key': 'ipv4', 'header': 'IPV4' , 'empty_val': '0.0.0.0'},
                {'key': 'ipv4_dg', 'header': 'Deafult Gateway', 'empty_val': '0.0.0.0'},
                {'key': 'ipv6', 'header': 'IPV6' , 'empty_val': '::'}
        ]
        _remove_empty_headers(values)

        max_lens = [len(h) for h in headers]
        table.header(headers)

        for i, client in enumerate(clients):            
            client_row_data = []
            for j, key in enumerate(client_keys):
                val = client.get(key, NO_ITEM_SYM)
                client_row_data.append(val)
                max_lens[j] = max(max_lens[j], len(val))
            table.add_row(client_row_data)

        # fix table look
        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))
        
        text_tables.print_table_with_header(table, table.title if print_header else '', buffer = sys.stdout)

    # Plugins CFG
    def send_plugin_cmd_to_ns(self, cmd, port, vlan = None, tpid = None, **kwargs):
        """
        Generic function for plugins, send `cmd` to a given namespace i.e get_cfg for plugin. 
        
            :parameters:
                cmd: string
                    Command to send.
                port: int
                    Port of namespace.
                vlan: list
                    List of vlan tags, defaults to None.
                tpid: list
                    List of vlan tpids, defaults to None.
        
            :returns:
                The requested data for command.
        """        
        params = conv_ns_for_tunnel(port, vlan, tpid)
        params.update(kwargs)
        return self._transmit_and_get_data(cmd, params)

    # Default Plugins
    @client_api('getter', True)
    def get_def_ns_plugs(self):
        ''' Gets the namespace default plugin '''
        return self._transmit_and_get_data('ctx_get_def_plugins', None)

    @client_api('getter', True)
    def get_def_c_plugs(self, port, vlan = None, tpid = None):
        ''' Gets the client default plugin in a specific namespace '''
        params = conv_ns_for_tunnel(port, vlan, tpid)
        return self._transmit_and_get_data('ctx_client_get_def_plugins', params)

############################       EMU      #############################
############################       API      #############################
############################                #############################
    @client_api('command', False)
    def set_verbose (self, level):
        """
            Sets verbose level

            :parameters:
                level : str
                    "none" - be silent no matter what
                    "critical"
                    "error" - show only errors (default client mode)
                    "warning"
                    "info"
                    "debug" - print everything

            :raises:
                None

        """
        validate_type('level', level, basestring)

        self.ctx.logger.set_verbose(level)

    @client_api('command', True)
    def add_ns(self, namespaces):
        """
            Add namespaces to EMU server.

            :parameters:
                namespaces: list
                    List of dictionaries with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
    
            :raises:
                + :exc:`TRexError`
        """
        ver_args = {'types':
                [{'name': 'namespaces', 'arg': namespaces, 't': list},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
                
        self._send_chunks('ctx_add', data_key = 'tunnels', data = namespaces)

        return RC_OK()

    @client_api('command', True)
    def remove_ns(self, namespaces):
        """
            Remove namespaces from EMU server.

            :parameters:
                namespaces: list
                    list of dictionaries with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}

            :raises:
                + :exc:`TRexError`
        """

        self._send_chunks('ctx_remove', data_key = 'tunnels', data = namespaces)

        return RC_OK()

    @client_api('command', True)
    def add_clients(self, namespace, clients, max_rate = None):
        """
            Add client to EMU server.

            :parameters:

                namespaces: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
                
                clients: list
                    List of dictionaries with the form: {'mac': '00:01:02:03:04:05', 'ipv4': '1.1.1.3', 'ipv4_dg':'1.1.1.2', 'ipv6': '00:00:01:02:03:04'}
                    `mac` is the only required field.

                max_rate: int
                    Max rate of clients, measured by number of clients per second. Default is all clients in 1 request.

            :raises:
                + :exc:`TRexError`
        """
        ver_args = {'types':
                [{'name': 'namespace', 'arg': namespace, 't': dict},
                {'name': 'clients', 'arg': clients, 't': list}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        for c in clients:
            c = self._client_to_bytes(c)

        self._send_chunks('ctx_client_add', add_data = {'tun': namespace}, data_key = 'clients',
                            data = clients, max_data_rate = max_rate, track_progress = True)

        return RC_OK()

    @client_api('command', True)
    def remove_clients(self, namespace, macs, max_rate):
        """
            Remove clients from a specific namespace.

            :parameters:
                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}

                macs: list
                    List of clients macs to remove.
                    i.e: ['00:aa:aa:aa:aa:aa', '00:aa:aa:aa:aa:ab']
                
                max_rate: int
                    Max rate of clients to send each time, measured by clients / sec.

            :raises:
                + :exc:`TRexError`
        """
        ver_args = {'types':
                [{'name': 'namespace', 'arg': namespace, 't': dict},
                {'name': 'macs', 'arg': macs, 't': list}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_client_remove', add_data = {'tun': namespace}, data_key = 'macs', data = macs, max_data_rate = max_rate)

        return RC_OK()

    @client_api('command', True)
    def remove_all_clients_and_ns(self, max_rate = None):
        """ 
            Remove all current namespaces and their clients from emu server 

            :parameters:
                max_rate: int
                    Max rate of clients to send each time, measured by clients / sec.
        """
        ns_keys = self.get_all_ns()

        for ns_key in ns_keys:
            clients = self.get_all_clients_for_ns(ns_key)
            self.remove_clients(ns_key, clients, max_rate)
        
        self.remove_ns(list(ns_keys))
        return RC_OK()


    # Default Plugins
    @client_api('command', True)
    def set_def_ns_plugs(self, def_plugs):
        """
            Set the namespace default plugins. Every new namespace in that profile will have that plugin.

            :parameters:

                def_plugs: dictionary
                    Map plugin_name -> plugin_data, each plugin here will be added to every new namespace.
                    If new namespace will provide a plugin, it will override the default one. 
        """
        ver_args = {'types':
                [{'name': 'def_plugs', 'arg': def_plugs, 't': dict},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_set_def_plugins', data_key = 'def_plugs', data = def_plugs)

        return RC_OK()

    @client_api('command', True)
    def set_def_c_plugs(self, namespace, def_plugs):
        """
            Set the client default plugins. Every new client in that namesapce will have that plugin.

            :parameters:

                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}

                def_plugs: dictionary
                    Map plugin_name -> plugin_data, each plugin here will be added to every new client.
                    If new client will provide a plugin, it will override the default one. 
        """
        ver_args = {'types':
                [{'name': 'def_plugs', 'arg': def_plugs, 't': dict},
                {'name': 'namespace', 'arg': namespace, 't': dict},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_client_set_def_plugins', add_data = {'tun': namespace}, data_key = 'def_plugs', data = def_plugs)

        return RC_OK()
    
    # Emu Profile
    @client_api('command', True)
    def load_profile(self, filename, max_rate, tunables):
        """
            Load emu profile from profile by its type. Supported type for now is .py

            :parameters:
                filename : string
                    filename (with path) of the profile

                tunables : string
                    tunables line. i.e: ".. -t --ns 1 --clients 10"

                max_rate : int
                    max clients rate to send (clients/sec), "None" means all clients in 1 request.

            :raises:
                + :exc:`TRexError`
        """
        if '-h' in tunables:
            # don't print external messages on help
            profile = EMUProfile.load(filename, tunables)
            return

        s = time.time()
        self.ctx.logger.pre_cmd("Converting file to profile")
        profile = EMUProfile.load(filename, tunables)
        if profile is None:            
            self.ctx.logger.post_cmd(False)
            self.err('Failed to convert EMU profile')
        self.ctx.logger.post_cmd(True)
        print("Converting profile took: %.3f [sec]" % (time.time() - s))
        self._start_profile(profile, max_rate)

    def _start_profile(self, profile, max_rate):
        """
            Start EMU profile with all the required commands to the server.

            :parameters:
                profile: EMUProfile
                    Emu profile, define all namespaces and clients.
                
                max_rate : int
                    max clients rate to send (clients/sec)
            :raises:
                + :exc:`TRexError`
        """
        if not isinstance(profile, EMUProfile):
            self.err('Profile must be from `EMUProfile` type, got: %s' % type(profile))

        self.ctx.logger.pre_cmd('Sending emu profile')
        s = time.time()
        try:
            # make sure there are no clients and ns in emu server
            self.remove_profile(max_rate)

            # set the default ns plugins
            if profile.def_ns_plugs is not None:
                self.set_def_ns_plugs(profile.def_ns_plugs)

            # adding all the namespaces in profile
            self.add_ns(profile.serialize_ns())

            # adding all the clients for each namespace
            for ns in profile.ns_list:
                # set client default plugins 
                if ns.def_c_plugs is not None:
                    self.set_def_c_plugs(ns.conv_to_key(), ns.def_c_plugs)
                self.add_clients(ns.fields, ns.get_clients(), max_rate = max_rate)
        except Exception as e:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Could not load profile, error: %s' % str(e))
        self.ctx.logger.post_cmd(True)
        print("Sending profile took: %.3f [sec]" % (time.time() - s))

    @client_api('command', True)
    def remove_profile(self, max_rate = None):
        """ Remove current profile from emu server, all namespaces and clients will be removed """
        self.ctx.logger.pre_cmd('Removing old emu profile')
        self.remove_all_clients_and_ns(max_rate)
        self.ctx.logger.post_cmd(True)
        return RC_OK()

############################   console   #############################
############################   commands  #############################
############################             #############################

    @plugin_api('load_profile', 'emu')
    def load_profile_line(self, line):
        '''Load a given profile to emu server\n'''
        
        parser = parsing_opts.gen_parser(self,
                                        "load_profile",
                                        self.load_profile_line.__doc__,
                                        parsing_opts.FILE_PATH,
                                        parsing_opts.MAX_CLIENT_RATE,
                                        parsing_opts.ARGPARSE_TUNABLES
                                        )
        opts = parser.parse_args(args = line.split())
        self.load_profile(opts.file[0], opts.max_rate, opts.tunables)
        return True

    @plugin_api('remove_profile', 'emu')
    def remove_profile_line(self, line):
        '''Remove current profile from emu server\n'''

        parser = parsing_opts.gen_parser(self,
                                        "remove_profile",
                                        self.remove_profile_line.__doc__,
                                        parsing_opts.MAX_CLIENT_RATE
                                        )

        opts = parser.parse_args(line.split())

        self.remove_profile(opts.max_rate)
        return True

    @plugin_api('show_all', 'emu')
    def show_all_line(self, line):
        '''Show all current namespaces & clients\n'''

        parser = parsing_opts.gen_parser(self,
                                        "show_all",
                                        self.show_all_line.__doc__,
                                        parsing_opts.EMU_MAX_SHOW,
                                        parsing_opts.SHOW_IPV6,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())

        self.print_all_ns_clients(max_ns_show = opts.max_ns, max_c_show = opts.max_clients, to_json = opts.json,
                                    to_yaml = opts.yaml, ipv6 = opts.ipv6)
        return True

    @plugin_api('show_ns_info', 'emu')
    def show_ns_info_line(self, line):
        '''Show namespace information\n'''

        parser = parsing_opts.gen_parser(self,
                                        "show_ns_info",
                                        self.show_ns_info_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())

        ns_info = self.get_info_ns([conv_ns(opts.port, opts.vlan, opts.tpid)])
        if len(ns_info):
            ns_info = ns_info[0]
        else:
            self.err('Cannot find any namespace with requested parameters')
     
        if opts.json or opts.yaml:
            return dump_json_yaml(ns_info, opts.json, opts.yaml)
        
        self._print_ns_table(ns_info)

        return True

    @plugin_api('show_client_info', 'emu')
    def show_client_info_line(self, line):
        '''Show client information\n'''

        parser = parsing_opts.gen_parser(self,
                                        "show_client_info",
                                        self.show_client_info_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT   
                                        )

        opts = parser.parse_args(line.split())
        c_info = self.get_info_client(conv_ns(opts.port, opts.vlan, opts.tpid), [opts.mac])
        if not len(c_info):
            self.err('Cannot find any client with requested parameters')
        else:
            c_info = c_info[0]

        if opts.json or opts.yaml:
            return dump_json_yaml(c_info, opts.json, opts.yaml)

        self._print_clients_table(clients = [c_info], ipv6 = True)

        return True

    def _base_show_counters(self, data_cnt, opts, req_ns = False):
        '''Base function for every counter using `cnt_cmd`'''
        def set_ns_params(port, vlan = None, tpid = None):
            vlan = vlan if vlan is not None else [0, 0]
            tpid = tpid if tpid is not None else [0, 0]
            data_cnt.set_add_ns_data(port, vlan, tpid)
        
        def run_on_demend(data_cnt, opts):
            if opts.headers:
                data_cnt.get_counters_headers()
            elif opts.clear:
                self.ctx.logger.pre_cmd("Clearing all counters")
                data_cnt.clear_counters()
                self.ctx.logger.post_cmd(True)
            else:
                data = data_cnt.get_counters(opts.tables, opts.cnt_types, opts.zero)
                DataCounter.print_counters(data, opts.verbose, opts.json, opts.yaml)

        # reset additional data in data counters
        data_cnt.set_add_ns_data(clear = True)

        if req_ns:
            if opts.port is not None:
                set_ns_params(opts.port, opts.vlan, opts.tpid)
            elif opts.all_ns:

                ns_gen = self.get_n_ns(amount = None)
                ns_i = 0

                for ns_chunk in ns_gen:
                    for ns in ns_chunk:
                        ns_i += 1
                        self._print_ns_table(ns, ns_i)
                        set_ns_params(ns.get('vport'), ns.get('tci'), ns.get('tpid'))
                        run_on_demend(data_cnt, opts)
                if not ns_i:
                    self.err('there are no namespaces in emu server')
                return True
            else:
                raise self.err('namespace information required, supply them or run with --all-ns ')
        
        run_on_demend(data_cnt, opts)

        return True

    @plugin_api('show_counters', 'emu')
    def show_counters_ctx_line(self, line):
        '''Show counters data from ctx according to given tables.\n
           Counter postfix: INFO - no postfix, WARNING - '+', ERROR - '*'\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_ctx",
                                        self.show_counters_ctx_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())

        return self._base_show_counters(self.general_data_c, opts)


    def get_plugin_methods (self):
        ''' Register all plugin methods from all plugins '''
        # Get all emu client plugin methods
        all_plugin_methods = self._get_plugin_methods_by_obj(self)

        # Get all plugins plugin methods
        current_plugins_dict = self._get_plugins()

        for plug_name, filename in current_plugins_dict.items():
            plugin_instance = self._create_plugin_inst_by_name(plug_name, filename)
            setattr(self, plug_name, plugin_instance)  # add plugin to emu client dynamically

            plug_methods = self._get_plugin_methods_by_obj(plugin_instance)
            all_plugin_methods.update(plug_methods)  # add plugin methods from plugin file

        return all_plugin_methods

    def _get_plugin_methods_by_obj(self, obj):
        def predicate (x):
            return inspect.ismethod(x) and getattr(x, 'api_type', None) == 'plugin'
    
        return {cmd[1].name : cmd[1] for cmd in inspect.getmembers(obj, predicate = predicate)}
        
    def _get_plugins(self):
            plugins = {}
            cur_dir = os.path.dirname(__file__)
            emu_plug_prefix = 'emu_plugin_'

            for f in glob.glob(os.path.join(cur_dir, 'emu_plugins', '%s*.py' % emu_plug_prefix)):
                module = os.path.basename(f)[:-3]
                plugin_name = module[len(emu_plug_prefix):]
                if plugin_name and plugin_name != 'base':
                    plugins[plugin_name] = f
            return plugins

    def _create_plugin_inst_by_name(self, name, filename):
            
            import_path = 'trex.emu.emu_plugins'

            try:
                m = imp.load_source(import_path, filename)
            except BaseException as e:
                self.err('Exception during import of %s: %s' % (import_path, e))
            
            # format for plugin name with uppercase i.e: "ARPPlugin"
            plugins_cls_name = '%sPlugin' % name.upper()
            
            plugin_class = vars(m).get(plugins_cls_name)
            if plugin_class is EMUPluginBase:
                self.err("Cannot load EMUPluginBase, inherit from this class") 
            if not issubclass(plugin_class, EMUPluginBase):
                self.err('Plugin "%s" class should inherit from EMUPluginBase' % name) 

            try:
                plug_inst = plugin_class(emu_client = self)
            except BaseException as e:
                self.err('Could not initialize the plugin "%s", error: %s' % (name, e))
            
            return plug_inst