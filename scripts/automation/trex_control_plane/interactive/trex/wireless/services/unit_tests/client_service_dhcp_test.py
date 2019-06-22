import sys
import os
import unittest
import time
import logging
import queue
from unittest.mock import patch

from collections import deque


from wireless.trex_wireless_client_state import ClientState
from wireless.services.client.client_service_dhcp import ClientServiceDHCP
from  wireless.services.wireless_service_dhcp import DHCP as _DHCP
from  wireless.services.wireless_service_dhcp import Dot11DHCPParser
from wireless.trex_wireless_ap import *
from wireless.trex_wireless_client import *
from scapy.all import *
from scapy.layers.dhcp import BOOTP, DHCP
from mocks import _Connection, _Rx_store, _worker, _env, _pubsub

from wireless.trex_wireless_config import *
global config
config = load_config()


@patch("wireless.services.client.client_service.ClientService.async_request_start", lambda x,first_start: None)
@patch("wireless.services.client.client_service.ClientService.async_request_stop", lambda x, done, success: None)
class ClientServiceDHCP_test(unittest.TestCase):
    """Test methods for ClientServiceDHCP."""
    
    def setUp(self):
        self.worker = _worker()  # mock worker
        self.ap = AP(worker=self.worker, ssl_ctx=SSL_Context(), port_layer_cfg=None, port_id=0,
                     mac="aa:bb:cc:dd:ee:ff", ip="9.9.9.9", port=5555, radio_mac="aa:bb:cc:dd:ee:00", wlc_ip="1.1.1.1", gateway_ip="2.2.2.2")
        self.client = APClient(self.worker, "aa:aa:aa:aa:aa:aa", None, self.ap)
        self.client.state = ClientState.RUN
        self.env = _env()
        self.tx_conn = _Connection() # mock of a multiprocessing.Connection with 'send' and 'recv" methods
        self.rx_store = _Rx_store()

        self.offer_template = (
            Dot11(FCfield='to-DS',
                subtype=8,
                type='Data',
                ID=0,
                addr1="ff:ff:ff:ff:ff:ff",
                addr2="ff:ff:ff:ff:ff:ff",
                addr3="ff:ff:ff:ff:ff:ff",
            ) / 
            Dot11QoS() /    
            LLC(dsap=170, ssap=170, ctrl=3) / # aa, aa, 3
            SNAP(code=2048) / # ethertype : ipv4
            IP(src="0.0.0.0",dst="42.42.42.42") / UDP(sport=67,dport=68) / 
            BOOTP(chaddr=b'123456',xid=42,yiaddr='42.42.42.42') / 
            DHCP(options=[("message-type","offer"), ("server_id", "1.2.3.4"), "end"])
        )

        self.ack_template = (
            Dot11(FCfield='to-DS',
                subtype=8,
                type='Data',
                ID=0,
                addr1="ff:ff:ff:ff:ff:ff",
                addr2="ff:ff:ff:ff:ff:ff",
                addr3="ff:ff:ff:ff:ff:ff",
            ) / 
            Dot11QoS() /    
            LLC(dsap=170, ssap=170, ctrl=3) / # aa, aa, 3
            SNAP(code=2048) / # ethertype : ipv4
            IP(src="0.0.0.0",dst="42.42.42.42") / UDP(sport=67,dport=68) / 
            BOOTP(chaddr=b'123456',xid=42,yiaddr='42.42.42.42') / 
            DHCP(options=[("message-type", "ack"), "end"])
        )


    def test_timeout(self):
        """Test the timeout and exponential bachkoff of the Service.
        Should be slot_time^(nb_retries + 1) + uniform_rand(-1,1) with a maximum number of retries after wich the timeout does not increase.
        """
        
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        
        slot_time = ClientServiceDHCP.SLOT_TIME
        max_retries = ClientServiceDHCP.MAX_RETRIES

        # expected waiting times for the first timeouts
        expected_times = [pow(slot_time, i+1) for i in range(max_retries)]
        
        for i in range(max_retries):
            wait_time = service.wait_time
            expected = expected_times[i]
            self.assertTrue(wait_time >= expected - 1 and wait_time <= expected + 1)

            service.timeout()

        # after MAX_RETRIES have been done, the waiting time should be reset
        expected_times = [pow(slot_time, i+1) for i in range(max_retries)]

        for i in range(max_retries):
            wait_time = service.wait_time
            expected = expected_times[i]
            self.assertTrue(wait_time >= expected - 1 and wait_time <= expected + 1)

            service.timeout()



    def test_with_ip(self):
        """Test of ClientServiceDHCP when an IP is already assigned to the client.
        The service should return directly and not send any packet in the connection.
        """

        self.client.ip = '1.2.3.4'

        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        gen = service.run()

        # the service should return immediately
        with self.assertRaises(StopIteration):
            next(gen)


    def test_init_state(self):
        """Test the ClientServiceDHCP in the DHCP.INIT state.
        While in this state, the service should send one DHCPDISCOVER.
        """

        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store

        gen = service.run()
        next(gen)  # launch the service

        next(gen)

        # check that a packet has been sent
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)

        # take this packet
        pkt = Dot11(self.tx_conn.tx_packets.get())
        # check that this is a DHCP packet
        self.assertTrue(DHCP in pkt)
        # check that is is a DHCPDISCOVER packet
        self.assertTrue(('message-type', 1) in pkt[DHCP].options)

        self.assertEqual(service.state, _DHCP.SELECTING)


    def test_selecting_state_no_dhcpoffer(self):
        """Test the ClientServiceDHCP in the DHCP.SELECTING state when there is no DHCPPOFFER received.
        While in this state, the service should wait for packets, and reply to one if any with a DHCPOFFER.
        """

        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.state = _DHCP.SELECTING

        gen = service.run()
    
        next(gen) # start the service

        # first action in SELECTING is to wait for DHCPOFFERS
        next(gen)

        # no packets are given : should timeout and go back to init, send the packet and go to SELECTING
        next(gen)
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)
        self.assertEqual(service.state, _DHCP.SELECTING)
        self.assertEqual(service.retries, 1)


    def test_selecting_state_dhcpoffer(self):
        """Test the ClientServiceDHCP in the DHCP.SELECTING state when there is one DHCPPOFFER received.
        While in this state, the service should wait for packets, and reply to one if any with a DHCPOFFER.
        """
    
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.xid = 42
        service.state = _DHCP.SELECTING
        gen = service.run()

        next(gen) # start the service

        # give one DHCPOFFER to the service
        offer = self.offer_template


        # first action in SELECTING is to wait for DHCPOFFERS
        next(gen)

        self.assertEqual(service.state, _DHCP.SELECTING)
        # send the offer
        gen.send([bytes(offer)])

        # next action is to select one DHCPOFFER, send a response and wait for DHCPACK
        self.assertEqual(service.state, _DHCP.REQUESTING)
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)

    def test_selecting_state_mutliple_dhcpoffers(self):
        """Test the ClientServiceDHCP in the DHCP.SELECTING state when there is multiple (3 in this case) DHCPPOFFER received.
        While in this state, the service should wait for packets, and reply to one if any with a DHCPOFFER.
        """
    
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.xid = 42
        service.state = _DHCP.SELECTING
        gen = service.run()
        next(gen) # start the service


        # give 3 DHCPOFFERs to the service
        offer1 = self.offer_template
        offer2 = offer1.copy()
        offer2[IP].dst = "41.41.41.41"
        offer2[BOOTP].yiaddr = "41.41.41.41"
        offer3 = offer1.copy()
        offer3[IP].dst = "43.43.43.43"
        offer3[BOOTP].yiaddr = "43.43.43.43"
        offers = [offer1, offer2, offer3]

        # first action in SELECTING is to wait for DHCPOFFERS
        next(gen)
        
        self.assertEqual(service.state, _DHCP.SELECTING)
        # send the offer``
        gen.send([bytes(offer) for offer in offers])

        # next action is to select one DHCPOFFER, send a response and wait for DHCPACK
        self.assertEqual(service.state, _DHCP.REQUESTING)
        self.assertEqual(self.tx_conn.tx_packets.qsize(), 1)


    def test_requesting_state_no_ack(self):
        """Test the ClientServiceDHCP in the DHCP.REQUESTING state when there is no DHCPACK (not DHCPNACK) received.
        While in this state, the service should wait for DHCPACKS, and timeout.
        """
    
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.xid = 42
        service.state = _DHCP.REQUESTING
        gen = service.run()

        next(gen) # start the service

        # first action should be to wait for packets
        next(gen)

        # no packets are sent
        gen.send([])
        # next action should be to timeout and rollback
        self.assertEqual(service.retries, 1)
        self.assertEqual(service.state, _DHCP.SELECTING)


    def test_requesting_state_ack(self):
        """Test the ClientServiceDHCP in the DHCP.REQUESTING state when there is one DHCPACK received (expected behaviour).
        While in this state, the service should wait for DHCPACKS, and if any, finish.
        """
    
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.xid = 42
        service.state = _DHCP.REQUESTING

        gen = service.run()

        service.offer = Dot11DHCPParser().parse(bytes(self.offer_template)) # set receveived offer

        ack = self.ack_template

        next(gen) # start the service

        # first action should be to wait for packets
        next(gen)

        # one ack is sent, next action should be to stop the service (dhcp is done)
        gen.send([bytes(ack)])

        with self.assertRaises(StopIteration):
            next(gen)

        # check that the service stopped while setting the ip
        self.assertEqual(service.client.ip, '42.42.42.42')

    def test_requesting_state_multiple_acks(self):
        """Test the ClientServiceDHCP in the DHCP.REQUESTING state when there is multiple (here 3) DHCPACK received (non expected behaviour).
        While in this state, the service should wait for DHCPACKS, select one and finish the service with correct ip.
        """
    
        service = ClientServiceDHCP(self.client, self.env, tx_conn=self.tx_conn, topics_to_subs={}, done_event=None)
        service.rx_store = self.rx_store
        service.xid = 42
        service.state = _DHCP.REQUESTING
        gen = service.run()

        service.offer = Dot11DHCPParser().parse(bytes(self.offer_template)) # set receveived offer

        ack1 = self.ack_template
        ack2 = ack1.copy()
        ack2[IP].dst = "41.41.41.41"
        ack2[BOOTP].yiaddr = "41.41.41.41"
        ack3 = ack1.copy()
        ack3[IP].dst = "43.43.43.43"
        ack3[BOOTP].yiaddr = "43.43.43.43"
        acks = [ack1, ack2, ack3]

        next(gen) # start the service

        # first action should be to wait for packets
        next(gen)

        # one ack is sent, next action should be to stop the service (dhcp is done)
        gen.send([bytes(ack) for ack in acks])
        
        with self.assertRaises(StopIteration):
            next(gen)

        # check that thes service stopped while setting the ip
        self.assertTrue(service.client.ip in ['42.42.42.42', "43.43.43.43", "41.41.41.41"])


    def test_requesting_state_nack(self):
        pass
    
    def test_requesting_state_multiple_nacks(self):
        pass
    



if __name__ == "__main__":
    unittest.main()
