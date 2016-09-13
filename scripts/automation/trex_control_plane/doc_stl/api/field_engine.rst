
Field Engine modules 
=======================

The Field Engine (FE) has limited number of instructions/operations to support most use cases. 
There is a plan to add LuaJIT to be more flexible at the cost of performance.
The FE can allocate stream variables in a stream context, write a stream variable to a packet offset, change packet size, and so on.

*Examples of Field Engine uses:*

* Change ipv4.tos 1-10
* Change packet size to a random value in the range 64 to 9K
* Create a range of flows (change src_ip, dest_ip, src_port, dest_port) 
* Update IPv4 checksum 


The following snippet creates a range of 64 bytes packets ::

  # split the range of IP to cores 
  #
  class STLS1(object):

    def __init__ (self):
        self.fsize  =64;

    def create_stream (self):
        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS

        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = STLScVmRaw( [   STLVmFlowVar ( "ip_src",  
                                            min_value="10.0.0.1",
                                            max_value="10.0.0.255", 
                                            size=4, 
                                            step=1,
                                            op="inc"),
                             STLVmWrFlowVar (fv_name="ip_src", 
                                             pkt_offset= "IP.src" ), # write ip to packet IP.src
                             STLVmFixIpv4(offset = "IP")                                # fix checksum
                           ],
                           split_by_field = "ip_src",
                           cache_size =255 # cache the packets, much better performance 
                        );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)
        stream = STLStream(packet = pkt,
                         mode = STLTXCont())
        #print(stream.to_code())
        return stream


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]


The following snippet creates a SYN attack::

    # create attack from random src_ip from 16.0.0.0-18.0.0.254 and random src_port 1025-65000    
    # attack 48.0.0.1 server 
        
    def create_stream (self):

        
        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                            STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),

                           STLVmFixIpv4(offset = "IP"), # fix checksum

                           STLVmWrFlowVar(fv_name="src_port", 
                                                pkt_offset= "TCP.sport") # fix udp len  

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())




STLScVmRaw class
----------------

Aggregate raw instructions objects 

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLScVmRaw
    :members: 
    :member-order: bysource


STLVmFlowVar 
------------

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLVmFlowVar
    :members: 
    :member-order: bysource

STLVmWrMaskFlowVar
------------------

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLVmWrMaskFlowVar
    :members: 
    :member-order: bysource

STLVmFixIpv4
------------------

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLVmFixIpv4
    :members: 
    :member-order: bysource
 

STLVmTrimPktSize
------------------

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLVmTrimPktSize
    :members: 
    :member-order: bysource

STLVmTupleGen
------------------

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLVmTupleGen
    :members: 
    :member-order: bysource

  

Field Engine snippet
--------------------

.. code-block:: python

    # FE Example1


        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = STLScVmRaw( [   STLVmTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.2", 
                                             port_min=1025, port_max=65535,
                                             name="tuple"), # define tuple gen 

                             # write ip to packet IP.src
                             STLVmWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), 
                             
                             STLVmFixIpv4(offset = "IP"),  # fix checksum
                             STLVmWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                                  ]
                              );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)


.. code-block:: python

    # FE Example2
        

        #range of source mac-addr

        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = STLScVmRaw( [ STLVmFlowVar(name="mac_src", 
                                        min_value=1, 
                                        max_value=30, 
                                        size=2, op="dec",step=1), 
                           STLVmWrMaskFlowVar(fv_name="mac_src", 
                                              pkt_offset= 11, 
                                              pkt_cast_size=1, 
                                              mask=0xff) 
                         ]
                        )


