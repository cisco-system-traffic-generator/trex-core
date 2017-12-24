# Example using prog_s.wait_for_peer_close() to let the client close the server first. 
# client -> FIN
#                  ACK <- server 
#                  FIN <- server 
# client -> ACK
# 

from trex_astf_lib.api import *


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>**********</pre></html>'

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self):
        # client commands
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(len(http_response))
        # implicit  close 

        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        prog_s.send(http_response)
        prog_s.wait_for_peer_close(); # wait for client to close the socket the issue a close 

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        tcp_params = ASTFTCPInfo(window=32768)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, tcp_info=tcp_params, ip_gen=ip_gen)
        temp_s = ASTFTCPServerTemplate(program=prog_s, tcp_info=tcp_params)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=template)
        return profile

    def get_profile(self,**kwargs):
        return self.create_profile()


def register():
    return Prof1()
