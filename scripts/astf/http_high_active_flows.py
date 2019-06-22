# create high number of flow using high delay 
#
# -t size=64,delay=1000
# you will need to add to trex_cfg.yaml 
# 
# memory    :                                          
#        dp_flows    : 7048576

from trex.astf.api import *


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response_template = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>{0}</pre></html>'

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self,res_size,delay):
        # client commands
        http_response = http_response_template.format('*'*res_size)

        http_response_template
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(len(http_response))

        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        prog_s.delay(delay); 
        prog_s.send(http_response)

        c_glob_info = ASTFGlobalInfo()
        # keep alive is much longer in sec time 128sec
        c_glob_info.tcp.keepinit = 200
        c_glob_info.tcp.keepidle = 200
        c_glob_info.tcp.keepintvl = 200

        s_glob_info = ASTFGlobalInfo()
        s_glob_info.tcp.keepinit = 200
        s_glob_info.tcp.keepidle = 200
        s_glob_info.tcp.keepintvl = 200


        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)


        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c,  ip_gen=ip_gen)
        temp_s = ASTFTCPServerTemplate(program=prog_s)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # profile
        profile = ASTFProfile(default_c_glob_info =c_glob_info,default_s_glob_info=s_glob_info,  default_ip_gen=ip_gen, templates=template)
        return profile

    def get_profile(self, **kwargs):
        res_size= kwargs.get('size',1)
        delay = kwargs.get('delay',1)
        print("delay {}".format(delay))
        return self.create_profile(res_size,delay)


def register():
    return Prof1()
