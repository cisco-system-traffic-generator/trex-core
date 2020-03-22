from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts

# init jsons example for SDK
INIT_JSON_NS = {'dhcpv6': {'mtu': 1500, 'dmac': [1, 2, 3, 4, 5 ,6], 'vec': [244, 0, 0, 0]}}
"""
:parameters:
    mtu: uint16
        Maximun transmission unit.

    dmac: [6]byte
        Designator mac.

    vec: list of [4]byte
        IPv4 vector representing multicast addresses.
"""

INIT_JSON_CLIENT = {'dhcpv6': {}}
"""
:parameters:
    Empty.
"""

class IGMPPlugin(EMUPluginBase):
    '''Defines igmp plugin'''

    plugin_name = 'IGMP'

    def __init__(self, emu_client):
        super(IGMPPlugin, self).__init__(emu_client, 'igmp_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, port, vlan, tpid):
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_get_cfg', port, vlan, tpid)

    @client_api('command', True)
    def set_cfg(self, port, vlan, tpid, mtu, dmac):
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_set_cfg', port, vlan, tpid, mtu = mtu, dmac = dmac)
  
    @client_api('command', True)
    def add_mc(self, port, vlan, tpid, ipv4_start, ipv4_count = 1):
        ipv4_vec = self.create_ip_vec(ipv4_start, ipv4_count)
        ipv4_vec = [conv_to_bytes(ip, 'ipv4') for ip in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_add', port, vlan, tpid, vec = ipv4_vec)

    @client_api('command', True)
    def remove_mc(self, port, vlan, tpid, ipv4_start, ipv4_count = 1):
        ipv4_vec = self.create_ip_vec(ipv4_start, ipv4_count)
        ipv4_vec = [conv_to_bytes(ip, 'ipv4') for ip in ipv4_vec]
        return self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', port, vlan, tpid, vec = ipv4_vec)

    @client_api('command', True)
    def iter_mc(self, port, vlan, tpid, ipv4_amount = None):
        params = conv_ns_for_tunnel(port, vlan, tpid)
        return self.emu_c._get_n_items(cmd = 'igmp_ns_iter', amount = ipv4_amount, **params)

    @client_api('command', True)
    def remove_all_mc(self, port, vlan, tpid):
        mcs = self.iter_mc(port, vlan, tpid)
        if mcs:
            self.emu_c._send_plugin_cmd_to_ns('igmp_ns_remove', port, vlan, tpid, vec = mcs)

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

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self.print_plug_cfg)
        else:
            res = self.get_cfg(opts.port, opts.vlan, opts.tpid)
            self.print_plug_cfg(res)
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

        if opts.mac is not None:
            opts.mac = conv_to_bytes(opts.mac, 'mac')

        if opts.all_ns:
            self.run_on_all_ns(self.set_cfg, mtu = opts.mtu, dmac = opts.mac)
        else:
            self.set_cfg(opts.port, opts.vlan, opts.tpid, mtu = opts.mtu, dmac = opts.mac)
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
            self.run_on_all_ns(self.add_mc, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        else:
            res = self.add_mc(opts.port, opts.vlan, opts.tpid, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
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
            self.run_on_all_ns(self.remove_mc, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
        else:
            res = self.remove_mc(opts.port, opts.vlan, opts.tpid, ipv4_start = opts.ipv4_start, ipv4_count = opts.ipv4_count)
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
            res = self.remove_all_mc(opts.port, opts.vlan, opts.tpid)
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
            res = self.iter_mc(opts.port, opts.vlan, opts.tpid)
            self.print_gen_data(data = res, **args)
           
        return True

    # Override functions
    def tear_down_ns(self, port, vlan, tpid):
        ''' This function will be called before removing this plugin from namespace'''
        try:
            self.remove_all_mc(port, vlan, tpid)
        except:
            pass