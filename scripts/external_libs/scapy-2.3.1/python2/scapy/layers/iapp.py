from scapy.fields import *
from scapy.packet import *
from scapy.layers.l2 import SNAP
from scapy.layers.netflow import NetflowHeader

class IAPP(Packet):
    name = "IAPP Header"
    fields_desc = [ 
                    LenField("iapp_id_length", None, "H"),
                    ByteField("type", 0),
                    ByteField("subtype", 0),
                    MACField("dst_mac", ETHER_ANY),
                    MACField("src_mac", ETHER_ANY)
                  ]

bind_layers( SNAP, IAPP, code=0 ) #code=0 is IAPP type
bind_layers( IAPP, NetflowHeader, type=0x63 ) #99 is Netflow