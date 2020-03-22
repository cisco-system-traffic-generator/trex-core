from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts

# init jsons example for SDK
INIT_JSON_NS = {'ipv6': {'mtu': 1500, 'dmac': [1, 2, 3, 4, 5 ,6], 'vec': [244, 0, 0, 0]}}
"""
:parameters:
    mtu: uint16
        Maximun transmission unit.

    dmac: [6]byte
        Designator mac.

    vec: list of [4]byte
        IPv4 vector representing multicast addresses.
"""

INIT_JSON_CLIENT = {'ipv6': {'nd_timer': 29, 'nd_timer_disable': False}}
"""
:parameters:
    nd_timer: uint32
        IPv6-nd timer.

    nd_timer_disable: bool
        Enable/Disable IPv6-nd timer.
"""

class IPV6Plugin(EMUPluginBase):
    '''Defines ipv6 plugin'''

    plugin_name = 'IPV6'
    IPV6_STATES = {16: 'Learned', 17: 'Incomplete', 18: 'Complete', 19: 'Refresh'}

    def __init__(self, emu_client):
        super(IPV6Plugin, self).__init__(emu_client, 'ipv6_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, port, vlan, tpid):
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_get_cfg', port, vlan, tpid)

    @client_api('command', True)
    def set_cfg(self, port, vlan, tpid, mtu, dmac):
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_set_cfg', port, vlan, tpid, mtu = mtu, dmac = dmac)
    
    @client_api('command', True)
    def add_mld(self, port, vlan, tpid, ipv6_start, ipv6_count):
        ipv6_vec = self.create_ip_vec(ipv6_start, ipv6_count, 'ipv6')
        ipv6_vec = [conv_to_bytes(ip, 'ipv6') for ip in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_add', port, vlan, tpid, vec = ipv6_vec)

    @client_api('command', True)
    def remove_mld(self, port, vlan, tpid, ipv6_start, ipv6_count):
        ipv6_vec = self.create_ip_vec(ipv6_start, ipv6_count, 'ipv6')
        ipv6_vec = [conv_to_bytes(ip, 'ipv6') for ip in ipv6_vec]
        return self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', port, vlan, tpid, vec = ipv6_vec)

    @client_api('command', True)
    def iter_mld(self, port, vlan, tpid, ipv6_amount = None):
        params = conv_ns_for_tunnel(port, vlan, tpid)
        return self.emu_c._get_n_items(cmd = 'ipv6_mld_ns_iter', amount = ipv6_amount, **params)

    @client_api('command', True)
    def remove_all_mld(self, port, vlan, tpid):
        mlds = self.iter_mld(port, vlan, tpid)
        mlds = [m['ipv6'] for m in mlds if m['management']]
        if mlds:
            self.emu_c._send_plugin_cmd_to_ns('ipv6_mld_ns_remove', port, vlan, tpid, vec = mlds)

    @client_api('getter', True)
    def show_cache(self, port, vlan, tpid):
        params = conv_ns_for_tunnel(port, vlan, tpid)
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

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self.print_plug_cfg)
        else:
            res = self.get_cfg(opts.port, opts.vlan, opts.tpid)
            self.print_plug_cfg(res)
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

        if opts.mac is not None:
            opts.mac = conv_to_bytes(opts.mac, 'mac')

        if opts.all_ns:
            self.run_on_all_ns(self.set_cfg, mtu = opts.mtu, dmac = opts.mac)
        else:
            self.set_cfg(opts.port, opts.vlan, opts.tpid, mtu = opts.mtu, dmac = opts.mac)
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
            self.run_on_all_ns(self.add_mld, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        else:
            res = self.add_mld(opts.port, opts.vlan, opts.tpid, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
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
            self.run_on_all_ns(self.remove_mld, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
        else:
            res = self.remove_mld(opts.port, opts.vlan, opts.tpid, ipv6_start = opts.ipv6_start, ipv6_count = opts.ipv6_count)
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
            res = self.iter_mld(opts.port, opts.vlan, opts.tpid)
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
            res = self.remove_all_mld(opts.port, opts.vlan, opts.tpid)
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
            res = self.show_cache(opts.port, opts.vlan, opts.tpid)
            self.print_table_by_keys(data = res, **args)

        return True
    
    # Override functions
    @client_api('getter', True)
    def tear_down_ns(self, port, vlan, tpid):
        ''' This function will be called before removing this plugin from namespace'''
        try:
            self.remove_all_mld(port, vlan, tpid)
        except:
            pass
