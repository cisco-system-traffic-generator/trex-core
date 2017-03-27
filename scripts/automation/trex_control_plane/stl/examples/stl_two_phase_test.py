import stl_path
from trex_stl_lib.api import *
from scapy.all import *
from scapy.layers.dhcp import DHCP, BOOTP
import time
import random

class DHCPClient(object):
    # message types
    DISCOVER = 1
    OFFER    = 2
    ACK      = 5
    NACK     = 6

    def __init__ (self, mac, tx, rx):
        self.mac = mac
        self.tx  = tx
        self.rx  = rx
        self.xid = random.getrandbits(32)

        # first state
        self.state   = 'INIT'
        self.actions = {'INIT'       : self._discover,
                        'SELECTING'  : self._wait_for_offer,
                        'REQUESTING' : self._request,
                        'WAITFORACK' : self._wait_for_ack}

    def get_xid (self):
        return self.xid
     
    def is_active (self):
        return self.state != 'DONE'
        
    def start (self):
        # send a discover message
        self.tx(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                      /BOOTP(chaddr=self.mac,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))
        
        # now wait for offers
        offers = yield self.xid
        
        # take the first one
        offer = offers[0]
        assert 'BOOTP' in offer
        assert pkt['DHCP options'].options[0][1] == DHCPClient.OFFER
        
        # save the offered address
        self.yiaddr = offer['BOOTP'].yiaddr
        
        # send a request packet
        self.tx(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                /BOOTP(chaddr=self.mac,xid=self.xid)/DHCP(options=[("message-type","request"),("requested_addr", self.yiaddr),"end"]))
        
        
        
    def next (self):
        self.actions[self.state]()
        
    def _discover (self):
        # send a discover message
        self.tx(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                      /BOOTP(chaddr=self.mac,xid=self.xid)/DHCP(options=[("message-type","discover"),"end"]))

        self.state = 'SELECTING'
        
        
    def _wait_for_offer (self):
        pkts = self.rx(self.xid)
        offers = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] == DHCPClient.OFFER]
        if not offers:
            return
            
        # take first offer
        offer = offers[0]
        options = {x[0]:x[1] for x in offer['DHCP options'].options if isinstance(x, tuple)}
        #print("Got an offer from server '{}' of address '{}'\n".format(options['server_id'], offer['BOOTP'].yiaddr))
        
        self.yiaddr = offer['BOOTP'].yiaddr
        self.state = 'REQUESTING'
        
    def _request (self):
        self.tx(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0",dst="255.255.255.255")/UDP(sport=68,dport=67) \
                /BOOTP(chaddr=self.mac,xid=self.xid)/DHCP(options=[("message-type","request"),("requested_addr", self.yiaddr),"end"]))

        self.state = 'WAITFORACK'


    def _wait_for_ack (self):
        pkts = self.rx(self.xid)
        acks = [pkt for pkt in pkts if pkt['DHCP options'].options[0][1] in (DHCPClient.ACK, DHCPClient.NACK)]
        if not acks:
            return
        
        assert len(acks) == 1
        ack = acks[0]
        options = {x[0]:x[1] for x in ack['DHCP options'].options if isinstance(x, tuple)}
        
        self.state = 'DONE'
        
        
class DHCPClientManager(object):
    def __init__ (self, c, port, mac_list):
        self.c        = c
        self.mac_list = list(mac_list)
        self.port     = port
        
        self.tx_buffer = []
        
        self.dhcp_clients = [DHCPClient(mac, self.tx, self.rx) for mac in mac_list]
            
        self.rx_buffer = {dhcp_client.get_xid() : [] for dhcp_client in self.dhcp_clients}
  

    # API
    def run(self):

        self.c.set_service_mode(ports = self.port)
        self.c.set_port_attr(ports = self.port, promiscuous = True)
        self.capture_id = self.c.start_capture(rx_ports = self.port)['id']
        
        try:
            self.__run()
        finally:
            self.c.stop_capture(self.capture_id)
            self.c.reset(ports = self.port)
        
        
        try:
            while True:
                client_states = [client.state for client in self.dhcp_clients]
                print(client_states)
                
                # traverse all active clients
                while self.active_clients:
                    active_client = self.active_clients.pop()
                    active_client.next()
                    if active_client.is_active():
                        self.next_active.append(active_client)
                
                self.flush_tx()
                self.fetch_rx()
                
                # flip
                self.active_clients = self.next_active
                self.next_active = []
                
                if not self.active_clients:
                    break
                    
            
        finally:
            self.c.stop_capture(self.capture_id)
            self.c.reset(ports = self.port)
            
    ############ internal ############
    def __run (self):
        self.active_clients = list(self.dhcp_clients)
        self.next_active = []
        
        
    def flush_tx (self):
        if self.tx_buffer:
            self.c.push_packets(ports = self.port, pkts = self.tx_buffer, force = True)
            self.c.wait_on_traffic()
            self.tx_buffer = []
            
    def fetch_rx (self, timeout_ms = 100):
        pkts = []
        
        while True:
            self.c.fetch_capture_packets(self.capture_id, pkts)
            if pkts:
                break
                
            # sleep 1 ms
            time.sleep(0.01)
            timeout_ms -= 1
            if timeout_ms <= 0:
                return
            
        
        for pkt in pkts:
            scapy_pkt = Ether(pkt['binary'])
            if 'BOOTP' in scapy_pkt:
                self.rx_buffer[scapy_pkt['BOOTP'].xid].append(scapy_pkt)
   
        
    def tx (self, pkt):
        self.tx_buffer.append(pkt)
        
    def rx (self, xid):
        pkts = self.rx_buffer[xid]
        self.rx_buffer[xid] = []
        return pkts
        
        
        
class TwoPhaseTest(object):
    def __init__ (self, server = "localhost", port = 0):
        self.c = STLClient(server = server)
        self.port = port
        
    def run (self):
        self.c.connect()
        self.c.remove_all_captures()
        self.c.reset(ports = self.port)
     
        
        try:
            self.c.set_service_mode(self.port)
            self.config_phase()
        finally:
            self.c.set_service_mode(self.port, False)
            
        self.run_phase()
        
        
    def config_phase (self):
        self.c.set_port_attr(ports = self.port, promiscuous = True)
           
        #dhcp_client_mngr = DHCPClientManager(self.c, self.port, [b'\xa0\x36\x9f\x20\xe6\xce'])
        dhcp_client_mngr = DHCPClientManager(self.c, self.port, [bytes([mac]) for mac in range(1, 50)])
        dhcp_client_mngr.run()
        
        exit(1)
        
        self.capture_id = self.c.start_capture(rx_ports = self.port)['id']
        
        try:
            #dhcp_client = DHCPClient(self.c, b'\xa0\x36\x9f\x20\xe6\xce', self.port, self.capture_id)
            clients = [DHCPClient(self.c, bytes([mac]), self.port, self.capture_id) for mac in range(1, 10)]
            for client in clients:
                client.run()
            
        finally:
            self.c.stop_capture(self.capture_id)
        
        
    def run_phase (self):
        pass
        
        
    def __del__ (self):
        self.c.disconnect()
        
        
test = TwoPhaseTest()
try:
    test.run()
except STLError as e:
    passed = False
    print(e)
    sys.exit(1)
    
    

