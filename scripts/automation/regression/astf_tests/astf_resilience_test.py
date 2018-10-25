from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command
from trex.astf.api import *
import time
from trex.stl.trex_stl_packet_builder_scapy import ip2int, int2ip

class ASTFResilience_Test(CASTFGeneral_Test):
    """Checking stability of ASTF in non-usual conditions """
    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        self.weak = self.is_VM

    def ip_gen(self, client_base, server_base, client_ips, server_ips):
        assert client_ips>0
        assert server_ips>0
        ip_gen_c = ASTFIPGenDist(ip_range = [client_base, int2ip(ip2int(client_base) + client_ips - 1)])
        ip_gen_s = ASTFIPGenDist(ip_range = [server_base, int2ip(ip2int(server_base) + server_ips - 1)])
        return ASTFIPGen(dist_client = ip_gen_c,
                         dist_server = ip_gen_s)


    def progs_gen(self, msg_len = 16):
        msg = 'x' * msg_len

        prog_c = ASTFProgram(side = 'c')
        prog_c.send(msg)
        prog_c.recv(len(msg))

        prog_s = ASTFProgram(side = 's')
        prog_s.recv(len(msg))
        #prog_s.delay(15000000)
        prog_s.send(msg)

        return prog_c, prog_s


    def profile_gen(self, client_ips, server_ips, templates):
        ip_gen = self.ip_gen('16.0.0.1', '48.0.0.1', client_ips, server_ips)
        prog_c, prog_s = self.progs_gen()

        templates_arr = []
        for i in range(templates):
            temp_c = ASTFTCPClientTemplate(program = prog_c, ip_gen = ip_gen, cps = i + 1)
            temp_s = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s)
            templates_arr.append(template)

        return ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)

    def test_astf_params(self):
        print('')

        for client_ips in (1<<8, 1<<16):
            for server_ips in (1<<8, 1<<16):
                for templates in (1, 1<<8, 1<<12):
                    if self.weak and templates > 1<<8:
                        continue

                    params = {
                        'client_ips': client_ips,
                        'server_ips': server_ips,
                        'templates': templates,
                    }

                    print('Creating profile with params: %s' % params)
                    profile = self.profile_gen(**params)
                    profile_str = profile.to_json_str()
                    print('Profile size: %s' % len(profile_str))

                    start_time = time.time()
                    self.astf_trex.load_profile(profile)
                    print('Load took: %g' % round(time.time() - start_time, 3))
                    start_time = time.time()
                    self.astf_trex.start(duration = 1, nc = True)
                    print('Start took: %g' % round(time.time() - start_time, 3))
                    self.astf_trex.stop()


