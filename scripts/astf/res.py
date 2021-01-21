# many templates,clients/server 
# start -f astf/res.py -m 1  -t size=32,clients=65000,servers=65000,templates=100

from trex.astf.api import *
from trex.stl.trex_stl_packet_builder_scapy import ip2int, int2ip
import argparse


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response_template = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>{0}</pre></html>'

class Prof1():
    def __init__(self):
        pass  # tunables

    def ip_gen(self, client_base, server_base, client_ips, server_ips):
        assert client_ips>0
        assert server_ips>0
        ip_gen_c = ASTFIPGenDist(ip_range = [client_base, int2ip(ip2int(client_base) + client_ips - 1)])
        ip_gen_s = ASTFIPGenDist(ip_range = [server_base, int2ip(ip2int(server_base) + server_ips - 1)])
        return ASTFIPGen(dist_client = ip_gen_c,
                         dist_server = ip_gen_s)

    def create_profile(self,kwargs):

        res_size = kwargs.get('size',16)
        clients = kwargs.get('clients',255)
        servers = kwargs.get('servers',255)
        templates = kwargs.get('templates',255)

        ip_gen = self.ip_gen('16.0.0.1', '48.0.0.1', clients, servers)

        # client commands
        http_response = http_response_template.format('*'*res_size)

        http_response_template
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(len(http_response))

        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        prog_s.send(http_response)

        templates_arr = []
        for i in range(templates):
            temp_c = ASTFTCPClientTemplate(program = prog_c, ip_gen = ip_gen, cps = i + 1)
            temp_s = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s)
            templates_arr.append(template)

        return ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)


        return self.create_profile(kwargs)


def register():
    return Prof1()
