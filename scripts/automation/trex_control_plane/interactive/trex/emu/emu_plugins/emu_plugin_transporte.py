from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json


class TRANSPORTEPlugin(EMUPluginBase):
    '''Defines Transport plugin  '''

    plugin_name = 'transe'

    # init jsons example for SDK
    INIT_JSON_NS = {'transe': {}}
    """
    :parameters:
        addr: string format [ip:port] example 48.0.0.1:9001

        size: bytes for request size, e.g. 10000

        loops: uint32  number of loops 

    """

    INIT_JSON_CLIENT = {'transe': {}}
    """
    """

    def __init__(self, emu_client):
        super(TRANSPORTEPlugin, self).__init__(emu_client, ns_cnt_rpc_cmd='transe_ns_cnt')

    # Plugins methods

    @plugin_api('transe_show_counters', 'emu')
    def transport_e_show_counters_line(self, line):
        '''Show transport example counters (per ns).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_transe",
                                        self.transport_e_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns = True)

        return True
