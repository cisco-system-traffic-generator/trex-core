Types
=====

In general, the whole Python API works with `lists of bytes`. For example, mac address of "00:00:00:0a:0b:01" would be used like that: [0, 0, 0, 10, 11, 1].
In order to make the transformation easier, we supplied EMUType's. 

Every type can accept string / list of bytes / type object in the constructor and able to convert it using V() or S() methods.

Also each type supports enumeration i.e: Mac('00:00:00:70:00:01')[2].S() -> '00:00:00:70:00:03'

Here is a simple example of useage:

.. literalinclude:: ../../interactive/trex/examples/emu/emu_types.py

Mac
---
.. autoclass:: trex.emu.trex_emu_conversions.Mac
    :members: 
    :inherited-members:
    :member-order: bysource

Ipv4
----
.. autoclass:: trex.emu.trex_emu_conversions.Ipv4
    :members: 
    :inherited-members:
    :member-order: bysource

IPv6
----
.. autoclass:: trex.emu.trex_emu_conversions.Ipv6
    :members: 
    :inherited-members:
    :member-order: bysource