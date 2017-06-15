"""
Handles VLANs

Author:
  Itay Marom

"""
from scapy.layers.l2 import Dot1Q, Dot1AD
from .trex_stl_exceptions import STLError


class VLAN(object):
    '''
        A simple class to handle VLANs
    '''
    
    def __init__ (self, vlan):
        # move to canonical form
        
        if vlan is None:
            # no VLAN and mark it as a default value
            self.is_def = True
            self.tags = ()
            return
        
        if isinstance(vlan, VLAN):
            # copy constructor
            self.tags = vlan.tags
            self.is_def = vlan.is_def
            return
            
        # make list of integer
        vlan_list = (vlan,) if isinstance(vlan, int) else vlan

        
        if len(vlan_list) > 2:
            raise STLError("only up to two VLAN tags are supported")

        for tag in vlan_list:
            if not type(tag) == int:
                raise STLError("invalid VLAN tag: '{0}' (int value expected)".format(tag))

            if not (tag in range(1, 4096)) :
                raise STLError("invalid VLAN tag: '{0}' (valid range: 1 - 4095)".format(tag))

        self.tags = tuple(vlan_list)
        self.is_def = False
        
    
    def __nonzero__ (self):
        return len(self.tags) > 0
        
        
    def __bool__ (self):
        return self.__nonzero__()
    
        
    def __iter__ (self):
        return self.tags.__iter__()
        
        
    def get_tags (self):
        return self.tags
        
        
    def is_default (self):
        '''
            returns True if no values were provided during
            the object creation
            it represents an empty VLAN as a default value
        '''
        return self.is_def
        
        
        
    def get_desc (self):
        
        if len(self.tags) == 1:
            return "VLAN '{0}'".format(self.tags[0])
        elif len(self.tags) == 2:
            return "QinQ '{0}/{1}'".format(self.tags[0], self.tags[1])
        else:
            return ''
            
        
    @staticmethod
    def extract (scapy_pkt):
        '''
            Given a scapy packet, returns all the VLANs
            in the packet
        '''
    
        # no VLANs
        if scapy_pkt.type not in (0x8100, 0x88a8):
            return []

        vlans = []
        vlan_layer = scapy_pkt.payload

        while type(vlan_layer) in (Dot1Q, Dot1AD):
            # append
            vlans.append(vlan_layer.vlan)

            # next
            vlan_layer = vlan_layer.payload

        return vlans
        
        
    def embed (self, scapy_pkt):
        '''
            Given a scapy packet, embedd the VLAN config
            into the packet
        '''
        if not self.tags:
            return
        
        ether         = scapy_pkt.getlayer(0)
        ether_payload = scapy_pkt.payload
        
        # single VLAN
        if len(self.tags) == 1:
            vlan = Dot1Q(vlan=self.tags[0])
            vlan.payload = ether.payload
            ether.payload = vlan
            
            
        # dobule VLAN
        elif len(self.tags) == 2:
            dot1ad          = Dot1AD(vlan=self.tags[0])
            dot1q           = Dot1Q(vlan=self.tags[1])
            dot1ad.payload  = dot1q
            dot1q.payload   = ether_payload
            
            ether.payload = dot1ad

