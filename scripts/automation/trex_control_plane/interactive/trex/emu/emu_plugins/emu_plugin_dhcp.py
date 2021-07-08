from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts


class DHCPPlugin(EMUPluginBase):
    """
    Defines DHCP plugin based on `DHCP <https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol>`_ 

    Implemented based on `RFC 2131 Client <https://datatracker.ietf.org/doc/html/rfc2131>`_ 
    """

    plugin_name = 'DHCP'

    # init jsons example for SDK
    INIT_JSON_NS = {'dhcp': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'dhcp': {'timerd': 5, 'timero': 10}}
    """
    :parameters:
        timerd: uint32
            DHCP timer discover in seconds.

        timero: uint32
            DHCP timer offer in seconds.

        options: dict
            Dictionary that contains DHCP Options:

    :options:

        discoverDhcpClassIdOption: string 
            ClassIdOption option for discover packet e.g. "MSFT 5.0"

        requestDhcpClassIdOption: string 
            ClassIdOption option for request packet 

        dis: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Discovery Packet.

            Each byte list should be of the format [Type][Data] **(pay attention, no length)**.

            For example:

                    .. highlight:: python
                    .. code-block:: python

                        "dis": [
                            [60, 8, 77, 83, 70, 84, 32, 53, 46, 48], # Vendor Class Id - Type 60
                            [0x0c, 0x68, 0x6f, 0x73, 0x74, 0x2d, 0x74, 0x72, 0x65, 0x78] # Hostname - Type 12
                        ]

        req: List[List[byte]]
             List of byte lists. Each byte list represents an Option that will be added to a Request Packet.


        ren: List[List[byte]]
             List of byte lists. Each byte list represents an Option that will be added to a Renew Packet.

    """

    def __init__(self, emu_client):
        super(DHCPPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='dhcp_client_cnt')

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
    @plugin_api('dhcp_show_counters', 'emu')
    def dhcp_show_counters_line(self, line):
        '''Show dhcp counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_dhcp",
                                        self.dhcp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
