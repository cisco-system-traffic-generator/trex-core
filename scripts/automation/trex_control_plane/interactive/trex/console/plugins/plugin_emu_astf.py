#!/usr/bin/python

from trex.console.plugins import *
from trex.common.trex_exceptions import TRexError
from trex.astf.topo import *
from trex.utils.text_opts import format_text

'''
This plugin is used in order to combine commands from EMU and ASTF such as sync topology.
'''

class EMU_ASTF_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'This plugin is used in order to combine commands from EMU and ASTF'

    def __init__(self):
        super(EMU_ASTF_Plugin, self).__init__()
        self.console = None
        self.emu_c   = None
        self.astf_c  = None

    def set_plugin_console(self, console):
        self.console = console

    def plugin_load(self):
        if self.console is None:
            raise TRexError('Trex console must be provided in order to load emu plugin')
        
        c = self.console.client
        c_mode = c.get_mode()
        if c_mode != "ASTF":
            raise TRexError('TRex client mode must be ASTF, current client mode: %s' % c_mode)
        
        if not hasattr(self.console, 'emu_client'):
            raise TRexError('Cannot find trex emu client in console, load emu plugin first')

        self.emu_c  = self.console.emu_client
        self.astf_c = c

    def do_sync_topo(self):
        '''
        Sync astf topology with current Emu server data.
        Requires resolved dgw mac address and ipv4 and ipv6 dgw resolved macs must not be different.
        '''
        emu_c, astf_c = self.emu_c, self.astf_c

        emu_topo = ASTFTopology()
        emu_ns_and_c = emu_c.get_all_ns_and_clients()
        empty_ip_val = ''

        for ns in emu_ns_and_c:
            # take only clients ns
            if ns['vport'] % 2:
                continue

            port = str(ns['vport'])
            vlans = filter(lambda x: x != 0, ns.get('tci', []))
            if len(vlans) > 1:
                raise TRexError('Cannot convert topo when namespace has more than 1 vlan')
            elif len(vlans) == 1:
                vlan = vlans[0]
            else:
                vlan = 0 

            for c_i, c in enumerate(ns.get('clients', []), start = 1):
                port_id  = '%s.%s' % (port, c_i)
                src_mac  = c['mac']  # mac is mandatory for client
                src_ipv4 = c.get('ipv4', empty_ip_val)
                src_ipv6 = c.get('ipv6', empty_ip_val)
                
                if src_ipv4 == '0.0.0.0' or src_ipv4 == empty_ip_val:
                    raise TRexError('Cannot sync: node with mac: "%s" has no ipv4 address' % src_mac)

                if c.get('dgw') and c['dgw'].get('resolve'):
                    ipv4_dst = c['dgw'].get('rmac')
                else:
                    ipv4_dst = None

                if c.get('ipv6_dgw') and c['ipv6_dgw'].get('resolve'):
                    ipv6_dst = c['ipv6_dgw'].get('rmac')
                else:
                    ipv6_dst = None

                if ipv4_dst is None and ipv6_dst is None:
                    raise TRexError('Cannot sync: node with mac: "%s" has no resolved default gateway mac address' % src_mac)

                if ipv4_dst is not None and ipv6_dst is not None and ipv4_dst != ipv6_dst:
                    raise TRexError('Cannot sync: node with mac: "%s" has different resolved default gateway mac address for ipv4 and ipv6' % src_mac)

                emu_topo.add_vif(port_id = port_id, src_mac = src_mac, src_ipv4 = src_ipv4, src_ipv6 = src_ipv6, vlan = vlan)
                emu_topo.add_gw(port_id = port_id, src_start = src_ipv4, src_end = src_ipv4, dst = ipv4_dst if ipv4_dst is not None else ipv6_dst)

        astf_c.topo_clear()
        astf_c.topo_load(emu_topo)
        astf_c.topo_resolve()
        
        astf_c.logger.info(format_text("emu topo synced\n", 'green', 'bold'))
