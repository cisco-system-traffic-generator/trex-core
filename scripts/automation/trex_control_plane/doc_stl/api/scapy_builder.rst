
Packet builder modules 
=======================

The packet builder module objective is to build a template packet for a stream and to create a Field engine program to change fields in the packet.

**Some examples for what can be done:**

* Build a IP/UDP/DNS packet and create a range of src_ip = 10.0.0.1-10.0.0.255
* Build a IP/UDP packets in IMIX sizes 


for example this snippet will create SYN Attack::

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

.. autoclass:: trex_stl_lib.trex_stl_packet_builder_scapy.STLPktBuilder
    :members: 
    :member-order: bysource


  


