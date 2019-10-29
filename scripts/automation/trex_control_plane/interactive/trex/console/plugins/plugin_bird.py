#!/usr/bin/python

from trex.console.plugins import *
from trex.stl.api import *

'''
Bird plugin
'''

class Bird_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'Bird plugin for simple communication with PyBirdserver'

    def plugin_load(self):
        self.add_argument("-p", "--port", type = int,
                dest     = 'port',
                required = True,
                help     = 'port to use')
        self.add_argument("-m", "--mac", type = str,
                dest     = 'mac', 
                required = True,
                help     = 'mac address to use')
        self.add_argument("--ipv4", type = str,
                dest     = 'ipv4', 
                help     = 'src ip to use')
        self.add_argument("--ipv4-subnet", type = int,
                dest     = 'ipv4_subnet', 
                help     = 'ipv4 subnet to use')
        self.add_argument("--ipv6-enable", action = "store_true",
                dest     = 'ipv6_enabled', 
                default  = False,
                help     = 'ipv6 enable, default False')
        self.add_argument("--ipv6-subnet", type = int,
                dest     = 'ipv6_subnet', 
                default  = 127,
                help     = 'ipv6 subnet ip to use, default 127')
        self.add_argument("--vlans", type = list,
                dest     = 'vlans', 
                help     = 'vlans for bird node')
        self.add_argument("--tpids", type = list,
                dest     = 'tpids', 
                help     = 'tpids for bird node')

        self.c = STLClient()
        

    def do_add_bird_node(self, port, mac, ipv4, ipv4_subnet, ipv6_enabled, ipv6_subnet, vlans, tpids):
        '''
        Simple adding bird node with arguments.
        '''

        self.c.set_bird_node(node_port   = port,
                                mac         = mac,
                                ipv4        = ipv4,
                                ipv4_subnet = ipv4_subnet,
                                ipv6_enabled = ipv6_enabled,
                                ipv6_subnet = ipv6_subnet,
                                vlans       = vlans,
                                tpids       = tpids)
