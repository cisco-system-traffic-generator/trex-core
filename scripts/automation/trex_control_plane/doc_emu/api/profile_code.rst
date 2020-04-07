
Profile Modules 
===============

The TRex EMUProfile includes a number of namespaces. Each namespace is identified by: port, vlan tci(up to 2) and tpids (would be extended in the future).
Each namespace has a list of clients (empty on initialize). Each client is identified with a unique mac address (key) in his own namespace. Clients also may have: ipv4, ipv4 default gateway, ipv6 addresses and more.

There are **Default Plugins** we can insert for every namespace / client. The default plugin of the profile will create each namespace with that plugin.
Same for default namespace plugin, each client in the specific namespace will be created with the default plugin.
In case we want to make a client with a particular plugin that's fine, the new client plugin will override the default one (same for namespace creation and profile default plugins).

**Notice** There is a difference between a namespace plugin (located at ctx, each new ns will have that plugs by default) and a client namespace (located at ns, each new client in the ns will have that plug by default).

**Example: (emu/simple_emu.py)**

.. literalinclude:: ../../../../emu/simple_emu.py

Every emu profile has `argparse` so tunable can be passed from outside and use the profile as they like.

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

EMUNamespaceKey class
---------------------

.. autoclass:: trex.emu.trex_emu_profile.EMUNamespaceKey
    :members: 
    :member-order: bysource

EMUClientKey class
------------------

.. autoclass:: trex.emu.trex_emu_profile.EMUClientKey
    :members: 
    :member-order: bysource


