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
        vm_valid_error=['__last','err_no_tcp','tcps_rexmttimeo','tcps_sndrexmitbyte','tcps_sndrexmitpack'];

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


    def _check_tcp_errors(self, c):
        tcp_c= c["tcp-v1"]["data"]["client"];
        if not self.validate_tcp(tcp_c):
           return False
        tcp_s= c["tcp-v1"]["data"]["server"];
        if not self.validate_tcp(tcp_s):
            return False

        self.check_c_tcp_counters(tcp_c)
        self.check_s_tcp_counters(tcp_s)

        self.check_counters(tcp_c['all']['tcps_sndbyte'],tcp_s['all']['tcps_rcvbyte'],"c.tcps_sndbyte != s.tcps_rcvbyte");
        self.check_counters(tcp_s['all']['tcps_sndbyte'],tcp_c['all']['tcps_rcvbyte'],"s.tcps_sndbyte != c.tcps_rcvbyte");

        return True

    def check_tcp_errors(self, trex_res):
        c=trex_res.get_latest_dump()
        pprint(c["tcp-v1"]["data"])
        if not self._check_tcp_errors(c):
            self.fail('Errors in tcp counters check ' )

    def test_tcp_http(self):
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            d = 120,
            f = 'astf/http_simple.py',
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
        self.check_CPU_benchmark(trex_res)
        if bypass == None:
           self.check_tcp_errors(trex_res)
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

        ret = self.trex.start_trex(
            cfg = '/etc/trex_cfg_mac.yaml',
            c = core,
            m = mult,
            d = 120,
            ipv6 =True,
            f = 'astf/http_simple.py',
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
        self.check_CPU_benchmark(trex_res)
        if bypass == None:
           self.check_tcp_errors(trex_res)
        else:
            print("BYPASS the counter test for now");


    def test_tcp_sfr(self):
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')

        core  = 1 #self.get_benchmark_param('cores')
        mult  = self.get_benchmark_param('multiplier')
        bypass = self.get_benchmark_param('bypass_result');

        ret = self.trex.start_trex(
            c = core,
            m = mult,
            d = 30,
            f = 'astf/sfr.py',
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
        self.check_CPU_benchmark(trex_res)
        if bypass == None:
           self.check_tcp_errors(trex_res)
        else:
            print("BYPASS the counter test for now");


    def tearDown(self):
        CTRexGeneral_Test.tearDown(self)
        pass

if __name__ == "__main__":
    pass
