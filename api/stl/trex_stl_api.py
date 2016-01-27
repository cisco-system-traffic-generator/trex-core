import os
import sys
import time


# update the import path to include the stateless client
root_path = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.join(root_path, '../../scripts/automation/trex_control_plane/'))

# aliasing
import common.trex_streams
from client_utils.packet_builder import CTRexPktBuilder
import common.trex_stl_exceptions
import client.trex_stateless_client

STLClient              = client.trex_stateless_client.STLClient
STLError               = common.trex_stl_exceptions.STLError
STLContStream          = common.trex_streams.STLContStream
STLSingleBurstStream   = common.trex_streams.STLSingleBurstStream
STLMultiBurstStream    = common.trex_streams.STLMultiBurstStream
STLPktBuilder          = CTRexPktBuilder

