Plugins
=======

Plugins are extentions for each EmuClient. They are emulating famous protocols like: arp, igmp, dhcp.. 

Every plugin is created with a json format, allowing alter some of the plugin's parameters. You can find the init JSON for each plugin, whether it's "INIT_JSON_NS" or "INIT_JSON_CLIENT" .
There is a seperation between init JSON of plugin namespace and plugin client. Pay attintion if your plugin will be created in the namespace or client contex.

**NOTICE**: Although sending a JSON with redundant parameters will not cause an error whatsoever, it's recommended to follow the init JSON structure.

Invoking plugin methods working as follows: c.xxx.yyy(z1, z2,..)
    | xxx - plugin name
    | yyy - method name (see `Emulation Plugins` section to view all plugin methods)
    | z1, z2 ... - method params.

Every plugin also has `counters` you can look into. They are good for debugging and monitoring the clients.  

**NOTICE**
The emu client will load dynamically every plugin under: `scripts/automation/trex_control_plane/interactive/trex/emu/emu_plugins`
Calling a plugin that isn't exists there will cause an error. 

Here are every plugin's methods, their init JSONs and examples of usage: 

ARP
---
**Example:**

.. literalinclude:: ../../interactive/trex/examples/emu/emu_arp.py
.. automodule:: trex.emu.emu_plugins.emu_plugin_arp
    :members: 
    :inherited-members:
    :member-order: bysource

IGMP
----
**Example:**

.. literalinclude:: ../../interactive/trex/examples/emu/emu_igmp.py
.. automodule:: trex.emu.emu_plugins.emu_plugin_igmp
    :members:
    :inherited-members:
    :member-order: bysource

ICMP
----
.. automodule:: trex.emu.emu_plugins.emu_plugin_icmp
    :members:
    :inherited-members:
    :member-order: bysource

DHCP
----
.. automodule:: trex.emu.emu_plugins.emu_plugin_dhcp
    :members:
    :inherited-members:
    :member-order: bysource

DHCPv6
------
.. automodule:: trex.emu.emu_plugins.emu_plugin_dhcpv6
    :members:
    :inherited-members:
    :member-order: bysource

IPv6
----
**Example:**

.. literalinclude:: ../../interactive/trex/examples/emu/emu_ipv6.py

.. automodule:: trex.emu.emu_plugins.emu_plugin_ipv6
    :members:
    :inherited-members:
    :member-order: bysource

Dot1x
-----
.. automodule:: trex.emu.emu_plugins.emu_plugin_dot1x
    :members:
    :inherited-members:
    :member-order: bysource