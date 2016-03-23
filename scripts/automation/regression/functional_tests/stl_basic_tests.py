
import outer_packages
from platform_cmd_link import *
import functional_general_test
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import nottest
from nose.plugins.attrib import attr
from trex import CTRexScenario
from trex_stl_lib import trex_stl_sim
from trex_stl_lib.trex_stl_streams import STLProfile
from trex_stl_lib.trex_stl_packet_builder_scapy import RawPcapReader, RawPcapWriter
import sys
import os
import subprocess
import shlex
from threading import Thread

@attr('run_on_trex')
class CStlBasic_Test(functional_general_test.CGeneralFunctional_Test):
    def setUp (self):
        self.test_path = os.path.abspath(os.getcwd())
        self.scripts_path = CTRexScenario.scripts_path

        self.verify_exists(os.path.join(self.scripts_path, "bp-sim-64-debug"))

        self.stl_sim = os.path.join(self.scripts_path, "stl-sim")

        self.verify_exists(self.stl_sim)

        self.profiles_path = os.path.join(self.scripts_path, "stl/yaml/")

        self.profiles = {}
        self.profiles['imix_3pkt'] = os.path.join(self.profiles_path, "imix_3pkt.yaml")
        self.profiles['imix_3pkt_vm'] = os.path.join(self.profiles_path, "imix_3pkt_vm.yaml")
        self.profiles['random_size_9k'] = os.path.join(self.profiles_path, "../udp_rand_len_9k.py")
        self.profiles['imix_tuple_gen'] = os.path.join(self.profiles_path, "imix_1pkt_tuple_gen.yaml")

        for k, v in self.profiles.items():
            self.verify_exists(v)

        self.valgrind_profiles = [ self.profiles['imix_3pkt_vm'],
                                   self.profiles['random_size_9k'],
                                   self.profiles['imix_tuple_gen'] ]

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
        pkts1 = list(RawPcapReader(cap1))
        pkts2 = list(RawPcapReader(cap2))

        assert_equal(len(pkts1), len(pkts2))

        for pkt1, pkt2, i in zip(pkts1, pkts2, range(1, len(pkts1))):
            ts1 = float(pkt1[1][0]) + (float(pkt1[1][1]) / 1e6)
            ts2 = float(pkt2[1][0]) + (float(pkt2[1][1]) / 1e6)

            if abs(ts1-ts2) > 0.000005: # 5 nsec
                raise AssertionError("TS error: cap files '{0}', '{1}' differ in cap #{2} - '{3}' vs. '{4}'".format(cap1, cap2, i, ts1, ts2))

            if pkt1[0] != pkt2[0]:
                raise AssertionError("RAW error: cap files '{0}', '{1}' differ in cap #{2}".format(cap1, cap2, i))



    def run_sim (self, yaml, output, options = "", silent = False, obj = None):
        if output:
            user_cmd = "-f {0} -o {1} {2}".format(yaml, output, options)
        else:
            user_cmd = "-f {0} {1}".format(yaml, options)

        if silent:
            user_cmd += " --silent"

        rc = trex_stl_sim.main(args = shlex.split(user_cmd))
        if obj:
            obj['rc'] = (rc == 0)

        return (rc == 0)



    def run_py_profile_path (self, profile, options,silent = False, do_no_remove=False,compare =True, test_generated=True, do_no_remove_generated = False):
        output_cap = "a.pcap"
        input_file =  os.path.join('stl/', profile)
        golden_file = os.path.join('exp',os.path.basename(profile).split('.')[0]+'.pcap');
        if os.path.exists(output_cap):
            os.unlink(output_cap)
        try:
            rc = self.run_sim(input_file, output_cap, options, silent)
            assert_equal(rc, True)
            #s='cp  '+output_cap+' '+golden_file;
            #print s
            #os.system(s)

            if compare:
                self.compare_caps(output_cap, golden_file)
        finally:
            if not do_no_remove:
                os.unlink(output_cap)

        if test_generated:
            try:
                generated_filename = input_file.replace('.py', '_GENERATED.py').replace('.yaml', '_GENERATED.py')
                if input_file.endswith('.py'):
                    profile = STLProfile.load_py(input_file)
                elif input_file.endswith('.yaml'):
                    profile = STLProfile.load_yaml(input_file)
                
                profile.dump_to_code(generated_filename)

                rc = self.run_sim(generated_filename, output_cap, options, silent)
                assert_equal(rc, True)

                if compare:
                    self.compare_caps(output_cap, golden_file)
            
            except Exception as e:
                print(e)

            finally:
                if not do_no_remove_generated:
                    os.unlink(generated_filename)
                    os.unlink(generated_filename + 'c')
                if not do_no_remove:
                    os.unlink(output_cap)


    def test_stl_profiles (self):

        p = [
             ["udp_1pkt_1mac_override.py","-m 1 -l 50",True],
             ["syn_attack.py","-m 1 -l 50",True],               # can't compare random now
             ["udp_1pkt_1mac.py","-m 1 -l 50",True],
             ["udp_1pkt_mac.py","-m 1 -l 50",True],
             ["udp_1pkt.py","-m 1 -l 50",True],
             ["udp_1pkt_tuple_gen.py","-m 1 -l 50",True],
             ["udp_rand_len_9k.py","-m 1 -l 50",True],           # can't do the compare
             ["udp_1pkt_mpls.py","-m 1 -l 50",True],
             ["udp_1pkt_mpls_vm.py","-m 1 ",True],
             ["imix.py","-m 1 -l 100",True],
             ["udp_inc_len_9k.py","-m 1 -l 100",True],
             ["udp_1pkt_range_clients.py","-m 1 -l 100",True],
             ["multi_burst_2st_1000pkt.py","-m 1 -l 100",True],
             ["pcap.py", "-m 1", True],
             ["pcap_with_vm.py", "-m 1", True],

            # YAML test
             ["yaml/burst_1000_pkt.yaml","-m 1 -l 100",True],
             ["yaml/burst_1pkt_1burst.yaml","-m 1 -l 100",True],
             ["yaml/burst_1pkt_vm.yaml","-m 1 -l 100",True],
             ["yaml/imix_1pkt.yaml","-m 1 -l 100",True],
             ["yaml/imix_1pkt_2.yaml","-m 1 -l 100",True],
             ["yaml/imix_1pkt_tuple_gen.yaml","-m 1 -l 100",True],
             ["yaml/imix_1pkt_vm.yaml","-m 1 -l 100",True],
             ["udp_1pkt_pcap.py","-m 1 -l 10",True],
             ["udp_3pkt_pcap.py","-m 1 -l 10",True],
             #["udp_1pkt_simple.py","-m 1 -l 3",True],
             ["udp_1pkt_pcap_relative_path.py","-m 1 -l 3",True],
             ["udp_1pkt_tuple_gen_split.py","-m 1 -c 2 -l 100",True],
             ["udp_1pkt_range_clients_split.py","-m 1 -c 2 -l 100",True],
             ["udp_1pkt_vxlan.py","-m 1 -c 1 -l 17",True, False], # can't generate: no VXLAN in Scapy, only in profile
             ["udp_1pkt_ipv6_in_ipv4.py","-m 1 -c 1 -l 17",True],
             ["yaml/imix_3pkt.yaml","-m 50kpps --limit 20 --cores 2",True],
             ["yaml/imix_3pkt_vm.yaml","-m 50kpps --limit 20 --cores 2",True],
             ["udp_1pkt_simple_mac_dst.py","-m 1 -l 1 ",True],
             ["udp_1pkt_simple_mac_src.py","-m 1 -l 1 ",True],
             ["udp_1pkt_simple_mac_dst_src.py","-m 1 -l 1 ",True],
             ["burst_3st_loop_x_times.py","-m 1 -l 20 ",True],
             ["udp_1pkt_mac_step.py","-m 1 -l 20 ",True],
             ["udp_1pkt_mac_mask1.py","-m 1 -l 20 ",True] ,
             ["udp_1pkt_mac_mask2.py","-m 1 -l 20 ",True],
             ["udp_1pkt_mac_mask3.py","-m 1 -l 20 ",True],
             ["udp_1pkt_simple_test2.py","-m 1 -l 10 ",True], # test split of packet with ip option
             ["udp_1pkt_simple_test.py","-m 1 -l 10 ",True],
             ["udp_1pkt_mac_mask5.py","-m 1 -l 30 ",True],
             ["udp_1pkt_range_clients_split_garp.py","-m 1 -l 50",True]


          ];


        p1  = [ ["udp_1pkt_range_clients_split_garp.py","-m 1 -l 50",True] ]


        for obj in p:
            try:
                test_generated = obj[3]
            except: # check generated if not said otherwise
                test_generated = True
            self.run_py_profile_path (obj[0],obj[1],compare =obj[2], test_generated = test_generated, do_no_remove=True, do_no_remove_generated = False)


    def test_hlt_profiles (self):
        p = (
            ['hlt/hlt_udp_inc_dec_len_9k.py', '-m 1 -l 20', True],
            ['hlt/hlt_imix_default.py', '-m 1 -l 20', True],
            ['hlt/hlt_imix_4rates.py', '-m 1 -l 20', True],
            ['hlt/hlt_david1.py', '-m 1 -l 20', True],
            ['hlt/hlt_david2.py', '-m 1 -l 20', True],
            ['hlt/hlt_david3.py', '-m 1 -l 20', True],
            ['hlt/hlt_david4.py', '-m 1 -l 20', True],
            ['hlt/hlt_wentong1.py', '-m 1 -l 20', True],
            ['hlt/hlt_wentong2.py', '-m 1 -l 20', True],
            ['hlt/hlt_tcp_ranges.py', '-m 1 -l 20', True],
            ['hlt/hlt_udp_ports.py', '-m 1 -l 20', True],
            ['hlt/hlt_udp_random_ports.py', '-m 1 -l 20', True],
            ['hlt/hlt_ip_ranges.py', '-m 1 -l 20', True],
            ['hlt/hlt_framesize_vm.py', '-m 1 -l 20', True],
            ['hlt/hlt_l3_length_vm.py', '-m 1 -l 20', True],
            ['hlt/hlt_vlan_default.py', '-m 1 -l 20', True],
            ['hlt/hlt_4vlans.py', '-m 1 -l 20', True],
            ['hlt/hlt_vlans_vm.py', '-m 1 -l 20', True],
            ['hlt/hlt_ipv6_default.py', '-m 1 -l 20', True],
            ['hlt/hlt_ipv6_ranges.py', '-m 1 -l 20', True],
            ['hlt/hlt_mac_ranges.py', '-m 1 -l 20', True],
            )

        for obj in p:
            self.run_py_profile_path (obj[0], obj[1], compare =obj[2], do_no_remove=True, do_no_remove_generated = False)

    # valgrind tests - this runs in multi thread as it safe (no output)
    def test_valgrind_various_profiles (self):
        print("\n")
        threads = []
        for profile in self.valgrind_profiles:
            print("\n*** VALGRIND: testing profile '{0}' ***\n".format(profile))
            obj = {'t': None, 'rc': None}
            t = Thread(target = self.run_sim,
                       kwargs = {'obj': obj, 'yaml': profile, 'output':None, 'options': "--cores 8 --limit 20 --valgrind", 'silent': True})
            obj['t'] = t

            threads.append(obj)
            t.start()

        for obj in threads:
            obj['t'].join()

        for obj in threads:
            assert_equal(obj['rc'], True)



