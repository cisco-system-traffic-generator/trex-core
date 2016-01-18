import os
import sys

api_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(api_path, 'automation/trex_control_plane/client/'))

from trex_stateless_client import CTRexStatelessClient, STLFailure

c = CTRexStatelessClient()

try:
    c.connect()
    #before_ipackets = x.get_stats().get_rel('ipackets')
    c.start(profiles = 'stl/imix_3pkt.yaml', ports = [1])
    #c.cmd_wait_on_traffic()

except STLFailure as e:
    print e
finally:
    c.teardown()

