Plugins
=======

Plugins are extensions for each EmuClient. They are emulating protocols like: ARP, IGMP, IPv6, DHCP, ICMP, DOT1X, IPFix.

Each plugin has an initialization JSON, allowing it to provide some startup plugin parameters. This init JSON can be applied to the client, namespace and thread level.
You can see examples of the init JSON for each plugin, whether it's "INIT_JSON_NS" (namespace) or "INIT_JSON_CLIENT" (client).

**NOTICE**: Although sending a JSON with redundant parameters will not cause an error whatsoever, it's recommended to follow the init JSON structure.

Invoking plugin methods works as follows: c.xxx.yyy (z1, z2, ..)
    | xxx - plugin name
    | yyy - method name (see the `Plugin Reference` section to view all plugin methods)
    | z1, z2 ... - method params.

Each plugin provides `counters` you can look into. Some of them at namespace level, and some others per client. They are good for debugging and monitoring.

**NOTICE**
The EMU client will load dynamically every plugin under: `scripts/automation/trex_control_plane/interactive/trex/emu/emu_plugins`
Calling a plugin that doesn't exist in the aforementioned directory will cause an error. 

We provide here a reference to all the plugins, their methods, init JSONs and some examples on how to use.

.. toctree::
    :maxdepth: 2

    plugins/arp
    plugins/cdp
    plugins/dhcp
    plugins/dhcpsrv
    plugins/dhcpv6
    plugins/dns
    plugins/dot1x
    plugins/icmp
    plugins/igmp
    plugins/ipfix
    plugins/ipv6
    plugins/lldp
    plugins/mdns
    plugins/tdl
    plugins/appsim

