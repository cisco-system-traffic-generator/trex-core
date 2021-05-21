from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json


class CDPPlugin(EMUPluginBase):
    '''Defines CDP plugin  '''

    plugin_name = 'CDP'

    # init jsons example for SDK
    INIT_JSON_NS = {'cdp': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'cdp': {}}
    """
    :parameters:

        timer: uint32
            time in seconds betwean packets 
        
        ver  : uint32
            1 or 2 

        cs   : uint16
            replace the checksum to generate bad checksum. in case of zero calculate the right one.

        options: dict
           the options to add to cdp 

    :options:

        raw:  array of byte 
           generic options array for cdp
           [60,8,77,83,70,84,32,53,46,48]

    """

    def __init__(self, emu_client):
        super(CDPPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='cdp_client_cnt')

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
    @plugin_api('cdp_show_counters', 'emu')
    def cdp_show_counters_line(self, line):
        '''Show cdp counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_cdp",
                                        self.cdp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
