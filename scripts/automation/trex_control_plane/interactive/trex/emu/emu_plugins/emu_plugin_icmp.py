from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json

class ICMPPlugin(EMUPluginBase):
    '''Defines ICMP plugin'''

    plugin_name = 'ICMP'

    def __init__(self, emu_client):
        super(ICMPPlugin, self).__init__(emu_client, 'icmp_ns_cnt')

    # Plugins methods

    @plugin_api('icmp_show_counters', 'emu')
    def icmp_show_counters_line(self, line):
        '''Show icmp counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_icmp",
                                        self.icmp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True
