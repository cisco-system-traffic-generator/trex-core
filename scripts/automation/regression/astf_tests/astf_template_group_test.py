from .astf_general_test import CASTFGeneral_Test, CTRexScenario
from trex.astf.api import *
from nose.tools import assert_raises, nottest
from pprint import pprint

from trex.common.trex_types import ALL_PROFILE_ID

import json
import random
import string

# we can send either Python bytes type as below:
http_req = b'GET /3384 HTTP/1.1\r\nHost: 22.0.0.3\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)\r\nAccept: */*\r\nAccept-Language: en-us\r\nAccept-Encoding: gzip, deflate, compress\r\n\r\n'
# or we can send Python string containing ascii chars, as below:
http_response = 'HTTP/1.1 200 OK\r\nServer: Microsoft-IIS/6.0\r\nContent-Type: text/html\r\nContent-Length: 32000\r\n\r\n<html><pre>**********</pre></html>'


class ASTFTemplateGroup_Test(CASTFGeneral_Test):

    """Checking the template group feature """

    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        self.c = self.astf_trex;
        self.setup = CTRexScenario.setup_name
        if self.setup in ['trex16']:
            self.skip("skipping templates' tests in trex16")


    def udp_http_prog(self):
        prog_c = ASTFProgram(stream=False)
        prog_c.send_msg(http_req)
        prog_c.recv_msg(1)

        prog_s = ASTFProgram(stream=False)
        prog_s.recv_msg(1)
        prog_s.send_msg(http_response)

        return prog_c, prog_s


    def tcp_http_prog(self):
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(len(http_response))

        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        prog_s.send(http_response)

        return prog_c, prog_s


    def ip_gen(self):
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        return ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)


    def profile_gen(self, program, cps_list):
        assert len(cps_list) < 10
        ip_gen = self.ip_gen()

        templates_arr = []
        for i in range(len(cps_list)):
            temp_c = ASTFTCPClientTemplate(port = 80 + i, program = program[0], ip_gen = ip_gen, cps = cps_list[i])
            temp_s = ASTFTCPServerTemplate(program = program[1], assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = str(i))
            templates_arr.append(template)

        return ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)


    def profile_gen_mixed(self, cps_list):
        assert len(cps_list) < 10
        ip_gen = self.ip_gen()
        prog_c1, prog_s1 = self.udp_http_prog()
        prog_c2, prog_s2 = self.tcp_http_prog()

        templates_arr = []
        for i in range(len(cps_list)):
            if i % 2: 
                prog_c, prog_s = prog_c1, prog_s1
            else:
                prog_c, prog_s = prog_c2, prog_s2
        
            temp_c = ASTFTCPClientTemplate(port = 80 + i, program = prog_c, ip_gen = ip_gen, cps = cps_list[i])
            temp_s = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = str(i))
            templates_arr.append(template)

        return ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)


    def check_cnt (self,c,name,val):
        if name not in c:
            self.fail('counter %s is not in counters ' %(name) )
        if c[name] != val:
            self.fail('counter {0} expect {1} value {2}  '.format(name,val,c[name]) )


    def validate_cnt (self, val1, val2, str_err, summary_stats, stats):
        if val1 !=val2:
            stats_fail_msg = 'summary_stats:\n%s\nstats:%s\n' %(json.dumps(summary_stats, indent=4), json.dumps(stats, indent=4)) 
            self.fail('%s\n%s' % (str_err, stats_fail_msg))


    def verify_cnt_and_summary(self, names, summary_stats, stats, load=False):
        sum_all = {'udps_sndpkt': 0, 'udps_sndbyte':0, 'udps_rcvpkt': 0,
                    'udps_rcvbyte': 0, 'tcps_sndbyte': 0, 'tcps_rcvbyte': 0}

        for name in names:
            c = stats[name]['client']
            s = stats[name]['server']
            if not load:
                self.validate_cnt(c.get('udps_sndpkt', 0), s.get('udps_rcvpkt', 0), "udps_sndpkt != udps_rcvpkt", summary_stats, stats)
                self.validate_cnt(s.get('udps_sndpkt', 0), c.get('udps_rcvpkt', 0), "udps_sndpkt != udps_rcvpkt", summary_stats, stats)
                self.validate_cnt(c.get('udps_sndbyte', 0), s.get('udps_rcvbyte', 0), "udps_sndbyte != udps_rcvbyte", summary_stats, stats)
                self.validate_cnt(s.get('udps_sndbyte', 0), c.get('udps_rcvbyte', 0), "udps_sndbyte != udps_rcvbyte", summary_stats, stats)
                self.validate_cnt(c.get('tcps_sndbyte', 0), s.get('tcps_rcvbyte', 0), "tcps_sndbyte != tcps_rcvbyte", summary_stats, stats)
                self.validate_cnt(s.get('tcps_sndbyte', 0), c.get('tcps_rcvbyte', 0), "tcps_sndbyte != tcps_rcvbyte", summary_stats, stats)
            for cnt in sum_all.keys():
                sum_all[cnt] += c.get(cnt, 0)
        
        for cnt in sum_all.keys():
            self.validate_cnt(summary_stats['client'].get(cnt, 0), sum_all[cnt], "summary" + cnt + "!= sum of " + cnt, summary_stats, stats)


    def coincident(self, a, b):
        """
        This method verifies two vectors are coincident (one is a positive scalar multiple of the other)
        up to some small error allowed.
        A dot B = |A| |B| cos (t) where t is the angle between the two vectors.
        """
        ab = sum(i[0]*i[1] for i in zip(a, b))
        abab = ab*ab
        aabb = sum(i[0]*i[1] for i in zip(a, a)) * sum(i[0]*i[1] for i in zip(b, b))
        if not abab > 0.95*aabb:
            self.fail("Connections per template group not as specified in cap list.")


    def verify_cps(self, names, stats, cps_list):
        counters = {'client': {'udp': {'flows_opened': [], 'zero': False, 'name': 'udps_connects'},
                                'tcp': {'flows_opened': [], 'zero': False, 'name': 'tcps_connattempt'}},
                    'server': {'udp': {'flows_opened': [], 'zero': False, 'name': 'udps_connects'},
                                'tcp': {'flows_opened': [], 'zero': False, 'name': 'tcps_connattempt'}}}

        for name in names:
            for section in counters:
                stats_name_section = stats[name][section]
                counters_section = counters[section]
                for connection in counters_section:
                    counters_sections_connection = counters_section[connection]
                    cnt = stats_name_section.get(counters_sections_connection['name'],0)
                    if cnt == 0:
                        counters_sections_connection['zero'] = True
                    else:
                        counters_sections_connection['flows_opened'].append(cnt)

        for section in counters:
            for connection in counters[section]:
                if not counters[section][connection]['zero'] == True:
                    self.coincident(cps_list, counters[section][connection]['flows_opened'])



    def verify(self, names, summary_stats, stats, cps_list):
        self.verify_cnt_and_summary(names, summary_stats, stats)
        self.verify_cps(names, stats, cps_list)


    def randomString(self, stringLength=10):
        """Generate a random string of fixed length """
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(stringLength))


    def test_astf_template_group_general(self):
        profiles_cps_list = []
        list_of_cps_list = [[1, 2], [random.randint(1, 7), random.randint(1,7)], [1, 1.5, 2, 2.5]]

        for cps_list in list_of_cps_list:
            num_of_cps = cps_list
            udp_prof = self.profile_gen(self.udp_http_prog(), cps_list)
            tcp_prof = self.profile_gen(self.tcp_http_prog(), cps_list)
            mixed_prof = self.profile_gen_mixed(cps_list)
            profiles_cps_list.append((udp_prof, num_of_cps))
            profiles_cps_list.append((tcp_prof, num_of_cps))
            profiles_cps_list.append((mixed_prof, num_of_cps))

        for profile_cps in profiles_cps_list:
            self.c.load_profile(profile_cps[0])
            self.c.clear_stats()
            self.c.start(duration = 1, mult = 100)
            self.c.wait_on_traffic()
            #verify we got the right list of names
            tg_names_received = self.c.get_tg_names()
            tg_names_sent = [str(i) for i in range(len(profile_cps[1]))]
            assert tg_names_received == tg_names_sent
            stats = self.c.get_traffic_tg_stats(tg_names_received)
            summary_stats = self.c.get_traffic_stats()
            self.verify(names = tg_names_received, summary_stats = summary_stats,
                                stats = stats, cps_list = profile_cps[1])


    def test_astf_template_group_long(self):
        profile = self.profile_gen(self.udp_http_prog(), [1, 2])
        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 60, mult = 100)
        self.c.wait_on_traffic()
        tg_names_received = self.c.get_tg_names()
        tg_names_sent = ['0', '1']
        assert tg_names_received == tg_names_sent
        stats = self.c.get_traffic_tg_stats(tg_names_received)
        summary_stats = self.c.get_traffic_stats()
        self.verify(names = tg_names_received, summary_stats = summary_stats,
                            stats = stats, cps_list = [1, 2])


    def test_astf_template_group_load(self):
        LOAD_NUMBER = 1500
        # generate LOAD_NUMBER templates with the same cps.
        ip_gen = self.ip_gen()
        prog_c, prog_s = self.udp_http_prog()

        templates_arr = []
        for i in range(LOAD_NUMBER):
            temp_c = ASTFTCPClientTemplate(port = 80 + i, program = prog_c, ip_gen = ip_gen)
            temp_s = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = str(i))
            templates_arr.append(template)

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)

        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1, mult = 100)
        self.c.wait_on_traffic()
        
        tg_names_received = self.c.get_tg_names()
        tg_names_sent = [str(i) for i in range(LOAD_NUMBER)]
        assert tg_names_received == tg_names_sent

        summary_stats = self.c.get_traffic_stats()
        stats = self.c.get_traffic_tg_stats(tg_names_received)

        self.verify_cnt_and_summary(names = tg_names_received, summary_stats = summary_stats, stats = stats, load=True)


    def test_astf_template_group_load_dynamic_profile(self):
        LOAD_NUMBER = 1500
        # generate LOAD_NUMBER templates with the same cps.
        ip_gen = self.ip_gen()
        prog_c, prog_s = self.udp_http_prog()

        templates_arr = []
        for i in range(LOAD_NUMBER):
            temp_c = ASTFTCPClientTemplate(port = 80 + i, program = prog_c, ip_gen = ip_gen)
            temp_s = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80 + i))
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = str(i))
            templates_arr.append(template)

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = templates_arr)
        print('Creating random name for the dynamic profile')
        random_profile = self.randomString()
        print('Dynamic profile name : %s' % str(random_profile))

        self.c.reset()
        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.start(duration = 1, mult = 100, pid_input=str(random_profile))
        self.c.wait_on_traffic()

        tg_names_received = self.c.get_tg_names(pid_input=str(random_profile))
        tg_names_sent = [str(i) for i in range(LOAD_NUMBER)]
        assert tg_names_received == tg_names_sent

        summary_stats = self.c.get_traffic_stats(pid_input=str(random_profile))
        stats = self.c.get_traffic_tg_stats(tg_names_received, pid_input=str(random_profile))

        self.verify_cnt_and_summary(names = tg_names_received, summary_stats = summary_stats, stats = stats, load=True)


    def test_astf_template_group_edge_cases(self):
        ip_gen = self.ip_gen()
        prog_c, prog_s = self.tcp_http_prog()

        temp_c1 = ASTFTCPClientTemplate(port = 80, program = prog_c, ip_gen = ip_gen)
        temp_s1 = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80))
        template1 = ASTFTemplate(client_template = temp_c1, server_template = temp_s1)

        temp_c2 = ASTFTCPClientTemplate(port = 81, program = prog_c, ip_gen = ip_gen)
        temp_s2 = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 81))
        template2 = ASTFTemplate(client_template = temp_c2, server_template = temp_s2, tg_name = 'A')

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = [template1, template2])

        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1)
        self.c.wait_on_traffic()

        tg_names_received = self.c.get_tg_names()
        assert tg_names_received == ['A']

        summary_stats = self.c.get_traffic_stats()
        stats = self.c.get_traffic_tg_stats('A')
        assert list(stats.keys()) == tg_names_received

        temp_c3 =  ASTFTCPClientTemplate(port = 82, program = prog_c, ip_gen = ip_gen)
        temp_s3 =  ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 82))
        template3 = ASTFTemplate(client_template = temp_c3, server_template = temp_s3, tg_name = 'B')

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = [template3, template2])

        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1)
        self.c.wait_on_traffic()

        tg_names_received = self.c.get_tg_names()
        assert tg_names_received == ['B', 'A']
        summary_stats = self.c.get_traffic_stats()
        stats = self.c.get_traffic_tg_stats(tg_names_received)
        assert list(stats.keys()).sort() == tg_names_received.sort()
        self.verify_cnt_and_summary(names = tg_names_received, summary_stats = summary_stats, stats = stats)


    def test_astf_template_group_edge_cases_dynamic_profile(self):
        ip_gen = self.ip_gen()
        prog_c, prog_s = self.tcp_http_prog()

        temp_c1 = ASTFTCPClientTemplate(port = 80, program = prog_c, ip_gen = ip_gen)
        temp_s1 = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80))
        template1 = ASTFTemplate(client_template = temp_c1, server_template = temp_s1)

        temp_c2 = ASTFTCPClientTemplate(port = 81, program = prog_c, ip_gen = ip_gen)
        temp_s2 = ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 81))
        template2 = ASTFTemplate(client_template = temp_c2, server_template = temp_s2, tg_name = 'A')

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = [template1, template2])

        print('Creating random name for the dynamic profile')
        random_profile = self.randomString()
        print('Dynamic profile name : %s' % str(random_profile))

        self.c.reset()
        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.start(duration = 1, pid_input=str(random_profile))
        self.c.wait_on_traffic()

        tg_names_received = self.c.get_tg_names(pid_input=str(random_profile))
        assert tg_names_received == ['A']

        summary_stats = self.c.get_traffic_stats(pid_input=str(random_profile))
        stats = self.c.get_traffic_tg_stats('A', pid_input=str(random_profile))
        assert list(stats.keys()) == tg_names_received

        temp_c3 =  ASTFTCPClientTemplate(port = 82, program = prog_c, ip_gen = ip_gen)
        temp_s3 =  ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 82))
        template3 = ASTFTemplate(client_template = temp_c3, server_template = temp_s3, tg_name = 'B')

        profile = ASTFProfile(default_ip_gen = ip_gen, templates = [template3, template2])

        print('Creating random name for the dynamic profile')
        random_profile = self.randomString()
        print('Dynamic profile name : %s' % str(random_profile))

        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.clear_stats(pid_input=str(ALL_PROFILE_ID))
        self.c.start(duration = 1, pid_input=str(random_profile))
        self.c.wait_on_traffic()

        tg_names_received = self.c.get_tg_names(pid_input=str(random_profile))
        assert tg_names_received == ['B', 'A']
        summary_stats = self.c.get_traffic_stats(pid_input=str(random_profile))
        stats = self.c.get_traffic_tg_stats(tg_names_received, pid_input=str(random_profile))
        assert list(stats.keys()).sort() == tg_names_received.sort()
        self.verify_cnt_and_summary(names = tg_names_received, summary_stats = summary_stats, stats = stats)


    def test_astf_template_group_negative(self):

        ip_gen = self.ip_gen()
        prog_c, prog_s = self.tcp_http_prog()
        temp_c =  ASTFTCPClientTemplate(port = 80, program = prog_c, ip_gen = ip_gen)
        temp_s =  ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80))

        with assert_raises(ASTFErrorWrongType):
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = 1)
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s)
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        with assert_raises(ASTFErrorBadTG):
            stats = self.c.get_traffic_tg_stats('A')
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        with assert_raises(ASTFErrorBadTG):
            # no profile loaded yet
            stats = self.c.get_traffic_tg_stats(['A'])
        with assert_raises(ASTFErrorBadTG):
            #list can't be empty
            self.c.get_traffic_tg_stats([])
        with assert_raises(ASTFErrorBadTG):
            # name doesn't exists
            self.c.get_traffic_tg_stats(["name doesn't exist"])
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1)
        self.c.wait_on_traffic()
        with assert_raises(ASTFErrorBadTG):
            self.c.get_traffic_tg_stats(['A', 'B'])
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1)
        self.c.wait_on_traffic()
        self.c.get_traffic_tg_stats(['A'])
        # now let's change the profile
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='B')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        self.c.load_profile(profile)
        self.c.clear_stats()
        self.c.start(duration = 1)
        self.c.wait_on_traffic()
        with assert_raises(ASTFErrorBadTG):
            # asking for old name, shouldn't be found
            self.c.get_traffic_tg_stats(['A'])
        with assert_raises(ASTFError):
            # empty name not allowed
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='')
        with assert_raises(ASTFError):
            #name too long
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name=100*'a')


    def test_astf_template_group_negative_dynamic_profile(self):

        ip_gen = self.ip_gen()
        prog_c, prog_s = self.tcp_http_prog()
        temp_c =  ASTFTCPClientTemplate(port = 80, program = prog_c, ip_gen = ip_gen)
        temp_s =  ASTFTCPServerTemplate(program = prog_s, assoc = ASTFAssociationRule(port = 80))
        self.c.reset()

        with assert_raises(ASTFErrorWrongType):
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name = 1)
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s)
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        with assert_raises(ASTFErrorBadTG):
            stats = self.c.get_traffic_tg_stats('A')
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        with assert_raises(ASTFErrorBadTG):
            # no profile loaded yet
            stats = self.c.get_traffic_tg_stats(['A'])
        with assert_raises(ASTFErrorBadTG):
            #list can't be empty
            self.c.get_traffic_tg_stats([])
        with assert_raises(ASTFErrorBadTG):
            # name doesn't exists
            self.c.get_traffic_tg_stats(["name doesn't exist"])
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        print('Creating random name for the dynamic profile')
        random_profile = self.randomString()
        print('Dynamic profile name : %s' % str(random_profile))
        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.clear_stats(pid_input=str(ALL_PROFILE_ID))
        self.c.start(duration = 1, pid_input=str(random_profile))
        self.c.wait_on_traffic()
        with assert_raises(ASTFErrorBadTG):
            self.c.get_traffic_tg_stats(['A', 'B'], pid_input=str(random_profile))

        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='A')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.clear_stats(pid_input=str(ALL_PROFILE_ID))
        self.c.start(duration = 1, pid_input=str(random_profile))
        self.c.wait_on_traffic()
        self.c.get_traffic_tg_stats(['A'], pid_input=str(random_profile))

        # now let's change the profile
        template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='B')
        profile = ASTFProfile(default_ip_gen = ip_gen, templates = template)
        self.c.load_profile(profile, pid_input=str(random_profile))
        self.c.clear_stats(pid_input=str(ALL_PROFILE_ID))
        self.c.start(duration = 1, pid_input=str(random_profile))
        self.c.wait_on_traffic()
        with assert_raises(ASTFErrorBadTG):
            # asking for old name, shouldn't be found
            self.c.get_traffic_tg_stats(['A'])
        with assert_raises(ASTFError):
            # empty name not allowed
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name='')
        with assert_raises(ASTFError):
            #name too long
            template = ASTFTemplate(client_template = temp_c, server_template = temp_s, tg_name=100*'a')
