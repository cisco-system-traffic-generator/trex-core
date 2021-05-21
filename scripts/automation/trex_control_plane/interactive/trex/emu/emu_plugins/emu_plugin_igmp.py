from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_conversions import Mac, Ipv4
from trex.emu.trex_emu_validator import EMUValidator

import trex.utils.parsing_opts as parsing_opts

def conv(g,s):
    vec=[]
    for i in range(len(g)):
        vec.append({"g":g[i],"s":s[i]})
    return vec    

def get_vec_mc(data):
    vec = [o["g"] for o in data]
    return vec


class IGMPPlugin(EMUPluginBase):
    '''Defines igmp plugin 

    Supports IPv4 IGMP v3/v2 RFC3376
      v3 supports the folowing filters 

      1. Exclude {}, meaning include all sources (*) 
      2. Include a vector of sources. The API is add/remove [(g,s1),(g,s2)..] meaning include to mc-group g a source s1 and s2 the mode would be INCLUDE {s1,s2}


    To change mode (include all [1] to include filter sources [2]) there is a need to remove and add the group again

     The implementation is in the namespace domain (shared for all the clients on the same network)
     One client ipv4/mac is the designator to answer the queries for all the clients.
     
     Scale
     
     1. unlimited number of groups
     2. ~1k sources per group (in case of INCLUDE)


     Don't forget to set the designator client

     The API does not support a rate policing so if you push a big vector it will be pushed in the fastest way to the DUT 
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
        super(IGMPPlugin, self).__init__(emu_client, ns_cnt_rpc_cmd='igmp_ns_cnt')

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

    def _mc_sg_gen(self, ns_key, g_vec,s_vec,cmd):
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'g_vec', 'arg': g_vec, 't': 'ipv4_mc', 'allow_list': True},
        {'name': 's_vec', 'arg': s_vec, 't': 'ipv4', 'allow_list': True},]
        EMUValidator.verify(ver_args)
        # convert 
        g_vec1 = [Ipv4(ip, mc = True) for ip in g_vec]
        g_vec1 = [ipv4.V() for ipv4 in g_vec1]

        # convert 
        s_vec1 = [Ipv4(ip) for ip in s_vec]
        s_vec1 = [ipv4.V() for ipv4 in s_vec1]
        if len(s_vec1) != len(g_vec1):
            raise TRexError('Validation error, len of g and s vector should be the same ')

        return self.emu_c._send_plugin_cmd_to_ns(cmd, ns_key, vec = conv(g_vec1,s_vec1))

    @client_api('command', True)
    def remove_mc_sg(self, ns_key, g_vec,s_vec):
        """
        Remove multicast addresses in namespace. 
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_vec: list of lists of bytes
                    Groups IPv4 addresses.
                s_vec: list of lists of bytes
                    Sources of IPv4 addresses. one source for each group

            .. code-block:: python

                    example 1

                    g_vec = [[239,1,1,1],[239,1,1,2]]
                    s_vec = [[10,0,0,1],[10,0,0,2]]

                    this will remove 
                                (g=[239,1,1,1],s=[10,0,0,1]) 
                                (g=[239,1,1,2],s=[10,0,0,2]) 

                    example 2

                    g_vec = [[239,1,1,1],[239,1,1,1]]
                    s_vec = [[10,0,0,1],[10,0,0,2]]

                    this will remove 
                                (g=[239,1,1,1],s=[10,0,0,1]) 
                                (g=[239,1,1,1],s=[10,0,0,2]) 


            :returns:
                bool : True on success.
        """
        return self._mc_sg_gen( ns_key, g_vec,s_vec,'igmp_ns_sg_remove')

    @client_api('command', True)
    def add_mc_sg(self, ns_key, g_vec,s_vec):
        """
        Add multicast(s,g) addresses in namespace.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_vec: list of lists of bytes
                    Groups IPv4 addresses.
                s_vec: list of lists of bytes
                    Sources of IPv4 addresses. one source for each group

            .. code-block:: python

                    example 1

                    g_vec = [[239,1,1,1],[239,1,1,2]]
                    s_vec = [[10,0,0,1],[10,0,0,2]]

                    this will add 
                                (g=[239,1,1,1],s=[10,0,0,1]) 
                                (g=[239,1,1,2],s=[10,0,0,2]) 

                    example 2

                    g_vec = [[239,1,1,1],[239,1,1,1]]
                    s_vec = [[10,0,0,1],[10,0,0,2]]

                    this will add 
                                (g=[239,1,1,1],s=[10,0,0,1]) 
                                (g=[239,1,1,1],s=[10,0,0,2]) 

                    the vectors should be in the same side and the there is no limit 
                    (it will be pushed in the fastest way to the server)


                         
            :returns:
                bool : True on success.
        """
        return self._mc_sg_gen( ns_key, g_vec,s_vec,'igmp_ns_sg_add')


    @client_api('command', True)
    def add_mc(self, ns_key, ipv4_vec):
        """
        Add multicast addresses in namespace.
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                ipv4_vec: list of lists of bytes
                    IPv4 addresses. for IGMPv3 this is g,* meaning accept all the sources 

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

    def _add_remove_gen_mc_sg(self, ns_key, g_start, g_count = 1,s_start=None, s_count = 1,cmd = None):
        """ """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
        {'name': 'g_start', 'arg': g_start, 't': 'ipv4_mc'},
        {'name': 'g_count', 'arg': g_count, 't': int},
        {'name': 's_start', 'arg': s_start, 't': 'ipv4'},
        {'name': 's_count', 'arg': s_count, 't': int},]
        EMUValidator.verify(ver_args)

        g_vec = self._create_ip_vec(g_start, g_count, 'ipv4', True)
        g_vec = [ip.V() for ip in g_vec]
        s_vec = self._create_ip_vec(s_start, s_count, 'ipv4', False)
        s_vec = [ip.V() for ip in s_vec]
        g_in=[]
        s_in=[]
        for i in range(len(g_vec)):
            for j in range(len(s_vec)):
                g_in.append(g_vec[i])
                s_in.append(s_vec[j])
        if cmd=="add":
           return self.add_mc_sg(ns_key, g_in,s_in)
        else:
           return self.remove_mc_sg(ns_key, g_in,s_in)


    @client_api('command', True)
    def add_gen_mc_sg(self, ns_key, g_start, g_count = 1,s_start=None, s_count = 1):
        """
        Add multicast addresses in namespace, generating sequence of addresses.
          
        
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_start: lists of bytes
                    IPv4 address of the first multicast address.
                g_count: int
                    | Amount of ips to continue from `g_start`, defaults to 0. 
                s_start: lists of bytes
                    IPv4 address of the first source group 
                s_count: int
                    Amount of ips for sources in each group 
            
            .. code-block:: python
                
                    for example 
                        g_start = [1, 0, 0, 0] , g_count = 2,s_start=[2, 0, 0, 0],s_count=1
                    
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
        remove multicast addresses in namespace, generating sequence of addresses.
          
            :parameters:
                ns_key: EMUNamespaceKey
                    see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                g_start: lists of bytes
                    IPv4 address of the first multicast address.
                g_count: int
                    | Amount of ips to continue from `g_start`, defaults to 0. 
                s_start: lists of bytes
                    IPv4 address of the first source group 
                s_count: int
                    Amount of ips for sources in each group 

            .. code-block:: python
                
                for example 
                    g_start = [1, 0, 0, 0] , g_count = 2,s_start=[2, 0, 0, 0],s_count=1
                
                (g,s)
                ([1, 0, 0, 0], [2, 0, 0, 0])
                ([1, 0, 0, 1], [2, 0, 0, 0])
        


            :returns:
                bool : True on success.
        """
        return self._add_remove_gen_mc_sg(ns_key, g_start, g_count,s_start, s_count,"remove")


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
            return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', ns_key, vec = get_vec_mc(mcs))
        else:
            return False

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
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns = True)
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


    @plugin_api('igmp_add_mc_sg', 'emu')
    def igmp_add_mc_sg_line(self, line):
        '''IGMP add mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_add_mc_sg",
                                        self.igmp_add_mc_sg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV4_G_START,
                                        parsing_opts.IPV4_G_COUNT,
                                        parsing_opts.IPV4_S_START,
                                        parsing_opts.IPV4_S_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            print(" not supported ! \n")
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.add_gen_mc_sg(ns_key, g_start = opts.g_start, g_count = opts.g_count,
                                     s_start = opts.s_start, s_count = opts.s_count)
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

    @plugin_api('igmp_remove_mc_sg', 'emu')
    def igmp_remove_mc_sg_line(self, line):
        '''IGMP remove mc command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "igmp_remove_mc_sg",
                                        self.igmp_remove_mc_sg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV4_G_START,
                                        parsing_opts.IPV4_G_COUNT,
                                        parsing_opts.IPV4_S_START,
                                        parsing_opts.IPV4_S_COUNT,
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            print(" not supported ! \n")
        else:
            self._validate_port(opts)
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.remove_gen_mc_sg(ns_key, g_start = opts.g_start, g_count = opts.g_count,
                                     s_start = opts.s_start, s_count = opts.s_count)
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
