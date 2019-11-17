#!/usr/bin/python

from trex.console.plugins import *
from trex.stl.api import *
from trex.pybird.bird_cfg_creator import *
from trex.pybird.bird_zmq_client import *

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
        self.pybird = PyBirdClient()
        self.pybird.connect()
        self.pybird.acquire()

    def plugin_unload(self):
        try:
            self.pybird.release()()
            self.pybird.disconnect()
        except Exception as e:
            print('Error while unloading bird plugin: \n' + str(e))        
        

    def do_add_bird_node(self, port, mac, ipv4, ipv4_subnet, ipv6_enabled, ipv6_subnet, vlans, tpids):
        ''' Simple adding bird node with arguments. '''
        self.c.connect()
        self.c.acquire(force = True)
        self.c.set_bird_node(node_port   = port,
                                mac         = mac,
                                ipv4        = ipv4,
                                ipv4_subnet = ipv4_subnet,
                                ipv6_enabled = ipv6_enabled,
                                ipv6_subnet = ipv6_subnet,
                                vlans       = vlans,
                                tpids       = tpids)

    def do_add_rip(self):
        ''' Adding rip protocol to bird configuration file. '''
        curr_conf = self.pybird.get_config()
        cfg_creator = BirdCFGCreator(curr_conf)
        cfg_creator.add_simple_rip()
        self.pybird.set_config(cfg_creator.build_config())
    
    def do_add_bgp(self):
        ''' Adding bgp protocol to bird configuration file. '''
        curr_conf = self.pybird.get_config()
        cfg_creator = BirdCFGCreator(curr_conf)
        cfg_creator.add_simple_bgp()
        self.pybird.set_config(cfg_creator.build_config())

    def do_add_ospf(self):
        ''' Adding ospf protocol to bird configuration file. '''
        curr_conf = self.pybird.get_config()
        cfg_creator = BirdCFGCreator(curr_conf)
        cfg_creator.add_simple_ospf()
        self.pybird.set_config(cfg_creator.build_config())

