from trex_stl_lib.api import *
import argparse


class STLS1(object):

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream (self):
        # pkt 
        p_l2  = Ether();
        p_l3  = IP(src="16.0.0.1",dst="48.0.0.1")
        p_l4  = UDP(dport=12,sport=1025)
        pyld_size = max(0, self.max_pkt_size_l3 - len(p_l3/p_l4));
        base_pkt = p_l2/p_l3/p_l4/('\x55'*(pyld_size))

        l3_len_fix =-(len(p_l2));
        l4_len_fix =-(len(p_l2/p_l3));


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="fv_rand", min_value=64, max_value=len(base_pkt), size=2, op="random"),
                           STLVmTrimPktSize("fv_rand"), # total packet size
                           STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "IP.len", add_val=l3_len_fix), # fix ip len 
                           STLVmFixIpv4(offset = "IP"),                                # fix checksum
                           STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "UDP.len", add_val=l4_len_fix) # fix udp len  
                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,
                         mode = STLTXCont())



    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # create 1 stream 
        return [ self.create_stream() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



