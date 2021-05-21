from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts


class TDLPlugin(EMUPluginBase):
    """
        Defines a new plugin implementing Cisco's The Definition Language (TDL).
    """

    plugin_name = 'tdl'

    # init json examples for SDK
    INIT_JSON_NS = None
    """
    There is currently no NS init json for TDL.
    """

    INIT_JSON_CLIENT = { 'tdl': "Pointer to INIT_JSON_CLIENT below" }
    """

    :parameters:

        dst: string
            The destination address. It is a string of the format host:port, where host can be an Ipv4 or an Ipv6 while the port should be a valid
            transport port. Example are '127.0.0.1:8080', '[2001:db8::1]:4739'

        udp_debug: bool
            Indicate if we are debugging and should use UDP instead of TCP.

        rate_pps: float32
            Rate of TDL packets (in pps), defaults to 1.

        header: dict
            Dictionary that contains TDL header fields. See header keys below.

        meta_data: list
            A list of dictionaries where each dictionary defines a new type by its metadata. See dictionary keys below.

        object: dict
            The TDL object we are serializing. See keys below.

        init_values: list
            List of dictionaries setting initial values for the TDL unconstructed types. See dictionary keys below.

        engines: list
            List of engines instructing how to change the TDL unconstructed types. See engine keys below.

    :header:

        magic: byte
            Magic Byte

        fru: byte
            Fru

        src_chassis: byte
            Source chassis

        src_slot: byte
            Source slot

        dst_chassis: byte
            Destination chassis

        dst_slot: byte
            Destination slot

        bay: byte
            Bay

        state_tracking: byte
            State tracking

        flag: byte
            Flag

        domain_hash: int32
            Domain Hash

        len: int32
            Length

        uuid: int64
            Universally unique ID

        tenant_id: int16
            Tenant ID

        luid: LUID
            Locally unique ID for the object we are serializing.

    :meta_data:

        name: string
            Name of this new type.

        type: string
            Type of this new type, it can be enum_def, flag_def, type_def etc.

        data: dict
            A dictionary that contains two entries:

            luid: LUID
                LUID of the new type we are defining

            entries: list
                Entries for the new type. Depends on the type.

    :object:

        name: string
            Name of the TDL object. Used only for reference to other values.

        type: string
            Type of the TDL object.

    :init_values: list of dict

        path: string
            Path to the object. Hierarchy levels are represented by. For example var1.var2.var3 represents var3 that is defined in var2 which is defined in var1.

        value: *
            Initial value for the TDL object, depending on the type of object.


    :engines:
    
        engine_name: string
            Name of the engine. Must be the name of one of the fields.

        engine_type: string
            Type of engine, can be {uint, histogram_uint, histogram_uint_range, histogram_uint_list, histogram_uint64, histogram_uint64_range, histogram_uint64_list}

        params: dictionary
            Dictionary of params for the engine. Each engine type has different params. Explore the EMU documentation `engine section <https://trex-tgn.cisco.com/trex/doc/trex_emu.html#_engines>`_
            for more information on engines.
    """


    def __init__(self, emu_client):
        """
        Initialize a TdlPlugin.

            :parameters:
                emu_client: :class:`trex.emu.trex_emu_client.EMUClient`
                    Valid EMU client.
        """
        super(TDLPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='tdl_c_cnt')

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
    @plugin_api('tdl_show_counters', 'emu')
    def tdl_show_counters_line(self, line):
        """Show Tdl counters.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_tdl",
                                        self.tdl_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True
