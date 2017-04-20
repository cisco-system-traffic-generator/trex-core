#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob


def get_error_in_percentage (golden, value):
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

        if self.is_virt_nics or CTRexScenario.setup_name == 'trex21':
            self.percentage = 5
            self.pps = 500
        else:
            self.percentage = 50
            self.pps = 50000
        
        # strict mode is only for 'wire only' connection
        if self.is_loopback and not (self.is_virt_nics or CTRexScenario.setup_name == 'trex21'):
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
        if  self.drv_name == 'net_mlx5':
            print("WARNING disable strict due to trex-325 on mlx5")
            self.strict = False


        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        self.profiles = get_stl_profiles()


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
            assert get_error_in_percentage(expected, got) < 0.05


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

                assert self.tx_port in stats
                assert self.rx_port in stats

                # cont. with duration should be quite percise - 5% error is relaxed enough

                assert get_error_in_percentage(golden, stats[self.tx_port]['opackets']) < 0.05
                assert get_error_in_percentage(golden, stats[self.rx_port]['ipackets']) < 0.05

                assert get_error_in_percentage(golden, stats[self.rx_port]['opackets']) < 0.05
                assert get_error_in_percentage(golden, stats[self.tx_port]['ipackets']) < 0.05


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
        if self.is_virt_nics or not self.is_loopback:
            self.skip('skipping profile tests for virtual / non loopback')
            return

        default_mult  = self.get_benchmark_param('mult',default="30%")
        skip_tests     = self.get_benchmark_param('skip',default=[])

        try:
            for profile in self.profiles:
                print('\nProfile: %s' % profile[len(CTRexScenario.scripts_path):]);

                skip = False
                for skip_test in skip_tests:
                    if skip_test in profile:
                        skip = True
                        break
                if skip:
                    print('  * Skip due to config file...')
                    continue

                p1 = STLProfile.load(profile, port_id = self.tx_port)
                p2 = STLProfile.load(profile, port_id = self.rx_port)

                # if profile contains custom MAC addrs we need promiscuous mode
                # but virtual NICs does not support promiscuous mode
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

            self.verify(stats[master]['opackets'], 52)
            self.verify(stats[slave]['opackets'], 48)

        except STLError as e:
            assert False , '{0}'.format(e)

