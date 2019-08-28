#!/usr/bin/python
import pprint
import argparse
from trex.utils import parsing_opts
from trex.emu.trex_emu_client import EMUClient
from trex.console.plugins import *

'''
EMU plugin
'''

# helper function
def _conv_ns(port, vlans, tpids):
    return {'vport': port, 'tci': vlans, 'tpid': tpids}

def _conv_client(mac, ipv4, dg, ipv6):
    return {'mac': mac, 'ipv4': ipv4, 'ipv4_dg': dg, 'ipv6': ipv6}

class Emu_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'Emu plugin is used in order to communicate with emulation server, i.e loading emu profiles'

    def __init__(self):
        super(Emu_Plugin, self).__init__()
        self.c = self.trex_client

    def plugin_load(self):
        # Namespace's args
        self.add_argument('-p', '--port', type = int,
            help = 'Port identifying the namespace')
        self.add_argument('--vlans', type = int, nargs = '*',
            help = 'Vlan tag/s of the namespace (limit is 2)')
        self.add_argument('--tpids', type = str, nargs = '*',
            help = 'Vlan tpid/s of the namespace (limit is 2)')

        # Client's args
        self.add_argument('--mac', type = str,
            help = 'Mac address identifying the client')
        self.add_argument('--ipv4', type = str,
            help = 'IPv4 address identifying the client')
        self.add_argument('--dg', type = str,
            help = 'Default gateway (IPv4) of the client')
        self.add_argument('--ipv6', type = str,
            help = 'IPv6 address identifying the client')

        # Profile args
        self.add_argument('-f', '--file', type = str, required = True, dest = 'file_path',
                            help = 'Path to an emu valid profile')

        self.add_argument('-t', '--tunables', type = str, default = '', nargs = argparse.REMAINDER,
                            help = 'Sets tunables for a profile. Notice -t MUST be the last flag in the line. Example: "load_profile -f emu/simple_emu.py -t -h" to see tunables help')
        self.add_argument('--max-rate', type = int,
                            help = 'Max clients send rate (clients / sec)')

        # Show all args
        self.add_argument('--max-ns', type = int, default = 1, dest = 'max_ns_show',
                            help = 'Max namespaces to show in table')
        self.add_argument('--max-clients', type = int, default = 10, dest = 'max_c_show',
                            help = 'Max clients to show in table')
        self.add_argument('--json', action = 'store_true', dest = 'to_json',
                            help = "Print data in json format")
        
        # Show counters args
        self.add_argument('--tables', type = str, nargs = '+', default = None,
                            help = 'Names of specific tables to show, i.e: ctx, mbuf-pool, mbuf-1024...')
        self.add_argument('--headers', action = 'store_true', default = False, dest = 'show_headers',
                            help = 'Only show the counters headers names')
        self.add_argument('--clear', action = 'store_true', default = False,
                            help = 'Clear all current counters')
        self.add_argument('-v', '--verbose', action = 'store_true', default = False,
                            help = 'Show verbose view of all the counters')
        self.add_argument('--filter', type = str, default = None, nargs = '*', dest = 'cnt_filter',
                            help = 'Filters counters by their type. Example: "show_counters --filter info warning"')
        self.add_argument('--zero', action = 'store_true', default = False,
                            help = 'Show the zeros counters as well')
        


    # Profile commands
    def do_load_profile(self, file_path, tunables, max_rate):
        ''' Loads an emu profile from a given file into emu server '''
        self.c.load_profile(file_path, tunables, max_rate)

    def do_remove_profile(self):
        ''' Remove all namespaces and clients from emu server '''
        self.c.remove_profile()

    # # Namespace's commands
        # def do_add_ns(self, port, vlans, tpids):
        #     ''' Add a new namespace for emu server '''
        #     self.c.add_ns([_conv_ns(port, vlans, tpids)])

        # def do_remove_ns(self, port, vlans, tpids):
        #     ''' Remove a specific namespace from emu server '''
        #     self.c.remove_ns([_conv_ns(port, vlans, tpids)])

        # # Client's commands
        # def do_add_client(self, port, vlans, tpids, mac, ipv4, dg, ipv6):
        #     ''' Add a new client to a specific namespace for emu server '''
        #     self.c.add_clients(namespace = _conv_ns(port, vlans, tpids),
        #                         clients = [_conv_client(mac, ipv4, dg, ipv6)])

        # def do_remove_client(self, port, vlans, tpids, mac):
        #     ''' Remove client from a specific namespace from emu server '''
        #     self.c.remove_clients(namespace = _conv_ns(port, vlans, tpids),
        #                             macs = [mac])

        # # Default plugins commands
        # def do_get_def_ns_plugs(self):
        #     ''' Prints the default namespace plugins that are currently in emu server '''
        #     res = self.c.get_def_ns_plugs()
        #     if res:
        #         print("Default namespace plugins:")
        #         pprint.pprint(res)
        #     else:
        #         print("There are no default namespace plugins")

        # def set_def_ns_plugs(self, plugs):
        #     ''' Sets the default namespace plugins in emu server '''
        #     self.c.set_def_ns_plugs(plugs)

        # def do_get_def_client_plugs(self, port, vlans, tpids):
        #     ''' Prints the default client plugins for a specific namespace '''
        #     res = self.c.get_def_client_plugs(namespcae = _conv_ns(port, vlans, tpids))
        #     if res:
        #         print("Default client plugins for requested namespace:")
        #         pprint.pprint(res)
        #     else:
        #         print("There are no default client plugins for requested namespace")

        # def do_set_def_client_plugs(self, port, vlans, tpids, plugs):
        #     ''' Sets the default client plugins for a specific namespace '''
        #     self.c.set_def_client_plugs(namespace = _conv_ns(port, vlans, tpids),
        #                                 def_plugs = plugs)

    # Show commands
    def do_show_all(self, max_ns_show, max_c_show, to_json, port, vlans, tpids, mac, ipv4, dg, ipv6):
        ''' Show all the namespace and clients that are currently on server'''
        filters = {'namespace': _conv_ns(port, vlans, tpids), 'client': _conv_client(mac, ipv4, dg, ipv6)}
        self.c.print_all_ns_clients(max_ns_show, max_c_show, to_json, filters)

    def do_show_counters(self, tables, show_headers, clear, verbose, cnt_filter, zero, to_json):
        ''' 
            Show counters data from ctx according to given tables.
            INFO - no postfix, WARNING - '+', ERROR - '*'
        '''
        if show_headers:
            self.c.get_cnt_headers()
            return
        elif clear:
            self.c.clear_counters()
        else:
            if cnt_filter is not None:
                cnt_filter = [f.upper() for f in cnt_filter]
            self.c.print_counters(tables, cnt_filter, verbose, zero, to_json)