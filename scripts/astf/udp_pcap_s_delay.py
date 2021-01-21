# Example for creating your program from pcap file with the option of adding server delay which enlarge each flow duration.  

from trex.astf.api import *
import argparse

class Prof1():

    def get_profile(self, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)

        args = parser.parse_args(tunables)

        delay =  kwargs.get('s_delay', None)
        if delay is not None:
            delay = ASTFCmdDelay(int(delay))

        file = kwargs.get('file', "../avl/delay_dns_0.pcap")
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen, s_delay = delay,
                            cap_list=[ASTFCapInfo(file=file,
                            cps=1)])



def register():
    return Prof1()
