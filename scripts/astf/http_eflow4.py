# Example of elephant flow using loop of send with tick var (in the server side)
# 
#
#sudo ./t-rex-64 --astf -f astf/http_eflow4.py -m 10 -d 1000 -c 1 -l 1000 -t send_time=10,recv_time=10,win=512
#
# size      : is in KB for each chuck in the loop
# send_time : in secs, server send responses time
# recv_time : in secs, client receive responses time
# win       : in KB, the maximum window size. make it big for BDP  
# mss       : the mss of the traffic 
#######

from trex.astf.api import *


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self,size, send_time, recv_time, mss, win):

        http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>'+('*'*size*1024)+'</pre></html>'

        # client commands
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.set_tick_var("var1")  # start the clock
        prog_c.set_label("a:")
        prog_c.recv(len(http_response), True)
        prog_c.jmp_dp("var1", "a:", recv_time)  # check time passed since "var1", in case it is less than "recv_time" jump to a:
        prog_c.reset()

        # server commands
        prog_s = ASTFProgram()
        prog_s.recv(len(http_req), True)
        prog_s.set_tick_var("var2")
        prog_s.set_label("b:")
        prog_s.send(http_response)
        prog_s.jmp_dp("var2", "b:", send_time)  # check time passed since "var2", in case it is less than "send_time" jump to b:
        prog_s.reset()

        info = ASTFGlobalInfo()
        if mss:
          info.tcp.mss = mss
        info.tcp.initwnd = 2  # start big
        info.tcp.no_delay = 1   # to get fast feedback for acks
        info.tcp.rxbufsize = win * 1024  # 1MB window 
        info.tcp.txbufsize = win * 1024  


        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c,  ip_gen=ip_gen, cps=1,limit=1)
        temp_s = ASTFTCPServerTemplate(program=prog_s)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, 
                              templates=template,                              
                              default_c_glob_info=info,
                              default_s_glob_info=info)

        return profile

    def get_profile(self,**kwargs):
        size = kwargs.get('size',1)
        send_time = kwargs.get('send_time', 2)
        if send_time < 0:
            send_time = 1
        recv_time = kwargs.get('recv_time', 2)
        if recv_time < 0:
            recv_time = 1
        mss  = kwargs.get('mss', 0)
        win  = kwargs.get('win', 32)
        return self.create_profile(size, send_time, recv_time, mss, win)


def register():
    return Prof1()
