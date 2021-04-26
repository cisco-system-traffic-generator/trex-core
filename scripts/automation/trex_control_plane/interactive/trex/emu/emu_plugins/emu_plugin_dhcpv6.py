from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts


class DHCPV6Plugin(EMUPluginBase):
    '''Defines DHCPV6 plugin
        RFC 8415  DHCPv6 client
    '''

    plugin_name = 'DHCPV6'

    # init jsons example for SDK
    INIT_JSON_NS = {'dhcpv6': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'dhcpv6': {'timerd': 5, 'timero': 10}}
    """
    :parameters:
        timerd: uint32
            DHCP timer discover in sec.
        timero: uint32
            DHCP timer offer in sec.
        options: dict
            Dictionary that contains options 

    :options:

        sol: array of bytes can include a few options ([option1,len1, bytes1..,option2,len2, bytes1..]
            for Solicit packet e.g. [0,6, 0,8,0, 17, 0, 23, 0, 24, 0,39, 0,6, 0,8,0, 17, 0, 23, 0, 24, 0,39] 2 options of vendor class 

        req: array of bytes 
            for Request packet same as sol field 

        rel: array of bytes 
            for Release packet same as sol field 

        reb: array of bytes 
            for Rebind packet same as sol field 

        ren: array of bytes
            for Renew packet same as sol field 

        rm_or: bool 
            remove default option request (should add the same extended options using sol,req)

        rm_vc: bool 
            remove default vendor class

    """

    def __init__(self, emu_client):
        super(DHCPV6Plugin, self).__init__(emu_client, 'dhcpv6_client_cnt')

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
    @plugin_api('dhcpv6_show_counters', 'emu')
    def dhcpv6_show_counters_line(self, line):
        '''Show dhcpv6 counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_dhcpv6",
                                        self.dhcpv6_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True
