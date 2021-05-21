from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json


class LLDPPlugin(EMUPluginBase):
    '''Defines LLDP plugin  '''

    plugin_name = 'LLDP'

    # init jsons example for SDK
    INIT_JSON_NS = {'lldp': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'lldp': {}}
    """
    :parameters:

        timer: uint32
            time in seconds betwean packets 
        options: dict
            Dictionary that contains options 

    :options:

        raw:  array of byte 
           generic options array for lldp options 
           [60,8,77,83,70,84,32,53,46,48]

        remove_default: bool
            remove the default headers 
           
    """

    def __init__(self, emu_client):
        super(LLDPPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='lldp_client_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_counters(self, c_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_client_counters(c_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_counters(self, c_key):
        return self._clear_client_counters(c_key)

    # Plugins methods
    @plugin_api('lldp_show_counters', 'emu')
    def lldp_show_counters_line(self, line):
        '''Show lldp counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_lldp",
                                        self.lldp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
