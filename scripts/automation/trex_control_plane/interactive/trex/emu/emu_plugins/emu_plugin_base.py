from ..trex_emu_counters import DataCounter
from ..trex_emu_conversions import *
from trex.utils import text_tables
from trex.emu.trex_emu_validator import EMUValidator

from trex.emu.api import *

class EMUPluginBase(object):
    ''' Every object inherit from this class can implement a plugin method with decorator @plugin_api('cmd_name', 'emu')'''
    
    def __init__(self, emu_client, cnt_rpc_cmd):
        self.emu_c  = emu_client
        self.conn   = emu_client.conn
        self.err    = emu_client._err  # simple err method
        self.data_c = DataCounter(emu_client.conn, cnt_rpc_cmd)

    # Common API
    @client_api('getter', True)
    def get_counters(self, ns_key, meta = False, zero = False, mask = None, clear = False):
        """
        Get plugin counters from emu server. 

            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                meta: bool
                    True will just return meta data, defaults to False.
                zero: bool
                    True will show zero values as well, defaults to False.
                mask: list of strings
                    List of strings representing tables to show (use --headers to see all tables), defaults to None means all of them.
                clear: bool
                    True will clear counters, defaults to False.

            :return:
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
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
            {'name': 'meta',  'arg': meta, 't': bool},
            {'name': 'zero',  'arg': zero, 't': bool},
            {'name': 'mask',  'arg': mask, 't': str, 'allow_list': True, 'must': False},
            {'name': 'clear', 'arg': clear, 't': bool},
        ]
        EMUValidator.verify(ver_args)
        self.data_c.set_add_data(ns_key = ns_key)
        return self.data_c._get_counters(meta, zero, mask, clear)

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
