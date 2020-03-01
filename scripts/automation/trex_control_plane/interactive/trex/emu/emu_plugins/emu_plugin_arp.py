from trex.emu.api import *
from emu_plugin_base import *
import trex.utils.parsing_opts as parsing_opts
import json

class ARPPlugin(EMUPluginBase):
    '''Defines arp plugin'''

    plugin_name = 'ARP'

    def __init__(self, emu_client):
        super(ARPPlugin, self).__init__(emu_client, 'arp_ns_cnt')

    # API methods
    @client_api('getter', True)
    def get_cfg(self, port, vlan, tpid):
        return self.emu_c.send_plugin_cmd_to_ns('arp_ns_get_cfg', port, vlan, tpid)

    @client_api('command', True)
    def set_cfg(self, port, vlan, tpid, enable):
        return self.emu_c.send_plugin_cmd_to_ns('arp_ns_set_cfg', port, vlan, tpid, enable = enable)

    @client_api('command', True)
    def cmd_query(self, port, vlan, tpid, mac, garp):
        params = conv_ns_for_tunnel(port, vlan, tpid)
        mac = conv_to_bytes(val = mac, val_type = 'mac')
        params.update({'mac': mac, 'garp': garp})
        return self.emu_c._transmit_and_get_data('arp_c_cmd_query', params)

    # Plugins methods
    @plugin_api('arp_show_counters', 'emu')
    def arp_show_counters_line(self, line):
        '''Show arp counters data from arp table, add --no-zero flag to hide zero values.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_arp",
                                        self.arp_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True

    @plugin_api('arp_get_cfg', 'emu')
    def arp_get_cfg_line(self, line):
        '''Arp get configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_get_cfg",
                                        self.arp_get_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_ALL_NS
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.get_cfg, print_ns_info = True, func_on_res = self._print_plug_cfg)
        else:
            res = self.get_cfg(opts.port, opts.vlan, opts.tpid)
            self._print_plug_cfg(res)
        return True

    @plugin_api('arp_set_cfg', 'emu')
    def arp_set_cfg_line(self, line):
        '''Arp set configuration command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_set_cfg",
                                        self.arp_set_cfg_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.CFG_ENABLE
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.set_cfg, enable = opts.enable)
        else:
            self.set_cfg(opts.port, opts.vlan, opts.tpid, opts.enable)
        return True
    
    @plugin_api('arp_cmd_query', 'emu')
    def arp_cmd_query_line(self, line):
        '''Arp cmd query command\n'''
        parser = parsing_opts.gen_parser(self,
                                        "arp_cmd_query",
                                        self.arp_cmd_query_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.MAC_REQ,
                                        parsing_opts.ARP_GARP
                                        )

        opts = parser.parse_args(line.split())

        if opts.all_ns:
            self.run_on_all_ns(self.cmd_query, mac = opts.mac, garp = opts.garp)
        else:
            self.cmd_query(opts.port, opts.vlan, opts.tpid, opts.mac, opts.garp)
        return True