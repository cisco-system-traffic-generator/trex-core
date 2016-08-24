
Client Module 
==================

The TRex Client provides access to the TRex server.

**Client and interfaces**

Multiple users can interact with one TRex server. Each user "owns" a different set of interfaces.
The protocol is JSON-RPC2 over ZMQ transport.

In addition to the Python API, a console-based API interface is also available.

Python-like example::
    
   c.start(ports = [0, 1], mult = "5mpps", duration = 10)

   c.start(ports = [0, 1], mult = "5mpps", duration = 10, core_mask = [0x1,0xe] )

   c.start(ports = [0, 1], mult = "5mpps", duration = 10, core_mask = core_mask=STLClient.CORE_MASK_PIN )
   
Console-like example::

  c.start_line (" -f stl/udp_1pkt_simple.py -m 10mpps --port 0 1 ")



Example 1 - Typical Python API::

    c = STLClient(username = "itay",server = "10.0.0.10", verbose_level = LoggerApi.VERBOSE_HIGH)

    try:
        # connect to server
        c.connect()

        # prepare our ports (my machine has 0 <--> 1 with static route)
        c.reset(ports = [0, 1])

        # add both streams to ports
        c.add_streams(s1, ports = [0])

        # clear the stats before injecting
        c.clear_stats()

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        c.wait_on_traffic(ports = [0, 1])
        
        # check for any warnings
        if c.get_warnings():
          # handle warnings here
          pass

    finally:
        c.disconnect()


STLClient class
---------------

.. autoclass:: trex_stl_lib.trex_stl_client.STLClient
    :members: 
    :member-order: bysource
    

STLClient snippet
-----------------


.. code-block:: python
    :caption: Example 1: Minimal example of client interacting with the TRex server

    c = STLClient()

    try:
        # connect to server
        c.connect()

        # prepare our ports (my machine has 0 <--> 1 with static route)
        c.reset(ports = [0, 1])

        # add both streams to ports
        c.add_streams(s1, ports = [0])

        # clear the stats before injecting
        c.clear_stats()

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        c.wait_on_traffic(ports = [0, 1])

        # check for any warnings
	if c.get_warnings():
	    # handle warnings here
	    pass

    finally:
        c.disconnect()



.. code-block:: python
    :caption: Example 2: Client can execute other functions while the TRex server is generating traffic


    c = STLClient()
    try:
        #connect to server
        c.connect()

        #..

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        while True  :
                # do somthing else
                os.sleep(1) # sleep for 1 sec 
                # check if the port is still active 
                if c.is_traffic_active(ports = [0, 1])==False
                        break;
                
    finally:
        c.disconnect()



.. code-block:: python
    :caption: Example 3: Console-like API interface


        def simple ():
        
            # create client
            #verbose_level = LoggerApi.VERBOSE_HIGH  # set to see JSON-RPC commands
            c = STLClient(verbose_level = LoggerApi.VERBOSE_REGULAR)
            passed = True
            
            try:
                # connect to server
                c.connect()
        
                my_ports=[0,1]
        
                # prepare our ports
                c.reset(ports = my_ports)
        
                print (" is connected {0}".format(c.is_connected()))
        
                print (" number of ports {0}".format(c.get_port_count()))
                print (" acquired_ports {0}".format(c.get_acquired_ports()))
                # port stats
                print c.get_stats(my_ports)  
        
                # port info, mac-addr info, speed 
                print c.get_port_info(my_ports)
        
                c.ping()
        
                print("start") 
                # start traffic on port 0,1 each 10mpps 
                c.start_line (" -f ../../../../stl/udp_1pkt_simple.py -m 10mpps --port 0 1 ")
                time.sleep(2);
                c.pause_line("--port 0 1");
                time.sleep(2);
                c.resume_line("--port 0 1");
                time.sleep(2);
                c.update_line("--port 0 1 -m 5mpps");   # reduce to 5 mpps
                time.sleep(2);
                c.stop_line("--port 0 1");  # stop both ports
        
            except STLError as e:
                passed = False
                print e
        
            finally:
                c.disconnect()


Example 4: Load profile from a file::

  def simple ():

    # create client
    #verbose_level = LoggerApi.VERBOSE_HIGH
    c = STLClient(verbose_level = LoggerApi.VERBOSE_REGULAR)
    passed = True
    
    try:
        # connect to server
        c.connect()

        my_ports=[0,1]

        # prepare our ports
        c.reset(ports = my_ports)

        profile_file =   "../../../../stl/udp_1pkt_simple.py"   # a traffic profile file 

        try:                                                    # load a profile
            profile = STLProfile.load(profile_file)
        except STLError as e:
            print format_text("\nError while loading profile '{0}'\n".format(profile_file), 'bold')
            print e.brief() + "\n"
            return

        print profile.dump_to_yaml()                            # print it as YAML

        c.remove_all_streams(my_ports)                          # remove all streams


        c.add_streams(profile.get_streams(), ports = my_ports)  # add them 

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)  # start for 10 sec

        # block until done
        c.wait_on_traffic(ports = [0, 1])                       # wait 


    finally:
        c.disconnect()
        

.. code-block:: python
    :caption: Example 5: pin cores to ports

    c = STLClient()

    try:
        # connect to server
        c.connect()

        # prepare our ports (my machine has 0 <--> 1 with static route)
        c.reset(ports = [0, 1])

        # add both streams to ports
        c.add_streams(s1, ports = [0])

        # clear the stats before injecting
        c.clear_stats()

        c.start(ports = [0, 1], mult = "5mpps", duration = 10, core_mask = [0x1,0x2]) # pin core to ports for better performance 

        # block until done
        c.wait_on_traffic(ports = [0, 1])

        # check for any warnings
	if c.get_warnings():
	    # handle warnings here
	    pass

    finally:
        c.disconnect()

