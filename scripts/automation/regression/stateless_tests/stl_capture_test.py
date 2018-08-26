#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import pprint
import zmq
import threading
import time
import tempfile
import socket
from scapy.utils import RawPcapReader
from nose.tools import assert_raises

def ip2num (ip_str):
    return struct.unpack('>L', socket.inet_pton(socket.AF_INET, ip_str))[0]
    
def num2ip (ip_num):
    return socket.inet_ntoa(struct.pack('>L', ip_num))

def ip_add (ip_str, cnt):
    return num2ip(ip2num(ip_str) + cnt)
    
    
class STLCapture_Test(CStlGeneral_Test):
    """Tests for capture packets"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)

        if not self.is_loopback:
            self.skip('capture tests are skipped on a non-loopback machine')

        if self.is_linux_stack:
            self.skip('capture tests are skipped with linux-based stack')

        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        self.c.connect()
        self.c.reset(ports = [self.tx_port, self.rx_port])

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')

        self.percentage = 5 if self.is_virt_nics else 50

        # some setups (enic) might add VLAN always
        self.nic_adds_vlan = CTRexScenario.setup_name in ['trex11']
        self.hostname = socket.gethostname()

    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    def __compare_captures (self, tx_pkt_list, rx_pkt_list):
        # make sure we have the same binaries in both lists
        tx_pkt_list_bin = {pkt['binary'] for pkt in tx_pkt_list}
        rx_pkt_list_bin = {pkt['binary'] for pkt in rx_pkt_list}
        
        if tx_pkt_list_bin != rx_pkt_list_bin:
            # if the NIC does not add VLAN - a simple binary compare will do
            if not self.nic_adds_vlan:
                assert 0, "TX and RX captures do not match"
            
            # the NIC adds VLAN - compare IP level
            tx_pkt_list_ip = { bytes((Ether(pkt))['IP']) for pkt in tx_pkt_list_bin}
            rx_pkt_list_ip = { bytes((Ether(pkt))['IP']) for pkt in rx_pkt_list_bin}
            
            if tx_pkt_list_ip != rx_pkt_list_ip:
                assert 0, "TX and RX captures do not match"
        
            
    # a simple capture test - inject packets and see the packets arrived the same
    def test_basic_capture (self):
        pkt_count = 100
        
        try:
            # move to service mode
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])
            
            # start a capture
            txc = self.c.start_capture(tx_ports = self.tx_port, limit = pkt_count)
            rxc = self.c.start_capture(rx_ports = self.rx_port, limit = pkt_count)
            
            # inject few packets with a VM
            vm = STLScVmRaw( [STLVmFlowVar ( "ip_src",  min_value="16.0.0.0", max_value="16.255.255.255", size=4, step = 7, op = "inc"),
                              STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src"),
                              STLVmFixIpv4(offset = "IP")
                              ]
                             );
              
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example',
                                vm = vm)
            
            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXSingleBurst(total_pkts = pkt_count,
                                                       percentage = self.percentage)
                               )
            
            self.c.add_streams(ports = self.tx_port, streams = [stream])
            
            self.c.start(ports = self.tx_port, force = True)
            self.c.wait_on_traffic(ports = self.tx_port)
            
            tx_pkt_list = []
            
            self.c.stop_capture(txc['id'], output = tx_pkt_list)
            with tempfile.NamedTemporaryFile() as rx_pcap:
                with assert_raises(TRexError):
                    self.c.stop_capture(rxc['id'], output = '/tmp/asdfasdfqwerasdf/azasdfas') # should raise TRexError

                self.c.stop_capture(rxc['id'], output = rx_pcap.name)
                rx_pkt_list = [{'binary': pkt[0]} for pkt in RawPcapReader(rx_pcap.name)]
            
            assert (len(tx_pkt_list) == len(rx_pkt_list) == pkt_count)
            
            # make sure we have the same binaries in both lists
            self.__compare_captures(tx_pkt_list, rx_pkt_list)
            

            # generate all the values that should be
            expected_src_ips = {ip_add('16.0.0.0', i * 7) for i in range(pkt_count)}
            got_src_ips = {(Ether(pkt['binary']))['IP'].src for pkt in rx_pkt_list}
            
            if expected_src_ips != got_src_ips:
                assert 0, "recieved packets do not match expected packets"
                
            
        except STLError as e:
            assert False , '{0}'.format(e)
            
        finally:
            self.c.remove_all_captures()
            self.c.set_service_mode(ports = self.rx_port, enabled = False)

            
    
            
    # in this test we apply captures under traffic multiple times
    def test_stress_capture (self):
        pkt_count = 100
        
        try:
            # move to service mode
            self.c.set_service_mode(ports = self.rx_port)
            
            # start heavy traffic
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
            
            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXCont(percentage = self.percentage)
                               )
            
            self.c.add_streams(ports = self.tx_port, streams = [stream])
            self.c.start(ports = self.tx_port, force = True)
            captures = [{'capture_id': None, 'limit': 50}, {'capture_id': None, 'limit': 80}, {'capture_id': None, 'limit': 100}]
            
            for i in range(0, 100):
                # start a few captures
                for capture in captures:
                    capture['capture_id'] = self.c.start_capture(rx_ports = [self.rx_port], limit = capture['limit'])['id']
                
                # a little time to wait for captures to be full
                wait_iterations = 0
                while True:
                    server_captures = self.c.get_capture_status()
                    counts = ([c['count'] for c in server_captures.values()])
                    if {50, 80, 100} == set(counts):
                        break
                        
                    time.sleep(0.1)
                    wait_iterations += 1
                    assert(wait_iterations <= 5)
                    
                
                for capture in captures:
                    capture_id = capture['capture_id']
                    
                    # make sure the server registers us and we are full
                    assert(capture['capture_id'] in server_captures.keys())
                    assert(server_captures[capture_id]['count'] == capture['limit'])
                    
                    # fetch packets
                    pkt_list = []
                    self.c.stop_capture(capture['capture_id'], pkt_list)
                    assert (len(pkt_list) == capture['limit'])
                
                    # a little sanity per packet
                    for pkt in pkt_list:
                        scapy_pkt = Ether(pkt['binary'])
                        assert(scapy_pkt['IP'].src == '16.0.0.1')
                        assert(scapy_pkt['IP'].dst == '48.0.0.1')
              
        except STLError as e:
            assert False , '{0}'.format(e)
            
        finally:
            self.c.remove_all_captures()
            self.c.set_service_mode(ports = self.rx_port, enabled = False)
            
    
    # in this test we capture and analyze the ARP request / response
    def test_arp_capture (self):
        if self.c.get_port_attr(self.tx_port)['layer_mode'] != 'IPv4':
            return self.skip('skipping ARP capture test for non-ipv4 config on port {0}'.format(self.tx_port))
            
        if self.c.get_port_attr(self.rx_port)['layer_mode'] != 'IPv4':
            return self.skip('skipping ARP capture test for non-ipv4 config on port {0}'.format(self.rx_port))
            
        try:
            # move to service mode
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])
                                                                        
            # start a capture
            capture_info = self.c.start_capture(rx_ports = [self.tx_port, self.rx_port], limit = 2)
         
            # generate an ARP request
            self.c.arp(ports = self.tx_port)
            
            pkts = []
            self.c.stop_capture(capture_info['id'], output = pkts)
        
            assert len(pkts) == 2
            
            # find the correct order
            if pkts[0]['port'] == self.rx_port:
                request  = pkts[0]
                response = pkts[1]
            else:
                request  = pkts[1]
                response = pkts[0]
            
            assert request['port']  == self.rx_port
            assert response['port'] == self.tx_port
            
            arp_request, arp_response = Ether(request['binary']), Ether(response['binary'])
            assert 'ARP' in arp_request
            assert 'ARP' in arp_response
            
            assert arp_request['ARP'].op  == 1
            assert arp_response['ARP'].op == 2
            
            assert arp_request['ARP'].pdst == arp_response['ARP'].psrc
            
            
        except STLError as e:
            assert False , '{0}'.format(e)
            
        finally:
             self.c.remove_all_captures()
             self.c.set_service_mode(ports = [self.tx_port, self.rx_port], enabled = False)

             
    # test PING
    def test_ping_capture (self):
        if self.c.get_port_attr(self.tx_port)['layer_mode'] != 'IPv4':
            return self.skip('skipping ARP capture test for non-ipv4 config on port {0}'.format(self.tx_port))
            
        if self.c.get_port_attr(self.rx_port)['layer_mode'] != 'IPv4':
            return self.skip('skipping ARP capture test for non-ipv4 config on port {0}'.format(self.rx_port))
            
        try:
            # move to service mode
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])

            # start a capture
            capture_info = self.c.start_capture(rx_ports = [self.tx_port, self.rx_port], limit = 100)

            # generate an ARP request
            tx_ipv4 = self.c.get_port_attr(port = self.tx_port)['src_ipv4']
            rx_ipv4 = self.c.get_port_attr(port = self.rx_port)['src_ipv4']
            
            count = 50
            
            self.c.ping_ip(src_port = self.tx_port, dst_ip = rx_ipv4, pkt_size = 1500, count = count, interval_sec = 0.01)

            pkts = []
            self.c.stop_capture(capture_info['id'], output = pkts)

            req_pkts = [Ether(pkt['binary']) for pkt in pkts if pkt['port'] == self.rx_port]
            res_pkts = [Ether(pkt['binary']) for pkt in pkts if pkt['port'] == self.tx_port]
            assert len(req_pkts) == count
            assert len(res_pkts) == count
            
            for req_pkt in req_pkts:
                assert 'ICMP' in req_pkt, req_pkt.command()
                assert req_pkt['IP'].src == tx_ipv4
                assert req_pkt['IP'].dst == rx_ipv4
                assert req_pkt['ICMP'].type == 8
                assert len(req_pkt) == 1500
                
            for res_pkt in res_pkts:
                assert 'ICMP' in res_pkt, res_pkt.command()
                assert res_pkt['IP'].src == rx_ipv4
                assert res_pkt['IP'].dst == tx_ipv4
                assert res_pkt['ICMP'].type == 0
                assert len(res_pkt) == 1500

                
        except STLError as e:
            assert False , '{0}'.format(e)

        finally:
             self.c.remove_all_captures()
             self.c.set_service_mode(ports = [self.tx_port, self.rx_port], enabled = False)


             
             
    # in this test we stress TX & RX captures in parallel
    def test_stress_tx_rx (self):
        pkt_count = 100
        
        try:
            # move to service mode
            self.c.set_service_mode(ports = [self.rx_port, self.tx_port])
            
            # start heavy traffic
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/IP()/'a_payload_example')
            
            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXCont(percentage = self.percentage)
                               )
            
            self.c.add_streams(ports = self.tx_port, streams = [stream])
            self.c.start(ports = self.tx_port, mult = "50%", force = True)
            
            
            # start a capture on the RX port
            capture_rx = self.c.start_capture(rx_ports = self.rx_port, limit = 1000)
            
            # now under traffic start/stop the TX capture
            for i in range(0, 1000):
                # start a common capture
                capture_txrx = self.c.start_capture(rx_ports = self.rx_port, tx_ports = self.tx_port, limit = 1000)
                self.c.stop_capture(capture_txrx['id'])
                
              
        except STLError as e:
            assert False , '{0}'.format(e)
            
        finally:
            self.c.remove_all_captures()
            self.c.set_service_mode(ports = [self.rx_port, self.tx_port], enabled = False)

    def test_tx_from_capture_port (self):
        '''
            test TX packets from the RX core using capture port mechanism
        '''
        rx_capture_id = None

        # use explicit values for easy comparsion
        tx_src_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['src']
        tx_dst_mac = self.c.ports[self.tx_port].get_layer_cfg()['ether']['dst']


        try:
            self.c.set_service_mode(ports = [self.tx_port, self.rx_port])

            # VICs adds VLAN 0 on RX side
            max_capture_packet = 2000
            rx_capture_id = self.c.start_capture(rx_ports = self.rx_port, bpf_filter = 'udp or (vlan and udp)', limit = max_capture_packet)['id']

            # Add ZeroMQ Socket
            zmq_context = zmq.Context()
            zmq_socket = zmq_context.socket(zmq.PAIR)
            tcp_port = zmq_socket.bind_to_random_port('tcp://*')
            self.c.start_capture_port(port = self.tx_port, endpoint = 'tcp://%s:%s' % (self.hostname, tcp_port))

            self.c.clear_stats()

            nb_packets = 200000
            assert max_capture_packet <= nb_packets
            pkt = bytes(Ether(src=tx_src_mac,dst=tx_dst_mac)/IP()/UDP(sport = 100,dport=1000)/('x' * 100))
            for _ in range(1,nb_packets):
                zmq_socket.send(pkt)

            # Wait for 1 sec so that stats are accurate
            time.sleep(1)
            stats = self.stl_trex.get_stats()

            # check capture status with timeout
            timeout = PassiveTimer(2)
            while not timeout.has_expired():
                caps = self.c.get_capture_status()
                assert(len(caps) == 1)
                if caps[rx_capture_id]['count'] == max_capture_packet:
                    break
                time.sleep(0.1)

            assert abs(max_capture_packet-caps[rx_capture_id]['count']) / max_capture_packet < 0.05

            # RX capture
            rx_pkts = []
            self.c.stop_capture(rx_capture_id, output = rx_pkts)
            rx_capture_id = None

            rx_pkts = [x['binary'] for x in rx_pkts]

            # RX pkts are not the same - loose check, all here and are UDP
            assert abs(max_capture_packet-len(rx_pkts)) / max_capture_packet < 0.05
            assert (all(['UDP' in Ether(x) for x in rx_pkts]))

            # Report the number of pps we were able to send
            print('Done, %6s TX pps' % (round(stats[self.rx_port]['rx_pps'],2)))
        finally:
            self.c.remove_all_captures()
            self.c.stop_capture_port(port = self.tx_port)
            self.c.set_service_mode(ports = [self.rx_port, self.tx_port], enabled = False)
            zmq_context.destroy()


    def test_rx_from_capture_port (self):
        '''
            test RX packets from the RX core using capture port mechanism
        '''

        pkt_count = 500000

        try:
            # move to service mode
            self.c.set_service_mode(ports = self.rx_port)

            # Add ZeroMQ Socket for RX Port
            zmq_context = zmq.Context()
            zmq_socket = zmq_context.socket(zmq.PAIR)
            tcp_port = zmq_socket.bind_to_random_port('tcp://*')
            self.c.start_capture_port(port = self.rx_port, endpoint = 'tcp://%s:%s' % (self.hostname, tcp_port))

            # Start a thread to receive and count how many packet we receive
            global failed
            failed = False
            def run():
                try:
                    nb_received = 0
                    first_packet = 0
                    while not zmq_socket.closed:
                        pkt = zmq_socket.recv()
                        if pkt:
                            if not first_packet:
                                first_packet = time.time()
                                # Only check first packet as Scapy is slow for benchmarking..
                                scapy_pkt = Ether(pkt)
                                assert(scapy_pkt['IP'].src == '16.0.0.1')
                                assert(scapy_pkt['IP'].dst == '48.0.0.1')
                            nb_received += 1
                            if nb_received == pkt_count:
                                delta = time.time()-first_packet
                                print('Done (%ss), %6s RX pps' % (round(delta,2), round(nb_received/delta,2)))
                                return
                except Exception as e:
                    global failed
                    failed = True
                    raise e
            t = threading.Thread(name="capture_port_thread", target=run)
            t.daemon=True
            t.start()

            # start heavy traffic
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/'a_payload_example')

            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXCont(percentage = self.percentage)
                               )

            self.c.add_streams(ports = self.tx_port, streams = [stream])
            self.c.start(ports = self.tx_port, force = True)

            # Wait until we have received everything
            t.join(timeout=15)
            assert not t.is_alive()
            assert not failed

        finally:
            self.c.remove_all_captures()
            self.c.stop_capture_port(port = self.rx_port)
            self.c.set_service_mode(ports = [self.rx_port], enabled = False)
            self.c.stop()
            zmq_context.destroy()


    def test_rx_from_capture_port_with_filter (self):
        '''
            test RX packets from the RX core using capture port mechanism
            and BPF filter on the port
        '''

        pkt_count = 1000

        try:
            # move to service mode
            self.c.set_service_mode(ports = self.rx_port)

            # Add ZeroMQ Socket for RX Port
            zmq_context = zmq.Context()
            zmq_socket = zmq_context.socket(zmq.PAIR)
            tcp_port = zmq_socket.bind_to_random_port('tcp://*')
            # Start with wrong filter
            self.c.start_capture_port(port = self.rx_port, endpoint = 'tcp://%s:%s' % (self.hostname, tcp_port), bpf_filter="ip host 18.0.0.1")

            # Then change it
            self.c.set_capture_port_bpf_filter(port = self.rx_port, bpf_filter="udp port 1222")

            # Start a thread to receive and count how many packet we receive
            global failed
            failed = False
            def run():
                try:
                    nb_received = 0
                    first_packet = 0
                    while not zmq_socket.closed:
                        pkt = zmq_socket.recv()
                        if pkt:
                            if not first_packet:
                                first_packet = time.time()

                            scapy_pkt = Ether(pkt)
                            assert(scapy_pkt['UDP'].dport == 1222)
                            nb_received += 1
                            if nb_received == pkt_count:
                                delta = time.time()-first_packet
                                print('Done (%ss), %6s RX pps' % (round(delta,2), round(nb_received/delta,2)))
                                return
                except Exception as e:
                    global failed
                    failed = True
                    raise e
            t = threading.Thread(name="capture_port_thread", target=run)
            t.daemon=True
            t.start()

            # start heavy traffic with wrong IP first
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=1222,sport=1025)/'a_payload_example')

            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXSingleBurst(percentage = self.percentage, total_pkts=pkt_count)
                               )

            self.c.add_streams(ports = self.tx_port, streams = [stream])

            # then start traffic with correct IP
            pkt = STLPktBuilder(pkt = Ether()/IP(src="18.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/'a_payload_example')

            stream = STLStream(name = 'burst2',
                               packet = pkt,
                               mode = STLTXSingleBurst(percentage = self.percentage, total_pkts=pkt_count)
                               )

            self.c.add_streams(ports = self.tx_port, streams = [stream])
            self.c.start(ports = self.tx_port, force = True)

            # Wait until we have received everything
            t.join(timeout=15)
            assert not t.is_alive()
            assert not failed

        finally:
            self.c.remove_all_captures()
            self.c.stop_capture_port(port = self.rx_port)
            self.c.set_service_mode(ports = [self.rx_port], enabled = False)
            self.c.stop()
            zmq_context.destroy()

    def test_capture_port_stress (self):
        '''
            test RX & Tx packets from the RX core using capture port mechanism
            while start & stopping the capture port
        '''

        try:
            # move to service mode
            self.c.set_service_mode(ports = self.rx_port)

            # Add ZeroMQ Socket for RX Port
            zmq_context = zmq.Context()
            zmq_socket = zmq_context.socket(zmq.PAIR)
            zmq_socket.setsockopt(zmq.LINGER, 0)
            tcp_port = zmq_socket.bind_to_random_port('tcp://*')

            # Start a thread to receive and send packets
            def run_rx_tx():
                try:
                    while not zmq_socket.closed:
                        pkt = zmq_socket.recv()
                        # Send it back
                        zmq_socket.send(pkt)
                except zmq.ContextTerminated:
                    pass

            t = threading.Thread(name="capture_port_thread_rx", target=run_rx_tx)
            t.daemon=True
            t.start()

            # start heavy traffic
            pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/'a_payload_example')

            stream = STLStream(name = 'burst',
                               packet = pkt,
                               mode = STLTXCont(percentage = self.percentage)
                               )

            self.c.add_streams(ports = self.tx_port, streams = [stream])
            self.c.start(ports = self.tx_port, force = True)

            # Now start & stop the capture port while doing the work
            for _ in range(10):
                self.c.start_capture_port(port = self.rx_port, endpoint = 'tcp://%s:%s' % (self.hostname, tcp_port))
                time.sleep(1)
                self.c.stop_capture_port(port = self.rx_port)
                time.sleep(1)

            # Wait until thread dies
            zmq_context.destroy()
            t.join(timeout=5)
            assert not t.is_alive()


        finally:
            self.c.remove_all_captures()
            self.c.stop()
            zmq_context.destroy()
            self.c.set_service_mode(ports = [self.rx_port], enabled = False)
