#!/usr/bin/python

from trex.console.plugins import *
from trex.stl.api import *
from trex.pybird.bird_cfg_creator import *
from trex.pybird.pybird_zmq_client import *

'''
Bird plugin
'''

class Bird_Plugin(ConsolePlugin):

    def __init__(self):
        super(Bird_Plugin, self).__init__()
        self.console = None

    def plugin_description(self):
        return 'Bird plugin for simple communication with PyBirdserver'

    def set_plugin_console(self, trex_console):
        self.console = trex_console

    def plugin_load(self):
        self.add_argument("-p", "--port", type = int,
                dest     = 'port',
                required = True,
                help     = 'port to use')
        self.add_argument("-m", "--mac", type = str,
                dest     = 'mac', 
                required = True,
                help     = 'mac address to use')
        # ipv4 args
        self.add_argument("--ipv4", type = str,
                dest     = 'ipv4', 
                help     = 'src ip to use')
        self.add_argument("--ipv4-subnet", type = int,
                dest     = 'ipv4_subnet', 
                help     = 'ipv4 subnet to use')
        # ipv6 args
        self.add_argument("--ipv6-enable", action = "store_true",
                dest     = 'ipv6_enabled', 
                default  = False,
                help     = 'ipv6 enable, default False')
        self.add_argument("--ipv6", type = str,
                dest     = 'ipv6',
                help     = 'ipv6 enable, default False')
        self.add_argument("--ipv6-subnet", type = int,
                dest     = 'ipv6_subnet', 
                help     = 'ipv6 subnet ip to use')
        # vlan args        
        self.add_argument("--vlans", type = int, nargs="*",
                dest     = 'vlans', 
                help     = 'vlans for bird node')
        self.add_argument("--tpids", type = int, nargs="*",
                dest     = 'tpids', 
                help     = 'tpids for bird node')
        self.add_argument("--mtu", type = int,
                dest     = 'mtu',
                help     = 'mtu of the bird node')

        # set_config params
        self.add_argument("-f", "--file", type = str,
                required = True,
                dest     = 'file_path',
                help     = 'file path where the config file is located')

        self.add_argument("-r", "--routes", type = str,
                dest     = 'routes_file',
                help     = 'file path where the routes are located')

        # add_route params
        self.add_argument("--first-ip", type = str,
                help     = 'first ip to start enumerating from')
        self.add_argument("--total-routes", type = int,
                help     = 'total routes to be added to bird config')
        self.add_argument("--next-hop", type = str,
                help     = 'next hop for each route, best practice with current bird interface ip')

        if self.console is None:
            raise TRexError("Trex console must be provided in order to load Bird plugin")

        self.pybird = PyBirdClient(ip=self.console.server)
        self.pybird.connect()
        self.pybird.acquire()

    def plugin_unload(self):
        if self.console is None:
            raise TRexError("Trex console must be provided in order to unload Bird plugin")

        self.pybird.release()
        self.pybird.disconnect()

    # Bird commands
    def do_add_node(self, port, mac, ipv4, ipv4_subnet, ipv6_enabled, ipv6, ipv6_subnet, vlans, tpids, mtu):
        ''' Simple adding bird node with arguments. '''

        self.trex_client.set_bird_node(node_port   = port,
                                mac           = mac,
                                ipv4          = ipv4,
                                ipv4_subnet   = ipv4_subnet,
                                ipv6_enabled  = ipv6_enabled,
                                ipv6          = ipv6,
                                ipv6_subnet   = ipv6_subnet,
                                vlans         = vlans,
                                tpids         = tpids,
                                mtu           = mtu)

    def do_set_empty_config(self):
        ''' Set empty bird config with no routing protocols. '''
        print(self.pybird.set_empty_config())

    def do_show_config(self):
        ''' Return the current bird configuration as it for now. '''
        print(self.pybird.get_config())

    def do_show_protocols(self):
        ''' Show the bird protocols in a user friendly way. '''
        print(self.pybird.get_protocols_info())

    def do_show_nodes(self, port):
        ''' Show all bird nodes on port '''
        res = self.trex_client.set_namespace(port, method='get_nodes', only_bird = True)
        list_of_macs = res['result']['nodes']
        res = self.trex_client.set_namespace(port, method='get_nodes_info', macs_list = list_of_macs)
        if len(res['result']['nodes']):
            self._print_nodes_info_as_table(res)
        else:
            print("No bird nodes on port %s" % port)

    def do_add_routes(self, first_ip, total_routes, next_hop):
        ''' Adding static routes to bird config. '''
        cfg_creator = self._get_bird_cfg_with_current_config()
        cfg_creator.add_many_routes(first_ip, total_routes, next_hop)
        res = self.pybird.set_config(cfg_creator.build_config())
        print("Bird configuration result: %s" % res)

    # Adding routing protocols
    def do_set_config(self, file_path, routes_file, first_ip, total_routes, next_hop):
        ''' Add wanted config to bird configuration. '''
        if routes_file and (first_ip or total_routes or next_hop):
            print("Cannot work with route file and generate routes args, choose only one of them")
            return

        if not os.path.isfile(file_path):
            print('The path: "%s" is not a valid file!' % file_path)
        with open(file_path, 'r') as f:
            new_config = f.read()

        if routes_file:
            if not os.path.isfile(routes_file):
                print('The path: "%s" is not a valid file!' % routes_file)
            with open(routes_file, 'r') as f:
                new_config += f.read()
        else:
            if first_ip or total_routes or next_hop:
                if not (first_ip and total_routes and next_hop):
                    print("Must specify all generate routes args")
                    return
                cfg_creator = BirdCFGCreator(new_config)
                cfg_creator.add_many_routes(first_ip, total_routes, next_hop)
                new_config = cfg_creator.build_config() 

        res = self.pybird.set_config(new_config)
        print("Bird configuration result: %s" % res)

    # helper functions
    def _get_bird_cfg_with_current_config(self):
        return BirdCFGCreator(self.pybird.get_config())

    def _print_nodes_info_as_table(self, res):
        headers = ["Node MAC", "ipv4 address", "ipv4 subnet", "ipv6 enabled", "ipv6 address", "ipv6 subnet", "vlans", "tpids"]
        table = text_tables.TRexTextTable('Bird nodes information')
        table.header(headers)

        table.set_cols_align(["c"] * len(headers))
        table.set_cols_width([17] * len(headers))
        table.set_cols_dtype(['t'] * len(headers))


        for node in res['result']['nodes']:
            table.add_row([node['ether']['src'],
            node['ipv4'].get('src', '-'),
            node['ipv4']['subnet'] if node['ipv4']['subnet'] != 0 else '-',
            "True" if node['ipv6']['enabled'] else "False",
            node['ipv6'].get('src', '-'),
            node['ipv6']['subnet'] if node['ipv6']['subnet'] != 0 else '-',
            node['vlan']['tags'] if node['vlan']['tags'] else '-',
            node['vlan']['tpids'] if node['vlan']['tpids'] else '-'
             ])

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)