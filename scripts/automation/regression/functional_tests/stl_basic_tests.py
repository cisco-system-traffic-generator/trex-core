
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
from trex_stl_lib.trex_stl_packet_builder_scapy import RawPcapReader, RawPcapWriter, Ether
from trex_stl_lib.utils.text_opts import *
from pprint import pprint

import sys
import json

if sys.version_info > (3,0):
    from io import StringIO
else:
    from cStringIO import StringIO

import os
import subprocess
import shlex
from threading import Thread
from collections import defaultdict


def scapy_pkt_show_to_str (scapy_pkt):
    capture = StringIO()
    save_stdout = sys.stdout
    sys.stdout = capture
    scapy_pkt.show()
    sys.stdout = save_stdout
    return capture.getvalue()


def compare_dicts_round(d1, d2, precision = 4):
    ''' Eliminates precision of floats in dicts '''
    return json.loads(json.dumps(d1), parse_float = lambda x:round(float(x), precision)) == json.loads(json.dumps(d2), parse_float = lambda x:round(float(x), precision))


def compare_caps (output, golden, max_diff_sec = 0.000005):
    pkts1 = []
    pkts2 = []
    pkts_ts_buckets = defaultdict(list)

    for pkt in RawPcapReader(output):
        ts = pkt[1][0] * 1e6 + pkt[1][1]
        pkts_ts_buckets[ts].append(pkt)
    # don't take last ts bucket, it can be cut in middle and packets inside bucket might be different
    #for ts in sorted(pkts_ts_buckets.keys())[:-1]:
    for ts in sorted(pkts_ts_buckets.keys()):
        pkts1.extend(sorted(pkts_ts_buckets[ts]))
    pkts_ts_buckets.clear()

    for pkt in RawPcapReader(golden):
        ts = pkt[1][0] * 1e6 + pkt[1][1]
        pkts_ts_buckets[ts].append(pkt)
    # don't take last ts bucket, it can be cut in middle and packets inside bucket might be different
    #for ts in sorted(pkts_ts_buckets.keys())[:-1]:
    for ts in sorted(pkts_ts_buckets.keys()):
        pkts2.extend(sorted(pkts_ts_buckets[ts]))

    assert_equal(len(pkts1), len(pkts2), 'Lengths of generated pcap (%s) and golden (%s) are different' % (output, golden))

    for pkt1, pkt2, i in zip(pkts1, pkts2, range(1, len(pkts1))):
        ts1 = float(pkt1[1][0]) + (float(pkt1[1][1]) / 1e6)
        ts2 = float(pkt2[1][0]) + (float(pkt2[1][1]) / 1e6)

        if abs(ts1-ts2) > max_diff_sec: # 5 nsec
            raise AssertionError("TS error: cap files '{0}', '{1}' differ in cap #{2} - '{3}' vs. '{4}'".format(output, golden, i, ts1, ts2))

        if pkt1[0] != pkt2[0]:
            errmsg = "RAW error: output file '{0}', differs from golden '{1}' in cap #{2}".format(output, golden, i)
            print(errmsg)

            print(format_text("\ndifferent fields for packet #{0}:".format(i), 'underline'))

            scapy_pkt1_info = scapy_pkt_show_to_str(Ether(pkt1[0])).split('\n')
            scapy_pkt2_info = scapy_pkt_show_to_str(Ether(pkt2[0])).split('\n')

            print(format_text("\nGot:\n", 'bold', 'underline'))
            for line, ref in zip(scapy_pkt1_info, scapy_pkt2_info):
                if line != ref:
                    print(format_text(line, 'bold'))
            
            print(format_text("\nExpected:\n", 'bold', 'underline'))
            for line, ref in zip(scapy_pkt2_info, scapy_pkt1_info):
                if line != ref:
                    print(format_text(line, 'bold'))

            print("\n")
            raise AssertionError(errmsg)



@attr('run_on_trex')
class CStlBasic_Test(functional_general_test.CGeneralFunctional_Test):
    def setUp (self):
        self.test_path = os.path.abspath(os.getcwd())
        self.scripts_path = CTRexScenario.scripts_path

        self.verify_exists(os.path.join(self.scripts_path, "bp-sim-64-debug"))

        self.stl_sim = os.path.join(self.scripts_path, "stl-sim")

        self.verify_exists(self.stl_sim)

        self.profiles_path = os.path.join(self.scripts_path, "stl/")

        self.profiles = {}
        self.profiles['imix'] = os.path.join(self.profiles_path, "imix.py")
        self.profiles['syn_attack'] = os.path.join(self.profiles_path, "syn_attack.py")
        self.profiles['random_size_9k'] = os.path.join(self.profiles_path, "udp_rand_len_9k.py")
        self.profiles['udp_tuple_gen'] = os.path.join(self.profiles_path, "udp_1pkt_tuple_gen.py")

        for k, v in self.profiles.items():
            self.verify_exists(v)

        self.valgrind_profiles = [ self.profiles['syn_attack'],
                                   self.profiles['random_size_9k'],
                                   self.profiles['udp_tuple_gen'] ]

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


    def run_sim (self, yaml, output, options = "", silent = False, obj = None, tunables = None):
        if output:
            user_cmd = "-f {0} -o {1} {2} -p {3}".format(yaml, output, options, self.scripts_path)
        else:
            user_cmd = "-f {0} {1} -p {2}".format(yaml, options, self.scripts_path)

        if silent:
            user_cmd += " --silent"

        if tunables:
            user_cmd += " -t"
            for k, v in tunables.items():
                user_cmd += " {0}={1}".format(k, v)

        rc = trex_stl_sim.main(args = shlex.split(user_cmd))
        if obj:
            obj['rc'] = (rc == 0)

        return (rc == 0)



    def run_py_profile_path (self,
                             profile,
                             options,
                             silent = False,
                             do_no_remove = False,
                             compare = True,
                             test_generated = True,
                             do_no_remove_generated = False,
                             tunables = None):

        print('\nTesting profile: %s' % profile)
        output_cap = "generated/a.pcap"
        input_file =  os.path.join('stl/', profile)
        golden_file = os.path.join('exp',os.path.basename(profile).split('.')[0]+'.pcap');
        if os.path.exists(output_cap):
            os.unlink(output_cap)
        try:
            rc = self.run_sim(yaml     = input_file,
                              output   = output_cap,
                              options  = options,
                              silent   = silent,
                              tunables = tunables)
            assert_equal(rc, True, 'Simulation on profile %s failed.' % profile)
            #s='cp  '+output_cap+' '+golden_file;
            #print s
            #os.system(s)

            if compare:
                compare_caps(output_cap, golden_file)
        finally:
            if not do_no_remove:
                os.unlink(output_cap)

        if test_generated:
            try:
                generated_filename = input_file.replace('.py', '_GENERATED.py').replace('.yaml', '_GENERATED.py')
                print('Generating %s' % generated_filename)
                if input_file.endswith('.py'):
                    profile = STLProfile.load_py(input_file, **(tunables if tunables else {}))
                elif input_file.endswith('.yaml'):
                    profile = STLProfile.load_yaml(input_file)
                
                profile.dump_to_code(generated_filename)

                if compare:
                    orig_json = profile.to_json()
                    gen_json  = STLProfile.load_py(generated_filename).to_json()
                    if not compare_dicts_round(orig_json, gen_json):
                        print('Original JSON:')
                        pprint(orig_json)
                        print('Generated JSON:')
                        pprint(gen_json)
                        raise Exception('Generated file differs from original in JSON.')


            finally:
                if not do_no_remove_generated:
                    if os.path.exists(generated_filename):
                        os.unlink(generated_filename)
                    # python 3 does not generate PYC under the same dir
                    if os.path.exists(generated_filename + 'c'):
                        os.unlink(generated_filename + 'c')


    def test_stl_profiles (self):
        p = [
             ["udp_1pkt_1mac_override.py","-m 1 -l 50",True],
             ["syn_attack.py","-m 1 -l 50",True],               
             ["udp_1pkt_1mac.py","-m 1 -l 50",True],
             ["udp_1pkt_mac.py","-m 1 -l 50",True],
             ["udp_1pkt.py","-m 1 -l 50",True],
             ["udp_1pkt_tuple_gen.py","-m 1 -l 50",True],
             ["udp_rand_len_9k.py","-m 1 -l 50",True],           
             ["udp_1pkt_mpls.py","-m 1 -l 50",True],
             ["udp_1pkt_mpls_vm.py","-m 1 ",True],
             ["imix.py","-m 1 -l 100",True],
             ["udp_inc_len_9k.py","-m 1 -l 100",True],
             ["udp_1pkt_range_clients.py","-m 1 -l 100",True],
             ["multi_burst_2st_1000pkt.py","-m 1 -l 100",True],
             ["pcap.py", "-m 1", True],
             ["pcap_with_vm.py", "-m 1", True],
             ["flow_stats.py", "-m 1 -l 1", True],
             ["flow_stats_latency.py", "-m 1 -l 1", True],

             
             ["udp_1pkt_pcap.py","-m 1 -l 10",True],
             ["udp_3pkt_pcap.py","-m 1 -l 10",True],
             #["udp_1pkt_simple.py","-m 1 -l 3",True],
             ["udp_1pkt_pcap_relative_path.py","-m 1 -l 3",True],
             ["udp_1pkt_tuple_gen_split.py","-m 1 -l 100",True],
             ["udp_1pkt_range_clients_split.py","-m 1 -l 100",True],
             ["udp_1pkt_vxlan.py","-m 1 -l 17",True, False], # can't generate: no VXLAN in Scapy, only in profile
             ["udp_1pkt_ipv6_in_ipv4.py","-m 1 -l 17",True],
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
             ["udp_1pkt_range_clients_split_garp.py","-m 1 -l 50",True],
             ["udp_1pkt_src_ip_split.py","-m 1 -l 50",True],
             ["udp_1pkt_repeat_random.py","-m 1 -l 50",True],
             ["syn_attack_fix_cs_hw.py", "-m 1 -l 50", True]
          ];

        p1 = [ ["udp_1pkt_repeat_random.py","-m 1 -l 50",True] ];

        for obj in p:
            try:
                test_generated = obj[3]
            except: # check generated if not said otherwise
                test_generated = True
            self.run_py_profile_path (obj[0],obj[1],compare =obj[2], test_generated = test_generated, do_no_remove=True, do_no_remove_generated = False)


    def test_hlt_profiles (self):
        p = (
            ['hlt/hlt_udp_inc_dec_len_9k.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_imix_default.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_imix_4rates.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_david1.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_david2.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_david3.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_david4.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_wentong1.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_wentong2.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_tcp_ranges.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_udp_ports.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_udp_random_ports.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_ip_ranges.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_framesize_vm.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_l3_length_vm.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_vlan_default.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_4vlans.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_vlans_vm.py', '-m 1 -l 20', True, {'random_seed': 1}],
            ['hlt/hlt_ipv6_default.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_ipv6_ranges.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_mac_ranges.py', '-m 1 -l 20', True, None],
            ['hlt/hlt_divya.py', '-m 1 -l 20', True, None],
            )

        for obj in p:
            self.run_py_profile_path (obj[0], obj[1], compare =obj[2], do_no_remove=True, do_no_remove_generated = False, tunables = obj[3])

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



    def test_multicore_scheduling (self):
        
        seed = time.time()

        # test with simple vars
        print(format_text("\nTesting multiple flow vars for multicore\n", 'underline'))
        rc = self.run_sim('stl/tests/multi_core_test.py', output = None, options = '--test_multi_core --limit=840 -t test_type=plain#seed={0} -m 27kpps'.format(seed), silent = True)
        assert_equal(rc, True)


        # test with tuple
        print(format_text("\nTesting multiple tuple generators for multicore\n", 'underline'))
        rc = self.run_sim('stl/tests/multi_core_test.py', output = None, options = '--test_multi_core --limit=840 -t test_type=tuple#seed={0} -m 27kpps'.format(seed), silent = True)
        assert_equal(rc, True)

        # some tests
        mc_tests = [
                    'stl/tests/single_cont.py',
                    'stl/tests/single_burst.py',
                    'stl/tests/multi_burst.py',
                   ]

        for mc_test in mc_tests:
            print(format_text("\ntesting {0} for multicore...\n".format(mc_test), 'underline'))
            rc = self.run_sim(mc_test, output = None, options = '--test_multi_core --limit=840 -m 27kpps', silent = True)
            assert_equal(rc, True)

        return


