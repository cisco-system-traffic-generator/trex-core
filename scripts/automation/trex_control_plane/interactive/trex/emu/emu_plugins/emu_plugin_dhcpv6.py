from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts


class DHCPV6Plugin(EMUPluginBase):
    """
    Defines DHCPv6 plugin based on `DHCPv6 <https://en.wikipedia.org/wiki/DHCPv6>`_ 

    Implemented based on `RFC 8415 Client <https://datatracker.ietf.org/doc/html/rfc8415>`_ 
    """

    plugin_name = 'DHCPV6'

    # init json example for SDK
    INIT_JSON_NS = {'dhcpv6': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'dhcpv6': {'timerd': 5, 'timero': 10}}
    """
    :parameters:
        timerd: uint32
            DHCP timer discover in seconds.

        timero: uint32
            DHCP timer offer in seconds.

        options: dict
            Dictionary that contains DHCPv6 Options:

    :options:

        rm_or: bool
            Remove default Option Request (Should add another Option Request using **sol**, **req**)

        rm_vc: bool
            Remove default Vendor Class Option

        sol: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Solicit Packet.

            Each byte list should be of the format [Code][Data] **(pay attention, no length)**.

            For example:

                    .. highlight:: python
                    .. code-block:: python

                        "sol": [
                           [0, 6, 0, 23, 0, 24, 0, 243, 0, 242, 0, 59, 0, 242, 0, 39], # Option Request - Code 6
                           [0, 15, 98, 101, 115] # User Class Option - Code 15
                        ]

        req: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Request Packet.

        rel: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Release Packet.

        reb: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Rebind Packet.

        ren: List[List[byte]]
            List of byte lists. Each byte list represents an Option that will be added to a Renew Packet.

    """

    def __init__(self, emu_client):
        super(DHCPV6Plugin, self).__init__(emu_client, client_cnt_rpc_cmd='dhcpv6_client_cnt')

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
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
