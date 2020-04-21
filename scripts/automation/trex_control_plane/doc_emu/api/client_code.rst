Client Module 
==================

The EMUClient provides access to the TRex emulation server.

User can interact with TRex emulation server using this client. The protocol is JSON-RPC2 over ZMQ transport.
for EMU to work, you must move the port to promiscuous and to get multicast  packet. You should be enable --software mode too 
   
Adding and removing clients example:

arp example:

.. literalinclude:: ../../interactive/trex/examples/emu/emu_arp.py

.. literalinclude:: ../../interactive/trex/examples/emu/emu_add_remove_clients.py

Wrapping up example:

.. literalinclude:: ../../interactive/trex/examples/emu/emu_building_profile.py

**NOTICE**: More emu_examples can be found at: `scripts/automation/trex_control_plane/interactive/trex/examples/emu`

In addition to the Python API, a console-based API interface is also available.

Console-like example ::

  $ > ./trex-console --emu 
  (trex)> emu_load_profile -f emu/simple_emu.py -t --ns 2 --clients 5
  ...
  (trex)> emu_show_all

This example will load the profile at: "emu/simple_emu.py" with tunnables of 2 namespace with 5 clients in each one. Emu profiles have their own argparse so if you aren't sure whats
the right tunnables, you may check them running: "emu_load_profile -f emu/simple_emu.py -t --help"

**IMPORTANT** -t must located at the end of the command.

You can explore more emu commands by entering "?" to the console and look under "Emulation Commands". Some of the commands have additional flags, you can check them
by entering "--help" at the end of the command.


EMUClient class
---------------

.. autoclass:: trex.emu.trex_emu_client.EMUClient
    :members: 
    :inherited-members:
    :member-order: bysource
