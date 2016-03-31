#!/router/bin/python
from stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob


def get_error_in_percentage (golden, value):
    return abs(golden - value) / float(golden)

def get_stl_profiles ():
    profiles_path = os.path.join(CTRexScenario.scripts_path, 'stl/')
    profiles = glob.glob(profiles_path + "/*.py") + glob.glob(profiles_path + "yaml/*.yaml")

    return profiles


class STLClient_Test(CStlGeneral_Test):
    """Tests for stateless client"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        
        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        self.c.connect()
        self.c.reset(ports = [self.tx_port, self.rx_port])

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
        self.profiles = get_stl_profiles()


    @classmethod
    def tearDownClass(cls):
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


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
                                                   pps = 1000)
                           )

            for i in range(0, 5):
                self.c.add_streams([b1], ports = [self.tx_port, self.rx_port])

                self.c.clear_stats()
                self.c.start(ports = [self.tx_port, self.rx_port])

                assert self.c.ports[self.tx_port].is_transmitting(), 'port should be active'
                assert self.c.ports[self.rx_port].is_transmitting(), 'port should be active'

                self.c.wait_on_traffic(ports = [self.tx_port, self.rx_port])
                stats = self.c.get_stats()

                assert self.tx_port in stats
                assert self.rx_port in stats

                assert stats[self.tx_port]['opackets'] == 100
                assert stats[self.rx_port]['ipackets'] == 100

                assert stats[self.rx_port]['opackets'] == 100
                assert stats[self.tx_port]['ipackets'] == 100

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
                                                  pps = 1000)
                           )

            for i in range(0, 5):
                self.c.add_streams([b1], ports = [self.tx_port, self.rx_port])

                self.c.clear_stats()
                self.c.start(ports = [self.tx_port, self.rx_port])

                assert self.c.ports[self.tx_port].is_transmitting(), 'port should be active'
                assert self.c.ports[self.rx_port].is_transmitting(), 'port should be active'

                self.c.wait_on_traffic(ports = [self.tx_port, self.rx_port])
                stats = self.c.get_stats()

                assert self.tx_port in stats
                assert self.rx_port in stats

                assert stats[self.tx_port]['opackets'] == 200
                assert stats[self.rx_port]['ipackets'] == 200

                assert stats[self.rx_port]['opackets'] == 200
                assert stats[self.tx_port]['ipackets'] == 200

                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])



        except STLError as e:
            assert False , '{0}'.format(e)


    #
    def test_basic_cont (self):
        pps = 49182
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
                assert get_error_in_percentage(stats[self.tx_port]['opackets'], golden) < 0.05
                assert get_error_in_percentage(stats[self.rx_port]['ipackets'], golden) < 0.05

                assert get_error_in_percentage(stats[self.rx_port]['opackets'], golden) < 0.05
                assert get_error_in_percentage(stats[self.tx_port]['ipackets'], golden) < 0.05


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
                           mode = STLTXCont(percentage = 60))

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
            print("skipping for non loopback / virtual")
            return

        try:
            self.c.set_port_attr(ports = [self.tx_port, self.rx_port], promiscuous = True)

            for profile in self.profiles:
                print("now testing profile {0}...\n").format(profile)

                p1 = STLProfile.load(profile, port_id = self.tx_port)
                p2 = STLProfile.load(profile, port_id = self.rx_port)

                if p1.has_flow_stats():
                    print("profile needs RX caps - skipping...")
                    continue

                self.c.add_streams(p1, ports = self.tx_port)
                self.c.add_streams(p2, ports = self.rx_port)

                self.c.clear_stats()

                self.c.start(ports = [self.tx_port, self.rx_port], mult = "30%")
                time.sleep(100 / 1000.0)

                if p1.is_pauseable() and p2.is_pauseable():
                    self.c.pause(ports = [self.tx_port, self.rx_port])
                    time.sleep(100 / 1000.0)

                    self.c.resume(ports = [self.tx_port, self.rx_port])
                    time.sleep(100 / 1000.0)

                self.c.stop(ports = [self.tx_port, self.rx_port])

                stats = self.c.get_stats()

                assert self.tx_port in stats, '{0} - no stats for TX port'.format(profile)
                assert self.rx_port in stats, '{0} - no stats for RX port'.format(profile)

                assert stats[self.tx_port]['opackets'] == stats[self.rx_port]['ipackets'], '{0} - number of TX packets differ from RX packets'.format(profile)

                assert stats[self.rx_port]['opackets'] == stats[self.tx_port]['ipackets'], '{0} - number of TX packets differ from RX packets'.format(profile)

                self.c.remove_all_streams(ports = [self.tx_port, self.rx_port])

        except STLError as e:
            assert False , '{0}'.format(e)


        finally:
            self.c.set_port_attr(ports = [self.tx_port, self.rx_port], promiscuous = False)
