from ..astf.arg_verify import ArgVerify
from ..common.trex_api_annotators import client_api, plugin_api
from ..common.trex_ctx import TRexCtx
from ..common.trex_exceptions import *
from ..common.trex_logger import ScreenLogger
from ..common.trex_types import *
from ..utils import parsing_opts, text_tables
from ..utils.common import *
from ..utils.text_opts import format_text, format_time
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
SEND_CHUNK_SIZE = 255  # size of each data chunk sending to emu server
RECV_CHUNK_SIZE = 255  # limited by emu server to 255

class SimplePolicer:

    MAX_BUCKET_LVL = SEND_CHUNK_SIZE * 2

    def __init__(self, cir, level, bucket_size):
        assert level <= SimplePolicer.MAX_BUCKET_LVL, 'Policer bucket level cannot be greater than %s' % SimplePolicer.MAX_BUCKET_LVL
        self.level = level
        self.bucket_size = bucket_size
        self.cir = cir
        self.last_time = 0.0

    def update(self, size):
        now_sec = time.time()

        if self.last_time == 0:
            if size > self.cir:
                raise TRexError("Policer cannot send %s when cir is %s for the first time" % (size, self.cir))
            self.last_time = now_sec
            return True  # first time

        if self.cir == 0:
            return False  # No rate

        # increase level in bucket
        d_time = now_sec - self.last_time
        d_size = d_time * self.cir
        self.level += d_size
        self.level = min(self.level, self.bucket_size)
        self.last_time = now_sec  # save current time

        # decrease level in bucket
        if self.level > size:
            self.level -= size
            return True
        else:
            return False


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

def dump_json_yaml(data, to_json = False, to_yaml = False, ident_size = 4):
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

def conv_return_val_to_str(f):

    def inner(*args, **kwargs):
        res = f(*args, **kwargs)

        for d in res:
            for key, value in d.items():
                d[key] = conv_to_str(value, key)

        return res

    return inner

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
        
        # used for ctx counter meta data
        self.meta_data = None

        # emu client holds every emu plugin
        self.general_data_c = DataCounter(self.conn, 'ctx_cnt')

        self.registered_plugs = {}  # keys as names values as emu_plugin objects i.e: {'arp': ARPPlugin, ...} 
        self.get_plugin_methods()  # important, bind emu_plugins to client

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

    def _send_chunks(self, cmd, data_key, data, chunk_size = SEND_CHUNK_SIZE, max_data_rate = None, add_data = None, track_progress = False):
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

        # data has no length
        if data is None:
            params = _create_params(data)
            rc = self._transmit(method_name = cmd, params = params)
            if not rc:
                self._err(rc.err())
            return RC_OK()

        # unlimited rate send everyting
        if max_data_rate is None:
            max_data_rate = len(data)
    
        policer = SimplePolicer(cir = max_data_rate, level = SEND_CHUNK_SIZE, bucket_size = SEND_CHUNK_SIZE * 4)
        chunk_size = min(max_data_rate, chunk_size)
        total_chunks = int(math.ceil(len(data) / float(chunk_size))) if chunk_size != 0 else 1
        total_chunks = total_chunks if total_chunks != 0 else 1
        if track_progress:
            print('')

        for chunk_i, chunked_data in enumerate(divide_into_chunks(data, chunk_size)):
            params = _create_params(chunked_data)

            while not policer.update(len(chunked_data)):
                # don't send more requests, let the server process the old ones
                time.sleep(0.001)

            rc = self._transmit(method_name = cmd, params = params)

            if not rc:
                self._err(rc.err())
            
            if track_progress:
                dots = '.' * (chunk_i % 7)
                percent = (chunk_i + 1) / float(total_chunks) * 100.0
                text = "Progress: {0:.2f}% {1}".format(percent, dots)
                
                sys.stdout.write('\r' + (' ' * 25) + '\r')  # clear line
                sys.stdout.write(format_text(text, 'yellow'))
                sys.stdout.flush()
        
        if track_progress:
            sys.stdout.write('\r' + (' ' * 25) + '\r')  # clear line

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
            self._err(rc.err())
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
    def _print_dict_as_table(self, data, title = None):
        """
        Print dictionary as a human readable table. 
        
            :parameters:
                data: dictionary
                    Data to show.
                title: string
                    Title to show above data, defaults to None means no title.
        """

        for k in get_val_types():
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


    def _err(self, msg):
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
    
    def _yield_n_items(self, cmd, amount = None, retry_num = 1, **kwargs):
        """
            Gets n items by calling `cmd`, iterating items from EMU server.

            :parameters:
                cmd: string
                    name of the iterator command

                amount: int
                    Amount of data to fetch each request, None is default means as much as you can.
                
                retry_num: int
                    Number of retries in case of failure, Default is 1.

            :return:
                yield list of amount namespaces keys i.e:[{u'tci': [1, 0], u'tpid': [33024, 0], u'vport': 1},
                                                    {u'tci': [2, 0], u'tpid': [33024, 0], u'vport': 1}]

            :raises:
                + :exc:`TRexError`
        """
        done, failed_num = False, 0

        while not done:

            reset = True

            while True:
                res = self._iter_ns(cmd, amount if amount is not None else SEND_CHUNK_SIZE, reset = reset, **kwargs)
                if 'error' in res.keys():
                    failed_num += 1
                    break
                if res['stopped'] or res['empty']:
                    done = True
                    break
                yield res['data']
                reset = False  # reset only at start
            
            if failed_num == retry_num:
                self._err("Reached max retry number, operation failed")

    def _get_n_items(self, cmd, amount = None, retry_num = 1, **kwargs):
        """
            Gets requested number of elements from emu server using a given command. no yield data, eager approach.
        
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
        gen = self._yield_n_items(cmd, amount, retry_num, **kwargs)
        for item in gen:
            res.extend(item)
        return res

    def _get_n_ns(self, amount = None):
        """
        Gets n namespaces. yield data, lazy approach.
        
            :parameters:
                amount: int
                    Amount of namespaces to request each time, defaults to None means max as you can.
        
            :raises:
                + :exc:`TRexError`

            :returns:
                list: List of namespaces.
        """        
        return self._yield_n_items('ctx_iter', amount = amount)

    def _get_all_ns(self):
        """
        Gets all the namespaces. No yield, eager approach.
                
            :returns:
                List: List of all namespaces.
        """        
        res = []
        for ns in self._get_n_ns(amount = RECV_CHUNK_SIZE):
            res.extend(ns)
        return res

    @client_api('getter', True)
    def get_all_ns_and_clients(self):
        """
        Gets all the namespaces and clients from emu server. No yield, eager approach.
        
            :returns:
                list: List of all namespaces and clients.
        """        
        ns_list = self._get_all_ns()
        for ns in ns_list:
            macs_in_ns = self.get_all_clients_for_ns(ns)
            ns['clients'] = self.get_info_client(ns, macs_in_ns)
            ns['tpid'] = [hex(t) for t in ns['tpid']]
        return ns_list

    @client_api('getter', True)
    def get_all_clients_for_ns(self, namespace):
        """
        Gets all clients for a given namespace. No yield, eager approach.
        
            :parameters:
                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
        
            :returns:
                list: List of all clients in namespace.
        """        
        res = []
        for c in self._yield_n_items(cmd = 'ctx_client_iter', amount = RECV_CHUNK_SIZE, tun = namespace):
            res.extend(c)
        return res

    @client_api('getter', True)
    @conv_return_val_to_str
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

    @client_api('getter', True)
    @conv_return_val_to_str
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

        return rc.data()

    # CTX Counters
    @client_api('getter', True)
    def get_counters(self, meta = False, zero = False, mask = None, clear = False):
        """
        Get global counters from emu server. 
        
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
    def print_all_ns_clients(self, max_ns_show, max_c_show , to_json = False, to_yaml = False, ipv6 = False, dg4 = False, dg6 = False, ipv6_router = False):
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
                
                ipv6: bool
                    Show client's ipv6 values.

                dg4: bool
                    Show client's default ipv4 gateway table.

                dg6: bool
                    Show client's default ipv6 gateway table.

                ipv6_router: bool
                    Show client's ipv6 router table.

            :return:
                None
        """

        if to_json or to_yaml:
            # gets all data, no pauses
            all_data = self.get_all_ns_and_clients()
            return dump_json_yaml(all_data, to_json, to_yaml)

        all_ns_gen = self._get_n_ns(amount = max_ns_show)

        glob_ns_num = 0
        for ns_chunk_i, ns_list in enumerate(all_ns_gen):
            ns_infos = self.get_info_ns(ns_list)

            if ns_chunk_i != 0:
                self._wait_for_pressed_key('Press Enter to see more namespaces')
            
            for ns_i, ns in enumerate(ns_list):
                glob_ns_num += 1

                # print namespace table
                self._print_ns_table(ns_infos[ns_i], ns_num = glob_ns_num)

                for client_i, clients_mac in enumerate(self._yield_n_items(cmd = 'ctx_client_iter', amount = max_c_show, tun = ns)):                    
                    is_first_time = client_i == 0
                    if not is_first_time != 0:
                        self._wait_for_pressed_key("Press Enter to see more clients")
                    clients = self.get_info_client(ns, clients_mac)
                    # sort by mac address
                    clients.sort(key = lambda x: mac_str_to_num(mac2str(x['mac'])))
                    self._print_clients_chunk(clients, ipv6, dg4, dg6, ipv6_router)
        
        if not glob_ns_num:
            text_tables.print_colored_line('There are no namespaces', 'yellow', buffer = sys.stdout)      

    def _print_clients_chunk(self, clients, ipv6, dg4, dg6, ipv6_router):
        """
        Print clients with all the wanted flags.
        
            :parameters:
                ipv6: bool
                    Show client's ipv6 values.

                dg4: bool
                    Show client's default ipv4 gateway table.

                dg6: bool
                    Show client's default ipv6 gateway table.

                ipv6_router: bool
                    Show client's ipv6 router table.
            :raises:
        
        """    
        if dg4:
            self._print_clients_table(clients, DG_KEYS_AND_HEADERS, inner_key = 'dgw', title = 'DG-4 Table', empty_msg = 'DG-4 Table is empty')
        if dg6:
            self._print_clients_table(clients, DG_KEYS_AND_HEADERS, inner_key = 'ipv6_dgw', title = 'DG-6 Table', empty_msg = 'DG-6 Table is empty')
        if ipv6_router:
            self._print_clients_table(clients, IPV6_ND_KEYS_AND_HEADERS, inner_key = 'ipv6_router', title = 'IPv6 Router Table', empty_msg = 'IPv6 Router Table is empty')
        relates = ['ipv6'] if ipv6 else None
        self._print_clients_table(clients, CLIENT_KEYS_AND_HEADERS, show_relates = relates, title = 'Clients information',
                                    empty_msg = "There is no client with these parameters")

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
        def get_printable_val(key):
            val = ns.get(key, NO_ITEM_SYM)
            if not val:
                return NO_ITEM_SYM if key != 'vport' else 0
            return val

        headers = ['Port', 'Vlan tags', 'Tpids', 'Plugins' , '#Clients']
        ns_keys = ['vport', 'tci', 'tpid', 'plug_names', 'active_clients']
        table = text_tables.TRexTextTable('Namespace information' if ns_num is None else 'Namespace #%s information' % ns_num)
        table.header(headers)

        data = [get_printable_val(k) for k in ns_keys]
        table.add_row(data)

        table.set_cols_align(['c'] * len(headers))
        table.set_cols_width([max(len(h), len(str(d))) for h, d in zip(headers, data)])
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout, color = 'blue')

    def _print_clients_table(self, clients, keys_and_headers, show_relates = None, inner_key = None, title = '', empty_msg = ''):
        """
        Print clients table. 
        
            :parameters:
                clients: list
                    List with clients data.
                
                keys_and_headers: list
                    Dictionary with meta-data on the keys format: [{'key': 'key_name', 'header': 'header_name', 'empty_val': 0}, ..]
                
                inner_key: string
                    Name of inner key to relates to. i.e: "ipv6_router" will print each client ipv6 router info. 
                
                title: string
                    Name of the title to print. 
                
                empty_msg: string
                    Message to print in case there is no data.
        
            :raises:
                + :exe:'TRexError'
        
            :returns:
                None
        """
        def _is_header_empty(key, empty_val):
            if inner_key is None:
                return all([c.get(key, empty_val) == empty_val for c in clients])
            else:
                return all([c.get(inner_key, {}).get(key, empty_val) == empty_val for c in clients if c.get(inner_key) is not None])

        def _add_non_empty_headers(values):
            for val in values:
                key, header, empty_val, relates, hide_from_table = val['key'], val['header'], val.get('empty_val'), val.get('relates') , val.get('hide_from_table', False)
                is_empty = _is_header_empty(key, empty_val)
                is_relates = relates is None or relates in show_relates
                if not hide_from_table and not is_empty and is_relates:
                    dependents = val.get('key_dependent')
                    # check key dependent
                    if dependents is not None and (all([k not in client_keys for k in dependents])):
                        continue
                    headers.append(header)
                    client_keys.append(key)

        def _check_empty():
            if len(clients) == 0:
                return True
            elif inner_key is not None:
                if all([c.get(inner_key) is None for c in clients]) or len(headers) == 1:
                    return True
            return False

        if show_relates is None:
            show_relates = []

        table = text_tables.TRexTextTable(title)
        headers = ['MAC']  # every client has a mac address
        client_keys = ['mac']

        _add_non_empty_headers(keys_and_headers)

        if _check_empty():
            text_tables.print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)      
            return

        max_lens = [len(h) for h in headers]
        table.header(headers)

        for i, client in enumerate(clients):
            client_row_data = []
            for j, key in enumerate(client_keys):
                if inner_key is None or key == 'mac':
                    val = str(client.get(key, NO_ITEM_SYM))
                else:
                    val = str(client.get(inner_key, {}).get(key, NO_ITEM_SYM)) 
                client_row_data.append(val)
                max_lens[j] = max(max_lens[j], len(val))
            table.add_row(client_row_data)

        # fix table look
        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))
        
        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout, color = 'cyan')        

    def _print_mbuf_table(self, mbuf_tables):
        """
        Prints a human readable mbuf table. 
        
            :parameters:
                mbuf_tables: list
                    List of dictionaries: [{'mbuf_size': 1024, 'mbuf_info': {...}}].
        """        
        def _update_max_len(max_lens, i, v):
            max_lens[i] = max(max_lens[i], len(str(v)))

        if len(mbuf_tables) == 0:
            text_tables.print_colored_line("There are no mbufs to show", 'red', buffer = sys.stdout)      
            return

        table_header = 'Mbuf Util'
        mbuf_table = text_tables.TRexTextTable(table_header)
        
        headers = ['Sizes:', '128b',  '256b', '512b', '1k', '2k', '4k', '9k',]
        mbufAlloc, mbufFree, mbufAllocCache, mbufFreeCache = ['Allocations'], ['Free'], ['Cache Allocations'], ['Cache Free']
        hitRate, actives = ['Hit Rate'], ['Actives']
        all_data = [headers, mbufAlloc, mbufFree, mbufAllocCache, mbufFreeCache, hitRate, actives]
        max_lens = [len(h) for h in headers]
        max_lens[0] = len('Cache Allocations')  # longest attribute in table

        for table_i, current_table in enumerate(mbuf_tables, start = 1):
            table_info = current_table.get('mbuf_info')
            if table_info is None:
                continue

            mbuf_alloc = table_info[0].get('value', 0)
            _update_max_len(max_lens, table_i, mbuf_alloc)
            mbufAlloc.append(mbuf_alloc)

            mbuf_free = table_info[1].get('value', 0)
            _update_max_len(max_lens, table_i, mbuf_free)
            mbufFree.append(mbuf_free)

            mbuf_alloc_cache = table_info[2].get('value', 0)
            _update_max_len(max_lens, table_i, mbuf_alloc_cache)
            mbufAllocCache.append(mbuf_alloc_cache)

            mbuf_free_cache = table_info[3].get('value', 0)
            _update_max_len(max_lens, table_i, mbuf_free_cache)
            mbufFreeCache.append(mbuf_free_cache)

            total_alloc = mbuf_alloc_cache + mbuf_alloc
            hit_rate = 0 if total_alloc == 0 else float(mbuf_alloc_cache) / total_alloc * 100.0
            hit_rate = '{:.1f}%'.format(hit_rate) 
            _update_max_len(max_lens, table_i, hit_rate)
            hitRate.append(hit_rate)

            active = mbuf_alloc + mbuf_alloc_cache - mbuf_free + mbuf_free_cache
            _update_max_len(max_lens, table_i, active)
            actives.append(active)

        mbuf_table.add_rows(all_data)

        # fix table look
        mbuf_table.set_cols_align(['l'] + (['c'] * (len(headers) - 1)))
        mbuf_table.set_cols_dtype(['a'] * len(headers))
        mbuf_table.set_cols_width(max_lens)

        text_tables.print_table_with_header(mbuf_table, header = table_header, buffer = sys.stdout)

    # Plugins CFG
    def _send_plugin_cmd_to_ns(self, cmd, port, vlan = None, tpid = None, **kwargs):
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
    
    # Emu Profile
    @client_api('command', True)
    def load_profile(self, filename, max_rate, tunables, dry = False):
        """
            Load emu profile from profile by its type. Supported type for now is .py

            :parameters:
                filename : string
                    filename (with path) of the profile

                tunables : string
                    tunables line. i.e: ".. -t --ns 1 --clients 10"

                max_rate : int
                    max clients rate to send (clients/sec), "None" means all clients in 1 request.

                dry: bool
                    True will not send the profile, only print as JSON.

            :raises:
                + :exc:`TRexError`
        """
        help_flags = ('-h', '--help')
        if any(h in tunables for h in help_flags):
            # don't print external messages on help
            profile = EMUProfile.load(filename, tunables)
            return

        s = time.time()
        self.ctx.logger.pre_cmd("Converting file to profile")
        profile = EMUProfile.load(filename, tunables)
        if profile is None:            
            self.ctx.logger.post_cmd(False)
            self._err('Failed to convert EMU profile')
        self.ctx.logger.post_cmd(True)
        print("Converting profile took: %s" % (format_time(time.time() - s)))
        if not dry:
            self._start_profile(profile, max_rate)
        else:
            dump_json_yaml(profile.to_json(), to_json = True)

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
            self._err('Profile must be from `EMUProfile` type, got: %s' % type(profile))
        s = time.time()
        try:
            # make sure there are no clients and ns in emu server
            self.remove_profile(max_rate)

            self.ctx.logger.pre_cmd('Sending emu profile')
            # set the default ns plugins
            self.set_def_ns_plugs(profile.def_ns_plugs)

            # adding all the namespaces in profile
            self.add_ns(profile.serialize_ns())

            # adding all the clients for each namespace
            for ns in profile.ns_list:
                # set client default plugins 
                self.set_def_c_plugs(ns.conv_to_key(), ns.def_c_plugs)
                self.add_clients(ns.fields, ns.get_clients(), max_rate = max_rate)
        except Exception as e:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Could not load profile, error: %s' % str(e))
        self.ctx.logger.post_cmd(True)
        print("Sending profile took: %s" % (format_time(time.time() - s)))

    @client_api('command', True)
    def remove_profile(self, max_rate = None):
        """
            Remove current profile from emu server, all namespaces and clients will be removed 
        
            :parameters:
                max_rate: int
                    Max rate of client / sec for removing clients, defaults to None.
        """        
        self.ctx.logger.pre_cmd('Removing old emu profile')
        self.remove_all_clients_and_ns(max_rate)
        self.ctx.logger.post_cmd(True)
        return RC_OK()

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
        # tear down all plugins
        for ns in namespaces:
            for pl_obj in self.registered_plugs.values():
                pl_obj.tear_down_ns(ns.get('vport'), ns.get('tci'), ns.get('tpid'))

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
                    Max rate of clients to remove each time, measured by clients / sec.

            :raises:
                + :exc:`TRexError`
        """
        ver_args = {'types':
                [{'name': 'namespace', 'arg': namespace, 't': dict},
                {'name': 'macs', 'arg': macs, 't': list}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_client_remove', add_data = {'tun': namespace}, data_key = 'macs', data = macs,
                            max_data_rate = max_rate, track_progress = True)

        return RC_OK()

    @client_api('command', True)
    def remove_all_clients_and_ns(self, max_rate = None):
        """ 
            Remove all current namespaces and their clients from emu server 

            :parameters:
                max_rate: int
                    Max rate of clients to send each time, measured by clients / sec.
        """
        ns_keys_gen = self._get_n_ns()

        for ns_chunk in ns_keys_gen:
            for ns_key in ns_chunk: 
                clients = self.get_all_clients_for_ns(ns_key)
                self.remove_clients(ns_key, clients, max_rate)
            self.remove_ns(list(ns_chunk))
    
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
                [{'name': 'def_plugs', 'arg': def_plugs, 't': dict, 'must': False},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_set_def_plugins', data_key = 'def_plugs', data = def_plugs)

        return RC_OK()

    @client_api('command', True)
    def set_def_c_plugs(self, namespace, def_plugs):
        """
            Set the client default plugins. Every new client in that namespace will have that plugin.

            :parameters:

                namespace: dictionary
                    Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}

                def_plugs: dictionary
                    Map plugin_name -> plugin_data, each plugin here will be added to every new client.
                    If new client will provide a plugin, it will override the default one. 
        """
        ver_args = {'types':
                [{'name': 'def_plugs', 'arg': def_plugs, 't': dict, 'must': False},
                {'name': 'namespace', 'arg': namespace, 't': dict},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self._send_chunks('ctx_client_set_def_plugins', add_data = {'tun': namespace}, data_key = 'def_plugs', data = def_plugs)

        return RC_OK()
    

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
                                        parsing_opts.ARGPARSE_TUNABLES,
                                        parsing_opts.EMU_DRY_RUN_JSON
                                        )
        opts = parser.parse_args(args = line.split())
        self.load_profile(opts.file[0], opts.max_rate, opts.tunables, opts.dry)
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
                                        parsing_opts.EMU_DUMPS_OPT,
                                        parsing_opts.EMU_SHOW_CLIENT_OPTIONS
                                        )

        opts = parser.parse_args(line.split())

        self.print_all_ns_clients(max_ns_show = opts.max_ns, max_c_show = opts.max_clients, to_json = opts.json,
                                    to_yaml = opts.yaml, ipv6 = opts.ipv6, dg4 = opts.ipv4_dg, dg6 = opts.ipv6_dg, ipv6_router = opts.ipv6_router)
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
            self._err('Cannot find any namespace with requested parameters')
     
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
                                        parsing_opts.EMU_DUMPS_OPT,
                                        parsing_opts.EMU_SHOW_CLIENT_OPTIONS
                                        )

        opts = parser.parse_args(line.split())
        c_info = self.get_info_client(conv_ns(opts.port, opts.vlan, opts.tpid), [opts.mac])
        if not len(c_info):
            self._err('Cannot find any client with requested parameters')
        else:
            c_info = c_info[0]

        if opts.json or opts.yaml:
            return dump_json_yaml(c_info, opts.json, opts.yaml)

        self._print_clients_chunk([c_info], opts.ipv6, opts.ipv4_dg, opts.ipv6_dg, opts.ipv6_router)

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

    @plugin_api('show_mbuf', 'emu')
    def show_mbuf_line(self, line):
        '''Show mbuf usage in a table.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_mbuf",
                                        self.show_mbuf_line.__doc__
                                        )
        mbufs = self.general_data_c.get_counters(table_regex = 'mbuf-*')
        mbufs = [{'mbuf_size': int(k.strip('mbuf-')), 'mbuf_info': v} for k, v in mbufs.items() if k.strip('mbuf-').isdigit()]
        mbufs.sort(key = lambda x: x['mbuf_size'])
        self._print_mbuf_table(mbufs)
        return True

    def _base_show_counters(self, data_cnt, opts, req_ns = False):
        '''Base function for every counter using `cnt_cmd`'''
        
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
        data_cnt.set_add_data(reset = True)

        if req_ns:
            if opts.port is not None:
                try:
                    mac = opts.mac
                except AttributeError:
                    mac = None
                data_cnt.set_add_data(opts.port, opts.vlan, opts.tpid, mac)
            elif opts.all_ns:

                ns_gen = self._get_n_ns(amount = None)
                ns_i = 0

                for ns_chunk in ns_gen:
                    for ns in ns_chunk:
                        ns_i += 1
                        self._print_ns_table(ns, ns_i)
                        data_cnt.set_add_data(ns.get('vport'), ns.get('tci'), ns.get('tpid'))
                        run_on_demend(data_cnt, opts)
                if not ns_i:
                    self._err('there are no namespaces in emu server')
                return True
            else:
                raise self._err('namespace information required, supply them or run with --all-ns ')
        
        run_on_demend(data_cnt, opts)

        return True

    def get_plugin_methods (self):
        ''' Register all plugin methods from all plugins '''
        # Get all emu client plugin methods
        all_plugin_methods = self._get_plugin_methods_by_obj(self)

        # Get all plugins plugin methods
        current_plugins_dict = self._get_plugins()

        for plug_name, filename in current_plugins_dict.items():
            plugin_instance = self._create_plugin_inst_by_name(plug_name, filename)
            setattr(self, plug_name, plugin_instance)  # add plugin to emu client dynamically
            self.registered_plugs[plug_name] = plugin_instance
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
                self._err('Exception during import of %s: %s' % (import_path, e.msg))
            
            # format for plugin name with uppercase i.e: "ARPPlugin"
            plugins_cls_name = '%sPlugin' % name.upper()
            
            plugin_class = vars(m).get(plugins_cls_name)
            if plugin_class is None:
                self._err('Plugin "%s" classname is not valid, should be: %s' % (name, plugins_cls_name) )
            if plugin_class is EMUPluginBase:
                self._err("Cannot load EMUPluginBase, inherit from this class") 
            if not issubclass(plugin_class, EMUPluginBase):
                self._err('Plugin "%s" class should inherit from EMUPluginBase' % name) 

            try:
                plug_inst = plugin_class(emu_client = self)
            except BaseException as e:
                self._err('Could not initialize the plugin "%s", error: %s' % (name, e))
            
            return plug_inst

############################   trex_client #############################
############################   functions  #############################
############################             #############################
# Those functions intentionally in the end for docs #################
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
