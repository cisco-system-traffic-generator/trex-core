from functools import wraps
from ..trex_emu_counters import DataCounter
from ..trex_emu_conversions import *
from trex.utils import text_tables
from trex.emu.trex_emu_validator import EMUValidator
from trex.emu.api import *


class EMUPluginBase(object):
    ''' Every object inherit from this class can implement a plugin method with decorator @plugin_api('cmd_name', 'emu')'''

    def __init__(self, emu_client, ns_cnt_rpc_cmd = None, client_cnt_rpc_cmd = None):
        self.emu_c  = emu_client
        self.conn   = emu_client.conn
        self.err    = emu_client._err  # simple err method
        if ns_cnt_rpc_cmd is None and client_cnt_rpc_cmd is None:
            raise TRexError("At least one of client or namespace counter commands must be provided.")
        self.ns_data_cnt = None
        self.client_data_cnt = None
        if ns_cnt_rpc_cmd:
            self.ns_data_cnt = DataCounter(emu_client.conn, ns_cnt_rpc_cmd)
        if client_cnt_rpc_cmd:
            self.client_data_cnt = DataCounter(emu_client.conn, client_cnt_rpc_cmd)

    # Common API
    @client_api('getter', True)
    def _get_ns_counters(self, ns_key, cnt_filter=None, zero=True, verbose=True):
        """
            Get the $PLUGIN_NAME counters of a namespace.

            :parameters:
                ns_key: :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                    EMUNamespaceKey

                cnt_filter: list
                    List of counters types as strings. i.e: ['INFO', 'ERROR', 'WARNING']. Default is None, means no filter.
                    If verbosity is off, this filter can't be used.

                zero: bool
                    Get zero values, default is True.

                verbose: bool
                    Show verbose version of each counter, default is True.

            :return: Dictionary of all wanted counters. When verbose is True, each plugin contains a list of counter dictionaries.

                .. highlight:: python
                .. code-block:: python

                    {'pluginName': [{'help': 'Explanation 1',
                                     'info': 'INFO',
                                     'name': 'counterOne',
                                     'unit': 'pkts',
                                     'value': 8,
                                     'zero': False},
                                    {'help': 'Explanation 2',
                                     'info': 'ERROR',
                                     'name': 'counterTwo',
                                     'unit': 'pkts',
                                     'value': 6,
                                     'zero': False}]}

                When verbose is False, each plugins returns a dictionary of counterName, value pairs.

                .. highlight:: python
                .. code-block:: python

                    {'pluginName': {'counterOne': 8, 'counterTwo': 6}}

            :raises: TRexError
        """
        if not self.ns_data_cnt:
            raise TRexError("Namespace counter command was not provided.")
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
                    {'name': 'cnt_filter', 'arg': cnt_filter, 't': list, 'must': False},
                    {'name': 'zero', 'arg': zero, 't': bool},
                    {'name': 'verbose', 'arg': verbose, 't': bool}]
        EMUValidator.verify(ver_args)
        self.ns_data_cnt.set_add_data(ns_key=ns_key)
        return self.ns_data_cnt.get_counters(cnt_filter=cnt_filter, zero=zero, verbose=verbose)

    @client_api('getter', True)
    def _get_client_counters(self, c_key, cnt_filter=None, zero=True, verbose=True):
        """
            Get the $PLUGIN_NAME counters of a client.

            :parameters:
                c_key: :class:`trex.emu.trex_emu_profile.EMUClientKey`
                    EMUClientKey

                cnt_filter: list
                    List of counters types as strings. i.e: ['INFO', 'ERROR', 'WARNING']. Default is None, means no filter.
                    If verbosity is off, this filter can't be used.

                zero: bool
                    Get zero values, default is True.

                verbose: bool
                    Show verbose version of each counter, default is True.

            :return: Dictionary of all wanted counters. When verbose is True, each plugin contains a list of counter dictionaries.

                .. highlight:: python
                .. code-block:: python

                    {'pluginName': [{'help': 'Explanation 1',
                                    'info': 'INFO',
                                    'name': 'counterOne',
                                    'unit': 'pkts',
                                    'value': 8,
                                    'zero': False},
                                    {'help': 'Explanation 2',
                                    'info': 'ERROR',
                                    'name': 'counterTwo',
                                    'unit': 'pkts',
                                    'value': 6,
                                    'zero': False}]}

                When verbose is False, each plugins returns a dictionary of counterName, value pairs.

                .. highlight:: python
                .. code-block:: python

                    {'pluginName': {'counterOne': 8, 'counterTwo': 6}}

            :raises: TRexError
        """
        if not self.client_data_cnt:
            raise TRexError("Client counter command was not provided.")
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey},
                    {'name': 'cnt_filter', 'arg': cnt_filter, 't': list, 'must': False},
                    {'name': 'zero', 'arg': zero, 't': bool},
                    {'name': 'verbose', 'arg': verbose, 't': bool}]
        EMUValidator.verify(ver_args)
        self.client_data_cnt.set_add_data(c_key=c_key)
        return self.client_data_cnt.get_counters(cnt_filter=cnt_filter, zero=zero, verbose=verbose)

    @client_api('command', True)
    def _clear_ns_counters(self, ns_key):
        """
            Clear the $PLUGIN_NAME counters of a namespace.

            :parameters:
                ns_key: :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                    EMUNamespaceKey

            :returns:
                Boolean indicating if clearing was successful.
                  - True: Clearing was successful.
                  - False: Clearing was not successful.

            :raises: TRexError
        """
        if not self.ns_data_cnt:
            raise TRexError("Namespace counter command was not provided.")
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        self.ns_data_cnt.set_add_data(ns_key=ns_key)
        return self.ns_data_cnt.clear_counters()

    @client_api("command", True)
    def _clear_client_counters(self, c_key):
        """
            Clear the $PLUGIN_NAME counters of a client.

            :parameters:
                c_key: :class:`trex.emu.trex_emu_profile.EMUClientKey`
                    EMUClientKey

            :returns:
                Boolean indicating if clearing was successful.
                  - True: Clearing was successful.
                  - False: Clearing was not successful.

            :raises: TRexError
        """
        if not self.client_data_cnt:
            raise TRexError("Client counter command was not provided.")
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey}]
        EMUValidator.verify(ver_args)
        self.client_data_cnt.set_add_data(c_key=c_key)
        return self.client_data_cnt.clear_counters()

    # Override functions
    def tear_down_ns(self, ns_key):
        """
        This function will be called before removing this plugin from namespace.

            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        """        
        pass

    # Common helper functions
    def run_on_all_ns(self, func, print_ns_info = False, func_on_res = None, func_on_res_args = None, **kwargs):
        if func_on_res_args is None:
            func_on_res_args = {}

        ns_gen = self.emu_c._get_n_ns(amount = None)
        glob_ns_num = 0

        for ns_chunk in ns_gen:
            ns_infos = self.emu_c.get_info_ns(list(ns_chunk))

            for ns_i, ns_key in enumerate(ns_chunk):
                glob_ns_num += 1

                if print_ns_info:
                    self.emu_c._print_ns_table(ns_infos[ns_i], glob_ns_num)
                res = func(ns_key, **kwargs)            
                if func_on_res is not None:
                    func_on_res(res, **func_on_res_args)
                    print('\n')  # new line between each info 
        if not glob_ns_num:
            self.err('there are no namespaces in emu server')

    def print_plug_cfg(self, cfg):
        self.emu_c._print_dict_as_table(cfg, title = 'Plugin "%s" cfg:' % self.plugin_name)

    def print_gen_data(self, data, title = None, empty_msg = 'empty'):

        if not data:
            text_tables.print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)
            return

        if title is not None:
            text_tables.print_colored_line(title, 'yellow', buffer = sys.stdout)

        for one_data in data:
            print(conv_unknown_to_str(one_data))
        print('')  # new line for seperation 

    def print_table_by_keys(self, data, keys_to_headers, title = None, empty_msg = 'empty'):

        def _iter_dict(d, keys, max_lens):
            row_data = []
            for j, key in enumerate(keys):
                val = str(conv_to_str(d.get(key), key))
                row_data.append(val)
                max_lens[j] = max(max_lens[j], len(val))
            return row_data

        if len(data) == 0:
            text_tables.print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)      
            return

        table = text_tables.TRexTextTable(title)
        headers = [e.get('header') for e in keys_to_headers]
        keys = [e.get('key') for e in keys_to_headers]
        
        max_lens = [len(h) for h in headers]
        table.header(headers)

        if type(data) is list:
            for one_record in data:
                row_data = _iter_dict(one_record, keys, max_lens)
                table.add_row(row_data)
        elif type(data) is dict:
                row_data = _iter_dict(data, keys, max_lens)
                table.add_row(row_data)

        # fix table look
        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))
        
        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)

    # Private functions
    def _create_ip_vec(self, ip_start, ip_count, ip_type = 'ipv4', mc = True):
        """
        Helper function, creates a vector for ipv4 or 6.
        Notice: _create_ip_vec([1, 0, 0, 0], 2) -> [[1, 0, 0, 0], [1, 0, 0, 1]]
        
            :parameters:
                ip_start: string
                    First ip in vector.
                ip_count: int
                    Total ip's to add.
                ip_type: str
                    ipv4 / ipv6, defaults to 'ipv4'.
                mc: bool
                    is multicast or not.
            :raises:
                + :exe:'TRexError'
            :returns:
                list: list of ip's as EMUType
        """
        if ip_type == 'ipv4':
            ip = Ipv4(ip_start, mc = mc)
        elif ip_type == 'ipv6':
            ip = Ipv6(ip_start, mc = mc)
        else:
            self.err('Unknown ip type: "%s", use ipv4 or ipv6' % ip_type)
        vec = []
        for i in range(ip_count):
            vec.append(ip[i])
        return vec

    def _validate_port(self, opts):
        if 'port' in opts and opts.port is None:
            if 'all_ns' in opts:
                self.err('Namespace information required, supply them or run with --all-ns')
            else:
                self.err('Namespace information required, missing port supply it with -p')

    @property
    def logger(self):
        return self.emu_c.logger


def update_docstring(text):
    """ Used to update the docstring for a function """
    def decorator(original_method):

        @wraps(original_method)
        def wrapper(self, *args, **kwargs):
            return original_method(self, *args ,**kwargs)

        wrapper.__doc__ = text
        return wrapper

    return decorator