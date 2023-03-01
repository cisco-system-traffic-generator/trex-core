# Example for multiple FTP flows simulation
# Options :
# fsize: file size in MB(Mega Bytes) to download by each flow
# nflows : number of flows to download by FTP
# tinc : time to increase flow, 0 to start all flows immediately, n to inclease number of flows by random(n) seconds
#
# Example of command
# sudo ./t-rex-64 -f astf/ftp_multiple_flows_simulation.py -m 1 --astf -d 120 -t fsize=10000,nflows=50,tinc=60
# download 10G file per each flow, total 50 flows increasing until 60 seconds
#
# *Note : It send FTP data(port 20) only

from trex.astf.api import *
import argparse


class ftp_sim():
    max_buf_size = 0
    ginfo = None
    ftp_get = "RETR file"

    def __init__(self):
        pass  # tunables

    def tune_tcp(self,mss):
        self.max_buf_size = 4*512*1024
        self.ginfo = ASTFGlobalInfo()
        if mss == 0:
          mss = 1460
        self.ginfo.tcp.mss = mss
        self.ginfo.tcp.rxbufsize = 2*512*1024   # Max rxbufsize=1MB
        self.ginfo.tcp.txbufsize = 2*512*1024   # Max txbufsize=1MB
        self.ginfo.tcp.no_delay_counter = mss*2 # RFC5681
        self.ginfo.tcp.initwnd = 20             # Max init wind=20

    def create_profile(self,fsize,nflows,tinc,speed):
        self.tune_tcp(0)
        if speed:   # limit download throughput by speed(Mbits/sec)
            mss = self.ginfo.tcp.mss
            min_delay = 20 * min(50, nflows)
            # calculate minimum size to send with min_delay time for achieving speed
            min_buf_size = speed/8 * min_delay
            # to avoid blocking at send, some spaces should be left in the buffer after the send.
            min_buf_size = int((min(min_buf_size, self.ginfo.tcp.txbufsize/2) + mss-1)/mss) * mss
            self.max_buf_size = max(2 * self.ginfo.tcp.initwnd * mss, min_buf_size)

        size_total = int(fsize*(1024*1024))
        loop = int(size_total/self.max_buf_size)
        remain = size_total%self.max_buf_size

        # client commands
        prog_c = ASTFProgram()
        if tinc != 0:
            prog_c.delay_rand(0,tinc*1000*1000)  # delay from 0 to random(tinc) seconds
        prog_c.send(self.ftp_get)
        prog_c.recv(size_total)

        # server commands
        prog_s = ASTFProgram()
        prog_s.recv(len(self.ftp_get))
        if loop:
            prog_s.set_send_blocking (False) # Set to non-blocking to maximize thput in single flow
            prog_s.set_var("var2",loop); # set var 0 to loop
            prog_s.set_label("a:");
            prog_s.send('',size=self.max_buf_size,fill='*')
            if speed:
                delay_us = max(int(self.max_buf_size/(speed/8)), min_delay)
                prog_s.delay(delay_us)
            prog_s.jmp_nz("var2","a:") # dec var "var2". in case it is *not* zero jump a:
        if remain:
            prog_s.send('',size=remain,fill='*')
        prog_s.wait_for_peer_close()

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["1.1.1.1", "1.1.1.100"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["1.1.2.1", "1.1.2.100"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, ip_gen=ip_gen, limit=nflows, port=20) # Port : FTP control(21), FTP data (20)
        temp_s = ASTFTCPServerTemplate(program=prog_s,assoc=ASTFAssociationRule(port=20))  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen,
                              templates=template,
                              default_c_glob_info=self.ginfo,
                              default_s_glob_info=self.ginfo)

        return profile

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--fsize',
                            type=int,
                            default=1000,
                            help='file size in MB(Mega Bytes) to download by each flow.')
        parser.add_argument('--nflows',
                            type=int,
                            default=1,
                            help='number of flows to download by FTP.')
        parser.add_argument('--tinc',
                            type=int,
                            default=0,
                            help='time to increase flow, 0 to start all flows immediately, n to increase number of flows by random(n) seconds.')
        parser.add_argument('--speed',
                            type=float,
                            default=0,
                            help='limit of downloading throughput (mega-bits/sec). deafult is unlimited')
        args = parser.parse_args(tunables)
        fsize = args.fsize
        nflows = args.nflows
        tinc = args.tinc
        speed = args.speed
        return self.create_profile(fsize,nflows,tinc,speed)


def register():
    return ftp_sim()
