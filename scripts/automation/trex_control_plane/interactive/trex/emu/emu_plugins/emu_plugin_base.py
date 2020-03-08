from ..trex_emu_counters import DataCounter
from ..trex_emu_conversions import *
from trex.utils import text_tables

from trex.emu.api import *

import itertools
import json

class EMUPluginBase(object):
    ''' Every object inherit from this class can implement a plugin method with decorator @plugin_api('cmd_name', 'emu')'''
    
    def __init__(self, emu_client, cnt_rpc_cmd):
        self.emu_c  = emu_client
        self.conn   = emu_client.conn
        self.err    = emu_client.err  # simple err method
        self.data_c = DataCounter(emu_client.conn, cnt_rpc_cmd)

    # Common API
    @client_api('getter', True)
    def get_counters(self, port, vlan = None, tpid = None, meta = False, zero = False, mask = None, clear = False):
        self.data_c.set_add_data(port, vlan, tpid)
        return self.data_c._get_counters(meta, zero, mask, clear)

    @client_api('getter', False)
    def print_all_ns_clients(self, data, verbose = False, to_json = False, to_yaml = False):
        DataCounter.print_counters(data, verbose, to_json, to_yaml)

    # Common helper functions
    def run_on_all_ns(self, func, print_ns_info = False, func_on_res = None, func_on_res_args = None, **kwargs):
        if func_on_res_args is None:
            func_on_res_args = {}

        ns_gen = self.emu_c.get_n_ns(amount = None)
        glob_ns_num = 0

        for ns_chunk in ns_gen:
            ns_infos = self.emu_c.get_info_ns(list(ns_chunk))

            for ns_i, ns in enumerate(ns_chunk):
                glob_ns_num += 1
                
                # add ns info to current namespace
                ns_info = ns_infos[ns_i]
                ns.update(ns_info)

                if print_ns_info:
                    self.emu_c._print_ns_table(ns, glob_ns_num)
                res = func(ns.get('vport'), ns.get('tci'), ns.get('tpid'), **kwargs)            
                if func_on_res is not None:
                    func_on_res(res, **func_on_res_args)
                    print('\n')  # new line between each info 
        if not glob_ns_num:
            self.err('there are no namespaces in emu server')

    def print_plug_cfg(self, cfg):
        self.emu_c.print_dict_as_table(cfg, title = 'Plugin "%s" cfg:' % self.plugin_name)

    def print_gen_data(self, data, title = None, empty_msg = 'empty'):

        if not data:
            text_tables.print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)
            return

        for data_i, data in enumerate(data):
            if type(data) is dict:
                self.emu_c.print_dict_as_table(data, title)
            else:
                if data_i == 0 and title is not None:
                    text_tables.print_colored_line(title, 'yellow', buffer = sys.stdout)
                print(conv_unknown_to_str(data))
        print()  # new line for seperation 

    @property
    def logger(self):
        return self.emu_c.logger
