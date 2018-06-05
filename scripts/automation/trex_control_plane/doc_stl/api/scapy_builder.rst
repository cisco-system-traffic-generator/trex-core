
Packet builder modules 
=======================

The packet builder module is used for building a template packet for a stream, and creating a Field Engine program to change fields in the packet.

**Examples:**

* Build a IP/UDP/DNS packet with a src_ip range of 10.0.0.1 to 10.0.0.255
* Build IP/UDP packets in IMIX sizes 


For example, this snippet creates a SYN attack::

    # create attack from random src_ip from 16.0.0.0-18.0.0.254 and random src_port 1025-65000    
    # attack 48.0.0.1 server 
        
    def create_stream (self):

        
        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")

        pkt = STLPktBuilder(pkt = base_pkt)

        return STLStream(packet = pkt,    #<<<<< set packet builder inside the stream 
                         mode = STLTXCont())




STLPktBuilder  class
--------------------

Aggregate a raw instructions objects 

.. autoclass:: trex.stl.trex_stl_packet_builder_scapy.STLPktBuilder
    :members: 
    :member-order: bysource


  


