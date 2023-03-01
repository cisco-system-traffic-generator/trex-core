# Example of elephant flow using loop of send (in the server side)
# 
#
#sudo ./t-rex-64 --astf -f astf/http_eflow2.py -m 10 -d 1000 -c 1 -l 1000 -t size=800,loop=100000,win=512,pipe=1
#
# size  : is in KB for each chuck in the loop
# loops : how many chunks to download
# win   : in KB, the maximum window size. make it big for BDP  
# pipe  : Don't block on each send, make them in the pipeline should be 1 for maximum performance 
# mss   : the mss of the traffic 
#######

from trex.astf.api import *
import argparse


# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self, size, loop, mss, win, pipe):

        http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>'+('*'*size*1024)+'</pre></html>'

        bsize = len(http_response)

        # client commands
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(bsize*loop)

        # server commands
        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        if pipe:
          prog_s.set_send_blocking (False)   # configure the state machine to continue to the next send when there is the queue has space 
        prog_s.set_var("var2",loop-1); # set var to loop-1 (there is another blocking send)
        prog_s.set_label("a:");
        prog_s.send(http_response)
        prog_s.jmp_nz("var2","a:") # dec var "var2". in case it is *not* zero jump a: 
        prog_s.set_send_blocking (True) # back to blocking mode 
        prog_s.send(http_response)


        info = ASTFGlobalInfo()
        info.tcp.mss = mss
        info.tcp.initwnd = 2  # start big
        info.tcp.no_delay = 0  # to get fast feedback for acks
        info.tcp.no_delay_counter = 2 * mss
        info.tcp.rxbufsize = win*1024  # 1MB window
        info.tcp.txbufsize = win*1024
        #info.tcp.do_rfc1323 =0


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
                              cap_list=[ASTFCapInfo(file="../avl/delay_10_rtp_160k_0.pcap",cps=20,port=53)],
                              default_c_glob_info=info,
                              default_s_glob_info=info)

        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--size',
                            type=int,
                            default=1,
                            help='size is in KB for each chuck in the loop')
        parser.add_argument('--loop',
                            type=int,
                            default=10,
                            help='how many chunks to download')
        parser.add_argument('--win',
                            type=int,
                            default=32,
                            help='win: in KB, the maximum window size. make it big for BDP')
        parser.add_argument('--pipe',
                            type=int,
                            default=0,
                            help="pipe: Don't block on each send, make them in the pipeline should be 1 for maximum performance.")
        parser.add_argument('--mss',
                            type=int,
                            default=1460,
                            help='the mss of the traffic.')
        args = parser.parse_args(tunables)

        size = args.size
        loop = args.loop
        if loop < 2:
            loop = 2
        mss = args.mss
        assert mss > 0, "mss must be greater than 0"
        win = args.win
        pipe = args.pipe
        return self.create_profile(size, loop, mss, win, pipe)


def register():
    return Prof1()
