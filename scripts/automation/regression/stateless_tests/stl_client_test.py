#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob
from nose.tools import nottest

def get_error_in_percentage (golden, value):
    if (golden==0):
        return(0.0);
    return abs(golden - value) / float(golden)

def get_stl_profiles ():
    profiles_path = os.path.join(CTRexScenario.scripts_path, 'stl/')
    py_profiles = glob.glob(profiles_path + "/*.py")
    yaml_profiles = glob.glob(profiles_path + "yaml/*.yaml")
    return py_profiles + yaml_profiles


class STLClient_Test(CStlGeneral_Test):
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
                return;
            else:
                if get_error_in_percentage(expected, got) < 0.05 :
                    return
                print(' ERROR verify expected: %d  got:%d ' % (expected,got) )
                assert(0);



    def test_basic_connect_disconnect (self):
        try:
            self.c.connect()
            assert self.c.is_connected(), 'client should be connected'
            self.c.disconnect()
            assert not self.c.is_connected(), 'client should be disconnected'

        except STLError as e:
            assert False , '{0}'.format(e)
  

    def test_basic_single_burst (self):

        try:                              
            b1 = STLStream(name = 'burst',
                           packet = self.pkt,
                           mode = STLTXSingleBurst(total_pkts = 100,
                                                   percentage = self.percentage)
                           )

            for i in range(0, 5):
                self.c.add_streams([b1], ports = [self.tx_port, self.rx_port])

                self.c.clear_stats()
                self.c.start(ports = [self.tx_port, self.rx_port])

                self.c.wait_on_traffic(ports = [self.tx_port, self.rx_port])
                stats = self.c.get_stats()

                assert self.tx_port in stats
                assert self.rx_port in stats

                self.verify(100, stats[self.tx_port]['opackets'])
                self.verify(100, stats[self.rx_port]['ipackets'])

                self.verify(100, stats[self.rx_port]['opackets'])
                self.verify(100, stats[self.tx_port]['ipackets'])


                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])



        except STLError as e:
            assert False , '{0}'.format(e)


    #
    def test_basic_multi_burst (self):
        try:
            b1 = STLStream(name = 'burst',
                           packet = self.pkt,
                           mode = STLTXMultiBurst(pkts_per_burst = 10,
                                                  count =  20,
                                                  percentage = self.percentage)
                           )

            for i in range(0, 5):
                self.c.add_streams([b1], ports = [self.tx_port, self.rx_port])

                self.c.clear_stats()
                self.c.start(ports = [self.tx_port, self.rx_port])

                self.c.wait_on_traffic(ports = [self.tx_port, self.rx_port])
                stats = self.c.get_stats()

                assert self.tx_port in stats
                assert self.rx_port in stats

                self.verify(200, stats[self.tx_port]['opackets'])
                self.verify(200, stats[self.rx_port]['ipackets'])

                self.verify(200, stats[self.rx_port]['opackets'])
                self.verify(200, stats[self.tx_port]['ipackets'])

                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])



        except STLError as e:
            assert False , '{0}'.format(e)


    #
    def test_basic_cont (self):
        pps = self.pps
        duration = 0.1
        golden = pps * duration

        try:
            b1 = STLStream(name = 'burst',
                           packet = self.pkt,
                           mode = STLTXCont(pps = pps)
                           )

            for i in range(0, 5):
                self.c.add_streams([b1], ports = [self.tx_port, self.rx_port])

                self.c.clear_stats()
                self.c.start(ports = [self.tx_port, self.rx_port], duration = duration)

                assert self.c.ports[self.tx_port].is_transmitting(), 'port should be active'
                assert self.c.ports[self.rx_port].is_transmitting(), 'port should be active'

                self.c.wait_on_traffic(ports = [self.tx_port, self.rx_port])
                stats = self.c.get_stats()

                assert self.tx_port in stats, 'tx port not in stats'
                assert self.rx_port in stats, 'rx port not in stats'

                # cont. with duration should be quite percise - 5% error is relaxed enough
                check_params = (
                    stats[self.tx_port]['opackets'],
                    stats[self.rx_port]['opackets'],
                    stats[self.tx_port]['ipackets'],
                    stats[self.rx_port]['ipackets'],
                    )
                for param in check_params:
                    assert get_error_in_percentage(golden, param) < 0.05, 'golden: %s, got: %s' % (golden, param)

                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])



        except STLError as e:
            assert False , '{0}'.format(e)


    def test_stress_connect_disconnect (self):
        try:
            for i in range(0, 100):
                self.c.connect()
                assert self.c.is_connected(), 'client should be connected'
                self.c.disconnect()
                assert not self.c.is_connected(), 'client should be disconnected'


        except STLError as e:
            assert False , '{0}'.format(e)


    def pause_resume_update_streams_iteration(self, delay, expected_pps):
        self.c.clear_stats(clear_flow_stats = False, clear_latency_stats = False, clear_xstats = False)
        time.sleep(delay)
        tx_pps = self.c.get_stats(ports = [0])[0]['opackets'] / delay
        assert (expected_pps * 0.9 < tx_pps < expected_pps * 1.1), 'expected TX ~%spps, got: %s' % (expected_pps, tx_pps)

    def test_pause_resume_update_streams(self):
        self.c.reset()
        s1 = STLStream(mode = STLTXSingleBurst(pps = 100, total_pkts = 9999))
        s2 = STLStream(mode = STLTXCont(pps = 100))
        s3 = STLStream(mode = STLTXCont(pps = 100))
        s4 = STLStream(mode = STLTXCont(pps = 100), flow_stats = STLFlowLatencyStats(pg_id = 1))

        s1_id, s2_id, s3_id, s4_id = self.c.add_streams([s1, s2, s3, s4], ports = [0])
        self.c.start(ports = [0])

        with self.assertRaises(STLError): # burst present => error
            self.c.pause_streams(port = 0, stream_ids = [s3_id])

        with self.assertRaises(STLError): # several ports => error
            self.c.pause_streams(port = [0, 1], stream_ids = [s3_id])

        self.c.stop(ports = [0])
        self.c.remove_streams([s1_id], ports = [0]) # get rid of burst
        self.c.start(ports = [0])

        self.c.update_streams(port = 0, mult = '10kpps', stream_ids = [s3_id, s4_id]) # latency is not affected
        self.pause_resume_update_streams_iteration(delay = 5, expected_pps = 10200)

        self.c.update_streams(port = 0, mult = '100pps', stream_ids = [s3_id])
        self.c.pause_streams(port = 0, stream_ids = [s3_id])
        self.pause_resume_update_streams_iteration(delay = 5, expected_pps = 200) # paused stream not transmitting

        self.c.resume_streams(port = 0, stream_ids = [s3_id])
        self.pause_resume_update_streams_iteration(delay = 5, expected_pps = 300) # resume the paused


    def test_stress_tx (self):
        try:
            s1 = STLStream(name = 'stress',
                           packet = self.pkt,
                           mode = STLTXCont(percentage = self.percentage))

            # add both streams to ports
            self.c.add_streams([s1], ports = [self.tx_port, self.rx_port])
            for i in range(0, 100):

                self.c.start(ports = [self.tx_port, self.rx_port])

                assert self.c.ports[self.tx_port].is_transmitting(), 'port should be active'
                assert self.c.ports[self.rx_port].is_transmitting(), 'port should be active'

                self.c.pause(ports = [self.tx_port, self.rx_port])

                assert self.c.ports[self.tx_port].is_paused(), 'port should be paused'
                assert self.c.ports[self.rx_port].is_paused(), 'port should be paused'

                self.c.resume(ports = [self.tx_port, self.rx_port])

                assert self.c.ports[self.tx_port].is_transmitting(), 'port should be active'
                assert self.c.ports[self.rx_port].is_transmitting(), 'port should be active'

                self.c.stop(ports = [self.tx_port, self.rx_port])

                assert not self.c.ports[self.tx_port].is_active(), 'port should be idle'
                assert not self.c.ports[self.rx_port].is_active(), 'port should be idle'

        except STLError as e:
            assert False , '{0}'.format(e)


    def test_all_profiles (self):
        #Work around for trex-405. Remove when it is resolved
        if  self.drv_name == 'net_mlx5' and 'VM' in self.modes:
            self.skip('Can not run on mlx VM currently - see trex-405 for details')

        if self.is_virt_nics or not self.is_loopback:
            self.skip('skipping profile tests for virtual / non loopback')
            return

        default_mult  = self.get_benchmark_param('mult',default="30%")
        skip_tests_per_setup     = self.get_benchmark_param('skip',default=[])
        skip_tests_global = ['imix_wlc.py']

        try:
            for profile in self.profiles:
                print('\nProfile: %s' % profile[len(CTRexScenario.scripts_path):]);

                skip = False
                for skip_test in skip_tests_per_setup:
                    if skip_test in profile:
                        skip = True
                        break
                if skip:
                    print('  * Skip due to config file...')
                    continue

                for skip_test in skip_tests_global:
                    if skip_test in profile:
                        skip = True
                        break
                if skip:
                    print('  * Skip due to global ignore of this profile...')
                    continue

                p1 = STLProfile.load(profile, port_id = self.tx_port)
                p2 = STLProfile.load(profile, port_id = self.rx_port)

                # if profile contains custom MAC addrs we need promiscuous mode
                # but virtual NICs does not support promiscuous mode
                if not self.is_vf_nics:
                    self.c.set_port_attr(ports = [self.tx_port, self.rx_port], promiscuous = False)

                if p1.has_custom_mac_addr() or p2.has_custom_mac_addr():
                    if self.is_virt_nics:
                        print("  * Skip due to Virtual NICs and promiscuous mode requirement...")
                        continue
                    elif self.is_vf_nics:
                        print("  * Skip due to VF NICs and promiscuous mode requirement...")
                        continue
                    else:
                        self.c.set_port_attr(ports = [self.tx_port, self.rx_port], promiscuous = True)

                if p1.has_flow_stats() or p2.has_flow_stats():
                    print("  * Skip due to RX caps requirement")
                    continue

                self.c.add_streams(p1, ports = self.tx_port)
                self.c.add_streams(p2, ports = self.rx_port)

                self.c.clear_stats()

                self.c.start(ports = [self.tx_port, self.rx_port], mult = default_mult)
                time.sleep(0.1)

                if p1.is_pauseable() and p2.is_pauseable():
                    self.c.pause(ports = [self.tx_port, self.rx_port])
                    time.sleep(0.1)

                    self.c.resume(ports = [self.tx_port, self.rx_port])
                    time.sleep(0.1)

                self.c.stop(ports = [self.tx_port, self.rx_port])

                stats = self.c.get_stats()

                assert self.tx_port in stats, '{0} - no stats for TX port'.format(profile)
                assert self.rx_port in stats, '{0} - no stats for RX port'.format(profile)

                self.verify(stats[self.tx_port]['opackets'], stats[self.rx_port]['ipackets'])
                self.verify(stats[self.rx_port]['opackets'], stats[self.tx_port]['ipackets'])

                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])

        except STLError as e:
            assert False , '{0}'.format(e)


        finally:
            if not self.is_vf_nics:
                self.c.set_port_attr(ports = [self.tx_port, self.rx_port], promiscuous = False)


    # see https://trex-tgn.cisco.com/youtrack/issue/trex-226
    def test_latency_pause_resume (self):

        try:    
                                      
            s1 = STLStream(name = 'latency',
                           packet = self.pkt,
                           mode = STLTXCont(percentage = self.percentage),
                           flow_stats = STLFlowLatencyStats(pg_id = 1))

            self.c.add_streams([s1], ports = self.tx_port)

            self.c.clear_stats()

            self.c.start(ports = self.tx_port)

            for i in range(100):
                self.c.pause()
                self.c.resume()

            self.c.stop()

        except STLError as e:
            assert False , '{0}'.format(e)


    def test_pcap_remote (self):
        try:
            pcap_file = os.path.join(CTRexScenario.scripts_path, 'automation/regression/test_pcaps/pcap_dual_test.erf')

            master = self.tx_port
            slave = master ^ 0x1

            self.c.reset(ports = [master, slave])
            self.c.clear_stats()
            self.c.push_remote(pcap_file,
                               ports = [master],
                               ipg_usec = 100,
                               is_dual = True)
            self.c.wait_on_traffic(ports = [master])

            stats = self.c.get_stats()
            self.verify(52, stats[master]['opackets'])
            self.verify(48, stats[slave]['opackets'])


        except STLError as e:
            if not self.is_dummy_ports:
                assert False , '{0}'.format(e)
        else:
            if self.is_dummy_ports and (master not in self.c.ports or slave not in self.c.ports): # negative test
                raise Exception('Should have raised exception with dummy port')

        
    def test_tx_from_rx (self):
        '''
            test TX packets from the RX core
        '''
        tx_capture_id = None
        rx_capture_id = None
        
        # use explicit values for easy comparsion
        tx_src_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['src']
        tx_dst_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['dst']
        
        
        try:
            # add some background traffic (TCP)
            s1 = STLStream(name = 'burst', packet = STLPktBuilder(Ether()/IP()/TCP()), mode = STLTXCont())
            self.c.add_streams(ports = [self.tx_port, self.rx_port], streams = [s1])
            self.c.start(ports = [self.tx_port, self.rx_port], mult = "5kpps")
            
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])
            
            # VICs adds VLAN 0 on RX side
            tx_capture_id = self.c.start_capture(tx_ports = self.tx_port, bpf_filter = 'udp')['id']
            rx_capture_id = self.c.start_capture(rx_ports = self.rx_port, bpf_filter = 'udp or (vlan and udp)')['id']
            
            pkts = [bytes(Ether(src=tx_src_mac,dst=tx_dst_mac)/IP()/UDP(sport = x,dport=1000)/('x' * 100)) for x in range(50000,50500)]
            self.c.push_packets(pkts, ports = self.tx_port, ipg_usec = 1e6 / self.pps)
            
            # check capture status with timeout
            timeout = PassiveTimer(2)
            
            while not timeout.has_expired():
                caps = self.c.get_capture_status()
                assert(len(caps) == 2)
                if (caps[tx_capture_id]['count'] == len(pkts)) and (caps[rx_capture_id]['count'] == len(pkts)):
                    break
                    
                time.sleep(0.1)

            assert(caps[tx_capture_id]['count'] == len(pkts))
            self.verify(len(pkts), caps[rx_capture_id]['count'])
            
            # TX capture
            tx_pkts = []
            self.c.stop_capture(tx_capture_id, output = tx_pkts)
            tx_capture_id = None
                
            # RX capture
            rx_pkts = []
            self.c.stop_capture(rx_capture_id, output = rx_pkts)
            rx_capture_id = None
            
            tx_pkts = [x['binary'] for x in tx_pkts]
            rx_pkts = [x['binary'] for x in rx_pkts]
            
            # TX pkts should be the same
            assert(set(pkts) == set(tx_pkts))
            
            # RX pkts are not the same - loose check, all here and are UDP
            self.verify(len(pkts), len(rx_pkts))
            assert (all(['UDP' in Ether(x) for x in rx_pkts]))
            
            
            
        except STLError as e:
            # cleanup if needed
            if tx_capture_id:
                self.c.stop_capture(tx_capture_id)
                
            if rx_capture_id:
                self.c.stop_capture(rx_capture_id)
                
            assert False , '{0}'.format(e)
            
        finally:
            self.cleanup()
            
            
    def test_bpf (self):
        '''
            test BPF filters
        '''
        tx_capture_id = None
        rx_capture_id = None
        
        # use explicit values for easy comparsion
        tx_src_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['src']
        tx_dst_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['dst']
        
        try:
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])

            # VICs adds VLAN 0 tagging
            bpf_filter = "udp and src portrange 1-250"
            bpf_filter = '{0} or (vlan and {0})'.format(bpf_filter)
            
            tx_capture_id = self.c.start_capture(tx_ports = self.tx_port, bpf_filter = bpf_filter)['id']
            rx_capture_id = self.c.start_capture(rx_ports = self.rx_port, bpf_filter = bpf_filter)['id']
           
            self.c.clear_stats(ports = self.tx_port)

            # real
            pkts = [bytes(Ether(src=tx_src_mac,dst=tx_dst_mac)/IP()/UDP(sport = x)/('x' * 100)) for x in range(500)]
            self.c.push_packets(pkts, ports = self.tx_port, ipg_usec = 1e6 / self.pps)
            
            # noise
            pkts = [bytes(Ether(src=tx_src_mac,dst=tx_dst_mac)/IP()/TCP(sport = x)/('x' * 100)) for x in range(500)]
            self.c.push_packets(pkts, ports = self.tx_port, ipg_usec = 1e6 / self.pps)
            
            # check capture status with timeout
            timeout = PassiveTimer(2)
            
            while not timeout.has_expired():
                opackets = self.c.get_stats(ports = self.tx_port)[self.tx_port]['opackets']
                if (opackets >= 1000):
                    break
                    
                time.sleep(0.1)
        
            # make sure
            caps = self.c.get_capture_status()
            assert(len(caps) == 2)
            assert(caps[tx_capture_id]['count'] == 250)
            assert(caps[rx_capture_id]['count'] == 250)
            
            
            
        except STLError as e:
            assert False , '{0}'.format(e)
            
        finally:
            # cleanup if needed
            if tx_capture_id:
                self.c.stop_capture(tx_capture_id)
                
            if rx_capture_id:
                self.c.stop_capture(rx_capture_id)
                
            self.cleanup()


    # tests core pinning with latency
    def show_cpu_usage (self):
        cpu_stats = [x['ports'] for x in self.c.get_util_stats()['cpu']]

        print('')
        for i, cpu in enumerate(cpu_stats):
            cpu = [x for x in cpu if x != -1]
            if cpu:
                print('core {0}: {1}'.format(i, cpu))

        print('')

    def get_cpu_usage (self):
        cpu_stats = [x['ports'] for x in self.c.get_util_stats()['cpu'] if x['ports'] != [-1, -1]]
        return cpu_stats


    def test_core_pinning (self):

        if self.c.get_server_system_info().get('dp_core_count_per_port') < 2:
            self.skip('pinning test requires at least 2 cores per interface')
        if self.is_dummy_ports:
            self.skip('pinning test is not designed for setup with dummy ports')

        s1 = STLStream(packet = self.pkt, mode = STLTXCont())
        self.c.reset([0, 1])

        try:
            self.c.add_streams(ports = [0, 1], streams = s1)

            # split mode
            self.c.start(ports = [0, 1], core_mask = STLClient.CORE_MASK_SPLIT)
            time.sleep(0.1)

            cpu_stats = self.get_cpu_usage()

            # make sure all cores operate on both ports
            assert all([stat == [0, 1] for stat in cpu_stats])

            self.c.stop(ports = [0, 1])

            # pin mode
            self.c.start(ports = [0, 1], core_mask = STLClient.CORE_MASK_PIN)
            time.sleep(0.1)

            cpu_stats = self.get_cpu_usage()

            # make sure cores were splitted equally
            if ( (len(cpu_stats) % 2) == 0):
                assert cpu_stats.count([0, -1]) == cpu_stats.count([-1, 1])
            else:
                assert abs(cpu_stats.count([0, -1]) - cpu_stats.count([-1, 1])) == 1

            self.c.stop(ports = [0, 1])


        except STLError as e:
            assert False , '{0}'.format(e)


    # check pinning with latency
    def test_core_pinning_latency (self):

        if self.c.get_server_system_info().get('dp_core_count_per_port') < 2:
            self.skip('pinning test requires at least 2 cores per interface')
        if self.is_dummy_ports:
            self.skip('pinning test is not designed for setup with dummy ports')

        s1 = STLStream(packet = self.pkt, mode = STLTXCont())

        l1 = STLStream(packet = self.pkt, mode = STLTXCont(), flow_stats = STLFlowLatencyStats(pg_id = 3))
        l2 = STLStream(packet = self.pkt, mode = STLTXCont(), flow_stats = STLFlowLatencyStats(pg_id = 4))

        self.c.reset([0, 1])

        try:
            self.c.add_streams(ports = 0, streams = [s1, l1])
            self.c.add_streams(ports = 1, streams = [s1, l2])

            # split mode
            self.c.start(ports = [0, 1], core_mask = STLClient.CORE_MASK_SPLIT)
            time.sleep(0.1)

            cpu_stats = self.get_cpu_usage()

            # make sure all cores operate on both ports
            assert all([stat == [0, 1] for stat in cpu_stats])

            self.c.stop(ports = [0, 1])

            # pin mode
            self.c.start(ports = [0, 1], core_mask = STLClient.CORE_MASK_PIN)
            time.sleep(0.1)

            # for pin mode with latency core 0 should opreate on both 
            cpu_stats = self.get_cpu_usage()

            # core 0 should be associated with both
            core_0 = cpu_stats.pop(0)
            assert (core_0 == [0, 1])

            # make sure cores were splitted equally
            if ( (len(cpu_stats) % 2) == 0):
                assert cpu_stats.count([0, -1]) == cpu_stats.count([-1, 1])
            else:
                assert abs(cpu_stats.count([0, -1]) - cpu_stats.count([-1, 1])) == 1

            self.c.stop(ports = [0, 1])


        except STLError as e:
            assert False , '{0}'.format(e)

