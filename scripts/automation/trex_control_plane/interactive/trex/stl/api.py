# some common data
from ..common.trex_exceptions import *
from ..common.trex_logger import Logger
from ..utils.common import *

# TRex STL
from .trex_stl_client import STLClient, PacketBuffer
from .trex_stl_packet_builder_scapy import *
from .trex_stl_streams import *
from ..common.trex_ns import *


# scapy
from scapy.all import Ether, IP, TCP, UDP


#######################
# backward compatible #
#######################

STLError      = TRexError
STLTypeError  = TRexTypeError

def stl_map_ports(client):
    return client.map_ports()

