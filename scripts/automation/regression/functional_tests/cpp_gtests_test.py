import outer_packages
from nose.plugins.attrib import attr
import functional_general_test
from trex import CTRexScenario
import os, sys
from subprocess import Popen, STDOUT
from stl_basic_tests import compare_caps
import shlex
import time
import errno
import tempfile

# runs command
def run_command(command, timeout = 15, poll_rate = 0.1, cwd = None):
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
        print('')
        bp_sim = os.path.join(CTRexScenario.scripts_path, 'bp-sim-64')
        ret, out = run_command('%s --ut' % bp_sim, cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of gtests (%s)' % ret)

    def test_gtests_valgrind(self):
        print('')
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, 'run-gtest-clean'), cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

    def test_gtests_tcp_valgrind(self):
        print('')
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, 'run-gtest-tcp-clean "gt_tcp.*"'), cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

    def test_bp_sim_client_cfg(self):
        print('')
        cmd = './bp-sim-64 --pcap -f cap2/dns.yaml --client_cfg automation/regression/cfg/client_cfg_vlan_mac.yaml -o generated/bp_sim_dns_vlans_gen.pcap'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

        compare_caps(output = os.path.join(CTRexScenario.scripts_path, 'generated/bp_sim_dns_vlans_gen.pcap'),
                     golden = 'functional_tests/golden/bp_sim_dns_vlans.pcap')

    def test_astf_sim_utl_sfr(self):
        print('')
        cmd = './astf-sim-utl --sfr'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50,cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of test_astf_sim_utl_sfr (%s)' % ret)


    def test_astf_sim_utl_sfr_drop(self):
        pass;
        print('')
        cmd = './astf-sim-utl --sfr  --cmd="--sim-mode=28,--sim-arg=0.1" --skip-counter-err'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50,cwd = CTRexScenario.scripts_path)
        print('Output:\n%s' % out)
        if ret:
            raise Exception('Non zero return status of test_astf_sim_utl_sfr_drop (%s)' % ret)

