from trex.astf.api import *


class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # program
        # Client program taken from client side of given file
        my_prog_c = ASTFProgram(file="../cap2/http_get.pcap", side="c")
        # Server program taken from server side of given file
        my_prog_s = ASTFProgram(file="../cap2/http_get.pcap", side="s")

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        ip_gen_c2 = ASTFIPGenDist(ip_range=["20.0.0.0", "20.0.0.255"], distribution="seq")
        ip_gen_s2 = ASTFIPGenDist(ip_range=["50.0.0.0", "50.0.255.255"], distribution="seq")
        ip_gen2 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c2,
                            dist_server=ip_gen_s2)

        # template
        temp_c = ASTFTCPClientTemplate(program=my_prog_c,  ip_gen=ip_gen)
        temp_c2 = ASTFTCPClientTemplate(program=my_prog_c, ip_gen=ip_gen2, port=81)

        temp_s = ASTFTCPServerTemplate(program=my_prog_s,)  # using default association
        temp_s2 = ASTFTCPServerTemplate(program=my_prog_s, assoc=ASTFAssociationRule(port=81))
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)
        template2 = ASTFTemplate(client_template=temp_c2, server_template=temp_s2)

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=[template, template2])
        return profile

    def get_profile(self, **kwargs):
        return self.create_profile()


def register():
    return Prof1()
