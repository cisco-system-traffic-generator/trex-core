
Plugin Init Json
==================

Every plugin in emu is created with a json format, allowing alter some of the plugin's parameters. Here will be listed all the aviable plugins and their init JSONs.
There is a seperation between init JSON of plugin namespace and plugin client. Pay attintion if your plugin will be created in the namespace or client contex.

**NOTICE**: Although sending a JSON with redundant parameters will not cause an error whatsoever, it's recommended to follow the init JSON structure.

Here are every plugin's init JSONs with an example of useage: 

ARP
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_arp
    :members: INIT_JSON_CLIENT, INIT_JSON_NS

IGMP
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_igmp
    :members: INIT_JSON_CLIENT, INIT_JSON_NS

ICMP
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_icmp
    :members: INIT_JSON_CLIENT, INIT_JSON_NS

DHCP
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_dhcp
    :members: INIT_JSON_CLIENT, INIT_JSON_NS

DHCPv6
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_dhcpv6
    :members: INIT_JSON_CLIENT, INIT_JSON_NS

IPv6
---------------
.. automodule:: trex.emu.emu_plugins.emu_plugin_ipv6
    :members: INIT_JSON_CLIENT, INIT_JSON_NS