#!/router/bin/python
from stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys

class STLRX_Test(CStlGeneral_Test):
    """Tests for RX feature"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        assert 'bi' in CTRexScenario.stl_ports_map

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]
        
        self.c = CTRexScenario.stl_trex

        self.c.reset(ports = [self.tx_port, self.rx_port])

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

    @classmethod
    def tearDownClass(cls):
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    def __verify_flow (self, pg_id, total_pkts, pkt_len):

        flow_stats = self.c.get_stats()['flow_stats'].get(pg_id)
        if not flow_stats:
            assert False, "no flow stats available"

        tx_pkts  = flow_stats['tx_pkts'].get(self.tx_port, 0)
        tx_bytes = flow_stats['tx_bytes'].get(self.tx_port, 0)
        rx_pkts  = flow_stats['rx_pkts'].get(self.rx_port, 0)

        if tx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            assert False, 'TX pkts mismatch - got: {0}, expected: {1}'.format(tx_pkts, total_pkts)

        if tx_bytes != (total_pkts * pkt_len):
            pprint.pprint(flow_stats)
            assert False, 'TX bytes mismatch - got: {0}, expected: {1}'.format(tx_bytes, (total_pkts * pkt_len))

        if rx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            assert False, 'RX pkts mismatch - got: {0}, expected: {1}'.format(rx_pkts, total_pkts)


    # RX itreation
    def __rx_iteration (self, exp_list):

        self.c.clear_stats()

        self.c.start(ports = [self.tx_port])
        self.c.wait_on_traffic(ports = [self.tx_port])

        for exp in exp_list:
            self.__verify_flow(exp['pg_id'], exp['total_pkts'], exp['pkt_len'])



    # one simple stream on TX --> RX
    def test_one_stream(self):
        total_pkts = 500000

        try:
            s1 = STLStream(name = 'rx',
                           packet = self.pkt,
                           flow_stats = STLFlowStats(pg_id = 5),
                           mode = STLTXSingleBurst(total_pkts = total_pkts,
                                                   percentage = 80
                                                   ))

            # add both streams to ports
            self.c.add_streams([s1], ports = [self.tx_port])

            print "\ninjecting {0} packets on port {1}\n".format(total_pkts, self.tx_port)

            exp = {'pg_id': 5, 'total_pkts': total_pkts, 'pkt_len': self.pkt.get_pkt_len()}

            self.__rx_iteration( [exp] )


        except STLError as e:
            assert False , '{0}'.format(e)


    # one simple stream on TX --> RX
    def test_multiple_streams(self):
        total_pkts = 500000

        try:
            streams = []
            exp = []
            # 10 identical streams
            for pg_id in range(20, 30):

                streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                         packet = self.pkt,
                                         flow_stats = STLFlowStats(pg_id = pg_id),
                                         mode = STLTXSingleBurst(total_pkts = total_pkts,
                                                                 pps = total_pkts * 5)))
                exp.append(['pg_id': pg_id, 'total_pkts': total_pkts, 'pkt_len': self.pkt.get_pkt_len()])

            # add both streams to ports
            self.c.add_streams(streams, ports = [self.tx_port])

            self.__rx_iteration(exp)


        except STLError as e:
            assert False , '{0}'.format(e)

