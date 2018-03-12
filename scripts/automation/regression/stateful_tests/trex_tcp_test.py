#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test, CTRexScenario
from CPlatform import CStaticRouteConfig, CNatConfig
from .tests_exceptions import *
#import sys
import time
import copy
from nose.tools import nottest
import traceback
from pprint import pprint


class CTRexTcp_Test(CTRexGeneral_Test):
    """This class defines the tcp test cases of the TRex traffic generator"""
    def __init__(self, *args, **kwargs):
        CTRexGeneral_Test.__init__(self, *args, **kwargs)

    def setUp(self):
        CTRexGeneral_Test.setUp(self)


    def validate_tcp(self,tcp_s):
        vm_valid_error=['__last','err_no_tcp','err_redirect_rx','tcps_rexmttimeo','tcps_sndrexmitbyte','tcps_sndrexmitpack'];

        if 'err' in tcp_s :
            if self.is_VM: # should be fixed , latency could be supported in this mode
                for key in tcp_s["err"]:
                    if key in vm_valid_error :
                        continue;
                    else:
                        return False;
            else:
               return(False);
        return True;

    def check_one_counter (self,c,name,val):
        d=c['all']
        if not (name in d):
            self.fail('counter %s is not in counters ' %(name) )
        if d[name] != val:
            self.fail('counter %s expect %d  value %d  ' %(name,val,d[name]) )


    def check_counters (self,val1,val2,str_err):
        if val1 !=val2:
            self.fail('%s ' %(str_err))

    def check_c_tcp_counters (self,c):
        self.check_one_counter (c,'m_active_flows',0)
        self.check_one_counter (c,'tcps_connects',c['all']['tcps_connects'])

    def check_s_tcp_counters (self,c):
        self.check_one_counter (c,'m_active_flows',0)
        self.check_one_counter (c,'tcps_accepts',c['all']['tcps_closed'])

    def check_c_udp_counters (self,c):
        self.check_one_counter (c,'m_active_flows',0)
        self.check_one_counter (c,'udps_connects',c['all']['udps_closed'])

    def check_s_udp_counters (self,c):
        self.check_one_counter (c,'m_active_flows',0)
        self.check_one_counter (c,'udps_accepts',c['all']['udps_closed'])


    def _check_tcp_errors(self, c,is_tcp,is_udp):
        tcp_c= c["tcp-v1"]["data"]["client"];
        if not self.validate_tcp(tcp_c):
           return False
        tcp_s= c["tcp-v1"]["data"]["server"];
        if not self.validate_tcp(tcp_s):
            return False

        if is_tcp:
          self.check_c_tcp_counters(tcp_c)
          self.check_s_tcp_counters(tcp_s)
          self.check_counters(tcp_c['all']['tcps_sndbyte'],tcp_s['all']['tcps_rcvbyte'],"c.tcps_sndbyte != s.tcps_rcvbyte");
          self.check_counters(tcp_s['all']['tcps_sndbyte'],tcp_c['all']['tcps_rcvbyte'],"s.tcps_sndbyte != c.tcps_rcvbyte");

        if is_udp :
          self.check_c_udp_counters(tcp_c)
          self.check_s_udp_counters(tcp_s)
          self.check_counters(tcp_c['all']['udps_sndbyte'],tcp_s['all']['udps_rcvbyte'],"c.udps_sndbyte != s.udps_rcvbyte");
          self.check_counters(tcp_c['all']['udps_rcvbyte'],tcp_s['all']['udps_sndbyte'],"c.udps_rcvbyte != s.udps_sndbyte");
          self.check_counters(tcp_c['all']['udps_sndpkt'],tcp_s['all']['udps_rcvpkt'],"c.udps_rcvpkt != s.udps_rcvpkt");
          self.check_counters(tcp_c['all']['udps_rcvpkt'],tcp_s['all']['udps_sndpkt'],"c.udps_rcvpkt != s.udps_sndpkt");


        return True

    def check_tcp_errors(self, trex_res,is_tcp,is_udp):
        c=trex_res.get_latest_dump()
        pprint(c["tcp-v1"]["data"])
        if not self._check_tcp_errors(c,is_tcp,is_udp):
            self.fail('Errors in tcp counters check ' )

    def get_duration(self):
        return 120

    def get_simple_tests(self):
        tests = [ {'name': 'http_simple.py','is_tcp' :True,'is_udp':False,'default':True},
                  {'name': 'udp_pcap.py','is_tcp' :False,'is_udp':True,'default':False}]
        return (tests);

    def get_sfr_tests(self):
        tests = [ {'name': 'sfr.py','is_tcp' :True,'is_udp':False,'m':1.0},
                  {'name': 'sfr_full.py','is_tcp' :True,'is_udp':True,'m':0.5}]
        return (tests);

    def test_tcp_http(self):
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');


        tests = self.get_simple_tests () 

        for obj in tests:
            ret = self.trex.start_trex(
                c = core,
                m = mult,
                d = self.get_duration(),
                f = 'astf/'+obj['name'],
                l = 1000,
                k = 10,
                astf =True
                )
    
            trex_res = self.trex.sample_to_run_finish()
    
            print("\nLATEST RESULT OBJECT:")
            print(trex_res)
            print ("\nLATEST DUMP:")
            #pprint(trex_res.get_latest_dump());
    
            self.check_general_scenario_results(trex_res,True,True)
            s=''
            if not obj['default']:
                s='.'+obj['name']
            self.check_CPU_benchmark(trex_res,elk_name = s)
            if bypass == None:
               self.check_tcp_errors(trex_res,obj['is_tcp'],obj['is_udp'])
            else:
                print("BYPASS the counter test for now");

    def test_ipv6_tcp_http(self):
        if self.is_virt_nics:
            self.skip('--ipv6 flag does not work correctly in with virtual NICs') # TODO: fix

        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')
            self.router.config_ipv6_pbr(mode = "config")


        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        tests = self.get_simple_tests () 

        for obj in tests:
            ret = self.trex.start_trex(
                cfg = '/etc/trex_cfg_mac.yaml',
                c = core,
                m = mult,
                d = self.get_duration(),
                ipv6 =True,
                f = 'astf/'+obj['name'],
                l = 1000,
                k = 10,
                astf =True
                )
    
            trex_res = self.trex.sample_to_run_finish()
    
            print("\nLATEST RESULT OBJECT:")
            print(trex_res)
            print ("\nLATEST DUMP:")
            #pprint(trex_res.get_latest_dump());
    
            self.check_general_scenario_results(trex_res,True,True)
            self.check_CPU_benchmark(trex_res,elk_name = "."+obj['name'])
            if bypass == None:
               self.check_tcp_errors(trex_res,obj['is_tcp'],obj['is_udp'])
            else:
                print("BYPASS the counter test for now");


    def test_tcp_sfr(self):
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        tests = self.get_sfr_tests ()

        for obj in tests:
            ret = self.trex.start_trex(
                c = core,
                m = mult*obj['m'],
                d = self.get_duration(),
                f = 'astf/'+obj['name'],
                l = 1000,
                k = 10,
                astf =True
                )
    
            trex_res = self.trex.sample_to_run_finish()
    
            print("\nLATEST RESULT OBJECT:")
            print(trex_res)
            print ("\nLATEST DUMP:")
            #pprint(trex_res.get_latest_dump());
    
            self.check_general_scenario_results(trex_res,True,True)
            self.check_CPU_benchmark(trex_res,elk_name = "."+obj['name'])
            if bypass == None:
               self.check_tcp_errors(trex_res,obj['is_tcp'],obj['is_udp'])
            else:
                print("BYPASS the counter test for now");



    def test_tcp_http_no_crash(self):
        """ 
         Request much higher speed than it could handle, make sure TRex does not crash in this case. 
         It is more important on setup that the port (C/S) are not on the same core (like trex-12)
         There is no need to test error counters in this case. 
        """ 
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        tests = self.get_simple_tests () 

        for obj in tests:
            ret = self.trex.start_trex(
                c = core,
                m = mult,
                d = self.get_duration(),
                nc = True,
                f = 'astf/'+obj['name'],
                l = 1000,
                k = 10,
                astf =True
                )
    
            trex_res = self.trex.sample_to_run_finish()
    
            print("\nLATEST RESULT OBJECT:")
            print(trex_res)
            print ("\nLATEST DUMP:")
            pprint(trex_res.get_latest_dump());

    def test_tcp_sfr_no_crash(self):
        """ 
         Request much higher speed than it could handle, make sure TRex does not crash in this case. 
         setup with LRO is better 
         There is no need to test error counters in this case. 
        """ 
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        tests = self.get_sfr_tests ()

        for obj in tests:
            ret = self.trex.start_trex(
                c = core,
                m = mult * obj['m'],
                d = self.get_duration(),
                nc = True,
                f = 'astf/'+obj['name'],
                l = 1000,
                k = 1,
                astf =True
                )
    
    
            trex_res = self.trex.sample_to_run_finish()
    
            print("\nLATEST RESULT OBJECT:")
            print(trex_res)
            print ("\nLATEST DUMP:")
            pprint(trex_res.get_latest_dump());


    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
