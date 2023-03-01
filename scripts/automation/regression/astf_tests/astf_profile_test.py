from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command
from trex.astf.api import *
import time
from trex.stl.trex_stl_packet_builder_scapy import ip2int, int2ip
import re
import pprint
import random
import string
import glob
from nose.tools import assert_raises, nottest
from functools import wraps
from trex.astf.trex_astf_client import Tunnel

class DynamicProfileTest:

    def __init__(self,
                 client, 
                 min_rand_duration,
                 max_rand_duration,
                 min_tick,
                 max_tick,
                 duration,
                 ):

        self.c = client
        self.min_rand_duration = min_rand_duration 
        self.max_rand_duration = max_rand_duration
        self.min_tick = min_tick
        self.max_tick = max_tick
        self.duration = duration

    def is_last_profile_msg(self, msg):
        m = re.match("Moved to state: Loaded", msg)
        if m:
            return True
        else:
           return False


    def run_test (self):

        passed = True
        c = self.c;

        try:
             # prepare our ports
             c.reset()

             port_id = 0
             profile_id = 0
             tick_action = 0
             profiles ={}
             tick = 1
             max_tick = self.duration
             stop = False

             c.clear_stats()

             while True:

                 if tick > tick_action and (tick<max_tick):
                     profile_name = str(profile_id)
                     port_n = 9000 + profile_id
                     tunables = {'port': port_n}
                     duration = random.randrange(self.min_rand_duration,self.max_rand_duration)

                     c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'http_simple_port_tunable.py'),
                                                 tunables=tunables,
                                                 pid_input=profile_name)
                     profiles[profile_id] = 1
                     print(" {} new profile {} {} {}".format(tick,profile_name,duration,len(profiles)))

                     c.start(duration = duration, pid_input=profile_name)
                     profile_id += 1
                     tick_action = tick + random.randrange(self.min_tick,self.max_tick) # next action 

                 time.sleep(1);
                 tick += 1


                 # check events 
                 while True:
                    event = c.pop_event ()
                    if event == None:
                        break;
                    else:
                        profile = self.is_last_profile_msg(event.msg)
                        if profile and tick>=max_tick:
                            print("stop");
                            stop=True
                 if stop:
                     break;

             r = c.astf_profile_state.values()
             assert( max(r) == 1 )

             stats = c.get_stats()
             diff = stats[1]["ipackets"] - stats[0]["opackets"] 
             assert(diff<2)


        except Exception as e:
            passed = False
            print(e)

        finally:
            c.reset()

        if c.get_warnings():
            print("\n\n*** test had warnings ****\n\n")
            for w in c.get_warnings():
                print(w)

        if passed and not c.get_warnings():
            return True
        else:
            return False


class ASTFProfile_Test(CASTFGeneral_Test):
    """Checking some profiles """
    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        c = self.astf_trex;
        assert(c.is_connected())
        self.weak = self.is_VM
        self.skip_test_trex_522 =False;
        setup= CTRexScenario.setup_name;
        if setup in ['trex16']:
            self.skip_test_trex_522 =True;
        if setup in ['trex16','trex41']:
            self.weak = True
        self.driver_params = self.get_driver_params()
        self.duration=None
        

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
        if not self.skip_test_trex_522:
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
            self.fail('ERROR in counters')

        # no errors 
        if is_tcp:
            self.check_tcp(stats)

        if is_udp:
            self.check_udp(stats)

    def check_latency_for_errors (self,port_id,counters):
        for k in ['m_l3_cs_err','m_l4_cs_err','m_length_error','m_no_id','m_no_ipv4_option','m_no_magic','m_seq_error','m_tx_pkt_err']:
            if counters[k] > 0:
                self.fail('error in latency port-%s, key: %s, val: %s' % (port_id, k, counters[k]))

    def get_max_latency (self):
        return (self.driver_params['latency_9k_max_average']*2)

    def allow_latency(self):
        return self.driver_params['latency_9k_enable'] and (not self.weak)

    def check_latency_stats (self, all_stats):
        if not self.allow_latency():
            return

        for port_id, port_stats in all_stats.items():
            hist = port_stats['hist']

            for k in ['s_avg', 'min_usec']:
               if hist[k] > self.get_max_latency ():
                   self.latency_error=hist[k];
                   
            for k in ['s_max']:
               if hist[k] > self.get_max_latency ():
                   self.latency_error=hist[k];

            # todo check the latency histogram 
            counters = port_stats['stats']

            self.check_latency_for_errors (port_id,counters)

    def try_few_times(func):
        @wraps(func)
        def wrapped(self, *args, **kwargs):
            # we see random failures with mlx, so do retries on it as well

            cnt =0;
            for i in range(0,5):
               self.latency_error = 0
               self.clear_fails()
               func(self, *args, **kwargs)
               if self.latency_error==0:
                   break;
               print(" retry due to latency issue {}".format(self.latency_error))

               cnt += 1 # continue in case of latency error, due to trex-541 

            if cnt==5:
                self.fail('latency is bigger {} than norma {} '.format(self.latency_error,self.get_max_latency()) )
        return wrapped


    @try_few_times
    def run_astf_profile(self, profile_name, m, is_udp, is_tcp, ipv6 =False, check_counters=True, nc = False, short_duration = False, tunnel_info = None, latency_pps = 1000):
        c = self.astf_trex;

        if ipv6 and self.driver_params.get('no_ipv6', False):
            return

        c.reset();
        d = self.get_duration()
        if short_duration:
            d = self.get_short_duration()

        print('starting profile %s for duration %s' % (profile_name, d))
        c.load_profile(self.get_profile_by_name(profile_name))
        c.clear_stats()
        c.start(duration = d,nc= nc,mult = m,ipv6 = ipv6,latency_pps = latency_pps)
        if tunnel_info:
            tunnel_type, clients_list = tunnel_info["tunnel_type"], tunnel_info["clients_list"]
            c.update_tunnel_client_record(clients_list, tunnel_type)
        c.wait_on_traffic()
        stats = c.get_stats()
        if check_counters:
            self.check_counters(stats,is_udp,is_tcp)
            self.check_latency_stats(stats['latency'])
            if c.get_warnings():
                print('\n\n*** test had warnings ****\n\n')
                for w in c.get_warnings():
                    print(w)


    @try_few_times
    def run_astf_profile_dynamic_profile(self, profile_name, m, is_udp, is_tcp, ipv6 =False, check_counters=True, nc = False, pid_input=None,short_duration = False):
        c = self.astf_trex;

        if ipv6 and self.driver_params.get('no_ipv6', False):
            return

        c.reset();

        d = self.get_duration()
        if short_duration:
            self.get_short_duration()
           
        print('starting profile %s for duration %s' % (profile_name, d))
        if pid_input:
            print('dynamic profile %s ' % (pid_input))
        c.load_profile(self.get_profile_by_name(profile_name),pid_input=pid_input)
        c.start(duration = d,nc= nc,mult = m,ipv6 = ipv6,latency_pps = 1000,pid_input=pid_input)
        c.wait_on_traffic()
        stats = c.get_stats(pid_input=pid_input)
        if check_counters:
            self.check_counters(stats,is_udp,is_tcp)
            self.check_latency_stats(stats['latency'])
            if c.get_warnings():
                print('\n\n*** test had warnings ****\n\n')
                for w in c.get_warnings():
                    print(w)


    def get_simple_params(self):
        tests = [ {'name': 'http_simple.py','is_tcp' :True,'is_udp':False,'default':True},
                  {'name': 'udp_pcap.py','is_tcp' :False,'is_udp':True,'default':False}
                  ]
        return (tests);

    def get_sfr_params(self):
        tests = [ {'name': 'sfr.py','is_tcp' :True,'is_udp':False,'m':1.0},
                  {'name': 'sfr_full.py','is_tcp' :True,'is_udp':True,'m':2.0}
                ]
        return (tests);

    def get_short_duration(self):
        if self.duration:
            return self.duration
        return random.randint(10, 15)

    def get_duration (self):
        if self.duration:
            return self.duration
        return random.randint(20, 60)

    def randomString(self, stringLength=10):
        """Generate a random string of fixed length """
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(stringLength))

    def test_astf_prof_simple(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_http')
        tests = self.get_simple_params() 
        for o in tests:
            self.run_astf_profile(o['name'],mult,o['is_udp'],o['is_tcp'])

        mult  = self.get_benchmark_param('multiplier',test_name = 'test_ipv6_tcp_http')
        for o in tests:
            self.run_astf_profile(o['name'],mult,o['is_udp'],o['is_tcp'],ipv6=True)

    def test_astf_prof_sfr(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_sfr')
        tests = self.get_sfr_params() 
        for o in tests:
            self.run_astf_profile(o['name'],mult*o['m'],o['is_udp'],o['is_tcp'])

    @nottest
    def test_astf_prof_no_crash_sfr(self):
        mult  = self.get_benchmark_param('multiplier',test_name = 'test_tcp_sfr_no_crash')
        tests = self.get_sfr_params() 
        for o in tests:
            for i_ipv6 in (True,False):
                self.run_astf_profile(o['name'],mult*o['m'],o['is_udp'],o['is_tcp'], ipv6=i_ipv6,check_counters=False, nc=True, short_duration=True)

    def get_profiles_from_sample_filter (self):
        profiles_path = os.path.join(CTRexScenario.scripts_path, 'astf/')
        py_profiles = glob.glob(profiles_path + "/*.py")
        return(py_profiles)


    def test_astf_prof_profiles(self):
        if self.weak:
            self.skip('not enough memory for this test')

        profiles = self.get_profiles_from_sample_filter ()
        duration=self.duration;
        # skip tests that are too long or not traffic profiles
        skip_list= ['http_by_l7_percent.py',
                    'http_simple_split.py',
                    'http_simple_split_per_core.py',
                    'http_manual_tunables4.py',
                    'http_simple_cc.py',
                    'http_simple_cc1.py',
                    'nginx.py',
                    'nginx_wget.py',
                    'sfr_full.py',
                    'sfr.py',
                    'param_mss_err2.py',
                    'http_simple_rss.py',
                    'udp_rtp.py',
                    'http_simple_src_mac.py',
                    'http_eflow3.py',
                    'wrapping_it_up_example.py',
                    'udp_topo.py', # Not traffic profile, but topology
                    'udp_topo_traffic.py',
                    'http_simple_emu.py',  # Emu profiles
                    'http_simple_emu_ipv6.py',
                    'http_many.py',
                    'gtpu_topo.py',
                    'gtpu_topo_latency.py',
                    ]
        self.duration=1
        try:
            for profile in profiles:
                fname=os.path.split(profile)[1]
                if fname in skip_list:
                    print(" skipping {}".format(fname))
                    continue;
                print(" running {}".format(fname))
                self.run_astf_profile(fname, m=1, is_udp=False, is_tcp=False, ipv6 =False, check_counters=False, nc = True)
        finally:
            self.duration = duration

    def test_gtpu_mode_tunnel_topo(self):
        c = self.astf_trex
        if not c.is_tunnel_supported(tunnel_type=1)['is_tunnel_supported']:
            self.skip("tunnel is no supported")

        duration = self.duration
        self.duration = 20
        fname = 'http_simple.py'
        try:
            c.reset()
            c.activate_tunnel(tunnel_type=1, activate=True, loopback=True)
            #loading tunnel topology that holds the tunnel context
            c.tunnels_topo_load(self.get_profile_by_name("gtpu_topo.py"))
            print(" running {}".format(fname))
            self.run_astf_profile(fname, m=1000, is_udp=False, is_tcp=True, latency_pps=0)
        finally:
            self.duration = duration
            c.reset()
            c.tunnels_topo_clear()
            c.activate_tunnel(tunnel_type=1, activate=False, loopback=False)

    def test_gtpu_mode_tunnel_topo_latency(self):
        c = self.astf_trex
        if not c.is_tunnel_supported(tunnel_type=1)['is_tunnel_supported']:
            self.skip("tunnel is no supported")

        duration = self.duration
        self.duration = 20
        fname = 'http_simple.py'
        try:
            c.reset()
            c.activate_tunnel(tunnel_type=1, activate=True, loopback=True)
            #loading tunnel topology that holds the tunnel context and latency client
            c.tunnels_topo_load(self.get_profile_by_name("gtpu_topo_latency.py"))
            self.run_astf_profile(fname, m=1000, is_udp=False, is_tcp=True)
        finally:
            self.duration = duration
            c.reset()
            c.tunnels_topo_clear()
            c.activate_tunnel(tunnel_type=1, activate=False, loopback=False)

    def test_gtpu_mode(self):
        c = self.astf_trex
        if not c.is_tunnel_supported(tunnel_type=1)['is_tunnel_supported']:
            self.skip("tunnel is no supported")
        
        duration = self.duration
        self.duration = 20
        fname = 'http_simple.py'
        version = 4
        sport = 5000
        add_client_cnt = 255
        client_db = dict()
        ip_prefix = "16.0.0."
        sip = "11.11.0.1"
        dip = "1.1.1.11"
        teid = 0
        while teid <= add_client_cnt:
            c_ip = ip_prefix + str(teid)
            c_ip = ip2int(c_ip)
            client_db[c_ip] = Tunnel(sip, dip, sport, teid, version)
            teid += 1

        tunnel_info = {"clients_list": client_db, "tunnel_type" : 1}

        try:
            c.reset()
            c.activate_tunnel(tunnel_type=1, activate=True, loopback=True)
            print(" running {}".format(fname))
            self.run_astf_profile(fname, m=1000, is_udp=False, is_tcp=True, tunnel_info=tunnel_info, latency_pps=0)
        finally:
            self.duration = duration
            c.reset()
            c.activate_tunnel(tunnel_type=1, activate=False, loopback=False)


    def test_astf_prof_profiles_dynamic_profile(self):
        if self.weak:
            self.skip('not enough memory for this test')

        profiles = self.get_profiles_from_sample_filter ()
        duration=self.duration;
        # skip tests that are too long or not traffic profiles
        skip_list= ['http_by_l7_percent.py',
                    'http_simple_split.py',
                    'http_simple_split_per_core.py',
                    'http_manual_tunables4.py',
                    'http_many.py',
                    'http_simple_cc.py',
                    'http_simple_cc1.py',
                    'nginx.py',
                    'nginx_wget.py',
                    'sfr_full.py',
                    'sfr.py',
                    'param_mss_err2.py',
                    'http_simple_rss.py',
                    'udp_rtp.py',
                    'http_simple_src_mac.py',
                    'http_eflow3.py',
                    'wrapping_it_up_example.py',
                    'udp_topo.py', # Not traffic profile, but topology
                    'udp_topo_traffic.py',
                    'http_simple_emu.py',  # Emu profiles
                    'http_simple_emu_ipv6.py',
                    'gtpu_topo.py',
                    'gtpu_topo_latency.py',
                    ]
        self.duration=1
        try:
            for profile in profiles:
                fname=os.path.split(profile)[1]
                if fname in skip_list:
                    print(" skipping {}".format(fname))
                    continue;
                print(" running {}".format(fname))
                random_profile = self.randomString()
                self.run_astf_profile_dynamic_profile(fname, m=1, is_udp=False, is_tcp=False, ipv6 =False, check_counters=False, nc = True, pid_input=str(random_profile))
        finally:
            self.duration = duration


    @try_few_times
    def do_latency(self,duration,stop_after=None):
        if self.weak:
           self.skip('not accurate latency')
        if not self.allow_latency():
            self.skip('not allowed latency here')

        c = self.astf_trex;
        # check polling mode 
        ticks=0;
        c.reset();
        profile_name='http_simple.py'
        print('starting profile %s ' % (profile_name))
        c.load_profile(self.get_profile_by_name(profile_name))
        c.clear_stats()
        c.start(duration = duration,nc= True,mult = 1000,ipv6 = False,latency_pps = 1000)
        while c.is_traffic_active():
            stats = c.get_stats()
            lat = stats['latency']
            print(" client active flows {},".format(stats['traffic']['client']['m_active_flows']))
            for port_id, port_stats in lat.items():
                stats_output = 'port_id: %s' % port_id
                hist = port_stats['hist']
                stats = port_stats['stats']
                self.check_latency_for_errors(port_id, stats)
                for t in ['s_max','s_avg']:
                    stats_output += ", {}: {:.0f}".format(t, hist[t])
                    if hist[t] > self.get_max_latency():
                        self.latency_error = hist[t]
                print(stats_output)
            print("")
            time.sleep(1);
            ticks += 1
            if stop_after:
                if ticks > stop_after:
                  c.stop()
                  break;

    @try_few_times
    def do_latency_dynamic_profile(self,duration,stop_after=None,pid_input=None):
        if self.weak:
           self.skip('not accurate latency')
        if not self.allow_latency():
            self.skip('not allowed latency here')

        c = self.astf_trex;
        # check polling mode 
        ticks=0;
        c.reset();
        profile_name='http_simple.py'
        print('starting profile %s ' % (profile_name))
        if pid_input:
            print('dynamic profile %s ' % (pid_input))
        c.load_profile(self.get_profile_by_name(profile_name),pid_input=pid_input)
        c.start(duration = duration,nc= True,mult = 1000,ipv6 = False,latency_pps = 1000,pid_input=pid_input)
        while c.is_traffic_active():
            stats = c.get_stats(pid_input=pid_input)
            lat = stats['latency']
            print(" client active flows {},".format(stats['traffic']['client']['m_active_flows']))
            for port_id, port_stats in lat.items():
                stats_output = 'port_id: %s' % port_id
                hist = port_stats['hist']
                stats = port_stats['stats']
                self.check_latency_for_errors(port_id, stats)
                for t in ['s_max','s_avg']:
                    stats_output += ", {}: {:.0f}".format(t, hist[t])
                    if hist[t] > self.get_max_latency():
                        self.latency_error = hist[t]
                print(stats_output)
            print("")
            time.sleep(1);
            ticks += 1
            if stop_after:
                if ticks > stop_after:
                  c.stop(pid_input=pid_input)
                  break;

    def test_astf_prof_latency(self):
        self.do_latency(30,stop_after=None)

    def test_astf_prof_latency_dynamic_profile(self):
        for index in range(5):
            self.do_latency_dynamic_profile(3,stop_after=None,pid_input=str(index))

    def test_astf_prof_latency_stop(self):
        self.do_latency(20,stop_after=10)

    def test_astf_prof_latency_stop_dynamic_profile(self):
        for index in range(5):
            self.do_latency_dynamic_profile(2,stop_after=1,pid_input=str(index))

    def test_astf_prof_only_latency(self):
        if self.weak:
           self.skip('not accurate latency')

        c = self.astf_trex;
        c.reset();
        ticks=0;
        c.clear_stats()
        c.stop_latency()
        c.start_latency(mult = 1000)
        while True:
            time.sleep(1);
            stats = c.get_stats()
            l=stats['latency']
            print(" client active flows {},".format(stats['traffic']['client']['m_active_flows']))
            for k in l:
               hist=l[k]['hist']
               stats =l[k]['stats']
               self.check_latency_for_errors(k,stats)
            print("")
            ticks += 1
            if ticks > 10:
                break;
        c.stop_latency()

    def test_random_add_remove_dynamic_profile (self):
        if self.weak:
            self.skip('not enough memory for this test')
        if not self.allow_latency():
            self.skip('not allowed latency here')

        test = DynamicProfileTest(self.astf_trex,1,50,1,2,120);
        test.run_test()
        
