# MIX templates + pcap files 

from trex.astf.api import *


udp_req = b'GET'

udp_response = 'ACK'

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # client commands
        prog_c = ASTFProgram(stream=False)
        prog_c.send_msg(udp_req)
        prog_c.recv_msg(1)

        prog_s = ASTFProgram(stream=False)
        prog_s.recv_msg(1)
        prog_s.send_msg(udp_response)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)


        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c,ip_gen=ip_gen,cps=10,port=52)
        temp_s = ASTFTCPServerTemplate(program=prog_s, assoc=ASTFAssociationRule(port=52))  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        #cap_list=[ASTFCapInfo(file="../avl/delay_dns_0.pcap",cps=5)]
        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=template, # add templates 
                               cap_list=[ASTFCapInfo(file="../avl/delay_dns_0.pcap",cps=2)] # add pcap files list 
                               )
        return profile

    def get_profile(self, **kwargs):
        return self.create_profile()


def register():
    return Prof1()
