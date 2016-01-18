
# simple test that uses simple API with stateless TRex
#from stl_test_api import BasicTestAPI
api_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(api_path, '../automation/trex_control_plane/client/'))

from trex_stateless_client import CTRexStatelessClient, LoggerApi

c = CTRexStatelessClient()
try:
    c.connect()
    #before_ipackets = x.get_stats().get_rel('ipackets')
    c.cmd_start_line("-f stl/imix_1pkt.yaml -m 5mpps -d 1")
    c.cmd_wait_on_traffic()
finally:
    c.disconnect()

#x = BasicTestAPI()
#
#try:
#    x.connect()
#
#    before_ipackets = x.get_stats().get_rel('ipackets')
#
#    print "input packets before test: %s" % before_ipackets
#
#    x.inject_profile("stl/imix_1pkt.yaml", rate = "5mpps", duration = 1)
#    x.wait_while_traffic_on()
#
#    after_ipackets = x.get_stats().get_rel('ipackets')
#
#    print "input packets after test: %s" % after_ipackets
#
#    if (after_ipackets - before_ipackets) == 5000000:
#        print "Test passed :-)\n"
#    else:
#        print "Test failed :-(\n"
#
#finally:
#    x.disconnect()


