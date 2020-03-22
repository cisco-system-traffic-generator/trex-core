from ..trex_emu_counters import DataCounter
from ..trex_emu_conversions import *
from trex.utils import text_tables
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
    def get_counters(self, port, vlan = None, tpid = None, meta = False, zero = False, mask = None, clear = False):
        self.data_c.set_add_data(port, vlan, tpid)
        return self.data_c._get_counters(meta, zero, mask, clear)

    @client_api('getter', False)
    def print_all_ns_clients(self, data, verbose = False, to_json = False, to_yaml = False):
        DataCounter.print_counters(data, verbose, to_json, to_yaml)

    # Override functions
    def tear_down_ns(self, port, vlan, tpid):
        ''' This function will be called before removing this plugin from namespace'''
        pass

    # Common helper functions
    def run_on_all_ns(self, func, print_ns_info = False, func_on_res = None, func_on_res_args = None, **kwargs):
        if func_on_res_args is None:
            func_on_res_args = {}

        ns_gen = self.emu_c._get_n_ns(amount = None)
        glob_ns_num = 0

        for ns_chunk in ns_gen:
            ns_infos = self.emu_c.get_info_ns(list(ns_chunk))

            for ns_i, ns in enumerate(ns_chunk):
                glob_ns_num += 1

                if print_ns_info:
                    self.emu_c._print_ns_table(ns_infos[ns_i], glob_ns_num)
                res = func(ns.get('vport'), ns.get('tci'), ns.get('tpid'), **kwargs)            
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

        if len(data) == 0:
            text_tables.print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)      
            return

        table = text_tables.TRexTextTable(title)
        headers = [e.get('header') for e in keys_to_headers]
        keys = [e.get('key') for e in keys_to_headers]
        
        max_lens = [len(h) for h in headers]
        table.header(headers)

        for i, one_record in enumerate(data):
            row_data = []
            for j, key in enumerate(keys):
                val = str(conv_to_str(one_record.get(key), key))
                row_data.append(val)
                max_lens[j] = max(max_lens[j], len(val))
            table.add_row(row_data)

        # fix table look
        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))
        
        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)


    def create_ip_vec(self, ip_start, ip_count, ip_type = 'ipv4'):
        """
        Helper function, creates a vector for ipv4 or 6.
        Notice: create_ip_vec('1.0.0.0', 0) -> ['1.0.0.0']
        
            :parameters:
                ip_start: string
                    First ip in vector.
                ip_count: int
                    Total ip's to add.
                ip_type: str
                    ipv4 / ipv6, defaults to 'ipv4'.
        
            :raises:
                + :exe:'TRexError'
        
            :returns:
                list: list of ip's as strings
        """
        vec = [ip_start]
        for _ in range(ip_count):
            if ip_type == 'ipv4':
                new_ip = increase_ip(vec[-1])
            elif ip_type == 'ipv6':
                new_ip = increase_ipv6(vec[-1])
            else:
                raise TRexError('Unknown ip type: "%s", use ipv4 or ipv6' % ip_type)
            vec.append(new_ip)
        return vec

    @property
    def logger(self):
        return self.emu_c.logger
