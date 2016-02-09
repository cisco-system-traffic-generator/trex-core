
import outer_packages
from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import nottest
from nose.plugins.attrib import attr
from unit_tests.trex_general_test import CTRexScenario
from dpkt import pcap

import sys
import os
import subprocess

# should be set to run explicitly, not as part of all regression tests
@attr('run_on_trex')
class CStlBasic_Test(functional_general_test.CGeneralFunctional_Test):
    def setUp (self):
        self.test_path = os.path.abspath(os.getcwd())
        self.scripts_path = CTRexScenario.scripts_path

        self.verify_exists(os.path.join(self.scripts_path, "bp-sim-64-debug"))

        self.stl_sim = os.path.join(self.scripts_path, "automation/trex_control_plane/client/trex_stateless_sim.py")
        self.verify_exists(self.stl_sim)

        self.profiles_path = os.path.join(self.scripts_path, "stl/")

        self.profiles = {}
        self.profiles['imix_3pkt'] = os.path.join(self.profiles_path, "imix_3pkt.yaml")
        self.profiles['imix_3pkt_vm'] = os.path.join(self.profiles_path, "imix_3pkt.yaml")
        self.profiles['random_size'] = os.path.join(self.profiles_path, "udp_rand_size.yaml")
        self.profiles['random_size_9k'] = os.path.join(self.profiles_path, "udp_rand_size_9k.yaml")
        self.profiles['imix_tuple_gen'] = os.path.join(self.profiles_path, "imix_1pkt_tuple_gen.yaml")

        for k, v in self.profiles.iteritems():
            self.verify_exists(v)

        self.valgrind_profiles = [ self.profiles['imix_3pkt_vm'], self.profiles['random_size_9k'], self.profiles['imix_tuple_gen']]

        self.golden_path = os.path.join(self.test_path,"stl/golden/")

        os.chdir(self.scripts_path)


    def tearDown (self):
        os.chdir(self.test_path)



    def get_golden (self, name):
        golden = os.path.join(self.golden_path, name)
        self.verify_exists(golden)
        return golden


    def verify_exists (self, name):
        if not os.path.exists(name):
            raise Exception("cannot find '{0}'".format(name))


    def compare_caps (self, cap1, cap2, max_diff_sec = 0.01):
        f1 = open(cap1, 'r')
        reader1 = pcap.Reader(f1)
        pkts1 = reader1.readpkts()

        f2 = open(cap2, 'r')
        reader2 = pcap.Reader(f2)
        pkts2 = reader2.readpkts()

        assert_equal(len(pkts1), len(pkts2))
        
        for pkt1, pkt2, i in zip(pkts1, pkts2, xrange(1, len(pkts1))):
            ts1 = pkt1[0]
            ts2 = pkt2[0]
            if abs(ts1-ts2) > 0.000005: # 5 nsec 
                raise AssertionError("TS error: cap files '{0}', '{1}' differ in cap #{2} - '{3}' vs. '{4}'".format(cap1, cap2, i, ts1, ts2))

            if pkt1[1] != pkt2[1]:
                raise AssertionError("RAW error: cap files '{0}', '{1}' differ in cap #{2}".format(cap1, cap2, i))



    def run_sim (self, yaml, output, options = "", silent = False):
        if output:
            user_cmd = "-f {0} -o {1} {2}".format(yaml, output, options)
        else:
            user_cmd = "-f {0} {1}".format(yaml, options)

        cmd = "{0} {1} {2}".format(sys.executable,
                                   self.stl_sim,
                                   user_cmd)

        if silent:
            devnull = open('/dev/null', 'w')
            rc = subprocess.call(cmd, shell = True, stdout = devnull)
        else:
            print cmd
            rc = subprocess.call(cmd, shell = True)

        return (rc == 0)


    def golden_run (self, testname,  profile, options, silent = False):

        output_cap = os.path.join("/tmp/", "{0}_test.cap".format(testname))
        golden_cap = os.path.join(self.test_path, "stl/golden/{0}_golden.cap".format(testname))

        try:
            rc = self.run_sim(self.profiles[profile], output_cap, options, silent)
            assert_equal(rc, True)

            self.compare_caps(output_cap, golden_cap)

        finally:
            os.unlink(output_cap)


    def run_py_profile_path (self, profile, options,silent = False, do_no_remove=False,compare =True):
        output_cap = "a.pcap"
        input_file =  os.path.join('stl/profiles/', profile)
        golden_file = os.path.join('exp',os.path.basename(profile).split('.')[0]+'.pcap');
        try:
            rc = self.run_sim(input_file, output_cap, options, silent)
            assert_equal(rc, True)
            if compare:
                self.compare_caps(output_cap, golden_file)
        finally:
            if  not do_no_remove: 
                os.unlink(output_cap)



    # test for IMIX
    def test_imix (self):
        self.golden_run("basic_imix", "imix_3pkt", "-m 50kpps --limit 500 --cores 8", silent = False)


    def test_vm (self):
        self.golden_run("basic_imix_vm", "imix_3pkt_vm", "-m 50kpps --limit 500 --cores 8", silent = False)


    def test_tuple_gen (self):
        self.golden_run("basic_tuple_gen", "imix_tuple_gen", "-m 50kpps --limit 500 --cores 8", silent = False)

    def test_all_profiles (self):
        p=[ 
            ["udp_1pkt_1mac_override.py","-m 1 -l 50",True],
            ["syn_attack.py","-m 1 -l 50",False],               # can't compare random now 
            ["udp_1pkt_1mac.py","-m 1 -l 50",True],
            ["udp_1pkt_mac.py","-m 1 -l 50",True],
            ["udp_1pkt.py","-m 1 -l 50",True],
            ["udp_1pkt_tuple_gen.py","-m 1 -l 50",True],
            ["udp_rand_len_9k.py","-m 1 -l 50",False],           # can't do the compare 
            ["udp_1pkt_mpls.py","-m 1 -l 50",True],
            ["udp_1pkt_mpls_vm.py","-m 1 ",True]
          ];

        #p=[ ["udp_1pkt_mpls_vm.py","-m 1 ",True] ]

        for obj in p:
            self.run_py_profile_path (obj[0],obj[1],compare =obj[2], do_no_remove=True)



    # valgrind tests
    def test_valgrind_various_profiles (self):

        print "\n"
        for profile in self.valgrind_profiles:
            print "\n*** testing profile '{0}' ***\n".format(profile)
            rc = self.run_sim(profile, output = None, options = "--cores 8 --limit 500 --valgrind", silent = False)
            assert_equal(rc, True)


