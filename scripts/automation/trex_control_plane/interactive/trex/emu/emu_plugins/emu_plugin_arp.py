from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts


class ARPPlugin(EMUPluginBase):
    '''Defines arp plugin  RFC 826 '''

    plugin_name = 'ARP'

    # init jsons example for SDK
    INIT_JSON_NS = {'arp': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'arp': {'timer': 60, 'timer_disable': False}}
    """
    :parameters:
        timer: uint32
            Arp timer.
        timer_disable: bool
            Is timer disable.
    """

    ARP_STATES = {
        16: 'Learned',
        17: 'Incomplete',
        18: 'Complete',
        19: 'Refresh'
    }


    def __init__(self, emu_client):
        """
        Init ArpPlugin. 

            :parameters:
                emu_client: EMUClient
                    Valid emu client.
        """
        super(ARPPlugin, self).__init__(emu_client, ns_cnt_rpc_cmd='arp_ns_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_counters(self, ns_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_ns_counters(ns_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_counters(self, ns_key):
        return self._clear_ns_counters(ns_key)

    @client_api('getter', True)
    def get_cfg(self, ns_key):
        """
        Get arp configurations. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :returns:
               | dict :
               | {
               |    "enable": true
               | }
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_ns('arp_ns_get_cfg', ns_key)

    @client_api('command', True)
    def set_cfg(self, ns_key, enable):
        """
        Set arp configurations. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                enable: bool
                    True for enabling arp.
        
            :returns:
               bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'enable', 'arg': enable, 't': bool}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_ns('arp_ns_set_cfg', ns_key, enable = enable)

    @client_api('command', True)
    def cmd_query(self, c_key, garp):
        """
        Query command for arp. 
        
            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
                garp: bool
                    True for gratuitous arp.

            :returns:
               bool : True on success.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey},
        {'name': 'garp', 'arg': garp, 't': bool}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client('arp_c_cmd_query', c_key, garp = garp)

    @client_api('getter', True)
    def show_cache(self, ns_key):
        """
        get arp cache (per namespace) shared by all clients 

            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :returns:
               | list : List of cache records looks like:
               |    [{'mac': [68, 3, 0, 23, 0, 1], 'refc': 0, 'resolve': True, 'ipv4': [10, 111, 168, 31], 'state': 'Learned'},
               |    {'mac': [68, 3, 0, 23, 0, 2], 'refc': 0, 'resolve': True, 'ipv4': [10, 111, 168, 32], 'state': 'Learned'}]
               |
               | Notice - addresses are in bytes arrays.
        """  
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        params = ns_key.conv_to_dict(add_tunnel_key = True)
        res = self.emu_c._get_n_items(cmd = 'arp_ns_iter', **params)
        for r in res:
            if 'state' in r:
                r['state'] = ARPPlugin.ARP_STATES.get(r['state'], 'Unknown state')
        return res

    # Plugins methods
    @plugin_api('arp_show_counters', 'emu')
    def arp_show_counters_line(self, line):
        '''Show arp counters data from arp table.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_arp",
                                        self.arp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns = True)
        return True

    @plugin_api('arp_get_cfg', 'emu')
    def arp_get_cfg_line(self, line):
        '''Arp get configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_get_cfg",
                                        self.arp_get_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self.print_plug_cfg)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.get_cfg(ns_key)
            self.print_plug_cfg(res)
        return True

    @plugin_api('arp_set_cfg', 'emu')
    def arp_set_cfg_line(self, line):
        '''Arp set configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_set_cfg",
                                        self.arp_set_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.ARP_ENABLE
                                        )

        opts = parser.parse_args(line.split())
        opts.enable = parsing_opts.ON_OFF_DICT.get(opts.enable)
        
        if opts.all_ns:
            self.run_on_all_ns(self.set_cfg, enable = opts.enable)
        else:
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            self.set_cfg(ns_key, opts.enable)
        return True
    
    @plugin_api('arp_cmd_query', 'emu')
    def arp_cmd_query_line(self, line):
        '''Arp cmd query command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_cmd_query",
                                        self.arp_cmd_query_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.ARP_GARP
                                        )

        opts = parser.parse_args(line.split())
        opts.garp = parsing_opts.ON_OFF_DICT.get(opts.garp)

        self._validate_port(opts)
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        self.cmd_query(c_key, opts.garp)
        return True

    @plugin_api('arp_show_cache', 'emu')
    def arp_show_cache_line(self, line):
        '''Arp show cache command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_show_cache",
                                        self.arp_show_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        keys_to_headers = [{'key': 'mac',      'header': 'MAC'},
                            {'key': 'ipv4',    'header': 'IPv4'},
                            {'key': 'refc',    'header': 'Ref.Count'},
                            {'key': 'resolve', 'header': 'Resolve'},
                            {'key': 'state',   'header': 'State'},
                            ]
        args = {'title': 'Arp cache', 'empty_msg': 'No arp cache in namespace', 'keys_to_headers': keys_to_headers}
        if opts.all_ns:
            self.run_on_all_ns(self.show_cache, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.show_cache(ns_key)
            self.print_table_by_keys(data = res, **args)

        return True
    