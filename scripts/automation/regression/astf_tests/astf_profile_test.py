from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command
from trex.astf.api import *
import time
from trex.stl.trex_stl_packet_builder_scapy import ip2int, int2ip
import pprint


def get_astf_profiles ():
    profiles_path = os.path.join(CTRexScenario.scripts_path, 'astf/')
    return(profiles_path)
    #py_profiles = glob.glob(profiles_path + "/*.py")
    #yaml_profiles = glob.glob(profiles_path + "yaml/*.yaml")
    #return py_profiles + yaml_profiles


class ASTFProfile_Test(CASTFGeneral_Test):
    """Checking some profiles """
    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        self.astf_trex.acquire()
        self.weak = self.is_VM
        self.skip_test_trex_522 =False;
        setup= CTRexScenario.setup_name;
        if setup in ['trex19','trex07','trex23']:
            self.skip_test_trex_522 =True;

    def get_profile_by_name (self,name):
        profiles_path = os.path.join(CTRexScenario.scripts_path, 'astf/'+name)
        return(profiles_path)


    def check_cnt (self,c,name,val):
        if name not in c:
            self.fail('counter %s is not in counters ' %(name) )
        if c[name] != val:
            self.fail('counter {0} expect {1} value {2}  '.format(name,val,c[name]) )

    def validate_cnt (self,val1,val2,str_err):
        if val1 !=val2:
            self.fail('%s ' %(str_err))

    def check_tcp (self,stats):
        t=stats['traffic']
        c=t['client']
        self.check_cnt(c,'m_active_flows',0)
        self.check_cnt(c,'tcps_connects',c['tcps_closed'])
        s=t['server']
        self.check_cnt(s,'m_active_flows',0)
        self.check_cnt(s,'tcps_accepts',s['tcps_closed'])

        self.validate_cnt (c['tcps_sndbyte'],s['tcps_rcvbyte'],"tcps_sndbyte != tcps_rcvbyte")
        self.validate_cnt (s['tcps_sndbyte'],c['tcps_rcvbyte'],"tcps_sndbyte != tcps_rcvbyte")


    def check_udp (self,stats):
        t=stats['traffic']
        c=t['client']
        self.check_cnt(c,'m_active_flows',0)
        self.check_cnt(c,'udps_connects',c['udps_closed'])
        s=t['server']
        self.check_cnt(s,'m_active_flows',0)
        self.check_cnt(s,'udps_accepts',s['udps_closed'])
        self.validate_cnt (c['udps_sndpkt'],s['udps_rcvpkt'],"udps_sndpkt != udps_rcvpkt")
        self.validate_cnt (s['udps_sndpkt'],c['udps_rcvpkt'],"udps_sndpkt != udps_rcvpkt")
        self.validate_cnt (c['udps_sndbyte'],s['udps_rcvbyte'],"udps_sndbyte != udps_rcvbyte")
        self.validate_cnt (s['udps_sndbyte'],c['udps_rcvbyte'],"udps_sndbyte != udps_rcvbyte")

    def check_counters (self,stats,is_udp,is_tcp):

        c=self.astf_trex;
        valid_error =[]
        if self.weak:
           valid_error += ['__last','err_no_tcp','err_redirect_rx','tcps_rexmttimeo','tcps_sndrexmitbyte','tcps_sndrexmitpack','udps_keepdrops'];
        if self.skip_test_trex_522:
           valid_error += ['udps_keepdrops']

        r=c.is_traffic_stats_error(stats['traffic'])
        if (not r[0]):
            is_error = False
        else:
            if len(valid_error):
               is_error = (len(set(valid_error).intersection(r[1].keys()))>0) # accept some errors in vm
            else:
               is_error = True

        if is_error:
            pprint.pprint(stats)
            pprint.pprint(r[1])
            self.fail('ERORR in counerts  ' )

        # no errors 
        if is_tcp:
            self.check_tcp(stats)

        if is_udp:
            self.check_udp(stats)



    def run_astf_profile(self,profile_name,m,d,is_udp,is_tcp,ipv6 =False,check_counters=True):
        c=self.astf_trex;

        try:
            c.reset();
            c.load_profile(self.get_profile_by_name(profile_name))
            c.clear_stats()
            c.start(duration = d,nc= False,mult = m,ipv6 = ipv6)
            c.wait_on_traffic()
            stats = c.get_stats()
            pprint.pprint(stats['traffic'])
            if check_counters:
              self.check_counters(stats,is_udp,is_tcp)
              if c.get_warnings():
                 print('\n\n*** test had warnings ****\n\n')
                 for w in c.get_warnings():
                    print(w)

        except TRexError as e:
            assert False, str(e)
            print(e)

        except AssertionError as e:
            assert False, str(e)
            print(e)
        print("PASS");

    def get_simple_params(self):
        tests = [ {'name': 'http_simple.py','is_tcp' :True,'is_udp':False,'default':True},
                  {'name': 'udp_pcap.py','is_tcp' :False,'is_udp':True,'default':False}]
        return (tests);

    def get_sfr_params(self):
        tests = [ {'name': 'sfr.py','is_tcp' :True,'is_udp':False,'m':1.0},
                  {'name': 'sfr_full.py','is_tcp' :True,'is_udp':True,'m':0.5}]
        return (tests);

    def get_duration (self):
        #return (120)
        return (10)

    def test_astf_simple(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_http')
        tests = self.get_simple_params() 
        d= self.get_duration ()
        for o in tests:
           self.run_astf_profile(o['name'],mult,d,o['is_udp'],o['is_tcp'])

        mult  = self.get_benchmark_param('multiplier',test_name = 'test_ipv6_tcp_http')
        for o in tests:
          self.run_astf_profile(o['name'],mult,d,o['is_udp'],o['is_tcp'],ipv6=True)

    def test_astf_sfr(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_sfr')
        tests = self.get_sfr_params() 
        d= self.get_duration ()
        for o in tests:
           self.run_astf_profile(o['name'],mult*o['m'],d,o['is_udp'],o['is_tcp'])

    def test_astf_sfr_no_crash(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_sfr_no_crash')
        tests = self.get_sfr_params() 
        d= self.get_duration ()
        for o in tests:
           for i_ipv6 in (True,False):
               self.run_astf_profile(o['name'],mult*o['m'],d,o['is_udp'],o['is_tcp'],ipv6=i_ipv6,check_counters=False)

