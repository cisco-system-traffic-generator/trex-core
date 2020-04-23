import os, sys
import time
import random
import string

from .astf_general_test import CASTFGeneral_Test, CTRexScenario
from nose.tools import assert_raises
from trex.astf.api import *
from trex.stl.trex_stl_packet_builder_scapy import ip2int, int2ip

class ASTFResilience_Test(CASTFGeneral_Test):
    """Checking stability of ASTF in non-usual conditions """
    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        self.weak = self.is_VM
        setup = CTRexScenario.setup_name
        if 'no_resilience' in CTRexScenario.modes:
            self.skip('not enough memory for this test')
        if setup in ['trex12']:
            self.weak = True
        self.low_memory = self.weak
        if setup in ['trex41']:
            self.low_memory = True # trex-41 uses the memory for the driver and crash


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
                    if self.weak:
                      if (client_ips > (1<<8)) and (server_ips >(1<<8)) :
                            continue;

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


    def randomString(self, stringLength=10):
        """Generate a random string of fixed length """
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(stringLength))


    def test_astf_params_dynamic_profile(self):
        print('')

        for client_ips in (1<<8, 1<<16):
            for server_ips in (1<<8, 1<<16):
                for templates in (1, 1<<8, 1<<12):
                    if self.weak:
                        if ( (templates > 1<<8) or 
                             (server_ips > 1<<8 ) or 
                             (client_ips > 1<<8 ) ):
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
                    print('Creating random name for the dynamic profile')
                    random_profile = self.randomString()
                    print('Dynamic profile name : %s' % str(random_profile))
                    self.astf_trex.load_profile(profile, pid_input=str(random_profile))
                    print('Load took: %g' % round(time.time() - start_time, 3))
                    start_time = time.time()
                    self.astf_trex.start(duration = 1, nc = True, pid_input=str(random_profile))
                    print('Start took: %g' % round(time.time() - start_time, 3))
                    self.astf_trex.stop(pid_input=str(random_profile))

        self.astf_trex.reset()


    def test_double_start_stop(self):
        print('')
        c = self.astf_trex
        c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'))
        c.start(duration = 20)
        with assert_raises(TRexError):
            c.start()
        c.stop()
        c.stop()


    def test_double_start_stop_dynamic_profile(self):
        print('')
        c = self.astf_trex
        random_profile_1 = self.randomString()
        print('Dynamic profile(1) : %s' % str(random_profile_1))
        c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'), pid_input=str(random_profile_1))
        c.start(duration = 20, pid_input=str(random_profile_1))

        with assert_raises(TRexError):
            random_profile_2 = self.randomString()
            print('Dynamic profile(2) : %s' % str(random_profile_2))
            c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'), pid_input=str(random_profile_2))
            c.start(pid_input=str(random_profile_2))

        c.stop(pid_input=str(random_profile_1))
        c.stop(pid_input=str(random_profile_2))


    def test_stress_start_stop(self):
        print('')
        c = self.astf_trex
        c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'))
        for _ in range(99):
            c.start()
            c.stop()

    def test_stress_start_stop_dynamic_profile(self):
        print('')
        c = self.astf_trex

        profiles =1000
        if self.low_memory:
            profiles = 100
 
        for n in range(profiles):
            profile_n = self.randomString()
            port_n = 9000 + n
            tunables = {'port': port_n}
            print('Dynamic profile added : %s' % str(profile_n))
            print('Port added : %s' % str(port_n))
            c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'http_simple_port_tunable.py'),
                           tunables=tunables,
                           pid_input=str(profile_n))
            c.start(duration = 20, pid_input=str(profile_n))

        c.wait_on_traffic()
        c.stop(pid_input = "*")
        c.reset()

