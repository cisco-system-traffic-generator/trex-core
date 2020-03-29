from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_conversions import Ipv6
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts


class IPV6Plugin(EMUPluginBase):
    '''Defines ipv6 plugin'''

    plugin_name = 'IPV6'
    IPV6_STATES = {
        16: 'Learned',
        17: 'Incomplete',
        18: 'Complete',
        19: 'Refresh'
    }

    # init jsons example for SDK
    INIT_JSON_NS = {'ipv6': {'mtu': 1500, 'dmac': [1, 2, 3, 4, 5 ,6], 'vec': [[244, 0, 0, 0], [244, 0, 0, 1]], 'version': 1}}
    """
    :parameters:
        mtu: uint16
            Maximun transmission unit.
        dmac: [6]byte
            Designator mac.
        vec: list of [4]byte
            IPv4 vector representing multicast addresses.
        version: uint16 
            The init version, 1 or 2 (default)
    """

    INIT_JSON_CLIENT = {'ipv6': {'nd_timer': 29, 'nd_timer_disable': False}}
    """
    :parameters:
        nd_timer: uint32
            IPv6-nd timer.

        nd_timer_disable: bool
            Enable/Disable IPv6-nd timer.
    """

    def __init__(self, emu_client):
        super(IPV6Plugin, self).__init__(emu_client, 'ipv6_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, ns_key):
        """
        Get IPv6 configuration from namespace.
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :return:
                | dict: IPv6 configuration like:
                | {'dmac': [0, 0, 0, 112, 0, 1], 'version': 2, 'mtu': 1500}
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_get_cfg', ns_key)

    @client_api('command', True)
    def set_cfg(self, ns_key, mtu, dmac):
        """
        Set IPv6 configuration on namespcae. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                mtu: int
                    MTU for ipv6 plugin.
                dmac: list of bytes
                    Designator mac for ipv6 plugin.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'mtu', 'arg': mtu, 't': 'mtu'},
        {'name': 'dmac', 'arg': dmac, 't': 'mac'},]
        EMUValidator.verify(ver_args)
        dmac = Mac(dmac)
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_set_cfg', ns_key, mtu = mtu, dmac = dmac.V())
    
    @client_api('command', True)
    def add_mld(self, ns_key, ipv6_vec):
        """
        Add mld to ipv6 plugin.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_vec: list of lists of bytes
                    List of ipv6 addresses. Must be a valid ipv6 mld address.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv6_vec', 'arg': ipv6_vec, 't': 'ipv6_mc', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ipv6_vec = [Ipv6(ip, mc = True) for ip in ipv6_vec]
        ipv6_vec = [ipv6.V() for ipv6 in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_add', ns_key, vec = ipv6_vec)

    @client_api('command', True)
    def add_gen_mld(self, ns_key, ipv6_start, ipv6_count = 1):
        """
        Add mld to ipv6 plugin, generating sequence of addresses. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_start: lists of bytes
                    ipv6 addresses to start from. Must be a valid ipv6 mld addresses.
                ipv6_count: int
                    | ipv6 addresses to add
                    | i.e -> `ipv6_start` = [0, .., 0] and `ipv6_count` = 2 ->[[0, .., 0], [0, .., 1]].
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv6_start', 'arg': ipv6_start, 't': 'ipv6_mc'},
        {'name': 'ipv6_count', 'arg': ipv6_count, 't': int}]
        EMUValidator.verify(ver_args)
        ipv6_vec = self._create_ip_vec(ipv6_start, ipv6_count, 'ipv6', True)
        ipv6_vec = [ip.V() for ip in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_add', ns_key, vec = ipv6_vec)

    @client_api('command', True)
    def remove_mld(self, ns_key, ipv6_vec):
        """
        Remove mld from ipv6 plugin. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_vec: list of lists of bytes
                    List of ipv6 addresses. Must be a valid ipv6 mld address.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv6_vec', 'arg': ipv6_vec, 't': 'ipv6_mc', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ipv6_vec = [Ipv6(ip, mc = True) for ip in ipv6_vec]
        ipv6_vec = [ipv6.V() for ipv6 in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', ns_key, vec = ipv6_vec)

    @client_api('command', True)
    def remove_gen_mld(self, ns_key, ipv6_start, ipv6_count = 1):
        """
        Remove mld from ipv6 plugin. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_start: lists of bytes
                    ipv6 address to start from.
                ipv6_count: int
                    | ipv6 addresses to add
                    | i.e -> `ipv6_start` = [0, .., 0] and `ipv6_count` = 2 ->[[0, .., 0], [0, .., 1]].
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv6_start', 'arg': ipv6_start, 't': 'ipv6_mc'},
        {'name': 'ipv6_count', 'arg': ipv6_count, 't': int}]
        EMUValidator.verify(ver_args)
        ipv6_vec = self._create_ip_vec(ipv6_start, ipv6_count, 'ipv6', True)
        ipv6_vec = [ip.V() for ip in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', ns_key, vec = ipv6_vec)

    @client_api('command', True)
    def iter_mld(self, ns_key, ipv6_amount = None):
        """
        Iterates over current mld's in ipv6 plugin. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_amount: int
                    Amount of ipv6 addresses to fetch, defaults to None means all.
        
            :returns:
                | list: List of ipv6 addresses dict:
                | {'refc': 100, 'management': False, 'ipv6': [255, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]}
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv6_amount', 'arg': ipv6_amount, 't': int, 'must': False},]
        EMUValidator.verify(ver_args)
        params = ns_key.conv_to_dict(add_tunnel_key = True)
        return self.emu_c._get_n_items(cmd = 'ipv6_mld_ns_iter', amount = ipv6_amount, **params)

    @client_api('command', True)
    def remove_all_mld(self, ns_key):
        '''
        Remove all user created mld(s) from ipv6 plugin.
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        '''
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        mlds = self.iter_mld(ns_key)
        mlds = [m['ipv6'] for m in mlds if m['management']]
        if mlds:
            self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', ns_key, vec = mlds)

    @client_api('getter', True)
    def show_cache(self, ns_key):
        """
        Return ipv6 cache for a given namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                
            :returns:
                | list: list of ipv6 cache records
                | [{
                |    'ipv6': list of 16 bytes,
                |    'refc': int,
                |    'state': string,
                |    'resolve': bool,
                |    'mac': list of 6 bytes}
                | ].
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},]
        EMUValidator.verify(ver_args)
        params = ns_key.conv_to_dict(add_tunnel_key = True)
        res = self.emu_c._get_n_items(cmd = 'ipv6_nd_ns_iter', **params)
        for r in res:
            if 'state' in r:
                r['state'] = IPV6Plugin.IPV6_STATES.get(r['state'], 'Unknown state')
        return res

    # Plugins methods
    @plugin_api('ipv6_show_counters', 'emu')
    def ipv6_show_counters_line(self, line):
        '''Show IPV6 counters data from ipv6 table.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_show_counters",
                                        self.ipv6_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True

    # cfg
    @plugin_api('ipv6_get_cfg', 'emu')
    def ipv6_get_cfg_line(self, line):
        '''IPV6 get configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_get_cfg",
                                        self.ipv6_get_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        keys_to_headers = [{'key': 'dmac', 'header': 'Designator Mac'},
                            {'key': 'mtu', 'header': 'MTU'},
                            {'key': 'version', 'header': 'Version'},]
        args = {'title': 'Ipv6 configuration', 'empty_msg': 'No ipv6 configurations', 'keys_to_headers': keys_to_headers}

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.get_cfg(ns_key)
            self.print_table_by_keys(data = res, **args)
        return True

    @plugin_api('ipv6_set_cfg', 'emu')
    def ipv6_set_cfg_line(self, line):
        '''IPV6 set configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_set_cfg",
                                        self.ipv6_set_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.MTU,
                                        parsing_opts.MAC_ADDRESS
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.set_cfg, mtu = opts.mtu, dmac = opts.mac)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            self.set_cfg(ns_key, mtu = opts.mtu, dmac = opts.mac)
        return True

    # mld
    @plugin_api('ipv6_add_mld', 'emu')
    def ipv6_add_mld_line(self, line):
        '''IPV6 add mld command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_add_mld",
                                        self.ipv6_add_mld_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV6_START,
                                        parsing_opts.IPV6_COUNT
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.add_gen_mld, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.add_gen_mld(ns_key, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        return True
      
    @plugin_api('ipv6_remove_mld', 'emu')
    def ipv6_remove_mld_line(self, line):
        '''IPV6 remove mld command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_remove_mld",
                                        self.ipv6_remove_mld_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV6_START,
                                        parsing_opts.IPV6_COUNT
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.remove_gen_mld, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_gen_mld(ns_key, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        return True

    @plugin_api('ipv6_show_mld', 'emu')
    def ipv6_show_mld_line(self, line):
        '''IPV6 show mld command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_show_mld",
                                        self.ipv6_show_mld_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())
        keys_to_headers = [{'key': 'ipv6',        'header': 'IPv6'},
                            {'key': 'refc',       'header': 'Ref.Count'},
                            {'key': 'management', 'header': 'From RPC'}]
        args = {'title': 'Current mld:', 'empty_msg': 'There are no mld in namespace', 'keys_to_headers': keys_to_headers}
        if opts.all_ns:
            self.run_on_all_ns(self.iter_mld, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.iter_mld(ns_key)
            self.print_table_by_keys(data = res, **args)
           
        return True

    @plugin_api('ipv6_remove_all_mld', 'emu')
    def ipv6_remove_all_mld_line(self, line):
        '''IPV6 remove all mld command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_remove_all_mld",
                                        self.ipv6_remove_all_mld_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.remove_all_mld)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_all_mld(ns_key)
        return True

    # cache
    @plugin_api('ipv6_show_cache', 'emu')
    def ipv6_show_cache_line(self, line):
        '''IPV6 show cache command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_show_cache",
                                        self.ipv6_show_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        keys_to_headers = [{'key': 'mac',      'header': 'MAC'},
                            {'key': 'ipv6',    'header': 'IPv6'},
                            {'key': 'refc',    'header': 'Ref.Count'},
                            {'key': 'resolve', 'header': 'Resolve'},
                            {'key': 'state',   'header': 'State'},
                        ]
        args = {'title': 'Ipv6 cache', 'empty_msg': 'No ipv6 cache in namespace', 'keys_to_headers': keys_to_headers}
        if opts.all_ns:
            self.run_on_all_ns(self.show_cache, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.show_cache(ns_key)
            self.print_table_by_keys(data = res, **args)

        return True
    
    # Override functions
    @client_api('getter', True)
    def tear_down_ns(self, ns_key):
        '''
        This function will be called before removing this plugin from namespace
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        '''
        try:
            self.remove_all_mld(ns_key)
        except:
            pass
