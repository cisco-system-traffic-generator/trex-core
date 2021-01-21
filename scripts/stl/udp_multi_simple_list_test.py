from trex_stl_lib.api import *
import argparse


def generate_payload(length):
      word = ''
      alphabet_size = len(string.ascii_letters)
      for i in range(length):
          word += string.ascii_letters[(i % alphabet_size)]
      return word


class STLS1(object):

    def create_stream1 (self):
        fsize_no_fcs = 129
        base_pkt_a = Ether()/IP(dst="48.0.0.1",options=IPOption(b'\x01\x01\x01\x00'))/UDP(dport=12,sport=1025)
        ipaddr_list = ["16.0.0.1","16.0.0.4","16.0.0.5","16.0.0.2","16.0.0.3","16.0.0.6"]
        vm1 = STLScVmRaw([
                          STLVmFlowVar(name="src",value_list=ipaddr_list, op="inc"),
                          STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src"),
                           # checksum
                         STLVmFixIpv4(offset = "IP")

                         ]
                         ) # split to cores base on the tuple generator

        #
        pkt_a = STLPktBuilder(pkt=base_pkt_a/generate_payload(fsize_no_fcs-len(base_pkt_a)), vm=vm1)


        return STLStream( packet = pkt_a ,
                          mode = STLTXCont() )


    def create_stream2 (self):
        fsize_no_fcs = 129
        base_pkt_a = Ether()/IP(dst="48.0.0.1",options=IPOption(b'\x01\x01\x01\x00'))/UDP(dport=12,sport=1025)
        ipaddr_list = ["17.0.0.1","17.0.0.4","17.0.0.5","17.0.0.2","17.0.0.3","17.0.0.6"]
        vm1 = STLScVmRaw([
                          STLVmFlowVar(name="src",value_list=ipaddr_list, op="dec"),
                          STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src"),
                           # checksum
                         STLVmFixIpv4(offset = "IP")

                         ]
                         ) # split to cores base on the tuple generator

        #
        pkt_a = STLPktBuilder(pkt=base_pkt_a/generate_payload(fsize_no_fcs-len(base_pkt_a)), vm=vm1)


        return STLStream( packet = pkt_a ,
                          mode = STLTXCont() )


    def create_stream3 (self):
        fsize_no_fcs = 129
        base_pkt_a = Ether()/IP(dst="48.0.0.1",options=IPOption(b'\x01\x01\x01\x00'))/UDP(dport=12,sport=1025)
        ipaddr_list = ["18.0.0.1","18.0.0.4","18.0.0.5","18.0.0.2","18.0.0.3","18.0.0.6"]
        vm1 = STLScVmRaw([
                          STLVmFlowVar(name="src",value_list=ipaddr_list, op="random"),
                          STLVmWrFlowVar(fv_name="src",pkt_offset= "IP.src"),
                           # checksum
                         STLVmFixIpv4(offset = "IP")

                         ]
                         ) # split to cores base on the tuple generator

        #
        pkt_a = STLPktBuilder(pkt=base_pkt_a/generate_payload(fsize_no_fcs-len(base_pkt_a)), vm=vm1)


        return STLStream( packet = pkt_a ,
                          mode = STLTXCont() )


    def get_streams (self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        # create 1 stream
        return [ self.create_stream1(),self.create_stream2(),self.create_stream3() ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



