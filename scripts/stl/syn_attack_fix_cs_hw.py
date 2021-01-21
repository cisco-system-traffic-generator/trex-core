from trex_stl_lib.api import *
import argparse


class STLS1(object):
    """ attack 48.0.0.1 at port 80
    """

    def __init__ (self):
        self.max_pkt_size_l3  =9*1024;

    def create_stream_tcp_syn (self):

        # TCP SYN
        base_pkt  = Ether()/IP(dst="48.0.0.1")/TCP(dport=80,flags="S")


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),


                           STLVmWrFlowVar(fv_name="src_port", 
                                          pkt_offset= "TCP.sport"), # fix udp len  

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IP",
                                              l4_offset = "TCP",
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_TCP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())


    def create_stream_ip (self):

        # IP packet 
        base_pkt  = Ether()/IP(dst="48.0.0.1")


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IP",
                                              l4_offset = 0, # 0 means auto calculate 
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_IP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_ip_pyload (self):

        # IP packet 
        base_pkt  = Ether()/IP(dst="48.0.0.1")/(30*'x')


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IP",
                                              l4_offset = 0, # 0 means auto calculate 
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_IP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_udp1 (self):

        # UDP packet
        base_pkt  = Ether()/IP(dst="48.0.0.1")/UDP(dport=80)/(30*'x')


        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),


                           STLVmWrFlowVar(fv_name="src_port", 
                                          pkt_offset= "UDP.sport"), # fix udp len  

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IP",
                                              l4_offset = "UDP",
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_UDP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_ipv6_tcp (self):

        # UDP packet
        base_pkt  = Ether()/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")/TCP(dport=80,flags="S")

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset ="IPv6.src",offset_fixup=12 ),


                           STLVmWrFlowVar(fv_name="src_port", 
                                          pkt_offset= "TCP.sport"), # fix udp len  

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IPv6",
                                              l4_offset = "TCP",
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_TCP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_ipv6_udp (self):

        # UDP packet
        base_pkt  = Ether()/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")/UDP(dport=80)

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset ="IPv6.src",offset_fixup=12 ),


                           STLVmWrFlowVar(fv_name="src_port", 
                                          pkt_offset= "UDP.sport"), # fix udp len  

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IPv6",
                                              l4_offset = "UDP",
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_UDP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_not_valid (self):

        # UDP packet
        base_pkt  = Ether()/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")/UDP(dport=80)

        # vm
        vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src", 
                                              min_value="16.0.0.0", 
                                              max_value="18.0.0.254", 
                                              size=4, op="random"),

                           STLVmFlowVar(name="src_port", 
                                              min_value=1025, 
                                              max_value=65000, 
                                              size=2, op="random"),

                           STLVmWrFlowVar(fv_name="ip_src", pkt_offset ="IPv6.src",offset_fixup=12 ),


                           STLVmWrFlowVar(fv_name="src_port", 
                                          pkt_offset= "UDP.sport"), # fix udp len  

                           # must be last 
                           STLVmFixChecksumHw(l3_offset = "IPv6",
                                              l4_offset = "IPv6", # not valid 
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_UDP )# hint, TRex can know that 

                          ]
                       )

        pkt = STLPktBuilder(pkt = base_pkt,
                            vm = vm)

        return STLStream(packet = pkt,
                         random_seed = 0x1234,# can be remove. will give the same random value any run
                         mode = STLTXCont())

    def create_stream_udp_random (self):
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
                           STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "UDP.len", add_val=l4_len_fix), # fix udp len  

                           STLVmFixChecksumHw(l3_offset = "IP",
                                              l4_offset = "UDP", # not valid 
                                              l4_type  = CTRexVmInsFixHwCs.L4_TYPE_UDP )# hint, TRex can know that 

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
        #return [ self.create_stream_ipv6_udp(),self.create_stream_tcp_syn(), self.create_stream_udp1(),self.create_stream_ipv6_tcp()]
        #return [ self.create_stream_udp_random () ]
        return [ self.create_stream_tcp_syn()]
        #return [ self.create_stream_ip()]
        #return [ self.create_stream_ip_pyload()]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



