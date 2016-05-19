#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys

class STLRX_Test(CStlGeneral_Test):
    """Tests for RX feature"""

    def setUp(self):
        per_driver_params = {"rte_vmxnet3_pmd": [1, 50], "rte_ixgbe_pmd": [30, 5000], "rte_i40e_pmd": [80, 5000],
                             "rte_igb_pmd": [80, 500], "rte_em_pmd": [1, 50], "rte_virtio_pmd": [1, 50]}

        CStlGeneral_Test.setUp(self)
        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        port_info = self.c.get_port_info(ports = self.rx_port)[0]
        cap = port_info['rx']['caps']
        print(port_info)
        if "flow_stats" not in cap or "latency" not in cap:
            self.skip('port {0} does not support RX'.format(self.rx_port))
        self.cap = cap

        self.rate_percent = per_driver_params[port_info['driver']][0]
        self.total_pkts = per_driver_params[port_info['driver']][1]
        self.c.reset(ports = [self.tx_port, self.rx_port])

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

    @classmethod
    def tearDownClass(cls):
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    def __verify_flow (self, pg_id, total_pkts, pkt_len, stats):
        flow_stats = stats['flow_stats'].get(pg_id)
        latency_stats = stats['latency'].get(pg_id)

        if not flow_stats:
            assert False, "no flow stats available"

        tx_pkts  = flow_stats['tx_pkts'].get(self.tx_port, 0)
        tx_bytes = flow_stats['tx_bytes'].get(self.tx_port, 0)
        rx_pkts  = flow_stats['rx_pkts'].get(self.rx_port, 0)
        if latency_stats is not None:
            drops = latency_stats['err_cntrs']['dropped']
            ooo = latency_stats['err_cntrs']['out_of_order']
            if drops != 0 or ooo != 0:
                pprint.pprint(latency_stats)
                tmp='Dropped or out of order packets - dropped: {0}, ooo: {1}'.format(drops, ooo)
                assert False, tmp

        if tx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            tmp = 'TX pkts mismatch - got: {0}, expected: {1}'.format(tx_pkts, total_pkts)
            assert False, tmp

        if tx_bytes != (total_pkts * pkt_len):
            pprint.pprint(flow_stats)
            tmp = 'TX bytes mismatch - got: {0}, expected: {1}'.format(tx_bytes, (total_pkts * pkt_len))
            assert False, tmp

        if rx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            tmp = 'RX pkts mismatch - got: {0}, expected: {1}'.format(rx_pkts, total_pkts)
            assert False, tmp

        if "rx_bytes" in self.cap:
            rx_bytes = flow_stats['rx_bytes'].get(self.rx_port, 0)
            if rx_bytes != (total_pkts * pkt_len):
                pprint.pprint(flow_stats)
                tmp = 'RX bytes mismatch - got: {0}, expected: {1}'.format(rx_bytes, (total_pkts * pkt_len))
                assert False, tmp


    # RX itreation
    def __rx_iteration (self, exp_list):

        self.c.clear_stats()

        self.c.start(ports = [self.tx_port])
        self.c.wait_on_traffic(ports = [self.tx_port])
        stats = self.c.get_stats()

        for exp in exp_list:
            self.__verify_flow(exp['pg_id'], exp['total_pkts'], exp['pkt_len'], stats)


    # one simple stream on TX --> RX
    def test_one_stream(self):
        total_pkts = self.total_pkts * 10

        try:
            s1 = STLStream(name = 'rx',
                           packet = self.pkt,
                           flow_stats = STLFlowLatencyStats(pg_id = 5),
                           mode = STLTXSingleBurst(total_pkts = total_pkts,
                                                   percentage = self.rate_percent
                                                   ))

            # add both streams to ports
            self.c.add_streams([s1], ports = [self.tx_port])

            print("\ninjecting {0} packets on port {1}\n".format(total_pkts, self.tx_port))

            exp = {'pg_id': 5, 'total_pkts': total_pkts, 'pkt_len': self.pkt.get_pkt_len()}

            self.__rx_iteration( [exp] )


        except STLError as e:
            assert False , '{0}'.format(e)


    # one simple stream on TX --> RX
    def test_multiple_streams(self):
        num_latency_streams = 110
        num_flow_stat_streams = 110
        total_pkts = int(self.total_pkts / num_latency_streams) / 2
        if total_pkts == 0:
            total_pkts = 1
        percent = float(self.rate_percent) / num_latency_streams / 2

        try:
            streams = []
            exp = []
            # 10 identical streams
            for pg_id in range(1, num_latency_streams):

                streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                         packet = self.pkt,
                                         flow_stats = STLFlowLatencyStats(pg_id = pg_id),
                                         mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent)))

                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': self.pkt.get_pkt_len()})

            for pg_id in range(num_latency_streams + 1, num_latency_streams + num_flow_stat_streams):

                streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                         packet = self.pkt,
                                         flow_stats = STLFlowStats(pg_id = pg_id),
                                         mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent)))

                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': self.pkt.get_pkt_len()})

            # add both streams to ports
            self.c.add_streams(streams, ports = [self.tx_port])

            self.__rx_iteration(exp)


        except STLError as e:
            assert False , '{0}'.format(e)

    def test_1_stream_many_iterations (self):
        total_pkts = self.total_pkts

        try:
            s1 = STLStream(name = 'rx',
                           packet = self.pkt,
                           flow_stats = STLFlowLatencyStats(pg_id = 5),
                           mode = STLTXSingleBurst(total_pkts = total_pkts,
                                                   percentage = self.rate_percent
                                                   ))

            # add both streams to ports
            self.c.add_streams([s1], ports = [self.tx_port])

            print("\ninjecting {0} packets on port {1}\n".format(total_pkts, self.tx_port))

            exp = {'pg_id': 5, 'total_pkts': total_pkts, 'pkt_len': self.pkt.get_pkt_len()}

            for i in range(0, 10):
                print("starting iteration {0}".format(i))
                self.__rx_iteration( [exp] )



        except STLError as e:
            assert False , '{0}'.format(e)
