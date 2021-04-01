from trex.emu.api import *
import argparse
import get_args 


class Prof1():
    def __init__(self):
        self.mac = Mac('00:00:00:70:00:01')
        self.def_ns_plugs  = {'igmp' : {'dmac':self.mac.V()}} 
        self.def_c_plugs  = None

    def create_profile(self, ns_size, clients_size,clientMac,dhcpOptions):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 0, [0, 0], [0x00, 0x00]
        for j in range(vport, ns_size + vport):
            ns_key = EMUNamespaceKey(vport  = j,
                                    tci     = tci,
                                    tpid    = tpid)
            ns = EMUNamespaceObj(ns_key = ns_key, def_c_plugs = self.def_c_plugs)

            mac = Mac(clientMac)
            ipv4 = Ipv4('0.0.0.0')
            dg = Ipv4('0.0.0.0')
            # create a different client each time
            for i in range(clients_size):       
                client = EMUClientObj(mac     = mac[i].V(),
                                      ipv4    = ipv4.V(),
                                      ipv4_dg = dg.V(),
                                      plugs   = {'arp': {},
                                                 'icmp': {},
                                                 'igmp': {}, 
                                                 'dhcp': dhcpOptions,
                                              },
                                      )
                ns.add_clients(client)
            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)

    def getDhcpPluginOptions(self,discoverDhcpClassIdOption, requestDhcpClassIdOption):
        o = {}
        if discoverDhcpClassIdOption:
          o['discoverDhcpClassIdOption'] = discoverDhcpClassIdOption
        if requestDhcpClassIdOption:
          o['requestDhcpClassIdOption'] = requestDhcpClassIdOption   
        if len(o)>0:
            return {'options':o}

        return o


     
    def get_profile(self, tuneables):
      # Argparse for tunables
        parser = argparse.ArgumentParser(description='Argparser for simple emu profile.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--ns', type = int, default = 1,
                    help='Number of namespaces to create')
        parser.add_argument('--clients', type = int, default = 1,
                    help='Number of clients to create in each namespace')

        parser.add_argument('--mac', type = str, default = '00:00:00:70:00:01',
                    help='Mac address of the first client')
        parser.add_argument('--discoverDhcpClassIdOption',  type = str,  
                   help='discover Dhcp ClassId Option')
        parser.add_argument('--requestDhcpClassIdOption', type = str,  
                   help='request Dhcp ClassId Option')


        args = parser.parse_args(tuneables)
        dhcpOptions=self.getDhcpPluginOptions(args.discoverDhcpClassIdOption,args.requestDhcpClassIdOption)
        return self.create_profile(args.ns, args.clients,args.mac,dhcpOptions)


def register():
    return Prof1()
