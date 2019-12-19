
Bird Node
=======================

:Learn more about Bird:
    - `Bird official documentation <https://bird.network.cz/?get_doc&f=bird.html&v=20>`_
    - `Bird integration with TRex <https://trex-tgn.cisco.com/trex/doc/trex_stateless.html#_bird_integration>`_

The API works when configuring `stack: linux_based` in trex_cfg and "--bird-server" flag is on when running TRex.
Using this API the user can add a veth to the bird namespace in TRex.

The following snippet create 2 bird nodes with ipv4 and ipv6 ::

    c = STLClient(verbose_level = 'error')
    c.connect()

    my_ports = [0, 1]
    c.acquire(ports = my_ports)

    # move to service mode
    c.set_service_mode(ports = my_ports, enabled = True)

    # create 2 veth's in bird namespace, each function will block
    c.set_bird_node(node_port      = 0,
                    mac            = "00:00:00:01:00:07",
                    ipv4           = "1.1.1.3",
                    ipv4_subnet    = 24,
                    ipv6_enabled   = True,
                    ipv6_subnet    = 124,
                    vlans          = [22],      # dot1q is a configuration of veth not namespace
                    tpids          = [0x8100])

    c.set_bird_node(node_port      = 1,
                    mac            = "00:00:00:01:00:08",
                    ipv4           = "1.1.2.3",
                    ipv4_subnet    = 24,
                    ipv6_enabled   = True,
                    ipv6_subnet    = 124)

More advanced way to do that::

    c = STLClient(verbose_level = 'error')
    c.connect()

    my_ports = [0, 1]
    c.acquire(ports = my_ports)

    # move to service mode
    c.set_service_mode(ports = my_ports, enabled = True)

    # remove all old name spaces from all the ports and set promiscuous mode
    c.namespace_remove_all()
    c.set_port_attr(promiscuous = True)

    cmds = NSCmds()
    mac = "00:00:00:01:00:07"
    ipv4 = "1.1.1.3"
    ipv4_subnet = 24
    ipv6_subnet = 24
    cmds.add_node(mac, is_bird = True)
    cmds.set_ipv4(mac, ipv4, subnet = ipv4_subnet, shared_ns = True)
    cmds.set_ipv6(mac, ipv6_enabled, subnet = ipv6_subnet, shared_ns = True)

    # start the batch
    c.set_namespace_start(node_port, cmds)

    # wait for the results
    res = c.wait_for_async_results(node_port)

    # print the results
    print(res)

*note*: calling "add_node" with is_bird = True and shared_ns = "my-ns" is not allowed, they are mutual exclusive arguments.

Get Stats::

    c = STLClient(verbose_level = 'error')
    c.connect()
    my_ports=[0,1]
    c.reset(ports = my_ports)

    # get all active nodes on port 0
    r = c.set_namespace(0, method = 'get_nodes')
    c.set_namespace (0, method='get_nodes_info', macs_list = r)

    r = c.set_namespace(0, method = 'counters_get_meta')
    r = c.set_namespace(0, method = 'counters_get_values', zeros = True)

build a x bird nodes ::

        def get_mac (prefix,index):
            mac="{}:{:02x}:{:02x}".format(prefix,(index>>8)&0xff,(index&0xff))
            return mac

        def get_ipv4 (prefix,index):
            ipv4="{}.{:d}.{:d}".format(prefix,(index>>8)&0xff,(index&0xff))
            return ipv4

        # build a networks of  00:01:02:03:xx:xx and 1.1.x.x and IPV6 =auto with subnet mask 24 for all
        def build_network (size):
            cmds = NSCmds()
            MAC_PREFIX = "00:01:02:03"
            IPV4_PREFIX = "1.1"
            ipv4_subnet = 24
            ipv6_subnet = 127
            for i in range(size):
                mac = get_mac(MAC_PREFIX, i + 257 + 1)
                ipv4 = get_ipv4(IPV4_PREFIX, 259 + i)
                cmds.add_node(mac, is_bird = True)
                cmds.set_ipv4(mac, ipv4, subnet = ipv4_subnet, shared_ns = True)
                cmds.set_ipv6(mac, ipv6_enabled, subnet = ipv6_subnet, shared_ns = True)

            return cmds


From NSCmds class
-----------------

.. autoclass:: trex.common.trex_client.TRexClient
    :members: set_bird_node
