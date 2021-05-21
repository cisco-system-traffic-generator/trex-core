from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json


class TRANSPORTPlugin(EMUPluginBase):
    '''Defines Transport plugin  '''

    plugin_name = 'transport'

    # init jsons example for SDK
    INIT_JSON_NS = {'transport': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'transport': {}}
    """
    """

    def __init__(self, emu_client):
        super(TRANSPORTPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='transport_client_cnt')

    # Plugins methods

    @plugin_api('transport_show_counters', 'emu')
    def transport_show_counters_line(self, line):
        '''Show transport counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_transport",
                                        self.transport_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
