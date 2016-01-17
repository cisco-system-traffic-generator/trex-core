from stl_test_api import BasicTestAPI

x = BasicTestAPI()

try:
    x.connect()

    s = x.get_stats()

    print "input packets before test: %s" % s['ipackets']

    x.inject_profile("stl/imix_1pkt.yaml", rate = "5gbps", duration = 1)
    x.wait_while_traffic_on()

    s = x.get_stats()

    print "input packets after test: %s" % s['ipackets']

    print "Test passed :-)\n"

finally:
    x.disconnect()


