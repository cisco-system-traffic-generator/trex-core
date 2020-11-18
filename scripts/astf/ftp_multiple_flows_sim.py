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

    def create_profile(self,fsize,nflows,tinc):
        self.tune_tcp(0)
        loop = int(fsize*(1024*1024)/self.max_buf_size)

        # client commands
        prog_c = ASTFProgram()
        if tinc != 0:
            prog_c.delay_rand(0,tinc*1000*1000)  # delay from 0 to random(tinc) seconds
        prog_c.send(self.ftp_get)
        prog_c.recv(self.max_buf_size*loop)

        # server commands
        prog_s = ASTFProgram()
        prog_s.recv(len(self.ftp_get))
        prog_s.set_send_blocking (False) # Set to non-blocking to maximize thput in single flow
        prog_s.set_var("var2",loop-1); # set var 0 to loop
        prog_s.set_label("a:");
        prog_s.send('',size=self.max_buf_size,fill='*')
        prog_s.jmp_nz("var2","a:") # dec var "var2". in case it is *not* zero jump a:
        prog_s.set_send_blocking (True) # back to blocking mode
        prog_s.send('',size=self.max_buf_size,fill='*')

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

    def get_profile(self,**kwargs):
        fsize = kwargs.get('fsize',1000)
        nflows = kwargs.get('nflows',1)
        tinc = kwargs.get('tinc',0)
        return self.create_profile(fsize,nflows,tinc)


def register():
    return ftp_sim()
