
Namespace modules 
=======================

The API works when  `stack: linux_based` in trex_cfg 
Using this API the user can add a separate Linux network namespace that represents a client. 
This client is virtualy attached to a bridge that connected to TRex port. ARP/IPV6 ND packets move from clients to DUT and DUT to clients but not from clients to clients due to n^2 flood startup issue.  Using this method it is possible to leverage linux ipv4/ipv6 network stack for emulating l2 protocols.

*notes*: adding/removing namespace is rather slow (~100msec) 

The following snippet create one namespace with ipv4 and ipv6 ::

    c = STLClient(verbose_level = 'error')
    c.connect()

    my_ports=[0,1]
    c.reset(ports = my_ports)

    # move to service mode 
    c.set_service_mode (ports = my_ports, enabled = True)


    cmds=NSCmds()
    MAC="00:01:02:03:04:05"
    cmds.add_node(MAC)  # add namespace
    cmds.set_vlan(MAC,[123,123]) # add valn + QinQ tags
    cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2") # configure ipv4 and default gateway 
    cmds.set_ipv6(MAC,True) # enable ipv6 (auto mode, get src addrees from the router 


     # start the batch 
    c.set_namespace_start( 0, cmds)
    # wait for the results     
    res = c.wait_for_async_results(0);

    # print the results    
    print(res);

Another method to do that::

    c = STLClient(verbose_level = 'error')
    c.connect()

    my_ports=[0,1]
    c.reset(ports = my_ports)

    # move to service mode 
    c.set_service_mode (ports = my_ports, enabled = True)

    # remove all old name spaces from all the ports 
    c.namespace_remove_all()
    
    # utility function 
    MAC="00:01:02:03:04:05"

    # each function will block     
    c.set_namespace(0,method='add_node',mac=MAC)
    c.set_namespace(0,method='set_vlan',vlans=[123,123])
    c.set_namespace(0,method='set_ipv4', mac=MAC, ipv4="1.1.1.3", dg="1.1.1.2")
    c.set_namespace(0,method='set_ipv6', mac=MAC, enable= True)

Get Stats::

    c = STLClient(verbose_level = 'error')
    c.connect()
    my_ports=[0,1]
    c.reset(ports = my_ports)

    # get all active nodes on port 0
    r=c.set_namespace(0, method='get_nodes')
    c.set_namespace (0, method='get_nodes_info', macs_list=r)
    
    r=c.set_namespace(0, method='counters_get_meta')
    r=c.set_namespace(0, method='counters_get_values', zeros=True)

build a networks of x clients ::

        def get_mac (prefix,index):
            mac="{}:{:02x}:{:02x}".format(prefix,(index>>8)&0xff,(index&0xff))
            return (mac)
        
        def get_ipv4 (prefix,index):
            ipv4="{}.{:d}.{:d}".format(prefix,(index>>8)&0xff,(index&0xff))
            return(ipv4)

        # build a networks of  00:01:02:03:xx:xx and 1.1.x.x and IPV6 =auto with default gateway of 1.1.1.2 for all        
        def build_network (size):
            cmds=NSCmds()
            MAC_PREFIX="00:01:02:03"
            IPV4_PREFIX="1.1"
            IPV4_DG ='1.1.1.2'
            for i in range(size):
                mac = get_mac (MAC_PREFIX,i+257+1)
                ipv4  =get_ipv4 (IPV4_PREFIX,259+i)
                cmds.add_node(mac)
                cmds.set_ipv4(mac,ipv4,IPV4_DG)
                cmds.set_ipv6(mac,True)
            return (cmds)


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


