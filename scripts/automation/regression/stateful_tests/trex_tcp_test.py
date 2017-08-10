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

    def _check_tcp_errors(self, c):
        tcp_c= c["tcp-v1"]["data"]["client"];
        if not self.validate_tcp(tcp_c):
           return False
        tcp_s= c["tcp-v1"]["data"]["server"];
        if not self.validate_tcp(tcp_s):
            return False
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
        no_latency_support = self.get_benchmark_param('no_latency_support');   # some devices does not support this yet

        if self.is_VM: # should be fixed , latency could be supported in this mode
            no_latency_support = 1;

        check_latency = True;
        if no_latency_support == None:
            ret = self.trex.start_trex(
                c = core,
                m = mult,
                nc = True,
                d = 120,
                f = 'cap2/http_simple.yaml',
                l = 1000,
                k = 10,
                tcp =True
                )
        else:
            check_latency = False;
            ret = self.trex.start_trex(
                c = core,
                m = mult,
                nc = True,
                d = 120,
                f = 'cap2/http_simple.yaml',
                k = 10,
                tcp =True
                )


        trex_res = self.trex.sample_to_run_finish()

        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        print ("\nLATEST DUMP:")
        #pprint(trex_res.get_latest_dump());

        self.check_general_scenario_results(trex_res,check_latency)
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
