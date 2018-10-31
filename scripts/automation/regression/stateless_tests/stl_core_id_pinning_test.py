#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob
from nose.tools import nottest


class CoreID_Test(CStlGeneral_Test):
    """Tests for stateless client"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)

        self.c = CTRexScenario.stl_trex

        assert 'bi' in CTRexScenario.stl_ports_map
        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]
        self.tx_ports = [self.tx_port]

        self.c.connect()
        self.c.reset(ports = self.tx_ports)

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        self.c.clear_stats()

        self.num_cores = self.c.get_server_system_info().get('dp_core_count_per_port')
        assert self.num_cores > 0, 'Cores count is zero, something bad is here'
        if self.num_cores == 1:
            self.skip('Must have more than one core for this test.')


    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    # tests core pinning with latency
    def get_used_cores (self):
        used_cores = []
        cpu_stats = [x['ports'] for x in self.c.get_util_stats()['cpu']]
        for i, cpu in enumerate(cpu_stats):
            cpu = [x for x in cpu if x != -1]
            if cpu:
                used_cores.append(i)
        return used_cores


    def test_core_id_negative(self):

        core_id = self.num_cores - 1

        try:
            s1 = STLStream(packet = self.pkt, mode = STLTXCont(pps = 10), core_id = core_id, 
                                              flow_stats = STLFlowLatencyStats(pg_id = 7))
        except TRexError as e:
            assert e.msg == "Core ID is not supported for latency streams."

        try:
            core_id = self.num_cores
            s1 = STLStream(packet = self.pkt, core_id=core_id)
            self.c.add_streams(streams = s1, ports = self.tx_ports)
        except TRexError as e:
            assert ("It must be an integer between 0" in e.msg or
                    "There is only one core" in e.msg)

        core_id = self.num_cores - 1
        expected_err_text = 'must be pinned to the same core'

        try:
            s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), next = 's2')
            s2 = STLStream(name = 's2', self_start = False, packet = self.pkt, mode = STLTXCont(pps = 10), core_id = core_id)
            self.c.add_streams(streams = [s1, s2], ports = self.tx_ports)
        except TRexError as e:
            assert expected_err_text in e.msg

        try:
            s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), next = 's2', core_id = core_id)
            s2 = STLStream(name = 's2', self_start = False, packet = self.pkt, mode = STLTXCont(pps = 10))
            self.c.add_streams(streams = [s1, s2], ports = self.tx_ports)
        except TRexError as e:
            assert expected_err_text in e.msg

        if self.num_cores > 1:
            try:
                s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), next = 's2', core_id = core_id)
                s2 = STLStream(name = 's2', self_start = False, packet = self.pkt, mode = STLTXCont(pps = 10), core_id = core_id - 1)
                self.c.add_streams(streams = [s1, s2], ports = self.tx_ports)
            except TRexError as e:
                assert expected_err_text in e.msg

            try:
                s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), next = 's3', core_id = core_id)
                s2 = STLStream(name = 's2', packet = self.pkt, mode = STLTXSingleBurst(pps = 10), next = 's3', core_id = core_id - 1)
                s3 = STLStream(name = 's3', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), core_id = core_id)
                self.c.add_streams(streams = [s1, s2, s3], ports = self.tx_ports)
            except TRexError as e:
                assert expected_err_text in e.msg


    def test_core_id_pinning_single(self):

        for i in range(min(self.num_cores, 3)):
            s = STLStream(packet = self.pkt, core_id = i)
            self.c.add_streams(streams = s, ports = self.tx_ports)
            self.c.start(ports = self.tx_ports, mult = "5%", duration = 3)
            time.sleep(1)
            used_cores = self.get_used_cores()
            assert used_cores == [i], 'Only core %s is expected to be active, but we got %s active' % (i, used_cores)
            self.c.stop(ports = self.tx_ports)
            self.c.remove_all_streams(ports = self.tx_ports)
            time.sleep(1)

        # asserting that stats are cleared
        used_cores = self.get_used_cores()
        assert not used_cores, 'Some cores are still utilized: %s' % used_cores


    def test_core_id_pinning_several_cores(self):

        cores_list = list(range(1, self.num_cores, 3))
        stream_list = [STLStream(packet = self.pkt, core_id = i) for i in cores_list]
        self.c.add_streams(streams = stream_list, ports = self.tx_ports)
        self.c.start(ports = self.tx_ports, mult = "5%", duration = 3)
        time.sleep(1)
        used_cores = self.get_used_cores()
        assert sorted(used_cores) == sorted(cores_list), 'Expected working cores: %s, got: %s' % (cores_list, used_cores)
        self.c.stop(ports = self.tx_ports)
        self.c.remove_all_streams(ports = self.tx_ports)
        time.sleep(1)

        #asserting that stats are cleared 
        used_cores = self.get_used_cores()
        assert not used_cores, 'Some cores are still utilized: %s' % used_cores


    def test_core_id_pinning_several_streams(self):

        core_id = self.num_cores - 1
        s1 = STLStream(packet = self.pkt, core_id = core_id)
        s2 = STLStream(packet = self.pkt, core_id = core_id)
        self.c.add_streams(streams = s1, ports = self.tx_ports)
        self.c.add_streams(streams = s2, ports = self.tx_ports)
        self.c.start(ports = self.tx_ports, mult = "5%", duration = 3)
        time.sleep(1)
        used_cores = self.get_used_cores()
        assert used_cores == [core_id], 'Only core %s is expected to be active, got: %s' % (core_id, used_cores)
        self.c.stop(ports = self.tx_ports)
        self.c.remove_all_streams(ports = self.tx_ports)

    def test_core_id_pinning_single_burst_loop(self):

        core_id = self.num_cores - 1
        s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), core_id = core_id, next = 's2')
        s2 = STLStream(name = 's2', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), core_id = core_id, next = 's1', self_start = False)
        self.c.add_streams(streams = [s1, s2], ports = self.tx_ports)
        self.c.start(ports = self.tx_ports, mult = "5%", duration = 3)
        time.sleep(1)
        used_cores = self.get_used_cores()
        assert used_cores == [core_id], 'Only core %s is expected to be active, got: %s' % (core_id, used_cores)
        self.c.stop(ports = self.tx_ports)
        self.c.remove_all_streams(ports = self.tx_ports)

