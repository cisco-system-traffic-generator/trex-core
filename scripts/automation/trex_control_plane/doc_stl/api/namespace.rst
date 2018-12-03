
Namespace modules 
=======================

The API works when   `stack: linux_based` in trex_cfg 
Using this API the user can add a separate Linux network namespace that represents a client. 
This client is virtualy attached to a bridge that connected to TRex port. ARP/IPV6 ND packets move from clients to DUT and DUT to clients but not from clients to clients due to n^2 flood startup issue.  Using this method we can leverage linux ipv4/ipv6 network stack for emulating l2 protocols.
notes: adding/removing namespace is rather slow and not linear in time (as you add more it will take more time to add a new one). up to 1000 it takes ~200msec to add one namepace 

The following snippet create namespace with ipv4 and ipv6  ::

    c = STLClient(verbose_level = 'error')
    c.connect()

    my_ports=[0,1]
    c.reset(ports = my_ports)

    # move to service mode 
    c.set_service_mode (ports = my_ports, enabled = True)

    cmds=NSCmds()
    MAC="00:01:02:03:04:05"
    cmds.add_node(MAC)  # add namespace
    #cmds.set_vlan(MAC,[123,123])
    cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2") # configure ipv4 and dg
    cmds.set_ipv6(MAC,True) # enable ipv6

    #cmds.clear_ipv4(MAC)
    #cmds.set_ipv6(MAC,False)
    #cmds.remove_node (MAC)
    # remove the ipv4 and ipv6 
    #cmds.remove_all();
    #cmds.get_nodes()
    #cmds.get_nodes_info ([MAC])
    

     # start the batch 
    c.set_namespace_start( 0, cmds)

    # wait for the results     
    res = c.wait_for_async_results(0);

    # print the results    
    print(res.data);


NSCmds class
------------

.. autoclass:: trex.common.trex_ns.NSCmds
    :members: 
    :inherited-members:
    :member-order: bysource


NSCmdResult class
-----------------

.. autoclass:: trex.common.trex_ns.NSCmdResult
    :members: 
    :inherited-members:
    :member-order: bysource


