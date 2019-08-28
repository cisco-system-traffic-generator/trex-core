from ..astf.arg_verify import ArgVerify
from ..common.trex_api_annotators import client_api, console_api
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
from trex_emu_profile import *
import pprint
import sys
import time

NO_ITEM_SYM = '-'

class SimplePolicer:

    def __init__(self, cir):
        self.cir = cir
        self.last_time = 0

    def update(self, size, now_sec):
        
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

CHUNK_SIZE = 50000

############################     helper     #############################
############################     funcs      #############################
############################                #############################

def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    if n < len(lst):
        for i in range(0, len(lst), n):
            yield lst[i:i + n]
    else:
        yield lst

def _conv_to_bytes(val, val_type):
    """ 
        Convert `val` to []bytes for EMU server. 
        i.e: '00:aa:aa:aa:aa:aa' -> [0, 170, 170, 170, 170, 170]
    """
    if val_type == 'ipv4' or val_type == 'ipv4_dg':
        return [ord(v) for v in socket.inet_pton(socket.AF_INET, val)]
    elif val_type == 'ipv6':
        return [ord(v) for v in socket.inet_pton(socket.AF_INET6, val)]
    elif val_type == 'mac':
        return [int(v, 16) for v in val.split(':')]
    else:
        raise TRexError("Unsupported key for client: %s" % val_type)

def _conv_to_str(val, delim, group_bytes = 1, pad_zeros = False, format_type = 'x'):
    """
        Convert bytes format to str.

        :parameters:
            val: dictionary
                Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
            
            delim: list
                list of dictionaries with the form: {'mac': '00:01:02:03:04:05', 'ipv4': '1.1.1.3', 'ipv4_dg':'1.1.1.2', 'ipv6': '00:00:01:02:03:04'}
                `mac` is the only required field.
            
            group_bytes: int
                Number of bytes to group together between delimiters.
                i.e: for mac address is 2 but for IPv6 is 4
            
            pad_zeros: bool
                True if each byte in string should be padded with zeros.
                i.e: 2 -> 02 
            
            format_type: str
                Type to convert using format, default is 'x' for hex. i.e for ipv4 format = 'd'

        :return:
            human readable string representation of val. 
    """
    bytes_as_str = ['0%s' % format(v, format_type) if pad_zeros and len(format(v, format_type)) < 2 else format(v, format_type) for v in val]
    
    n = len(bytes_as_str)
    grouped = ['' for _ in range(n // group_bytes)]
    for i in range(len(grouped)):
        for j in range(group_bytes):
            grouped[i] += bytes_as_str[j + (i * group_bytes)]
    return delim.join(grouped)

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

        self.profile_loaded = False

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

    def _send_chunks(self, cmd, data_key, data, chunk_size = CHUNK_SIZE, max_data_rate = None, add_data = None):
        """
            Send `cmd` to EMU server with `data` over ZMQ separated to chunks using `max_data_rate`.
            
            i.e: self._send_chunks('ctx_client_add', data_key = 'clients', data = clients_data, max_data_rate = 100, add_data = namespace)
            will send add client request, with a rate of 100 clients

            :parameters:
                cmd: string
                    Name of command
                
                data_key: string
                    Key for data, i.e: `tun` for namespace sending.

                data: list
                    List with data to send, i.e: [{key: value..}, ... ,{key: value}]
                    This will be separated into chunks according to max_data_rate.
                
                chunk_size: int
                    size of every chunk to send.

                max_data_rate: int
                    Rate as an upper bound of data information per second.

                add_data: dictionary
                    Dictionary representing additional data to send in each separate chunk.
                    i.e: {'tun': {'vport': 10, ...}}   
            
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

        policer = SimplePolicer(cir = max_data_rate)
        for chunked_data in chunks(data, chunk_size):
            params = _create_params(chunked_data)

            while not policer.update(len(chunked_data), time.time()):
                # don't send more requests, let the server process the old ones
                time.sleep(0.5)

            rc = self._transmit(method_name = cmd, params = params)

            if not rc:
                raise TRexError(rc.err())

        return RC_OK()

    def _iter_ns(self, cmd, count, reset = False, ns = None):
        """
            Iterate namespace on EMU server.

            :parameters:
                count: int
                    Number of namespaces to iterate in this request.

                reset: bool
                    True if iterator should start from the beggining.
        """
        ver_args = {'types':
            [{'name': 'count', 'arg': count, 't': int},
            {'name': 'reset', 'arg': reset, 't': bool}]
            }
        params = {'count': count, 'reset': reset}
        if ns is not None:
            ver_args['types'].append({'name': 'ns', 'arg': ns, 't': dict})
            params['tun'] = ns
        ArgVerify.verify(self.__class__.__name__, ver_args)

        rc = self._transmit(cmd, params)

        if not rc:
            raise TRexError(rc.err())

        return rc.data()

    def _client_to_bytes(self, c):
        for key in c:
            if c[key] is None:
                continue
            try:
                c[key] = _conv_to_bytes(c[key], key)
            except Exception as e:
                print("Cannot convert client's key: %s: %s\nException:\n%s" % (key, c[key], str(e)))

    def _filter_data(self, data_list, filters):
        """
            Returns new list of `data_list` filtered by filters.
            
            :parameters:
                data_list: list
                    list of data to filter.
                
                filters: dictionary
                    Dictionary with keys "namespace" and "client" and values to filter from `data_list`.
                    i.e: {'namespace': {port': 1234}, 'client': {'ipv4': '1.1.1.3'}} will leave only ns with 1234 port and clients with 1.1.1.3 ipv4.

            :return:
                New filtered list of `data_list`                
        """
        if filters is None:
            return data_list

        ns_list = []
        for ns in data_list:
            # filter ns
            if all(ns[k] == v for k, v in filters['namespace'].items() if v is not None):
                new_ns = dict(ns)
                new_ns.pop('clients')
                new_clients = []
                # filter clients
                for client in ns['clients']:
                    if all(client[k] == v for k, v in filters['client'].items() if v is not None):
                        new_clients.append(client)
                new_ns['clients'] = new_clients
                ns_list.append(new_ns)
        
        return ns_list

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
    def disconnect (self, stop_traffic = True, release_ports = True):
        """
            Disconnects from the server

            :parameters:
                stop_traffic : bool
                    Attempts to stop traffic before disconnecting.
                release_ports : bool
                    Attempts to release all the acquired ports.

        """

        # try to stop ports but do nothing if not possible
        if stop_traffic:
            try:
                self.stop()
            except TRexError:
                pass


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
    def get_all_ns_and_clients(self, retry_num = None):

        all_ns = self.get_all_ns(retry_num)

        for ns in all_ns:
            ns_info = self.get_info_ns(ns)[0]
            ns.update(ns_info)

            clients_mac = self.get_all_clients_for_ns(ns, retry_num)
            ns['clients'] = self.get_info_client(ns, clients_mac)

            # convert ns tpids to hex string for readability
            ns['tpid'] = [hex(tp) for tp in ns['tpid']]

        return all_ns


    @client_api('getter', True)
    def get_all_ns(self, retry_num = None):
        """
            Gets all the namespaces from EMU server.

            :parameters:
                retry_num: int
                    Number of retries in case of failure, Default is None means no retry.

            :return:
                List of all namespaces keys i.e:[{u'tci': [1, 0], u'tpid': [33024, 0], u'vport': 1},
                                                    {u'tci': [2, 0], u'tpid': [33024, 0], u'vport': 1}]

            :raises:
                + :exc:`TRexError`
        """
        CHUNK_SIZE = 255  # limited by EMU server
        done, failed = False, 0

        while not done:
            if failed == retry_num:
                raise TRexError("Reached max retry number, operation failed")
            all_ns = []
            reset = True
            while True:
                res = self._iter_ns('ctx_iter', CHUNK_SIZE, reset = reset)
                if 'error' in res.keys():
                    failed += 1
                    break
                if res['stopped'] or res['empty']:
                    done = True
                    break
                all_ns.extend(res['data'])
                reset = False

        return all_ns

    @client_api('getter', True)    
    def get_info_ns(self, namespaces):
        """ 
            Return information about a given namespace.

            :parameters:

                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}

        :return:
            List of all namespaces with additional information i.e:[{'tci': [1, 0], 'tpid': [33024, 0], 'vport': 1,
                                                                    'number of plugins': 2, 'number of clients': 5}]

        :raises:
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

    @client_api('getter', True)
    def get_all_clients(self):
        """ Return all the current clients from every namespace in emu server """
        CHUNK_SIZE = 255  # limited by EMU server
        done = False
        ns_keys = self.get_all_ns()        
        all_clients = {}

        for ns_key in ns_keys:
            all_clients[str(ns_key)] = self.get_all_clients_for_ns(ns_key)

        return all_clients
    
    @client_api('getter', True)
    def get_all_clients_for_ns(self, namespace, retry_num = None):
        """
            Gets all the clients of a specific namespace from EMU server.

            :parameters:
    
                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
                
                retry_num: int
                    Number of retries in case of failure, None means no retry.

            :return:
                List of all clients mac values

            :raises:
                + :exc:`TRexError`
        """
        CHUNK_SIZE = 255  # limited by emu server
        done, failed = False, 0

        while not done:
            if failed == retry_num:
                raise TRexError("Reached max retry number, operation failed")
            all_clients = []
            reset = True
            while True:
                res = self._iter_ns('ctx_client_iter', CHUNK_SIZE, reset = reset, ns = namespace)
                if 'error' in res.keys():
                    failed += 1
                    break
                if res['stopped'] or res['empty']:
                    done = True
                    break
                all_clients.extend(res['data'])
                reset = False

        return all_clients

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
            macs = [_conv_to_bytes(m, 'mac') for m in macs]

        params = {'tun': namespace, 'macs': macs}

        rc = self._transmit('ctx_client_get_info', params)

        if not rc:
            raise TRexError(rc.err())

        # converting data from bytes to str
        for d in rc.data():
            for key, value in d.items():
                if key in self.client_keys_map:
                    d[key] = _conv_to_str(value, **self.client_keys_map[key])
        return rc.data()

    # CTX Counters
    @client_api('getter', True)
    def print_counters(self, tables = None, cnt_filter = None, verbose = False, show_zero = False, to_json = False):
        """
            Print tables for each ctx counter/

            :parameters:
                tables: list of strings
                    Names of all the wanted counters table. If not supplied, will print all of them.

                cnt_filter: list
                    List of counters type as strings. i.e: ['INFO', 'ERROR']

                verbose: bool
                    Show verbose version of counter tables.
                
                show_zero: bool
                    Show zero values, default is False

                to_json: bool
                    Prints a json with the wanted counters.
        """

        ver_args = {'types':
                [{'name': 'tables', 'arg': tables, 't': str, 'allow_list': True, 'must': False},
                {'name': 'cnt_filter', 'arg': cnt_filter, 't': list, 'must': False},
                {'name': 'verbose', 'arg': verbose, 't': bool},
                {'name': 'to_json', 'arg': to_json, 't': bool},]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        if tables is not None:
            tables = listify(tables)
        
        if self.meta_data is None:
            self.meta_data = self._get_counters(meta = True)
        
        res = self._get_counters(meta = False, mask = tables)
        self._update_meta_cnt(res)
        res = self._filter_cnt(tables, cnt_filter, verbose, show_zero)

        if to_json:
            pprint.pprint(res)
            return

        if all(len(c) == 0 for c in res.values()):
            text_tables.print_colored_line('There are no tables to show with current filter', 'yellow', buffer = sys.stdout)   
            return

        headers = ['name', 'value']
        if verbose:
            headers += ['unit', 'zero', 'help']

        for table_name, counters in res.items():
            if len(counters):
                self._print_cnt_table(table_name, counters, headers, verbose)


    def _update_meta_cnt(self, curr_cnts):
        ''' Update meta counters with the current values '''
        for table_name, table_data in self.meta_data.items():
            curr_table = curr_cnts.get(table_name, {})
            for cnt in table_data['meta']:
                cnt_name = cnt['name']
                cnt['value'] = curr_table.get(cnt_name, 0)

    def _filter_cnt(self, tables, cnt_filter, verbose, show_zero):
        ''' Return a new list with all the filtered counters '''
        
        def _pass_filter(cnt, cnt_filter, show_zero):
            if cnt_filter is not None and cnt.get('info') not in cnt_filter: 
                return False
            if not show_zero and cnt.get('value', 0) == 0:
                return False
            return True

        un_verbose_keys = {'name', 'value', 'info'}
        res = {}
        for table_name, table_data in self.meta_data.items():
            # filter tables
            if tables is None or table_name in tables:
                new_cnt_list = [] 

                for cnt in table_data['meta']:
                    if not _pass_filter(cnt, cnt_filter, show_zero):
                        continue
                    if verbose:
                        new_cnt_list.append(cnt)
                    else:
                        cnt_dict = {key: value for key, value in cnt.items() if key in un_verbose_keys}
                        new_cnt_list.append(cnt_dict)
                if new_cnt_list:
                    res[table_name] = new_cnt_list
        return res

    def _print_cnt_table(self, table_name, counters, headers, verbose):
        """
            Prints one ctx counter table, using the meta data values to reduce the zero value counters that doesn't send.  

            :parameters:
                table_name: str
                    Name of the counters table
                
                counters: list
                    List of dictionaries with data to print about table_name. Keys as counter names and values as counter value. 
                
                headers: list
                    List of all the headers in the table as strings.

                filters: list
                    List of counters type as strings. i.e: ['INFO', 'ERROR']

                verbose: bool
                    Show verbose version of counter tables.
        """
        def _get_info_postfix(info):
            info = info.upper()
            if info == 'INFO':
                return ''
            elif info == 'WARNING':
                return '+'
            elif info == 'ERROR':
                return '*'
            else:
                return ''

        table = text_tables.TRexTextTable('%s counters' % table_name)
        table.header(headers)
        max_lens = [len(h) for h in headers]

        for cnt_info in counters:
            row_data = []
            for i, h in enumerate(headers):
                cnt_val = str(cnt_info.get(h, '-'))
                if h == 'name':
                    postfix = _get_info_postfix(cnt_info.get('info'))
                    cnt_val = cnt_info.get('name', '-') + postfix
                max_lens[i] = max(max_lens[i], len(cnt_val))
                row_data.append(cnt_val)
            table.add_row(row_data)

        # fix table look
        table.set_cols_align(['l'] + ['c'] * (len(headers) - 2) + ['l'])  # only middle headers are aligned to middle
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)

    def _get_counters(self, meta = False, zero = False, mask = None, clear = False):
        """
            Gets counters from EMU server.

            :parameters:
                meta: bool
                    Get all the meta data.
                
                zero: bool
                    Bring zero values, default is False for optimizations.

                mask: list
                    list of string, get only specific counters blocks if it is empty get all.

                clear: bool
                    Clear all current counters.
            :return:
                dictionary describing counters of clients, fields that don't appear are treated as zero valued.
        """
        def _parse_info(info_code):
            info_dict = {
                0x12: 'INFO', 0x13: 'WARNING', 0x14: 'ERROR' 
            }
            return info_dict.get(info_code, 'UNKNOWN_TYPE')

        ver_args = {'types':
                [{'name': 'meta', 'arg': meta, 't': bool},
                {'name': 'zero', 'arg': zero, 't': bool},
                {'name': 'mask', 'arg': mask, 't': str, 'allow_list': True, 'must': False}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        if mask is not None:
            mask = listify(mask)
        params = {'meta': meta, 'zero': zero, 'mask': mask, 'clear': clear}

        rc = self._transmit('ctx_cnt', params)

        if not rc:
            raise TRexError(rc.err())
        
        if meta:
            # convert info values to string in meta mode
            for table_data in rc.data().values():
                table_cnts = table_data['meta'] 
                for cnt in table_cnts:
                    cnt['info'] = _parse_info(cnt['info'])

        return rc.data()
    
    def get_cnt_headers(self):
        """ Simply print the counters headers names """
        if self.meta_data is None:
            self.meta_data = self._get_counters(meta = True)
        print('Current counters headers are: %s ' % list(self.meta_data.keys()))

    def clear_counters(self):
        ''' Clears all the current counters in server'''
        self.ctx.logger.pre_cmd("Clearing all counters")
        self._get_counters(clear = True)
        self.ctx.logger.post_cmd(True)


    # Ns & Client Table Printing
    @client_api('getter', True)
    def print_all_ns_clients(self, max_ns_show, max_c_show , to_json = False, filters = None):
        """
            Prints all the namespaces and clients in tables.

            :parameters:

                max_ns_show: int
                    Max namespace to show each time.

                max_c_show: int
                    Max clients to show each time.

                to_json: bool
                    Print the data in a json format.
                
                filters: dictionary
                    Dictionary with keys "namespace" and "client" and values to filter from `data_list`. Default is `None` means no filter.
                    i.e: {'namespace': {port': 1234}, 'client': {'ipv4': '1.1.1.3'}} will leave only ns with 1234 port and clients with 1.1.1.3 ipv4.

        """
        all_data = self.get_all_ns_and_clients()
        all_data = self._filter_data(all_data, filters)

        if to_json:
            pprint.pprint(all_data)
            return

        if len(all_data) == 0:
            text_tables.print_colored_line('There are no namespaces', 'yellow', buffer = sys.stdout)      
            return
        else:
            text_tables.print_colored_line('\tIn this thread ctx there are %s namespaces' % len(all_data), 'yellow', buffer = sys.stdout)      

        for i, ns in enumerate(all_data):
            if i != 0 and i % max_ns_show == 0 and i != len(all_data) - 1:
                left_ns = len(all_data) - i 
                raw_input("There are %s more namespaces, Press Enter to see the next %s" % (left_ns, min(max_ns_show, left_ns)))
            self._print_table_ns(ns, ns_num = i + 1)

            clients_info = ns['clients']

            first_c_iter = True
            for j, (table, number_of_clients) in enumerate(self._yield_table_client(clients_info, max_c_show)):
                text_tables.print_table_with_header(table, table.title if first_c_iter else "", buffer = sys.stdout)
                if number_of_clients != 0:
                    raw_input("There are more clients, Press Enter to see the next %s" % number_of_clients)
                first_c_iter = False
            print('\n\n')

    def _print_table_ns(self, ns, ns_num):
        headers = ['Port', 'Vlan tags', 'Tpids', '#Plugins' , '#Clients']
        table = text_tables.TRexTextTable('Namespace #%s information' % ns_num)
        table.header(headers)

        data = [
                ns.get('vport', NO_ITEM_SYM),
                ns.get('tci', NO_ITEM_SYM),
                ns.get('tpid', NO_ITEM_SYM),
                ns.get('plugins_count', NO_ITEM_SYM),
                ns.get('active_clients', NO_ITEM_SYM),
        ]
        table.add_row(data)

        table.set_cols_align(['c'] * len(headers))
        table.set_cols_width([max(len(h), len(str(d))) for h, d in zip(headers, data)])
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)

    def _yield_table_client(self, clients, mac_c_show):

        if len(clients) == 0:
            text_tables.print_colored_line("There are not clients for this namespace", 'yellow', buffer = sys.stdout)      
            return
        else:
            text_tables.print_colored_line('\tIn this namespace there are %s clients' % len(clients), 'yellow', buffer = sys.stdout)      

        table = text_tables.TRexTextTable('Clients information')
        headers = ["MAC", "IPv4", "Deafult Gateway", "IPv6"]
        table.header(headers)

        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width([17] * len(headers))
        table.set_cols_dtype(['t'] * len(headers))

        for i, client in enumerate(clients):
            if i != 0 and i % mac_c_show == 0:
                # yield small table and how many clients in the next iteration
                yield table, min(mac_c_show, (len(clients) - i))
                table.reset()
                table.header(headers)

            table.add_row([
                            client.get('mac', NO_ITEM_SYM),
                            client.get('ipv4', NO_ITEM_SYM),
                            client.get('ipv4_dg', NO_ITEM_SYM),
                            client.get('ipv6', NO_ITEM_SYM),
            ])
        yield table, 0

    # Default Plugins
    @client_api('getter', True)
    def get_def_ns_plugs(self):
        """ Gets the namespace default plugin """
        rc = self._transmit('ctx_get_def_plugins')

        if not rc:
            raise TRexError(rc.err())
        return rc.data()

    @client_api('getter', True)
    def get_def_client_plugs(self, namespace):
        """ Gets the client default plugin in a specific namespace """
        rc = self._transmit('ctx_client_get_def_plugins', tun = namespace)

        if not rc:
            raise TRexError(rc.err())
        return rc.data()

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

        self._send_chunks('ctx_client_add', add_data = {'tun': namespace}, data_key = 'clients', data = clients, max_data_rate = max_rate)

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
        
        self.remove_ns(ns_keys)
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
    def set_def_client_plugs(self, namespace, def_plugs):
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
    def load_profile(self, filename, tunables, max_rate):
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
        validate_type('filename', filename, basestring)

        s = time.time()
        self.ctx.logger.pre_cmd("Converting file to profile")
        profile = EMUProfile.load(filename, tunables)
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
            raise TRexError('Profile must be from `EMUProfile` type')

        self.ctx.logger.pre_cmd('Sending emu profile')
        s = time.time()
        try:
            # make sure there are no clients and ns in emu server
            self.remove_all_clients_and_ns()

            # set the default ns plugins
            if profile.def_ns_plugs is not None:
                self.set_def_ns_plugs(profile.def_ns_plugs)

            # adding all the namespaces
            self.add_ns(profile.serialize_ns())

            # adding all the clients for each namespace
            for ns in profile.ns_list:
                # set client default plugins 
                if ns.def_client_plugs is not None:
                    self.set_def_client_plugs(ns.conv_to_key(), ns.def_client_plugs)
                self.add_clients(ns.fields, ns.get_clients(), max_rate = max_rate)
        except Exception as e:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Could not load profile, error: %s' % str(e))
        e = time.time()
        self.ctx.logger.post_cmd(True)
        print("Sending profile took: %.3f [sec]" % (e - s))
        self.profile_loaded = True

    @client_api('command', True)
    def remove_profile(self):
        """ Remove current profile from emu server, all namespaces and clients will be removed """
        if self.profile_loaded:
            self.ctx.logger.pre_cmd('Removing emu profile')
            self.remove_all_clients_and_ns()
        else:
            raise TRexError('No active profile to remove')
        self.ctx.logger.post_cmd(True)
        self.profile_loaded = False
        return RC_OK()

############################   console   #############################
############################   commands  #############################
############################             #############################
