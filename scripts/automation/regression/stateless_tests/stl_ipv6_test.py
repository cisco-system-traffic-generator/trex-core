#!/usr/bin/python
from .stl_general_test import CStlGeneral_Test
from trex_stl_lib.api import *

class STLIPv6_Test(CStlGeneral_Test):
    """Tests for IPv6 scan6/ping_ip """

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        print('')
        self.stl_trex.set_service_mode(ports = [0])

    def tearDown(self):
        CStlGeneral_Test.tearDown(self)
        self.stl_trex.set_service_mode(ports = [0], enabled = False)

    def test_ipv6_ping(self):
        ping_count = 5
        expected_replies = 4 # allow one loss
        results = self.stl_trex.ping_ip(src_port = 0, dst_ip = 'ff02::1', count = ping_count)
        good_replies = len(filter(lambda result: result['status'] == 'success', results))
        if self.is_loopback:
            # negative test, loopback
            if good_replies > 0:
                self.fail('We should not respond to IPv6 in loopback at this stage, bug!\nOutput: %s' % results)
            else:
                print('No IPv6 replies in loopback as expected.')
        else:
            # positive test, DUT
            if good_replies < expected_replies:
                self.fail('Got only %s good replies out of %s.\nOutput: %s' % (good_replies, ping_count, results))
            else:
                print('Got replies from DUT as expected.')

        # negative test, unknown IP
        results = self.stl_trex.ping_ip(src_port = 0, dst_ip = '1234::1234', count = ping_count)
        good_replies = len(filter(lambda result: result['status'] == 'success', results))
        if good_replies > 0:
            self.fail('We have answers from unknown IPv6, bug!\nOutput: %s' % results)
        else:
            print('Got no replies from unknown IPv6 as expected.')

    def test_ipv6_scan6(self):
        results = self.stl_trex.scan6(ports = 0)
        if self.is_loopback:
            # negative test, loopback
            if results[0]:
                self.fail("Scan6 found devices in loopback, we don't answer to IPv6 now, bug!\nOutput: %s" % results)
            else:
                print('No devices found in loopback as expected.')
        else:
            # positive test, DUT
            if len(results[0]) > 1:
                self.fail('Found more than one device at port 0: %s' % results)
            elif len(results[0]) == 1:
                print('Found one device as expected:\n{type:10} - {mac:20} - {ipv6}'.format(**results[0][0]))
            else:
                self.fail('Did not find IPv6 devices.')


