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

    def create_profile(self, interval, attempts):
        # client commands
        prog_c = ASTFProgram(stream=False, addon='latency')
        prog_c.set_var('times', attempts)
        prog_c.set_label('L_loop:')
        if True:
            prog_c.send_msg(http_req)
            prog_c.delay(interval*1000000)
            prog_c.jmp_nz('times', 'L_loop:')

        prog_s = ASTFProgram(stream=False, addon='latency')
        prog_s.recv_msg(1)
        prog_s.send_msg(http_response)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)


        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c,ip_gen=ip_gen)
        temp_s = ASTFTCPServerTemplate(program=prog_s)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s, tg_name='latency')

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=template)
        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--interval', type=int, default=0.1, help="interval between sending packets in seconds")
        parser.add_argument('--attempts', type=int, default=10, help="number of sending packets")

        args = parser.parse_args(tunables)
        return self.create_profile(args.interval, args.attempts)


def register():
    return Prof1()
