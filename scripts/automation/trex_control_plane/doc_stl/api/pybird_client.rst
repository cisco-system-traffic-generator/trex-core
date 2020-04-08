
PyBirdClient
=======================

:Learn more about Bird:
    - `Bird official documentation <https://bird.network.cz/?get_doc&f=bird.html&v=20>`_
    - `Bird integration with TRex <https://trex-tgn.cisco.com/trex/doc/trex_stateless.html#_bird_integration>`_

The API was created in order to communicate with PyBirdBirdServer located at the TRex machine.
Using this API the user can do various of bird commands on the Bird process remotely i.e: send and get current bird configuration.

The following snippet using BirdCFGCreator and push new bird config to server ::

    pybird_c = PyBirdClient(ip='localhost', port=4509)
    pybird_c.connect()
    pybird_c.acquire()

    bird_cfg = BirdCFGCreator()
    bgp_data1 = """
        local 1.1.1.3 as 65000;
        neighbor 1.1.1.1 as 65000;
        ipv4 {
                import all;
                export all;
        };"""

    bgp_data2 = """
        local 1.1.2.3 as 65000;
        neighbor 1.1.2.1 as 65000;
        ipv4 {
                import all;
                export all;
        };"""

    bird_cfg.add_protocol(protocol = "bgp", name = "my_bgp1", data = bgp_data1)
    bird_cfg.add_protocol(protocol = "bgp", name = "my_bgp2", data = bgp_data2)
    bird_cfg.add_route(dst_cidr = "42.42.42.42/32", next_hop = "1.1.1.3")
    bird_cfg.add_route(dst_cidr = "42.42.42.43/32", next_hop = "1.1.2.3")
    
    # sending our new config
    pybird_c.set_config(new_cfg = bird_cfg.build_config())
   
    # check that our protocols are up
    pybird_c.check_protocols_up(['my_bgp1, my_bgp2'])

    # exit gracefully
    pybird_c.release()
    pybird_c.disconnect()

Print current configuration ::

    pybird_c = PyBirdClient(ip='localhost', port=4509)
    pybird_c.connect()
    pybird_c.acquire()

    # getting our current config
    cfg = pybird_c.get_config()
    print(cfg)

    # exit gracefully
    pybird_c.release()
    pybird_c.disconnect()

PyBirdClient class
------------------

.. autoclass:: trex.pybird.pybird_zmq_client.PyBirdClient
    :members: 
    :inherited-members:
    :member-order: bysource