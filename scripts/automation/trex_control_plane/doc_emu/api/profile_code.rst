
Traffic profile modules 
=======================

The TRex EMUProfile includes a number of namespaces. Each namespace is identified by: port, vlan tci(up to 2) and tpids.
Each namespace has a list of clients (empty on initialize). Each client is identified with a uniqe mac address in his own namespace. Clients also may have: ipv4, ipv4 default gateway, ipv6 addresses and more.

There are **Default Plugins** we can insert for every namespace / client. The default plugin of the profile will create each namespace with that plugin.
Same for default namespace plugin, each client in the specific namespace will be created with the default plugin.
In case we want to make a client with a particular plugin that's fine, the new client plugin will override the default one (same for namespace creation and profile default plugins).

**Notice**

There is a difference between a namespace plugin (located at ctx, each new ns will have that plugs by default) and a client namespace (located at ns, each new client in the ns will have that plug by default).

Example: (emu/simple_emu.py)::

    def __init__(self):
        self.def_c_plugs = {'arp': {'enable': True},
                            'icmp': {'param': 42}}
        self.def_ns_plugs = {'arp': {'enable': False}}

    def create_profile(self, ns_size, clients_size):
        ns_list = []

        # create different namespace each time
        vport, tci, tpid = 1, [1, 1], [0x8100, 0x8100]
        for i in range(1, ns_size + 1):
            ns = EMUNamespaceObj(vport  = vport,
                                tci     = tci,
                                tpid    = tpid,
                                def_client_plugs = self.def_c_plugs
                                )
            vport, tci, tpid = inc_ns(vport, tci, tpid)


            mac = '00:00:00:00:00:01'
            ipv4 = '1.1.1.1'
            ipv4_dg = '1.1.1.2'
            ipv6 = generate_ipv6(mac)
            # create a different client each time
            for _ in range(clients_size):
                client = EMUClientObj(mac     = mac,
                                      ipv4    = ipv4,
                                      ipv4_dg = ipv4_dg,
                                      ipv6    = ipv6
                                      )
                ns.add_clients(client)
                mac, ipv4, ipv4_dg, ipv6 = inc_client(mac, ipv4, ipv4_dg)

            ns_list.append(ns)

        return EMUProfile(ns = ns_list, def_ns_plugs = self.def_ns_plugs)


EMUProfile class
----------------

.. autoclass:: trex.emu.trex_emu_profile.EMUProfile
    :members: 
    :member-order: bysource

EMUNamespaceObj class
---------------------

.. autoclass:: trex.emu.trex_emu_profile.EMUNamespaceObj
    :members: 
    :member-order: bysource
    

EMUClientObj class
------------------

.. autoclass:: trex.emu.trex_emu_profile.EMUClientObj
    :members: 
    :member-order: bysource

