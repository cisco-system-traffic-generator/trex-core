from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts

class IPV6Plugin(EMUPluginBase):
    '''Defines ipv6 plugin'''

    plugin_name = 'IPV6'

    def __init__(self, emu_client):
        super(IPV6Plugin, self).__init__(emu_client, 'ipv6_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, port, vlan, tpid):
        return self.emu_c.send_plugin_cmd_to_ns('ipv6_mld_ns_get_cfg', port, vlan, tpid)

    @client_api('command', True)
    def set_cfg(self, port, vlan, tpid, mtu, dmac):
        return self.emu_c.send_plugin_cmd_to_ns('ipv6_mld_ns_set_cfg', port, vlan, tpid, mtu = mtu, dmac = dmac)
    
    @client_api('command', True)
    def add_mld(self, port, vlan, tpid, ipv6_vec):
        return self.emu_c.send_plugin_cmd_to_ns('ipv6_mld_ns_add', port, vlan, tpid, vec = ipv6_vec)

    @client_api('command', True)
    def remove_mld(self, port, vlan, tpid, ipv6_vec):
        return self.emu_c.send_plugin_cmd_to_ns('ipv6_mld_ns_remove', port, vlan, tpid, vec = ipv6_vec)

    @client_api('command', True)
    def iter_mld(self, port, vlan, tpid, ipv6_amount = None):
        params = conv_ns_for_tunnel(port, vlan, tpid)
        return self.emu_c.get_n_items(cmd = 'ipv6_mld_ns_iter', amount = ipv6_amount, **params)

    @client_api('getter', True)
    def show_cache(self, port, vlan, tpid):
        params = conv_ns_for_tunnel(port, vlan, tpid)
        return self.emu_c.get_n_items(cmd = 'ipv6_nd_ns_iter', **params)


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
                                        parsing_opts.IPV6_VEC
                                        )

        opts = parser.parse_args(line.split())

        if opts.ipv6 is not None:
            opts.ipv6 = [conv_to_bytes(ipv6, 'ipv6') for ipv6 in opts.ipv6]

        if opts.all_ns:
            self.run_on_all_ns(self.add_mld, ipv6_vec = opts.ipv6)
        else:
            res = self.add_mld(opts.port, opts.vlan, opts.tpid, ipv6_vec = opts.ipv6)
        return True
      
    @plugin_api('ipv6_remove_mld', 'emu')
    def ipv6_remove_mld_line(self, line):
        '''IPV6 remove mld command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_remove_mld",
                                        self.ipv6_remove_mld_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.IPV6_VEC
                                        )

        opts = parser.parse_args(line.split())

        if opts.ipv6 is not None:
            opts.ipv6 = [conv_to_bytes(ipv6, 'ipv6') for ipv6 in opts.ipv6]

        if opts.all_ns:
            self.run_on_all_ns(self.remove_mld, ipv6_vec = opts.ipv6)
        else:
            res = self.remove_mld(opts.port, opts.vlan, opts.tpid, ipv6_vec = opts.ipv6)
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
        args = {'title': 'Current mld:', 'empty_msg': 'There are no mld in namespace'}
        if opts.all_ns:
            self.run_on_all_ns(self.iter_mld, print_ns_info = True, func_on_res = self.print_gen_data, func_on_res_args = args)
        else:
            res = self.iter_mld(opts.port, opts.vlan, opts.tpid)
            self.print_gen_data(data = res, **args)
           
        return True

    # cache
    @plugin_api('ipv6_show_cache', 'emu')
    def ipv6_show_cache(self, line):
        '''IPV6 show cache command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipv6_show_cache",
                                        self.ipv6_show_cache.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        args = {'title': 'Ipv6 cache', 'empty_msg': 'No ipv6 cache in namespace'}
        if opts.all_ns:
            self.run_on_all_ns(self.show_cache, print_ns_info = True, func_on_res = self.print_gen_data, func_on_res_args = args)
        else:
            res = self.show_cache(opts.port, opts.vlan, opts.tpid)
            self.print_gen_data(data = res, **args)

        return True
    