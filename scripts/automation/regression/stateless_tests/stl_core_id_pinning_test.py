#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob
from nose.tools import nottest

def get_error_in_percentage (golden, value):
    if (golden==0):
        return(0.0)
    return abs(golden - value) / float(golden)

def get_stl_profiles ():
    profiles_path = os.path.join(CTRexScenario.scripts_path, 'stl/')
    py_profiles = glob.glob(profiles_path + "/*.py")
    yaml_profiles = glob.glob(profiles_path + "yaml/*.yaml")
    return py_profiles + yaml_profiles


class CoreID_Test(CStlGeneral_Test):
    """Tests for stateless client"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        self.weak = self.is_virt_nics or CTRexScenario.setup_name in ('trex21', 'trex22')

        if self.weak:
            self.percentage = 5
            self.pps = 500
        else:
            self.percentage = 50
            self.pps = 50000
        
        # strict mode is only for 'wire only' connection
        if self.is_loopback and not self.weak:
            self.strict = True
        else:
            self.strict = False

        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        self.c.connect()
        self.c.reset(ports = [self.tx_port, self.rx_port])

        port_info = self.c.get_port_info(ports = self.rx_port)[0]

        drv_name = port_info['driver']

        self.drv_name = drv_name

        # due to defect trex-325 
        #if  self.drv_name == 'net_mlx5':
        #    print("WARNING disable strict due to trex-325 on mlx5")
        #    self.strict = False


        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        self.profiles = get_stl_profiles()
        
        self.c.clear_stats()

        
    def cleanup (self):
        self.c.remove_all_captures()
        self.c.reset(ports = [self.tx_port, self.rx_port])
        
            
    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    def verify (self, expected, got):
        if self.strict:
            assert expected == got
        else:
            if expected==0:
                return
            else:
                if get_error_in_percentage(expected, got) < 0.05 :
                    return
                print(' ERROR verify expected: %d  got:%d ' % (expected,got) )
                assert(0)



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

        num_cores = self.c.get_server_system_info().get('dp_core_count_per_port')
        core_id = num_cores - 1
        self.c.acquire([0, 1])

        try:
            s1 = STLStream(packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), core_id = core_id)
        except TRexError as e:
            assert (e.msg == "Core ID is supported only for Continuous mode.")

        try:
            s1 = STLStream(packet = self.pkt, mode = STLTXMultiBurst(pps=10, pkts_per_burst = 3), core_id = core_id)
        except TRexError as e:
            assert e.msg == "Core ID is supported only for Continuous mode."

        try:
            s1 = STLStream(packet = self.pkt, mode = STLTXCont(pps = 10), core_id = core_id, 
                                              flow_stats = STLFlowLatencyStats(pg_id = 7))
        except TRexError as e:
            assert e.msg == "Core ID is not supported for latency streams."

        try:
            s1 = STLStream(name = 's1', packet = self.pkt, mode = STLTXSingleBurst(total_pkts = 3), next = 's2')
            s2 = STLStream(name = 's2', self_start = False, packet = self.pkt, mode = STLTXCont(pps = 10), core_id = core_id)
        except TRexError as e:
            assert e.msg == "Core ID is supported only for streams that aren't being pointed at."

        try:
            core_id = num_cores
            s1 = STLStream(packet = self.pkt, mode = STLTXCont(), core_id=core_id)
            self.c.add_streams(streams = s1, ports = [0, 1])
        except TRexError as e:
            assert "It must be an integer between 0" in e.msg


    def test_core_id_pinning(self):

        num_cores = self.c.get_server_system_info().get('dp_core_count_per_port')
        assert num_cores > 0 
        # self.skip('...') if num_cores <= core_id

        # part 1
        for i in range(num_cores):
            self.c.reset(ports = [0, 1])
            s = STLStream(packet = self.pkt, mode = STLTXCont(), core_id = i)
            self.c.add_streams(streams = s, ports = [0, 1])
            self.c.start(ports = [0, 1], mult = "5%", duration = 3)
            time.sleep(0.1)
            used_cores = self.get_used_cores()
            assert used_cores == [i]
            self.c.stop(ports = [0, 1])

        #asserting that stats are cleared 
        time.sleep(1) # the stats are always the average of the last second, hence we wait a second to clear the stats.
        used_cores = self.get_used_cores()
        assert len(used_cores) == 0  #no cores are utilized

        # part 2
        stream_list = [STLStream(packet = self.pkt, mode = STLTXCont(), core_id = i) for i in range(0, num_cores, 3)]
        for stream in stream_list:
            self.c.add_streams(streams = stream, ports = [0, 1])
        self.c.start(ports = [0, 1], mult = "5%", duration = 3)
        time.sleep(0.1)
        used_cores = self.get_used_cores()
        assert used_cores == range(0, num_cores, 3)
        self.c.stop(ports = [0, 1])
    
        #asserting that stats are cleared 
        time.sleep(1) # the stats are always the average of the last second, hence we wait a second to clear the stats.
        used_cores = self.get_used_cores()
        assert len(used_cores) == 0  #no cores are utilized

        #part 3
        self.c.remove_all_streams(ports = [0, 1])
        s1 = STLStream(packet = self.pkt, mode = STLTXCont(), core_id = 0)
        s2 = STLStream(packet = self.pkt, mode = STLTXCont(), core_id = num_cores - 1)
        self.c.add_streams(streams = s1, ports = [0, 1])
        self.c.add_streams(streams = s2, ports = [0, 1])
        self.c.start(ports = [0, 1], mult = "5%", duration = 3)
        time.sleep(0.1)
        used_cores = self.get_used_cores()
        assert used_cores == [0, 6]
        self.c.stop(ports = [0, 1])
