from trex_stl_lib.api import *


# x clients override the LSB of destination
#Base src ip : 55.55.1.1, dst ip: Fixed
#Increment src ipt portion starting at 55.55.1.1 for 'n' number of clients (55.55.1.1, 55.55.1.2)
#Src MAC: start with 0000.dddd.0001, increment mac in steps of 1
#Dst MAC: Fixed    (will be taken from trex_conf.yaml


class STLS1(object):

    def __init__ (self):
        self.num_clients  =30000; # max is 16bit
        self.fsize        =64

    def create_stream (self):

        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS
        base_pkt =  Ether(src="00:00:dd:dd:00:01")/IP(src="55.55.1.1",dst="58.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'

        vm = CTRexScRaw( [ STLVmFlowVar(name="mac_src", min_value=1, max_value=self.num_clients, size=2, op="inc"), # 1 byte varible, range 1-10
                           STLVmWrFlowVar(fv_name="mac_src", pkt_offset= 10),                          # write it to LSB of ethernet.src 
                           STLVmWrFlowVar(fv_name="mac_src" ,pkt_offset="IP.src",offset_fixup=2),       # it is 2 byte so there is a need to fixup in 2 bytes
                           STLVmFixIpv4(offset = "IP")
                          ]
                         ,split_by_field = "mac_src"  # split 
                       )

        return STLStream(packet = STLPktBuilder(pkt = base_pkt/pad,vm = vm),
                         mode = STLTXCont( pps=10 ))

    def get_streams (self, direction = 0):
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



