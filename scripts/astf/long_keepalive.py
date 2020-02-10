# Testing keepalives longer than 253 seconds 

from trex.astf.api import *

class Prof1():
    def __init__(self):
        pass  # tunables

    def create_profile(self, keep_alive, delay_sec):
        # client commands
        prog_c = ASTFProgram()
        prog_c.connect()
        prog_c.delay(1000000 * delay_sec)

        # server commands
        prog_s = ASTFProgram()
        prog_s.accept()
        prog_s.wait_for_peer_close()

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.0.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c,  ip_gen=ip_gen, limit = 1)
        temp_s = ASTFTCPServerTemplate(program=prog_s)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # client will send keep-alives to server
        c_glob_info = ASTFGlobalInfo()
        c_glob_info.tcp.keepinit = keep_alive
        c_glob_info.tcp.keepidle = keep_alive
        c_glob_info.tcp.keepintvl = keep_alive

        s_glob_info = ASTFGlobalInfo()
        s_glob_info.tcp.keepinit = keep_alive
        s_glob_info.tcp.keepidle = keep_alive
        s_glob_info.tcp.keepintvl = keep_alive

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=template,
                                default_c_glob_info=c_glob_info,
                                default_s_glob_info=s_glob_info
                                )
        return profile

    def get_profile(self, **kwargs):
        keep_alive = kwargs.get('keepalive', 300)  # This keepalive is intentionally long enough for test
        delay_sec  = kwargs.get('delay', 5)
        return self.create_profile(keep_alive, delay_sec)


def register():
    return Prof1()
