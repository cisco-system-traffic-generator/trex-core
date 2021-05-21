from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_conversions import Ipv6
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts

def conv(g,s):
    vec=[]
    for i in range(len(g)):
        vec.append({"g":g[i],"s":s[i]})
    return vec    

class IPV6Plugin(EMUPluginBase):
    '''Defines ipv6 plugin  

        RFC 4443: Internet Control Message Protocol (ICMPv6) for the Internet Protocol Version 6 (IPv6)
        RFC 4861: Neighbor Discovery for IP Version 6 (IPv6)
        RFC 4862: IPv6 Stateless Address Autoconfiguration.

        not implemented:

        RFC4941: random local ipv6 using md5

    '''

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
            Designator mac. IMPORTANT !!
        vec: list of [16]byte
            IPv4 vector representing multicast addresses.
        version: uint16 
            The init version, 1 or 2 (default). It learns the version from the first Query.
    """

    INIT_JSON_CLIENT = {'ipv6': {'nd_timer': 29, 'nd_timer_disable': False}}
    """
    :parameters:
        nd_timer: uint32
            IPv6-nd timer.

        nd_timer_disable: bool
            Enable/Disable IPv6-nd timer.
    """

    MIN_ROWS = 5
    MIN_COLS = 120

    def __init__(self, emu_client):
        super(IPV6Plugin, self).__init__(emu_client, ns_cnt_rpc_cmd='ipv6_ns_cnt')

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
    
    def _mc_sg_gen(self, ns_key, g_vec,s_vec,cmd):
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'g_vec', 'arg': g_vec, 't': 'ipv6_mc', 'allow_list': True},
        {'name': 's_vec', 'arg': s_vec, 't': 'ipv6', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        # convert 
        g_vec1 = [Ipv6(ip, mc = True) for ip in g_vec]
        g_vec1 = [ipv6.V() for ipv6 in g_vec1]

        # convert 
        s_vec1 = [Ipv6(ip) for ip in s_vec]
        s_vec1 = [ipv6.V() for ipv6 in s_vec1]
        if len(s_vec1) != len(g_vec1):
            raise TRexError('Validation error, len of g and s vector should be the same ')

        return self.emu_c._send_plugin_cmd_to_ns(cmd, ns_key, vec = conv(g_vec1,s_vec1))

    @client_api('command', True)
    def remove_mld_sg(self, ns_key, g_vec,s_vec):
        """
        Remove (g,s) multicast addresses in namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_vec: list of lists of bytes
                    Groups IPv6 addresses (multicast)
                s_vec: list of lists of bytes
                    Sources of IPv6 addresses. one source for each group the size of the vectors should be the same

            .. code-block:: python

                    example 1

                    g_vec = [ip1.V(),ip1.V()]
                    s_vec = [ip2.V(),ip3.V()]

                    this will remove 
                                (g=ip1.V(),s=ip2.V()) 
                                (g=ip1.V(),s=ip3.V()) 


            :returns:
                bool : True on success.
        """
        return self._mc_sg_gen( ns_key, g_vec,s_vec,'ipv6_mld_ns_sg_remove')

    @client_api('command', True)
    def add_mld_sg(self, ns_key, g_vec,s_vec):
        """
        Add multicast (g,s) addresses in namespace.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_vec: list of lists of bytes
                    Groups IPv4 addresses.
                s_vec: list of lists of bytes
                    Sources of IPv4 addresses. one source for each group

            .. code-block:: python

                    example 1

                    g_vec = [ip1.V(),ip1.V()]
                    s_vec = [ip2.V(),ip3.V()]

                    this will add
                                (g=ip1.V(),s=ip2.V()) 
                                (g=ip1.V(),s=ip3.V()) 

                    the vectors should be in the same side and the there is no limit 
                    (it will be pushed in the fastest way to the server)

                         
            :returns:
                bool : True on success.
        """
        return self._mc_sg_gen( ns_key, g_vec,s_vec,'ipv6_mld_ns_sg_add')

    @client_api('command', True)
    def add_mld(self, ns_key, ipv6_vec):
        """
        Add mld to ipv6 plugin. For MLDv2 this is g,* meaning accept all the sources 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv6_vec: list of lists of bytes
                    List of ipv6 addresses. Must be a valid ipv6 mld address. .e.g.[[0xff,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1] ]

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

    def _add_remove_gen_mc_sg(self, ns_key, g_start, g_count = 1,s_start=None, s_count = 1,cmd = None):
        """ """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'g_start', 'arg': g_start, 't': 'ipv6_mc'},
        {'name': 'g_count', 'arg': g_count, 't': int},
        {'name': 's_start', 'arg': s_start, 't': 'ipv6'},
        {'name': 's_count', 'arg': s_count, 't': int},]
        EMUValidator.verify(ver_args)

        g_vec = self._create_ip_vec(g_start, g_count, 'ipv6', True)
        g_vec = [ip.V() for ip in g_vec]
        s_vec = self._create_ip_vec(s_start, s_count, 'ipv6', False)
        s_vec = [ip.V() for ip in s_vec]
        g_in=[]
        s_in=[]
        for i in range(len(g_vec)):
            for j in range(len(s_vec)):
                g_in.append(g_vec[i])
                s_in.append(s_vec[j])
        if cmd=="add":
           return self.add_mld_sg(ns_key, g_in,s_in)
        else:
           return self.remove_mld_sg(ns_key, g_in,s_in)

    @client_api('command', True)
    def add_gen_mc_sg(self, ns_key, g_start, g_count = 1,s_start=None, s_count = 1):
        """
        Add multicast addresses (g,s) in namespace, generating sequence of addresses.
          
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_start: lists of bytes
                    IPv6 address of the first multicast address.
                g_count: int
                    | Amount of ips to continue from `g_start`, defaults to 0. 
                s_start: lists of bytes
                    IPv6 address of the first source group 
                s_count: int
                    Amount of ips for sources in each group 
            
            .. code-block:: python
                
                    for example (using ipv4 address)
                        g_start = [1, 0, 0, 0] ,g_count = 2, s_start=[2, 0, 0, 0], s_count=1
                    
                    (g,s)
                    ([1, 0, 0, 0], [2, 0, 0, 0])
                    ([1, 0, 0, 1], [2, 0, 0, 0])

                
            :returns:
                bool : True on success.
        """
        return self._add_remove_gen_mc_sg(ns_key, g_start, g_count,s_start, s_count,"add")

    @client_api('command', True)
    def remove_gen_mc_sg(self, ns_key, g_start, g_count = 1,s_start=None, s_count = 1):
        """
        remove multicast addresses (g,s) in namespace, generating sequence of addresses.
          
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_start: lists of bytes
                    IPv6 address of the first multicast address.
                g_count: int
                    | Amount of ips to continue from `g_start`, defaults to 0. 
                s_start: lists of bytes
                    IPv6 address of the first source group 
                s_count: int
                    Amount of ips for sources in each group 

            .. code-block:: python
                
                for example (using ipv4 address)
                    g_start = [1, 0, 0, 0] , g_count = 2,s_start=[2, 0, 0, 0],s_count=1
                
                (g,s)
                ([1, 0, 0, 0], [2, 0, 0, 0])
                ([1, 0, 0, 1], [2, 0, 0, 0])
        

            :returns:
                bool : True on success.
        """
        return self._add_remove_gen_mc_sg(ns_key, g_start, g_count,s_start, s_count,"remove")

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
            return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', ns_key, vec = mlds)
        return True     

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

    #ping API
    @client_api('command', True)
    def start_ping(self, c_key, amount=None, pace=None, dst=None, src=None, timeout=None, payload_size=None):
        """
            Start pinging, sending Echo Requests.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

                amount: int
                    Amount of Echo Requests to send.

                pace: float
                    Pace in which to send the packets in pps (packets per second).

                dst: list of bytes
                    Destination IPv6. For example: [0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34]
                
                src: list of bytes
                    Source IPv6. User must own this IPv6 in order to be able to ping with it.

                timeout: int
                    Time to collect the results in seconds, starting when the last Echo Request is sent.

                payload_size: int
                    Size of the ICMP payload, in bytes.

            :returns:
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'amount', 'arg': amount, 't': int, 'must': False},
                    {'name': 'pace', 'arg': pace, 't': float,'must': False},
                    {'name': 'dst', 'arg': dst, 't': 'ipv6', 'must': False},
                    {'name': 'src', 'arg': src, 't': 'ipv6', 'must': False},
                    {'name': 'timeout', 'arg': timeout, 't': int, 'must': False},
                    {'name': 'payload_size', 'arg': payload_size, 't': int, 'must': False}]
        EMUValidator.verify(ver_args)
        try:
            success = self.emu_c._send_plugin_cmd_to_client(cmd='ipv6_start_ping', c_key=c_key, amount=amount, pace=pace,
                                                            dst=dst, src=src, timeout=timeout, payloadSize=payload_size)
            return success, None
        except TRexError as err:
            return None, err

    @client_api('command', True)
    def get_ping_stats(self, c_key, zero=True):
        """
            Get the stats of an active ping.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

                zero: boolean
                    Get values that equal zero aswell.

            :returns:
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'zero', 'arg': zero, 't': bool, 'must': False}]
        EMUValidator.verify(ver_args)
        try:
            data = self.emu_c._send_plugin_cmd_to_client(cmd='ipv6_get_ping_stats', c_key=c_key, zero=zero)
            return data, None
        except TRexError as err:
            return None, err

    @client_api('command', True)
    def stop_ping(self, c_key):
        """
            Stop an ongoing ping.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

            :returns:
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        try:
            success = self.emu_c._send_plugin_cmd_to_client('ipv6_stop_ping', c_key=c_key)
            return success, None
        except TRexError as err:
            return None, err

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
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns = True)
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

    @plugin_api('ipv6_add_mld_sg', 'emu')
    def ipv6_add_mld_sg_line(self, line):
        '''MLD add mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_add_mld_sg",
                                        self.ipv6_add_mld_sg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV6_G_START,
                                        parsing_opts.IPV6_G_COUNT,
                                        parsing_opts.IPV6_S_START,
                                        parsing_opts.IPV6_S_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            print(" not supported ! \n")
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.add_gen_mc_sg(ns_key, g_start = opts.g6_start, g_count = opts.g6_count,
                                     s_start = opts.s6_start, s_count = opts.s6_count)
        return True

    @plugin_api('ipv6_remove_mld_sg', 'emu')
    def ipv6_remove_mld_sg_line(self, line):
        '''MLD remove mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_remove_mld_sg",
                                        self.ipv6_add_mld_sg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV6_G_START,
                                        parsing_opts.IPV6_G_COUNT,
                                        parsing_opts.IPV6_S_START,
                                        parsing_opts.IPV6_S_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            print(" not supported ! \n")
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_gen_mc_sg(ns_key, g_start = opts.g6_start, g_count = opts.g6_count,
                                     s_start = opts.s6_start, s_count = opts.s6_count)
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
                            {'key': 'management', 'header': 'From RPC'},
                            {'key': 'mode', 'header': 'Mode'},
                            {'key': 'scnt', 'header': 'Sources'},
                            {'key': 'svu', 'header': 'S'},
                            ]

        args = {'title': 'Current mld:', 'empty_msg': 'There are no mld in namespace', 'keys_to_headers': keys_to_headers}
        if opts.all_ns:
            self.run_on_all_ns(self.iter_mld, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.iter_mld(ns_key)
            # convert 
            for i in range(len(res)):
                d = res[i]
                if 'sv' in d:
                    res[i]['scnt']=len(res)
                    s = ''
                    cnt = 0 
                    for ip in res[i]['sv']:
                        s+="["+Ipv6(ip).S()+"], "
                        cnt+=1
                        if cnt>4:
                            s+= " ..."
                    res[i]['svu']=s
                else:    
                   res[i]['scnt']=0
                   res[i]['svu']=''

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

    # ping
    @plugin_api('ipv6_ping', 'emu')
    def ipv6_ping(self, line):
        """ICMPv6 ping utility (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "ipv6_ping",
                                         self.ipv6_ping.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_ICMPv6_PING_PARAMS,
                                         )

        rows, cols = os.popen('stty size', 'r').read().split()
        if (int(rows) < self.MIN_ROWS) or (int(cols) < self.MIN_COLS):
            msg = "\nPing requires console screen size of at least {0}x{1}, current is {2}x{3}".format(self.MIN_COLS,
                                                                                                    self.MIN_ROWS,
                                                                                                    cols,
                                                                                                    rows)
            text_tables.print_colored_line(msg, 'red', buffer=sys.stdout)
            return

        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)

        self.emu_c.logger.pre_cmd('Starting to ping : ')
        success, err = self.start_ping(c_key=c_key, amount=opts.ping_amount, pace=opts.ping_pace, dst=opts.pingv6_dst,
                                       src=opts.pingv6_src, payload_size=opts.ping_size, timeout=1)
        if err is not None:
            self.emu_c.logger.post_cmd(False)
            text_tables.print_colored_line(err.msg, 'yellow', buffer=sys.stdout)
        else:
            self.emu_c.logger.post_cmd(True)
            amount = opts.ping_amount if opts.ping_amount is not None else 5  # default is 5 packets
            try:
                while True:
                    time.sleep(1) # Don't get statistics too often as RPC requests might overload the Emu Server.
                    stats, err = self.get_ping_stats(c_key=c_key)
                    if err is None:
                        stats = stats['icmp_ping_stats']
                        sent = stats['requestsSent']
                        rcv = stats['repliesInOrder']
                        percent = sent / float(amount) * 100.0
                        # the latency from the server is in usec
                        min_lat_msec = float(stats['minLatency']) / 1000
                        max_lat_msec = float(stats['maxLatency']) / 1000
                        avg_lat_msec = float(stats['avgLatency']) / 1000
                        err = int(stats['repliesOutOfOrder']) + int(stats['repliesMalformedPkt']) + \
                              int(stats['repliesBadLatency']) + int(stats['repliesBadIdentifier']) + \
                              int (stats['dstUnreachable'])

                        text = "Progress: {0:.2f}%, Sent: {1}/{2}, Rcv: {3}/{2}, Err: {4}/{2}, RTT min/avg/max = {5:.2f}/{6:.2f}/{7:.2f} ms" \
                                                            .format(percent, sent, amount, rcv, err, min_lat_msec, avg_lat_msec, max_lat_msec)

                        sys.stdout.write('\r' + (' ' * self.MIN_COLS) +'\r')  # clear line
                        sys.stdout.write(format_text(text, 'yellow'))
                        sys.stdout.flush()
                        if sent == amount:
                            sys.stdout.write(format_text('\n\nCompleted\n\n', 'yellow'))
                            sys.stdout.flush()
                            break
                    else:
                        # Trying to collect stats after completion.
                        break
            except KeyboardInterrupt:
                text_tables.print_colored_line('\nInterrupted by a keyboard signal (probably ctrl + c).', 'yellow', buffer=sys.stdout)
                self.emu_c.logger.pre_cmd('Attempting to stop ping : ')
                success, err = self.stop_ping(c_key=c_key)
                if err is None:
                    self.emu_c.logger.post_cmd(True)
                else:
                    self.emu_c.logger.post_cmd(False)
                    text_tables.print_colored_line(err.msg, 'yellow', buffer=sys.stdout)