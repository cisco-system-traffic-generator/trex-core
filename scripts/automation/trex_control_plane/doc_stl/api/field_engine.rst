
Field Engine modules 
=======================

The Field Engine (FE) has limited number of instructions/operation for supporting most use cases. 
There is a plan to add LuaJIT to be more flexible at the cost of performance.
The FE can allocate stream variables in a Stream context, write a stream variable to a packet offset, change packet size,  etc.

*Some examples for what can be done:*

* Change ipv4.tos 1-10
* Change packet size to be random in the range 64-9K
* Create range of flows (change src_ip, dest_ip, src_port, dest_port) 
* Update IPv4 checksum 


for example this snippet will create SYN Attack::

    # create attack from random src_ip from 16.0.0.0-18.0.0.254 and random src_port 1025-65000    
    # attack 48.0.0.1 server 
        
    def create_stream (self):

        
        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")


        # vm
        vm = CTRexScRaw( [ STLVmFlowVar(name="ip_src", 
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




CTRexScRaw class
----------------

Aggregate a raw instructions objects 

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexScRaw
    :members: 
    :member-order: bysource


STLVmFlowVar 
------------

It is alias for CTRexVmDescFlowVar

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescFlowVar
    :members: 
    :member-order: bysource

STLVmWrMaskFlowVar
------------------

It is alias for CTRexVmDescWrMaskFlowVar

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescWrMaskFlowVar
    :members: 
    :member-order: bysource

STLVmFixIpv4
------------------

It is alias for CTRexVmDescFixIpv4

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescFixIpv4
    :members: 
    :member-order: bysource
 

STLVmTrimPktSize
------------------

It is alias for CTRexVmDescTrimPktSize

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescTrimPktSize
    :members: 
    :member-order: bysource

STLVmTupleGen
------------------

It is alias for CTRexVmDescTupleGen

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescTupleGen
    :members: 
    :member-order: bysource

STLVmTupleGen
------------------

It is alias for STLVmTupleGen

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.CTRexVmDescTupleGen
    :members: 
    :member-order: bysource

  

Field Engine snippet
--------------------

Example1:: 


        base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = CTRexScRaw( [   STLVmTupleGen ( ip_min="16.0.0.1", ip_max="16.0.0.2", 
                                                   port_min=1025, port_max=65535,
                                                    name="tuple"), # define tuple gen 

                             STLVmWrFlowVar (fv_name="tuple.ip", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             STLVmFixIpv4(offset = "IP"),                                # fix checksum
                             STLVmWrFlowVar (fv_name="tuple.port", pkt_offset= "UDP.sport" )  #write udp.port
                                  ]
                              );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        

Example2::

        #range of source mac-addr

        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = CTRexScRaw( [ STLVmFlowVar(name="mac_src", min_value=1, max_value=30, size=2, op="dec",step=1), 
                           STLVmWrMaskFlowVar(fv_name="mac_src", pkt_offset= 11,pkt_cast_size=1, mask=0xff) 
                          ]
                       )


