
from .trex_stl_streams import STLStream, STLTXSingleBurst
from .trex_stl_packet_builder_scapy import STLPktBuilder

from scapy.layers.l2 import Ether, ARP
from scapy.layers.inet import IP, ICMP

import time
 
# a generic abstract class for resolving using the server
class Resolver(object):
    def __init__ (self, port, queue_size = 100):
        self.port = port
     
    # code to execute before sending any request - return RC object
    def pre_send (self):
        raise NotImplementedError()
        
    # return a list of streams for request
    def generate_request (self):
        raise NotImplementedError()
        
    # return None for more packets otherwise RC object
    def on_pkt_rx (self, pkt):
        raise NotImplementedError()
    
    # return value in case of timeout
    def on_timeout_err (self, retries):
        raise NotImplementedError()
    
    ##################### API ######################
    def resolve (self, retries = 0):

        # first cleanup
        rc = self.port.remove_all_streams()
        if not rc:
            return rc

        # call the specific class implementation
        rc = self.pre_send()
        if not rc:
            return rc

        # start the iteration
        try:

            # add the stream(s)
            self.port.add_streams(self.generate_request())

            rc = self.port.set_attr(rx_filter_mode = 'all')
            if not rc:
                return rc
                
            rc = self.port.set_rx_queue(size = 100)
            if not rc:
                return rc
            
            return self.resolve_wrapper(retries)
            
        finally:
            # best effort restore
            self.port.set_attr(rx_filter_mode = 'hw')
            self.port.remove_rx_queue()
            self.port.remove_all_streams()
                
    
    # main resolve function
    def resolve_wrapper (self, retries):
            
        # retry for 'retries'
        index = 0
        while True:
            rc = self.resolve_iteration()
            if rc is not None:
                return rc
            
            if index >= retries:
                return self.on_timeout_err(retries)
                
            index += 1
            time.sleep(0.1)
            
            

    def resolve_iteration (self):
        
        mult = {'op': 'abs', 'type' : 'percentage', 'value': 100}
        rc = self.port.start(mul = mult, force = False, duration = -1, mask = 0xffffffff)
        if not rc:
            return rc

        # save the start timestamp
        self.start_ts = rc.data()['ts']
        
        # block until traffic finishes
        while self.port.is_active():
            time.sleep(0.01)

        return self.wait_for_rx_response()
        
             
    def wait_for_rx_response (self):

        # we try to fetch response for 5 times
        polling = 5
        
        while polling > 0:
            
            # fetch the queue
            rx_pkts = self.port.get_rx_queue_pkts()
            
            # might be an error
            if not rx_pkts:
                return rx_pkts
                
            # for each packet - examine it
            for pkt in rx_pkts.data():
                rc = self.on_pkt_rx(pkt)
                if rc is not None:
                    return rc
                
            if polling == 0:
                return None
                
            polling -= 1
            time.sleep(0.1)
          
 
        
        
        
class ARPResolver(Resolver):
    def __init__ (self, port_id):
        super(ARPResolver, self).__init__(port_id)
        
    # before resolve
    def pre_send (self):
        self.dst = self.port.get_dst_addr()
        self.src = self.port.get_src_addr()
        
        if self.dst['ipv4'] is None:
            return self.port.err("Port has a non-IPv4 destination: '{0}'".format(dst['mac']))
            
        if self.src['ipv4'] is None:
            return self.port.err('Port must have an IPv4 source address configured')

        # invalidate the current ARP resolution (if exists)
        return self.port.invalidate_arp()
        

    # return a list of streams for request
    def generate_request (self):
                
        base_pkt = Ether(dst="ff:ff:ff:ff:ff:ff")/ARP(psrc = self.src['ipv4'], pdst = self.dst['ipv4'], hwsrc = self.src['mac'])
        s1 = STLStream( packet = STLPktBuilder(pkt = base_pkt), mode = STLTXSingleBurst(total_pkts = 1) )

        return [s1]


    # return None in case more packets are needed else the status rc
    def on_pkt_rx (self, pkt):
        scapy_pkt = Ether(pkt['binary'])
        if not 'ARP' in scapy_pkt:
            return None

        arp = scapy_pkt['ARP']

        # check this is the right ARP (ARP reply with the address)
        if (arp.op != 2) or (arp.psrc != self.dst['ipv4']):
            return None

        
        rc = self.port.set_arp_resolution(arp.psrc, arp.hwsrc)
        if not rc:
            return rc
            
        return self.port.ok('Port {0} - Recieved ARP reply from: {1}, hw: {2}'.format(self.port.port_id, arp.psrc, arp.hwsrc))
        

    def on_timeout_err (self, retries):
        return self.port.err('failed to receive ARP response ({0} retries)'.format(retries))


        
 
    #################### ping resolver ####################
           
class PingResolver(Resolver):
    def __init__ (self, port, ping_ip, pkt_size):
        super(PingResolver, self).__init__(port)
        self.ping_ip = ping_ip
        self.pkt_size = pkt_size
                
    def pre_send (self):
            
        self.src = self.port.get_src_addr()
        self.dst = self.port.get_dst_addr()
        
        if self.src['ipv4'] is None:
            return self.port.err('Ping - port does not have an IPv4 address configured')
            
        if self.dst['mac'] is None:
            return self.port.err('Ping - port has an unresolved destination, cannot determine next hop MAC address')
        
        if self.ping_ip == self.src['ipv4']:
            return self.port.err('Ping - cannot ping own IP')
            
        return self.port.ok()
            
        
    # return a list of streams for request
    def generate_request (self):
                    
        src = self.port.get_src_addr()
        dst = self.port.get_dst_addr()
              
        base_pkt = Ether(dst = dst['mac'])/IP(src = src['ipv4'], dst = self.ping_ip)/ICMP(type = 8)
        pad = max(0, self.pkt_size - len(base_pkt))
        
        base_pkt = base_pkt / (pad * 'x')
        
        #base_pkt.show2()
        s1 = STLStream( packet = STLPktBuilder(pkt = base_pkt), mode = STLTXSingleBurst(total_pkts = 1) )

        return [s1]
        
    # return None for more packets otherwise RC object
    def on_pkt_rx (self, pkt):
        scapy_pkt = Ether(pkt['binary'])
        if not 'ICMP' in scapy_pkt:
            return None
        
        #scapy_pkt.show2()    
        ip = scapy_pkt['IP']
        
        icmp = scapy_pkt['ICMP']
        
        dt = pkt['ts'] - self.start_ts
        
        if icmp.type == 0:
            # echo reply
            return self.port.ok('Reply from {0}: bytes={1}, time={2:.2f}ms, TTL={3}'.format(ip.src, len(pkt['binary']), dt * 1000, ip.ttl))
            
        # unreachable
        elif icmp.type == 3:
            return self.port.ok('Reply from {0}: Destination host unreachable'.format(icmp.src))
        else:
            scapy_pkt.show2()
            return self.port.err('unknown ICMP reply')
            
            
    
    # return the str of a timeout err
    def on_timeout_err (self, retries):
        return self.port.ok('Request timed out.')
