# Example for creating your program by specifying buffers to send, without relaying on pcap file

from trex.astf.api import *
import argparse


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>**********</pre></html>'

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # client commands
        prog_c = ASTFProgram(stream=False)
        prog_c.send_msg(http_req)
        prog_c.recv_msg(1)

        prog_s = ASTFProgram(stream=False)
        prog_s.recv_msg(1)
        prog_s.send_msg(http_response)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)


        # template
        temp_c1 = ASTFTCPClientTemplate(port=80, program=prog_c,ip_gen=ip_gen, cps = 1)
        temp_c2 = ASTFTCPClientTemplate(port=81, program=prog_c, ip_gen=ip_gen, cps = 2)
        temp_s1 = ASTFTCPServerTemplate(program=prog_s, assoc=ASTFAssociationRule(80))  # using default association
        temp_s2 = ASTFTCPServerTemplate(program=prog_s, assoc=ASTFAssociationRule(81))  # using default association
        template1 = ASTFTemplate(client_template=temp_c1, server_template=temp_s1, tg_name = '1x')
        template2 = ASTFTemplate(client_template=temp_c2, server_template=temp_s2, tg_name = '2x')

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=[template1, template2])
        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)
        return self.create_profile()


def register():
    return Prof1()
