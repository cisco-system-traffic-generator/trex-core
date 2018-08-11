import outer_packages
from nose.plugins.attrib import attr
import functional_general_test
from trex_scenario import CTRexScenario
import os, sys
from subprocess import Popen, STDOUT
from stl_basic_tests import compare_caps
import shlex
import time
import errno
import tempfile
import platform

cur_dir = os.path.dirname(__file__)

# runs command
def run_command(command, timeout = 60, poll_rate = 0.1, cwd = None):
    # pipes might stuck, even with timeout
    with tempfile.TemporaryFile() as stdout_file:
        proc = Popen(shlex.split(command), stdout = stdout_file, stderr = STDOUT, cwd = cwd, close_fds = True, universal_newlines = True)
        if timeout > 0:
            for i in range(int(timeout/poll_rate)):
                time.sleep(poll_rate)
                if proc.poll() is not None: # process stopped
                    break
            if proc.poll() is None:
                proc.kill() # timeout
                stdout_file.seek(0)
                return (errno.ETIME, '%s\n\n...Timeout of %s second(s) is reached!' % (stdout_file.read().decode(errors = 'replace'), timeout))
        else:
            proc.wait()
        stdout_file.seek(0)
        return (proc.returncode, stdout_file.read().decode(errors = 'replace'))

@attr('run_on_trex')
class CPP_Test(functional_general_test.CGeneralFunctional_Test):
    def test_gtests_all(self):
        bp_sim = os.path.join(CTRexScenario.scripts_path, 'bp-sim-64')
        ret, out = run_command('%s --ut' % bp_sim, cwd = CTRexScenario.scripts_path)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of gtests (%s)' % ret)

    def test_gtests_valgrind(self):
        ret, out = run_command(os.path.join(cur_dir, 'run-gtest-clean'), cwd = cur_dir)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

    def test_gtests_tcp_valgrind(self):
        ret, out = run_command(os.path.join(cur_dir, 'run-gtest-tcp-clean "gt_tcp.*"'), cwd = cur_dir)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

    def test_bp_sim_client_cfg(self):
        cmd = './bp-sim-64 --pcap -f cap2/dns.yaml --client_cfg automation/regression/cfg/client_cfg_vlan_mac.yaml -o generated/bp_sim_dns_vlans_gen.pcap'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), cwd = CTRexScenario.scripts_path)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

        compare_caps(output = os.path.join(CTRexScenario.scripts_path, 'generated/bp_sim_dns_vlans_gen.pcap'),
                     golden = 'functional_tests/golden/bp_sim_dns_vlans.pcap')

    def test_astf_sim_utl_sfr(self):
        # tshark of trex-05 is the valid one 
        print(platform.node())
        if not ("csi-trex-05" in platform.node()):
            print("skip")
            return;
        cmd = './astf-sim-utl --sfr'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50,cwd = CTRexScenario.scripts_path)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of test_astf_sim_utl_sfr (%s)' % ret)


    def test_astf_sim_utl_sfr_drop(self):
        # tshark of trex-05 is the valid one 
        if platform.node()!="csi-trex-05.cisco.com":
            return;
        cmd = './astf-sim-utl --sfr  --cmd="--sim-mode=28,--sim-arg=0.1" --skip-counter-err'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50,cwd = CTRexScenario.scripts_path)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of test_astf_sim_utl_sfr_drop (%s)' % ret)

