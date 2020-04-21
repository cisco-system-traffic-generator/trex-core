from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_conversions import Mac, Ipv4
from trex.emu.trex_emu_validator import EMUValidator

import trex.utils.parsing_opts as parsing_opts


class IGMPPlugin(EMUPluginBase):
    '''Defines igmp plugin 

    Support a simplified version of IPv4 IGMP v3/v2  RFC3376
    v3 does not support EXCLUDE/INCLUDE per client
    The implementation is in the namespace domain (shared for all the clients on the same network)
    One client ipv4/mac is the designator to answer the queries for all the clients.
    This implementation can scale with the number of mc 
    don't forget to set the designator client
   '''

    plugin_name = 'IGMP'

    # init jsons example for SDK
    INIT_JSON_NS = {'igmp': {'mtu': 1500, 'dmac': [1, 2, 3, 4, 5 ,6], 'vec': [[244, 0, 0, 0], [244, 0, 0, 1]], 'version': 1}}
    """
    :parameters:
        mtu: uint16
            Maximun transmission unit. (Required)
        dmac: [6]byte
            Designator mac. IMPORTANT !! can be set by the API set_cfg too 
        vec: list of [4]byte
            IPv4 vector representing multicast addresses.
        version: uint16
            The init version of IGMP 2 or 3 (default). After the first query it learns the version from the Query.
    """

    INIT_JSON_CLIENT = {'igmp': {}}
    """
    :parameters:
        Empty.
    """

    def __init__(self, emu_client):
        """
        Init IGMPPlugin. 

            :parameters:
                emu_client: EMUClient
                    Valid emu client.
        """        
        super(IGMPPlugin, self).__init__(emu_client, 'igmp_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, ns_key):
        """
        Get igmp configurations. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
            :returns:
               | dict :
               | {
               |    "dmac": [0, 0, 0, 0, 0, 0],
               |    "version": 3,
               |    "mtu": 1500
               | }
        """          
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_get_cfg', ns_key)

    @client_api('command', True)
    def set_cfg(self, ns_key, mtu, dmac):
        """
        Set arp configurations in namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                mtu: bool
                    True for enabling arp.
                dmac: list of bytes
                    Designator mac.
        
            :returns:
               bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'mtu', 'arg': mtu, 't': 'mtu'},
        {'name': 'dmac', 'arg': dmac, 't': 'mac'},]
        EMUValidator.verify(ver_args)
        dmac = Mac(dmac)
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_set_cfg', ns_key, mtu = mtu, dmac = dmac.V())

    @client_api('command', True)
    def add_mc(self, ns_key, ipv4_vec):
        """
        Add multicast addresses in namespace.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_vec: list of lists of bytes
                    IPv4 addresses.

            :returns:
                bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv4_vec', 'arg': ipv4_vec, 't': 'ipv4_mc', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ipv4_vec = [Ipv4(ip, mc = True) for ip in ipv4_vec]
        ipv4_vec = [ipv4.V() for ipv4 in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_add', ns_key, vec = ipv4_vec)

    @client_api('command', True)
    def add_gen_mc(self, ns_key, ipv4_start, ipv4_count = 1):
        """
        Add multicast addresses in namespace, generating sequence of addresses.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_start: lists of bytes
                    IPv4 address of the first multicast address.
                ipv4_count: int
                    | Amount of ips to continue from `ipv4_start`, defaults to 0. 
                    | i.e: ipv4_start = [1, 0, 0, 0] , ipv4_count = 2 -> [[1, 0, 0, 0], [1, 0, 0, 1]]
        
            :returns:
                bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv4_start', 'arg': ipv4_start, 't': 'ipv4_mc'},
        {'name': 'ipv4_count', 'arg': ipv4_count, 't': int}]
        EMUValidator.verify(ver_args)
        ipv4_vec = self._create_ip_vec(ipv4_start, ipv4_count, 'ipv4', True)
        ipv4_vec = [ip.V() for ip in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_add', ns_key, vec = ipv4_vec)

    @client_api('command', True)
    def remove_mc(self, ns_key, ipv4_vec):
        """
        Remove multicast addresses in namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_vec: list of lists of bytes
                    IPv4 multicast addresses.

            :returns:
                bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv4_vec', 'arg': ipv4_vec, 't': 'ipv4_mc', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        ipv4_vec = [Ipv4(ip, mc = True) for ip in ipv4_vec]
        ipv4_vec = [ipv4.V() for ipv4 in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', ns_key, vec = ipv4_vec)

    @client_api('command', True)
    def remove_gen_mc(self, ns_key, ipv4_start, ipv4_count = 1):
        """
        Remove multicast addresses in namespace, generating sequence of addresses.        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_start: list of bytes
                    IPv4 address of the first multicast address.
                ipv4_count: int
                    | Amount of ips to continue from `ipv4_start`, defaults to 0. 
                    | i.e: ipv4_start = [1, 0, 0, 0] , ipv4_count = 2 -> [[1, 0, 0, 0], [1, 0, 0, 1]]
        
            :returns:
                bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv4_start', 'arg': ipv4_start, 't': 'ipv4_mc'},
        {'name': 'ipv4_count', 'arg': ipv4_count, 't': int}]
        EMUValidator.verify(ver_args)    
        ipv4_vec = self._create_ip_vec(ipv4_start, ipv4_count, 'ipv4', True)
        ipv4_vec = [ip.V() for ip in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', ns_key, vec = ipv4_vec)

    @client_api('command', True)
    def iter_mc(self, ns_key, ipv4_amount = None):
        """
        Iterate multicast addresses in namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_count: int
                    Amount of ips to get from emu server, defaults to None means all. 
        
            :returns:
                list : List of ips as list of bytes. i.e: [[224, 0, 0, 1], [224, 0, 0, 1]]
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'ipv4_amount', 'arg': ipv4_amount, 't': int, 'must': False},]
        EMUValidator.verify(ver_args)
        params = ns_key.conv_to_dict(True)
        return self.emu_c._get_n_items(cmd = 'igmp_ns_iter', amount = ipv4_amount, **params)

    @client_api('command', True)
    def remove_all_mc(self, ns_key):
        """
        Remove all multicast addresses in namespace.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        
            :return:
               bool : True on success.
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},]
        EMUValidator.verify(ver_args)    
        mcs = self.iter_mc(ns_key)
        if mcs:
            self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', ns_key, vec = mcs)

    # Plugins methods
    @plugin_api('igmp_show_counters', 'emu')
    def igmp_show_counters_line(self, line):
        '''Show IGMP counters data from igmp table.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_show_counters",
                                        self.igmp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True

    @plugin_api('igmp_get_cfg', 'emu')
    def igmp_get_cfg_line(self, line):
        '''IGMP get configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_get_cfg",
                                        self.igmp_get_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        keys_to_headers = [{'key': 'dmac',     'header': 'Designator MAC'},
                            {'key': 'version', 'header': 'Version'},
                            {'key': 'mtu',    'header': 'MTU'},
                            ]
        args = {'title': 'IGMP Configuration', 'empty_msg': 'No IGMP Configuration', 'keys_to_headers': keys_to_headers}

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.get_cfg(ns_key)
            self.print_table_by_keys(data = res, **args)
        return True

    @plugin_api('igmp_set_cfg', 'emu')
    def igmp_set_cfg_line(self, line):
        '''IGMP set configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_set_cfg",
                                        self.igmp_set_cfg_line.__doc__,
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

    @plugin_api('igmp_add_mc', 'emu')
    def igmp_add_mc_line(self, line):
        '''IGMP add mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_add_mc",
                                        self.igmp_add_mc_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV4_START,
                                        parsing_opts.IPV4_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.add_gen_mc, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.add_gen_mc(ns_key, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        return True

    @plugin_api('igmp_remove_mc', 'emu')
    def igmp_remove_mc_line(self, line):
        '''IGMP remove mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_remove_mc",
                                        self.igmp_remove_mc_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV4_START,
                                        parsing_opts.IPV4_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.remove_gen_mc, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_gen_mc(ns_key, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        return True

    @plugin_api('igmp_remove_all_mc', 'emu')
    def igmp_remove_all_mc_line(self, line):
        '''IGMP remove all mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_remove_all_mc",
                                        self.igmp_remove_all_mc_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.remove_all_mc)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_all_mc(ns_key)
        return True

    @plugin_api('igmp_show_mc', 'emu')
    def igmp_show_mc_line(self, line):
        '''IGMP show mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_show_mc",
                                        self.igmp_show_mc_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        args = {'title': 'Current mc:', 'empty_msg': 'There are no mc in namespace'}
        if opts.all_ns:
            self.run_on_all_ns(self.iter_mc, print_ns_info = True, func_on_res = self.print_gen_data, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.iter_mc(ns_key)
            self.print_gen_data(data = res, **args)
           
        return True

    # Override functions
    def tear_down_ns(self, ns_key):
        ''' 
        This function will be called before removing this plugin from namespace
            :parameters:
                ns_key: EMUNamespaceKey
                see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        '''
        try:
            self.remove_all_mc(ns_key)
        except:
            pass
