
Client Module 
==================

The EMUClient provides access to the TRex emulation server.

User can interact with TRex emulation server. The protocol is JSON-RPC2 over ZMQ transport.

In addition to the Python API, a console-based API interface is also available.

Python-like example::
    
   c.load_profile('path/to/profile')
   
Console-like example ::

  $ > ./trex-console --emu 
  (trex)> plugins emu load_profile -f emu/simple_emu.py -t --ns 2 --clients 5

This example will load the profile at: "emu/simple_emu.py" with tunnables of 2 namespace with 5 clients in each one.
You can explore more emu commands by entering "?" to the console and look under "Emulation Commands". 

Example - Typical Python API::

    c = EMUClient(username = "elad", server = '10.0.0.10', sync_port = 4510, verbose_level = "error")

    # connect to server
    c.connect()

    # start the emu profile
    c.load_profile('emu/simple_emu.py')

    # print tables of namespaces and clients
    c.print_all_ns_clients()

    # print all the ctx counters
    res = c.get_counters()
    print(res)

Plugins Methods
---------------
You may also use emu API in order to applay plugins methods. The general pattern will be:

`client.xxx.yyy()`

xxx - plugin name

yyy - plugin method

For example::

    c = EMUClient(username = "elad", server = '10.0.0.10', sync_port = 4510, verbose_level = "error")

    # connect to server
    c.connect()

    # start the emu profile
    c.load_profile('emu/simple_emu.py')

    # show arp cache
    res = c.arp.show_cache(port = 0)
    print(res)

**NOTICE**
The emu client will load dynamically every plugin under: `scripts/automation/trex_control_plane/interactive/trex/emu/emu_plugins`
Calling a plugin that isn't exists there will cause an error. 

EMUClient class
---------------

.. autoclass:: trex.emu.trex_emu_client.EMUClient
    :members: 
    :inherited-members:
    :member-order: bysource
