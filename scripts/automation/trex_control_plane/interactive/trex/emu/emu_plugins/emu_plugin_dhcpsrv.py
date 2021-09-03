from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts


class DHCPSRVPlugin(EMUPluginBase):
    """
    Defines DHCP Server plugin based on `DHCP <https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol>`_ 

    Implemented based on `RFC 2131 Server <https://datatracker.ietf.org/doc/html/rfc2131>`_ 
    """

    plugin_name = 'DHCPSRV'

    INIT_JSON_NS = {'dhcpsrv': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'dhcpsrv': "Pointer to INIT_JSON_NS below"}
    """
    :parameters:
        default_lease: uint32
            Default lease time in seconds to offer to DHCP clients. Defaults to 300 seconds, 5 mins.

        max_lease: uint32
            Maximal lease time in seconds that the server is willing to offer the client in case he requests a specific lease.
            If `default_lease` is provided and greater than an unprovided `max_lease`, then `max_lease` will be overridden 
            by `default_lease`. Defaults to 600 seconds, 10 mins.

        min_lease: uint32
            Minimal lease time in seconds that the server is willing to offer the client in case he requests a specific lease.
            If `default_lease` is provided and less than an unprovided `min_lease`, then `min_lease` will be overridden
            by `default_lease`. Defaults to 60 seconds, 1 min.

        next_server_ip: str
            IPv4 address of the next server as a field. In case you provide it, the server will write the IPv4 as the next server IPv4
            in the packets it sends. Defaults to 0.0.0.0.

        pools: list
            List of dictionaries that represent IPv4 pools or otherwise known as scopes. At lease one pool must be provided.
            Each dictionary is composed of:

            :min: str

                Minimal IPv4 address of the pool. If this happens to be the Network Id, this address will be skipped.

            :max: str

                Maximal IPv4 address of the pool. If this happens to be the Broadcast Id, this address will be skipped.

            :prefix: uint8

                Subnet Mask represented as a prefix, an unsigned integer between (0, 32) non exclusive.

            :exclude: list

                List of IPv4 strings that are excluded from the pool and can't be offered to the client.

            .. note:: Two different pools cannot be in the same subnet. If two pools share the same subnet, with the current implementation we will always offer an IP from the first pool in the list. 

            .. highlight:: python
            .. code-block:: python

                "pools": [
                    {
                        "min": "192.168.0.0",
                        "max": "192.168.0.100",
                        "prefix": 24,
                        "exclude": ["192.168.0.1", "192.168.0.2"]
                    },
                    {
                        "min": "10.0.0.2",
                        "max": "10.0.255.255",
                        "prefix": 8
                    }
                ]


        options: dict
            Dictionary that contains DHCP Options. There are three keys possible: `offer`, `ack` and `nak`.
            Each key represents a DHCP Response that the server can send. 

            Each key's value is a list.  The list is composed by dictionaries, where each dictionary represents a DHCP option.

            Options are represented by their type (byte), and their value (byte list).

            In the following example, we add the following options to `offer` and `ack` responses. 

            Type: 6 (DNS Server) -> Value (8.8.8.8)

            Type: 15 (Domain Name) -> Value cisco.com

            .. highlight:: python
            .. code-block:: python

                "options": {
                    "offer": [
                        {
                            "type": 6,
                            "data": [8, 8, 8, 8]
                        },
                        {
                            "type": 15,
                            "data": [99, 105, 115, 99, 111, 46, 99, 111, 109]
                        }
                    ]
                    "ack": [
                        {
                            "type": 6,
                            "data": [8, 8, 8, 8]
                        },
                        {
                            "type": 15,
                            "data": [99, 105, 115, 99, 111, 46, 99, 111, 109]
                        }
                    ]
                }
    """

    def __init__(self, emu_client):
        super(DHCPSRVPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='dhcpsrv_c_cnt')

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
    @plugin_api('dhcpsrv_show_counters', 'emu')
    def dhcpsrv_show_counters_line(self, line):
        '''Show DHCP Server counters.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_dhcpsrv",
                                        self.dhcpsrv_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
