import os
import sys
import time

api_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(api_path, 'automation/trex_control_plane/client/'))

from trex_stateless_client import CTRexStatelessClient, STLError

c = CTRexStatelessClient()

try:
    for i in xrange(0, 100):
        c.connect("RO")
        c.disconnect()
        
    #
    #c.stop()
    #before_ipackets = x.get_stats().get_rel('ipackets')

    #c.start(profiles = 'stl/imix_3pkt.yaml', ports = [0,1], mult = "1gbps")

    #for i in xrange(0, 10):
    #    time.sleep(5)
    #    c.update(ports = [0,1], mult = "1gbps+")

    #c.cmd_wait_on_traffic()
    #c.stop()

except STLError as e:
    print e
finally:
    pass
    #c.teardown()

