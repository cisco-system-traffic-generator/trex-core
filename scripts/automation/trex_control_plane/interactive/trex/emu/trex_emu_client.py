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
from .emu_plugins.emu_plugin_base import *
from .trex_emu_counters import *
from .trex_emu_profile import *
from .trex_emu_conversions import *
from .trex_emu_validator import EMUValidator

import glob
import imp
import inspect
import os
import math
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

def divide_into_chunks(data, n):
    """
        Yield successive n-sized chunks from data if data is a list. In case data isn't list type, just yield data.
            :parameters:
                data: list or anything else
                    In case of a list divide it into chunks with length n. Any other type will just yield itself.
                n: int
                    Length of each chunk to yield.
                :yield:
                    list sized n with elements from data, or just data if data is not list.
    """
    if type(data) is list and n < len(data):
        for i in range(0, len(data), n):
            yield data[i:i + n]
    else:
        yield data


def divide_dict_into_chunks(d, n):
    if max([len(v) if type(v) is list else 1 for v in d.values()]) <= n:
        yield d
    else:
        gens = {}  # creating the generators for large values
        small_values = {}  # storing the smaller values separately
        for k, v in d.items():
            if isinstance(v, list) and len(v) > n:
                gens[k] = divide_into_chunks(v, n)
            else: 
                small_values[k] = v
        
        while True:
            d_chunked = {}
            for k, g in gens.items():
                try:
                    chunk = next(g)
                    d_chunked[k] = chunk
                except StopIteration:
                    pass
            if len(d_chunked) > 0:
                d_chunked.update(small_values)  # adding the small values, like tunnel key in clients adding
                yield d_chunked
            else:
                break


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

        res = listify(res)
        for d in res:
            for key, value in d.items():
                d[key] = conv_to_str(value, key)

        return res

    return inner


def wait_for_pressed_key(msg):
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

        api_ver = {'name': 'EMU', 'major': 1, 'minor': 1}

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

    # disconnect from server
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

    def _send_chunks(self, cmd, data, chunk_size = SEND_CHUNK_SIZE, max_data_rate = None, track_progress = False):
        """
            Send `cmd` to EMU server with `data` over ZMQ separated to chunks using `max_data_rate`.
            i.e: self._send_chunks('ctx_client_add', data_key = 'clients', data = clients_data, max_data_rate = 100, add_data = namespace)
            will send add client request, with a rate of 100 clients
            
            :parameters:
                cmd: string
                    Name of command.
                
                data: dict
                    Dict with data to send, i.e: {key1: [values..], ... ,key2: [values]}}
                    This will be separated into chunks according to max_data_rate.
                    Notice that if values are lists, they will be divided into chunks any other case will send them as is.
                
                chunk_size: int
                    Size of every chunk to send.

                max_data_rate: int
                    Rate as an upper bound of data information per second.

                track_progress: bool
                    True will show a progress bar of the sending, defaults to False.
        
            :raises:
                + :exc: `TRexError` in case of invalid command
            :return:
                Depends: Depends on the command. return rc.data(), check what the server returns.
        """
        
        def _check_rc(rc, response):
            if not rc:
                self._err(rc.err())
            else:
                d = listify(rc.data())
                response.extend(d)
        def _get_max_len(data):
            return max([len(v) if type(v) is list else 1 for v in data.values()])

        response = []

        # data has no length
        if data is None:
            rc = self._transmit(method_name = cmd, params = {})
            _check_rc(rc, response)
            return response

        # unlimited rate send everyting
        if max_data_rate is None:
            max_data_rate = float('inf')
    
        policer = SimplePolicer(cir = max_data_rate, level = SEND_CHUNK_SIZE, bucket_size = SEND_CHUNK_SIZE * 4)
        chunk_size = min(max_data_rate, chunk_size)
        data_max_len = _get_max_len(data)
        total_chunks = int(math.ceil(data_max_len / float(chunk_size))) if chunk_size != 0 else 1
        total_chunks = total_chunks if total_chunks != 0 else 1
        if track_progress:
            print('')
        data_gen = divide_dict_into_chunks(data, chunk_size)
        for chunk_i, chunked_data in enumerate(data_gen):

            while not policer.update(chunk_size):
                # don't send more requests, let the server process the old ones
                time.sleep(0.001)

            rc = self._transmit(method_name = cmd, params = chunked_data)

            _check_rc(rc, response)

            if track_progress:
                dots = '.' * (chunk_i % 7)
                percent = (chunk_i + 1) / float(total_chunks) * 100.0
                text = "Progress: {0:.2f}% {1}".format(percent, dots)
                
                sys.stdout.write('\r' + (' ' * 40) + '\r')  # clear line
                sys.stdout.write(format_text(text, 'yellow'))
                sys.stdout.flush()
        
        if track_progress:
            sys.stdout.write('\r' + (' ' * 40) + '\r')  # clear line

        return response[0] if len(response) == 1 else response

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
                dict: Returns the data of cmd in form: {'empty': False, 'stopped': True, 'data': [..]}.
        """
        params = {'count': count, 'reset': reset}
        params.update(kwargs)

        rc = self._transmit(cmd, params)

        if not rc:
            raise TRexError(rc.err())

        return rc.data()

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

    def _conv_macs_and_validate_ns(self, c_keys):
        '''
        Get all macs in bytes and validate same ns for all client keys.
        :parameters:
            clients: list of EMUClientKey
                clients to work on.
        :return:
            dict: Dictionary ready to send to emu server for multi-client commands. 
        '''
        first_ns_key = c_keys[0].ns_key.conv_to_dict(add_tunnel_key = False)
        # gather all macs
        macs = []
        for k in c_keys:
            macs.append(k.mac.V())
            ns_key = k.ns_key.conv_to_dict(False)
            if first_ns_key != ns_key:
                self.err('Cannot get info for clients from different namespaces')
        data = {'macs': macs}
        data.update(c_keys[0].ns_key.conv_to_dict(add_tunnel_key = True))
        return data

############################   Getters   #############################
############################             #############################
############################             #############################

    def _yield_n_items(self, cmd, amount = None, retry_num = 1, **kwargs):
        """
            Gets n items by calling `cmd`, iterating items from EMU server. This is a generic function for every iterator command
            such as ctx_iter, ctx_client_iter.. 

            :parameters:
                cmd: string
                    name of the iterator command

                amount: int
                    Amount of data to fetch each request, None is default means as much as you can.
                
                retry_num: int
                    Number of retries in case of failure, Default is 1.

            :yield:
                yield a list of `amount` namespaces keys i.e:[{u'tci': [1, 0], u'tpid': [33024, 0], u'vport': 1},
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

    def _yield_n_clients_for_ns(self, ns_key, amount = None):
        """
        Gets n namespaces. yield data, lazy approach.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                amount: int
                    Amount of namespaces to request each time, defaults to None means max as you can.

            :yields:
                list: List of `amount` EMUClientKeys in namespace.
        """
        ns_tunnel = ns_key.conv_to_dict(True)
        res = self._yield_n_items(cmd = 'ctx_client_iter', amount = amount, **ns_tunnel)
        for r in res:
            c_keys = []
            for mac in r:
                c_keys.append(EMUClientKey(ns_key, mac))
            yield c_keys

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
        res_gen = self._yield_n_items('ctx_iter', amount = amount)
        
        for chunk in res_gen:
            # conv data to EMUNamespaceKey
            for i, ns_entry in enumerate(chunk):
                chunk[i] = EMUNamespaceKey(**ns_entry)
            yield chunk

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
                | list: List of dictionaries with all namespaces and clients.
                | i.e: [{'vport': 0, 'tci': [1, 0], 'tpid': [33024, 0],
                |        'clients': [{'mac': '00:00:00:70:00:01', 'ipv4': '1.1.2.3', 'ipv4_dg': '1.1.2.1', 'ipv4_mtu': 1500, 'ipv6_local': 'fe80::200:ff:fe70:1', 'ipv6_slaac': '::', 'ipv6': '2001:db8:1::2', 'dg_ipv6': '::', 'dhcp_ipv6': '::', 'ipv4_force_dg': False, 'ipv4_force_mac': '00:00:00:00:00:00', 'ipv6_force_dg': False, 'ipv6_force_mac': '00:00:00:00:00:00', 'dgw': {'resolve': False, 'rmac': '00:00:00:00:00:00'}, 'ipv6_router': None, 'ipv6_dgw': None, 'plug_names': 'arp, igmp'}
                | ]}]
        """        
        ns_keys_list = self._get_all_ns()
        res = []
        for ns_key in ns_keys_list:
            ns_dict = ns_key.conv_to_dict(False)
            macs_in_ns = self.get_all_clients_for_ns(ns_key)
            ns_dict['clients'] = self.get_info_clients(macs_in_ns)
            res.append(ns_dict)
        return res

    @client_api('getter', True)
    def get_all_clients_for_ns(self, ns_key):
        """
        Gets all clients for a given namespace. No yield, eager approach.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`

            :returns:
                list: List of EMUClientKey represeting all clients in namespace.
        """
        ver_args = [{'name': 'ns_keys', 'arg': ns_key, 't': EMUNamespaceKey, 'allow_list': True},]
        EMUValidator.verify(ver_args)

        res = []
        for c_keys in self._yield_n_clients_for_ns(ns_key, amount = RECV_CHUNK_SIZE):
            res.extend(c_keys)
        return res

    @client_api('getter', True)
    @conv_return_val_to_str
    def get_info_ns(self, ns_keys):
        """
            Return information about a given namespace/s.
        
            :parameters:
                ns_keys: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        
            :return:
                List of all namespaces with additional information i.e:[{'tci': [1, 0], 'tpid': [33024, 0], 'vport': 1,
                                                                        'number of plugins': 2, 'number of clients': 5}]
        """
        ver_args = [{'name': 'ns_keys', 'arg': ns_keys, 't': EMUNamespaceKey, 'allow_list': True},]
        EMUValidator.verify(ver_args)
        
        ns_keys = listify(ns_keys)
        ns_keys = [k.conv_to_dict(False) for k in ns_keys]
        return self._send_chunks(cmd = 'ctx_get_info', data = {'tunnels': ns_keys})

    @client_api('getter', True)
    @conv_return_val_to_str
    def get_info_clients(self, c_keys):
        """
            Get information for a list of clients from a specific namespace.

            :parameters:
                c_keys: list of EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
            
            :return:
                | List of dictionaries with data for each client, the order is the same order of macs.
                | i.e: [{'mac': '00:00:00:70:00:01', 'ipv4': '1.1.2.3', 'ipv4_dg': '1.1.2.1', 'ipv4_mtu': 1500, 'ipv6_local': 'fe80::200:ff:fe70:1', 'ipv6_slaac': '::', 'ipv6': '2001:db8:1::2', 'dg_ipv6': '::', 'dhcp_ipv6': '::', 'ipv4_force_dg': False, 'ipv4_force_mac': '00:00:00:00:00:00', 'ipv6_force_dg': False, 'ipv6_force_mac': '00:00:00:00:00:00', 'dgw': {'resolve': False, 'rmac': '00:00:00:00:00:00'}, 'ipv6_router': None, 'ipv6_dgw': None, 'plug_names': 'arp, igmp'}]
        """
        ver_args = [{'name': 'c_keys', 'arg': c_keys, 't': EMUClientKey, 'allow_list': True},]
        EMUValidator.verify(ver_args)
        if len(c_keys) == 0:
            return[] 
        
        data = self._conv_macs_and_validate_ns(c_keys)
        return self._send_chunks(cmd = 'ctx_client_get_info', data = data)

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
                | dict: Dictionary of all wanted counters. Keys as tables names and values as dictionaries with data.
                | example when meta = True, all counters will be held in res['table_name']['meta'] as a list.
                | {
                |    'parser': {
                |       'meta': [{
                |           "info": "INFO",
                |            "name": "eventsChangeSrc",
                |            "value": 1,
                |            "zero": false,
                |           "unit": "ops",
                |            "help": "change src ipv4 events"
                |            }],
                |    'name': 'parser'}
                | }
                | 
                | example when meta = False, all counters will be held in res['table_name'] as a dict, counter name as a keys and numeric value as values.
                | {
                |     'parser': {'eventsChangeSrc': 1, ...}
                | }
        """
        ver_args = [{'name': 'meta', 'arg': meta, 't': bool},
            {'name': 'zero', 'arg': zero, 't': bool},
            {'name': 'mask', 'arg': mask, 't': str, 'allow_list': True, 'must': False},
            {'name': 'clear', 'arg': clear, 't': bool},
        ]
        EMUValidator.verify(ver_args)
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
        """
        ver_args = [{'name': 'max_ns_show', 'arg': max_ns_show, 't': int},
            {'name': 'max_c_show', 'arg': max_c_show, 't': int},
            {'name': 'to_json', 'arg': to_json, 't': bool},
            {'name': 'to_yaml', 'arg': to_yaml, 't': bool},
            {'name': 'ipv6', 'arg': ipv6, 't': bool},
            {'name': 'dg4', 'arg': dg4, 't': bool},
            {'name': 'dg6', 'arg': dg6, 't': bool},
            {'name': 'ipv6_router', 'arg': ipv6_router, 't': bool},
        ]
        EMUValidator.verify(ver_args)
        if to_json or to_yaml:
            # gets all data, no pauses
            all_data = self.get_all_ns_and_clients()
            return dump_json_yaml(all_data, to_json, to_yaml)

        all_ns_gen = self._get_n_ns(amount = max_ns_show)

        glob_ns_num = 0
        for ns_chunk_i, ns_list in enumerate(all_ns_gen):
            ns_infos = self.get_info_ns(ns_list)

            if ns_chunk_i != 0:
                wait_for_pressed_key('Press Enter to see more namespaces')
            
            for ns_i, ns_key in enumerate(ns_list):
                glob_ns_num += 1

                # print namespace table
                self._print_ns_table(ns_infos[ns_i], ns_num = glob_ns_num)

                c_gen = self._yield_n_clients_for_ns(ns_key, amount = max_c_show)
                for c_i, c_keys in enumerate(c_gen):                    
                    first_iter = c_i == 0 
                    if not first_iter:
                        wait_for_pressed_key("Press Enter to see more clients")
                    clients = self.get_info_clients(c_keys)
                    # sort by mac address
                    clients.sort(key = lambda x: mac_str_to_num(mac2str(x['mac'])))
                    self._print_clients_chunk(clients, ipv6, dg4, dg6, ipv6_router, first_iter)
        
        if not glob_ns_num:
            text_tables.print_colored_line('There are no namespaces', 'yellow', buffer = sys.stdout)      

    def _print_clients_chunk(self, clients, ipv6, dg4, dg6, ipv6_router, show_title = True):
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
        self._print_clients_table(clients, CLIENT_KEYS_AND_HEADERS, show_relates = relates, title = 'Clients information' if show_title else '',
                                    empty_msg = "There is no client with these parameters")

    def _print_ns_table(self, ns, ns_num = None):
        """
        Print namespace table. 
        
            :parameters:
                ns: dict
                    namespace info as dict
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
    def _send_plugin_cmd_to_ns(self, cmd, ns_key, **kwargs):
        """
        Generic function for plugins, send `cmd` to a given namespace i.e get_cfg for plugin. 
        
            :parameters:
                cmd: string
                    Command to send.
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :returns:
                The requested data for command.
        """
        data = ns_key.conv_to_dict(add_tunnel_key = True)
        data.update(kwargs)
        return self._send_chunks(cmd, data = data)

    def _send_plugin_cmd_to_client(self, cmd, c_key, **kwargs):
        """
        Generic function for plugins, send `cmd` to 1 client useage in arp cmd_query.
        
            :parameters:
                cmd: string
                    Command to send.
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
            :returns:
                The requested data for command.
        """
        data = c_key.conv_to_dict(add_ns = True, to_bytes = True)
        data.update(kwargs)
        return self._send_chunks(cmd, data = data)

    def _send_plugin_cmd_to_clients(self, cmd, c_keys, **kwargs):
        """
        Generic function for plugins, send `cmd` to a list of clients keys i.e dot1x get_clients_info cmd. 
        
            :parameters:
                cmd: string
                    Command to send.
                c_keys: list of EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
            :returns:
                The requested data for command.
        """
        c_keys = listify(c_keys)
        data = self._conv_macs_and_validate_ns(c_keys)
        return self._send_chunks(cmd, data = data)

    # Default Plugins
    @client_api('getter', True)
    def get_def_ns_plugs(self):
        ''' Gets the namespace default plugin '''
        return self._send_chunks('ctx_get_def_plugins', data = None)[0]['def_plugs']

    @client_api('getter', True)
    def get_def_c_plugs(self, ns_key):
        '''
        Gets the client default plugin in a specific namespace 
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        '''
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},]
        EMUValidator.verify(ver_args)
        data = ns_key.conv_to_dict(True)
        return self._send_chunks(cmd = 'ctx_client_get_def_plugins', data = data)[0]['def_plugs']

############################       EMU      #############################
############################       API      #############################
############################                #############################

    @client_api('command', True)
    def shutdown(self, time):
        """Shut down the EMU server gracefully after time seconds.
        If the server is marked for shutdown and it wasn't shut yet, you can
        recall this to set a new time for shutdown.

        Args:
            time (uint32): Time in seconds to shutdown the server from now.
        """
        validate_type("shutdown_time", time, int)
        if not 0 <= time <= 0xFFFFFFFF:
            raise TRexError("Shutdown time {} is not a valid uint32.".format(time))

        if time == 0:
            self.ctx.logger.pre_cmd("Shutting down the EMU server now.")
        else:
            self.ctx.logger.pre_cmd("Shutting down the EMU server in {time} seconds.".format(time=time))
        rc = self._transmit("shutdown", params={"time": time})
        self.ctx.logger.post_cmd(rc)

        if time == 0:
            # server is shut, should disconnect
            self.disconnect()

    # Emu Profile
    @client_api('command', True)
    def load_profile(self, profile, max_rate = None, tunables = None, dry = False, verbose = False):
        """
            .. code-block:: python

                ### Creating a profile and loading it ###

                # creating a namespace key with no vlans
                ns_key = EMUNamespaceKey(vport = 0)

                # plugs for namespace
                plugs = {'igmp': {}}

                # default plugins for each client in the namespace
                def_c_plugs = {'arp': {}}

                # creating a simple client
                mac = Mac('00:00:00:70:00:01)   # creating Mac obj
                kwargs = {
                    'ipv4': [1, 1, 1, 3], 'ipv4_dg': [1, 1, 1, 1],
                    'plugs': {'icmp': {}}
                }
                client = EMUClientObj(mac.V(), **kwargs)  # converting mac to list of bytes

                # creating the namespace with 1 client
                ns = EMUNamespaceObj(ns_key = ns_key, clients = [client], plugs = plugs, def_c_plugs = def_c_plugs)

                # every ns in the profile will have by default ipv6 plugin
                def_ns_plugs = {'ipv6': {}}

                # creating the profile and send it
                profile = EMUProfile(ns = ns, def_ns_plugs = def_ns_plugs)
                emu_client.load_profile(profile = profile)  # using EMUClient

                ### loading from a file ###
                # loading profile from a file, creating 10K clients, sending 1K clients per second.
                emu_client.load_profile(profile = 'emu/simple_emu.py', max_rate = 1000, tunables = ['--clients', '10000'])

            Load emu profile, might be EMUProfile object or a path to a valid profile. Supported type for now is .py
            **Pay attention** sending many clients with no `max_rate` may cause the router to crash, if you are going to send more than 10K clients perhaps you should limit `max_rate` using the plicer to 1000/sec ~.
            
            :parameters:
                profile : string or EMUProfile
                    Filename (with path) of the profile or a valid EMUProfile object. 

                max_rate : int
                    Max clients rate to send (clients/sec), "None" means with no policer interference. see :class:`trex.emu.trex_emu_profile.EMUProfile`

                tunables : list of strings
                    Tunables line as list of strings. i.e: ['--ns', '1', '--clients', '10'].
                
                dry: bool
                    True will not send the profile, only print as JSON.

                verbose: bool
                    True will print timings for converting and sending profile.

            :raises:
                + :exc:`TRexError` In any case of invalid profile.
        """
        if tunables is None:
            tunables = []

        ver_args =[
            {'name': 'profile', 'arg': profile, 't': [EMUProfile, str]},
            {'name': 'max_rate', 'arg': max_rate, 't': int, 'must': False},
            {'name': 'tunables', 'arg': tunables, 't': 'tunables'},
            {'name': 'dry', 'arg': dry, 't': bool},
            {'name': 'verbose', 'arg': verbose, 't': bool},
        ]
        EMUValidator.verify(ver_args)

        help_flags = ('-h', '--help')
        if any(h in tunables for h in help_flags):
            # don't print external messages on help
            profile = EMUProfile.load(profile, tunables)
            return

        s = time.time()
        self.ctx.logger.pre_cmd("Converting file to profile")
        if type(profile) is str:
            profile = EMUProfile.load(profile, tunables)
        if profile is None:            
            self.ctx.logger.post_cmd(False)
            self._err('Failed to convert EMU profile')
        self.ctx.logger.post_cmd(True)
        if verbose:
            print("Converting profile took: %s" % (format_time(time.time() - s)))
        if not dry:
            self._start_profile(profile, max_rate, verbose = verbose)
        else:
            dump_json_yaml(profile.to_json(), to_json = True)

    def _start_profile(self, profile, max_rate, verbose = False):
        """
            Start EMU profile with all the required commands to the server.

            :parameters:
                profile: EMUProfile
                    Emu profile, define all namespaces and clients. see :class:`trex.emu.trex_emu_profile.EMUProfile`
                
                max_rate : int
                    max clients rate to send (clients/sec)

                verbose: bool
                    True will print more messages and a progress bar.
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
            self.add_ns(profile.ns_list)

            # adding all the clients for each namespace
            for ns in profile.ns_list:
                # set client default plugins
                ns_key = ns.key
                self.set_def_c_plugs(ns_key, ns.def_c_plugs)
                clients_list = list(ns.c_map.values())
                self.add_clients(ns_key, clients_list, max_rate = max_rate, verbose = verbose)
        except Exception as e:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Could not load profile, error: %s' % str(e))
        self.ctx.logger.post_cmd(True)
        if verbose:
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
    def add_ns(self, ns_list):
        """
            Add namespaces to EMU server.

            :parameters:
                ns_list: list of EMUNamespaceObj
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceObj`

            :raises:
                + :exc:`TRexError`
        """
        ver_args = [{'name': 'ns_list', 'arg': ns_list, 't': EMUNamespaceObj, 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ns_list = listify(ns_list)
        namespaces_fields = [ns.get_fields() for ns in ns_list]
        data = {'tunnels': namespaces_fields}
        self._send_chunks('ctx_add', data = data)

        return RC_OK()

    @client_api('command', True)
    def remove_ns(self, ns_keys):
        """
            Remove namespaces from EMU server.

            :parameters:
                ns_keys: list of EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :raises:
                + :exc:`TRexError`
        """
        ver_args = [{'name': 'ns_keys', 'arg': ns_keys, 't': EMUNamespaceKey, 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ns_keys = listify(ns_keys)
        # tear down all plugins
        for ns_key in ns_keys:
            for pl_obj in self.registered_plugs.values():
                pl_obj.tear_down_ns(ns_key)

        namespaces_keys = [k.conv_to_dict(False) for k in ns_keys]
        self._send_chunks('ctx_remove', data = {'tunnels': namespaces_keys})

        return RC_OK()

    @client_api('command', True)
    def add_clients(self, ns_key, clients, max_rate = None, verbose = False):
        """
            Add client to EMU server.

            :parameters:

                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`

                clients: list of EMUClientObj
                    see :class:`trex.emu.trex_emu_profile.EMUClientObj`

                max_rate: int
                    Max clients rate to send (clients/sec), "None" means with no policer interference.

                verbose: bool
                    True will print messages to screen as well as a progress bar.

            :raises:
                + :exc:`TRexError`
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
                {'name': 'clients', 'arg': clients, 't': EMUClientObj, 'allow_list': True},
                {'name': 'max_rate', 'arg': max_rate, 't': int, 'must': False},]
        EMUValidator.verify(ver_args)
        clients = listify(clients)
        clients_fields = [c.get_fields(to_bytes = True) for c in clients]
        data = {'clients': clients_fields}
        data.update(ns_key.conv_to_dict(add_tunnel_key = True))
        self._send_chunks('ctx_client_add', data = data, max_data_rate = max_rate, track_progress = verbose)

        return RC_OK()

    @client_api('command', True)
    def remove_clients(self, c_keys, max_rate):
        """
            Remove clients from a specific namespace.

            :parameters:
                c_keys: list of EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
                
                max_rate: int
                    Max clients rate to send (clients/sec), "None" means with no policer interference.

            :raises:
                + :exc:`TRexError`
        """
        ver_args = [{'name': 'c_keys', 'arg': c_keys, 't': EMUClientKey, 'allow_list': True},
                {'name': 'max_rate', 'arg': max_rate, 't': int, 'must': False}]
        EMUValidator.verify(ver_args)
        c_keys = listify(c_keys)
        if len(c_keys) == 0:
            return RC_OK()
        data = self._conv_macs_and_validate_ns(c_keys)
        self._send_chunks('ctx_client_remove', data = data, max_data_rate = max_rate, track_progress = True)

        return RC_OK()

    @client_api('command', True)
    def remove_all_clients_and_ns(self, max_rate = None):
        """ 
            Remove all current namespaces and their clients from emu server 

            :parameters:
                max_rate: int
                    Max clients rate to send (clients/sec), "None" means with no policer interference.
        """
        ver_args = [{'name': 'max_rate', 'arg': max_rate, 't': int, 'must': False}]
        EMUValidator.verify(ver_args)
        ns_keys_gen = self._get_n_ns()

        for ns_chunk in ns_keys_gen:
            for ns_key in ns_chunk:
                c_keys = self.get_all_clients_for_ns(ns_key)
                self.remove_clients(c_keys, max_rate)
            self.remove_ns(ns_chunk)
    
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
        ver_args = [{'name': 'def_plugs', 'arg': def_plugs, 't': dict, 'must': False},]
        EMUValidator.verify(ver_args)

        self._send_chunks('ctx_set_def_plugins', data = {'def_plugs': def_plugs})

        return RC_OK()

    @client_api('command', True)
    def set_def_c_plugs(self, ns_key, def_plugs):
        """
            Set the client default plugins. Every new client in that namespace will have that plugin.

            :parameters:

                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`

                def_plugs: dictionary
                    Map plugin_name -> plugin_data, each plugin here will be added to every new client.
                    If new client will provide a plugin, it will override the default one. 
        """
        ver_args =[{'name': 'def_plugs', 'arg': def_plugs, 't': dict, 'must': False},
                {'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey, 'allow_list': True},
        ]
        EMUValidator.verify(ver_args)
        data = {'def_plugs': def_plugs}
        data.update(ns_key.conv_to_dict(add_tunnel_key = True))
        self._send_chunks('ctx_client_set_def_plugins', data = data)

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

    @plugin_api('shutdown', 'emu')
    def shutdown_line(self, line):
        """Shutdown the EMU server.\n"""

        parser = parsing_opts.gen_parser(self,
                                "shutdown",
                                self.shutdown_line.__doc__,
                                parsing_opts.EMU_SHUTDOWN_TIME,
                                )
        opts = parser.parse_args(args = line.split())
        self.shutdown(opts.time)
        return True

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
        self.load_profile(opts.file[0], opts.max_rate, opts.tunables, opts.dry, verbose = True)
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
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        ns_info = self.get_info_ns([ns_key])
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
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = [EMUClientKey(ns_key = ns_key, mac = opts.mac)]
        c_info = self.get_info_clients(c_key)
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
                ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
                c_key = None if mac is None else EMUClientKey(ns_key, mac)
                data_cnt.set_add_data(ns_key = ns_key, c_key = c_key)
            elif opts.all_ns:

                ns_gen = self._get_n_ns(amount = None)
                ns_i = 0

                for ns_chunk in ns_gen:
                    for ns_key in ns_chunk:
                        ns_i += 1
                        self._print_ns_table(ns_key.conv_to_dict(False), ns_i)
                        data_cnt.set_add_data(ns_key = ns_key)
                        run_on_demend(data_cnt, opts)
                if not ns_i:
                    self._err('there are no namespaces in emu server')
                return True
            else:
                raise self._err('Namespace information required, supply them or run with --all-ns ')

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
                self._err('Exception during import of %s, filename: "%s", message: %s' % (import_path, filename, e))
            
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
    def connect(self):
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

    @client_api('getter', False)
    def is_connected(self):
        """
            Indicate if the connection is established to the server.

            :parameters:
                None

            :return:
                True if the connection is established to the server
                o.w False

            :raises:
                None
        """
        return self.conn.is_connected()

    @client_api('command', True)
    def acquire(self, force = False):
        pass

    @client_api('command', True)
    def release(self, force = False):
        pass

    @client_api('command', False)
    def disconnect(self, release_ports = True):
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
