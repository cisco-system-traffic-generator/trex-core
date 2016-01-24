import os
import sys
import time


# update the import path to include the stateless client
root_path = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.join(root_path, '../../scripts/automation/trex_control_plane/client/'))
sys.path.insert(0, os.path.join(root_path, '../../scripts/automation/trex_control_plane/client_utils/'))
sys.path.insert(0, os.path.join(root_path, '../../scripts/automation/trex_control_plane/client_utils/'))

# aliasing
import trex_stateless_client
STLClient  = trex_stateless_client.STLClient
STLError   = trex_stateless_client.STLError

