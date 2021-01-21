# Example for creating your program by specifying buffers to send, without relaying on pcap file

from trex.astf.api import *
import argparse


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
http_req1 = b'POST /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
http_req2 = b'PUT /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>**********</pre></html>'

class Prof1():
    def __init__(self):
        pass  # tunables

    def _udp_client_prog(self, req, res):
        prog_c = ASTFProgram(stream=False)
        prog_c.send_msg(req)
        prog_c.recv_msg(1)
        return prog_c

    def _udp_server_prog(self, req, res):
        prog_s = ASTFProgram(stream=False)
        prog_s.recv_msg(1)
        prog_s.send_msg(res)
        return prog_s

    def _tcp_client_prog(self, req, res):
        prog_c = ASTFProgram(stream=True)
        prog_c.send(req)
        prog_c.recv(len(res))
        return prog_c

    def _tcp_server_prog(self, req, res):
        prog_s = ASTFProgram(stream=True)
        prog_s.recv(len(req))
        prog_s.send(res)
        return prog_s

    def create_profile(self, proto='all', temp='all'):
        # UDP client and server commands
        udp_prog_c = self._udp_client_prog(http_req, http_response)
        udp_prog_c1 = self._udp_client_prog(http_req1, http_response)
        udp_prog_c2 = self._udp_client_prog(http_req2, http_response)

        udp_prog_s = self._udp_server_prog(http_req, http_response)

        # TCP client and server commands
        tcp_prog_c = self._tcp_client_prog(http_req, http_response)
        tcp_prog_c1 = self._tcp_client_prog(http_req1, http_response)
        tcp_prog_c2 = self._tcp_client_prog(http_req2, http_response)

        tcp_prog_s = self._tcp_server_prog(http_req, http_response)
        tcp_prog_s1 = self._tcp_server_prog(http_req1, http_response)
        tcp_prog_s2 = self._tcp_server_prog(http_req2, http_response)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # use default port 80
        assoc_by_ip = ASTFAssociationRule(ip_start="48.0.0.0", ip_end="48.0.255.255")
        assoc_by_l7 = ASTFAssociationRule(ip_start="48.0.0.0", ip_end="48.0.255.255",
                                          l7_map={"offset":[0,1,2,3]})

        templates = []
        # UDP templates
        udp_temp_c = ASTFTCPClientTemplate(program=udp_prog_c, ip_gen=ip_gen)
        udp_temp_s = ASTFTCPServerTemplate(program=udp_prog_s, assoc=assoc_by_ip)
        udp_template = ASTFTemplate(client_template=udp_temp_c, server_template=udp_temp_s, tg_name='udp')

        udp_temp_s1 = ASTFTCPServerTemplate(program=udp_prog_s, assoc=assoc_by_l7)
        udp_temp_c1 = ASTFTCPClientTemplate(program=udp_prog_c1, ip_gen=ip_gen)
        udp_template1 = ASTFTemplate(client_template=udp_temp_c1, server_template=udp_temp_s1, tg_name='udp1')

        udp_temp_c2 = ASTFTCPClientTemplate(program=udp_prog_c2, ip_gen=ip_gen)
        udp_template2 = ASTFTemplate(client_template=udp_temp_c2, server_template=udp_temp_s1, tg_name='udp2')

        if proto == 'all' or proto == 'udp':
            if temp == 'all' or temp == 'ip':
                templates += [ udp_template ]
            if temp == 'all' or temp == 'payload':
                templates += [ udp_template1, udp_template2 ]

        # TCP templates
        tcp_temp_c = ASTFTCPClientTemplate(program=tcp_prog_c, ip_gen=ip_gen)
        tcp_temp_s = ASTFTCPServerTemplate(program=tcp_prog_s, assoc=assoc_by_ip)
        tcp_template = ASTFTemplate(client_template=tcp_temp_c, server_template=tcp_temp_s, tg_name='tcp')

        tcp_temp_c1 = ASTFTCPClientTemplate(program=tcp_prog_c1, ip_gen=ip_gen)
        tcp_temp_s1 = ASTFTCPServerTemplate(program=tcp_prog_s1, assoc=assoc_by_l7)
        tcp_template1 = ASTFTemplate(client_template=tcp_temp_c1, server_template=tcp_temp_s1, tg_name='tcp1')

        tcp_temp_c2 = ASTFTCPClientTemplate(program=tcp_prog_c2, ip_gen=ip_gen)
        tcp_temp_s2 = ASTFTCPServerTemplate(program=tcp_prog_s2, assoc=assoc_by_l7)
        tcp_template2 = ASTFTemplate(client_template=tcp_temp_c2, server_template=tcp_temp_s2, tg_name='tcp2')

        if proto == 'all' or proto == 'tcp':
            if temp == 'all' or temp == 'ip':
                templates += [ tcp_template ]
            if temp == 'all' or temp == 'payload':
                templates += [ tcp_template1, tcp_template2 ]

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=templates)
        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--proto',
                            type=str,
                            default='all',
                            choices={'all', 'tcp', 'udp'},
                            help='')
        parser.add_argument('--temp',
                            type=str,
                            default='all',
                            choices={'all', 'ip', 'payload'},
                            help='')

        args = parser.parse_args(tunables)

        proto = args.proto.lower()
        temp = args.temp.lower()
        return self.create_profile(proto, temp)


def register():
    return Prof1()
