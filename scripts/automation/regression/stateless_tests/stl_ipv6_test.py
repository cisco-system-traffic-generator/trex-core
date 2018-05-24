#!/usr/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *

class STLIPv6_Test(CStlGeneral_Test):
    """Tests for IPv6 scan6/ping_ip """

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        if self.is_vdev:
            self.skip("We don't know what to expect with vdev.")
        print('')
        self.stl_trex.reset()
        self.stl_trex.set_service_mode()

    def tearDown(self):
        CStlGeneral_Test.tearDown(self)
        self.stl_trex.set_service_mode(enabled = False)

    def conf_ipv6(self, tx_enabled, rx_enabled, tx_src = None, rx_src = None):
        tx, rx = CTRexScenario.stl_ports_map['bi'][0]
        self.stl_trex.conf_ipv6(tx, tx_enabled, tx_src)
        self.stl_trex.conf_ipv6(rx, rx_enabled, rx_src)
        return tx, rx

    def filter_ping_results(self, results):
        return list(filter(lambda result: result['status'] == 'success', results))

    def test_stl_ipv6_ping(self):
        ping_count = 5
        expected_replies = 4 # allow one loss

        results = self.stl_trex.ping_ip(src_port = 0, dst_ip = 'ff02::1', count = ping_count)
        good_replies = len(self.filter_ping_results(results))
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

        # negative test, non-existing IP
        dst_ip = '1234::1234'
        results = self.stl_trex.ping_ip(src_port = 0, dst_ip = dst_ip, count = 2)
        good_replies = len(self.filter_ping_results(results))
        if good_replies > 0:
            self.fail('We have answers from non-existing IPv6 %s, bug!\nOutput: %s' % (dst_ip, results))
        else:
            print('Got no replies from non-existing IPv6 %s as expected.' % dst_ip)

    def test_ipv6_ping_linux_based_stack(self):
        '''Testing Linux-based stack ping in different scenarios'''
        if not (self.is_linux_stack and self.is_loopback):
            self.skip('Relevant only for Linux-based stack in loopback')
        rx_src = '1111::1112'
        try:
            for tx_enabled in (True, False):
                for rx_enabled in (True, False):
                    for tx_src in ('1111::1111', None):
                        print('tx_enabled: %5s,   rx_enabled: %5s,   tx_src: %10s' % (tx_enabled, rx_enabled, tx_src))
                        tx, rx = self.conf_ipv6(tx_enabled, rx_enabled, tx_src, rx_src)
                        results = self.stl_trex.ping_ip(src_port = tx, dst_ip = rx_src, count = 2)
                        replies = self.filter_ping_results(results)
                        if rx_enabled:
                            assert replies, 'Got no replies while RX port has IPv6 enabled'
                        else:
                            assert not replies, 'Got replies while RX port has IPv6 disabled: %s' % replies
        finally:
            self.conf_ipv6(False, False)


    def test_stl_ipv6_scan6(self):
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


