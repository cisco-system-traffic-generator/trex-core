#!/usr/bin/python
from __future__ import print_function

import stl_path
from trex.stl.api import *
from trex.common.services.trex_service_icmp import ServiceICMP
from trex.common.services.trex_service_dhcp import ServiceDHCP

from functools import partial

try:
    input = raw_input
except NameError:
    pass

wait_for_key = input
    

def random_mac ():
    c = partial(random.randint, 0 ,255)
    return '%02x:%02x:%02x:%02x:%02x:%02x' % (c(), c(), c(), c(), c(), c())
    
def random_mac_range (count):
    return [random_mac() for _ in range(count)]


class DHCPTest(object):
    def __init__ (self, port):
        self.port = port
        self.c    = STLClient()
        
    def run (self, count):
            
        try:
            self.c.connect()
            self.c.reset(ports = self.port)
            self.ctx  = self.c.create_service_ctx(port = self.port)
            
            # create clients
            clients = self.setup(count)
            if not clients:
                print('\nno clients have sucessfully registered...exiting...\n')
                exit(1)
                
            # inject traffic
            self.inject(clients)
            
            # teardown - release clients
            self.teardown(clients)
            
            
        except STLError as e:
            print(e)
            exit(1)
            
        finally:
            self.c.disconnect()
            

            
    def setup (self, count):
            
        # phase one - service context
        self.c.set_service_mode(ports = self.port)
        
        try:
            # create DHCP clients
            clients = self.create_dhcp_clients(count)
            if not clients:
                return
            
            print('\n\nPress Return to send PING from DHCP clients to the server...')
            wait_for_key()
            
            # ping from every client
            pings = self.ping_from_clients(clients)
            
            return clients
            
        finally:
            self.c.set_service_mode(ports = self.port, enabled = False)
        
            
    def inject (self, clients):
        print('\n\nPress Return to generate high speed traffic from all clients...')
        wait_for_key()
        
        print('\n*** step 4: generating UDP traffic from {} clients ***\n'.format(len(clients)))
            
        streams = []
        for client in clients:
            record = client.get_record()
            base_pkt = Ether(src=record.client_mac)/IP(src=record.client_ip,dst=record.server_ip)/UDP()
            pkt = STLPktBuilder(pkt = base_pkt, vm = [])
            
            streams.append(STLStream(packet = pkt, mode = STLTXSingleBurst(total_pkts = 1000)))
        
        self.c.add_streams(ports = self.port, streams = streams)
        self.c.start(ports = self.port, mult = '100%')
        self.c.wait_on_traffic()
        
        print('\n*** Done ***\n')
        
    def teardown (self, clients):
        print('\n\nPress Return to release all DHCP clients...')
        wait_for_key()
        
        try:
            # move back to service mode for releasing DHCPs
            self.c.set_service_mode(ports = self.port)
            self.release_dhcp_clients(clients)
            
        finally:
            self.c.set_service_mode(ports = self.port, enabled = False)

        
        
    def create_dhcp_clients (self, count):
        dhcps = [ServiceDHCP(mac = random_mac(), verbose_level = ServiceDHCP.INFO) for _ in range(count)]

        # execute all the registered services
        print('\n*** step 1: starting DHCP acquire for {} clients ***\n'.format(len(dhcps)))
        self.ctx.run(dhcps)
        
        print('\n*** step 2: DHCP acquire results ***\n')
        for dhcp in dhcps:
            record = dhcp.get_record()
            print('client: MAC {0} - DHCP: {1}'.format(dhcp.get_mac(),record))
        
        # filter those that succeeded
        bounded_dhcps = [dhcp for dhcp in dhcps if dhcp.state == 'BOUND']
        
        return bounded_dhcps
        
        
        
    def ping_from_clients (self, clients):

        print('\n*** step 3: sending PING request from {0} clients to DHCP server ***\n'.format(len(clients)))
        
        pings = [ServiceICMP(self.ctx, src_ip = client.get_record().client_ip, dst_ip = client.get_record().server_ip,
                                verbose_level = ServiceICMP.INFO) \
                 for client in clients]

        self.ctx.run(pings)
        
        
    def release_dhcp_clients (self, clients):
        print('\n*** step 5: starting DHCP release for {} clients ***\n'.format(len(clients)))
        self.ctx.run(clients)
        
        
    
def main ():

    print('How many DHCP clients to create: ', end='')
    count = int(input())

    dhcp_test = DHCPTest(0)
    dhcp_test.run(count)
    
   
if __name__ == '__main__':
    main()

