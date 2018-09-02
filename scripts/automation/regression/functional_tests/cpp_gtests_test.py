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
def run_command(command, timeout = 60, cwd = CTRexScenario.scripts_path):
    # pipes might stuck, even with timeout
    with tempfile.TemporaryFile() as stdout_file:
        proc = Popen(shlex.split(command), stdout = stdout_file, stderr = STDOUT, cwd = cwd, close_fds = True, universal_newlines = True)
        return get_proc_status(proc, stdout_file, timeout)

def run_command_bg(command, stdout_file, cwd = CTRexScenario.scripts_path):
    return Popen(shlex.split(command), stdout = stdout_file, stderr = STDOUT, cwd = cwd, close_fds = True, universal_newlines = True)

def get_proc_status(proc, stdout_file, timeout = 60):
    poll_rate = 0.1
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

    def test_gtests_positive(self):
        tests = [
            {'name': 'All gtests (non-valgrind)', 'cmd': './bp-sim-64 --ut'},
            {'name': 'run-gtest-clean', 'cmd': os.path.join(cur_dir, 'run-gtest-clean')},
            {'name': 'gt_tcp.* valgrind', 'cmd': os.path.join(cur_dir, 'run-gtest-tcp-clean "gt_tcp.*"')},
            {'name': '*.astf_positive* valgrind', 'cmd': os.path.join(cur_dir, 'run-gtest-tcp-clean "*.astf_positive*"')}]

        # run in parallel
        for test in tests:
            test['stdout'] = tempfile.TemporaryFile()
            test['proc'] = run_command_bg(test['cmd'], test['stdout'])

        failed_tests = []
        for test in tests:
            ret, out = get_proc_status(test['proc'], test['stdout'])
            if ret:
                print('Non-zero return status of test %s. Output:\n%s' % (test['name'], out))
                failed_tests.append(test['name'])

        if failed_tests:
            raise Exception('Some tests failed (%s)' % ', '.join(failed_tests))


    def test_gtests_astf_negative(self):
        print('')
        cmd = './bp-sim-64 --ut --gtest_filter=*.astf_negative* --gtest_list_tests'
        ret, out = run_command(cmd)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Could not get negative ASTF tests list')
        tests = []
        for line in out.splitlines():
            if line.startswith('  '):
                tests.append({
                    'name': line.strip(),
                    'stdout': tempfile.TemporaryFile(),
                    })

        not_failed_tests = []
        # run in parallel
        for test in tests:
            # we run several in parallel
            test['proc'] = run_command_bg(os.path.join(cur_dir, 'run-gtest-tcp-clean "*.%s*"' % test['name']), test['stdout'])
        for test in tests:
            ret, out = get_proc_status(test['proc'], test['stdout'])
            if not ret:
                print('Zero return status of test %s. Output:\n%s' % (test['name'], out))
                not_failed_tests.append(test['name'])

        print('Total negative tests: %d' % len(tests))
        print('Failed (as expected): %d' % (len(tests) - len(not_failed_tests)))
        print('Did not fail: %d' % len(not_failed_tests))
        if not_failed_tests:
            raise Exception('Some negative tests did not fail (%s)' % ', '.join(not_failed_tests))


    def test_bp_sim_client_cfg(self):
        cmd = './bp-sim-64 --pcap -f cap2/dns.yaml --client_cfg automation/regression/cfg/client_cfg_vlan_mac.yaml -o generated/bp_sim_dns_vlans_gen.pcap'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd))
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of Valgrind gtests (%s)' % ret)

        compare_caps(output = os.path.join(CTRexScenario.scripts_path, 'generated/bp_sim_dns_vlans_gen.pcap'),
                     golden = 'functional_tests/golden/bp_sim_dns_vlans.pcap')

    def test_astf_sim_utl_sfr(self):
        # tshark of trex-05 is the valid one
        print(platform.node())
        if 'csi-trex-05' not in platform.node():
            print("Can run only at csi-trex-05, skip here")
            return
        
        cmd = './astf-sim-utl --sfr'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of test_astf_sim_utl_sfr (%s)' % ret)


    def test_astf_sim_utl_sfr_drop(self):
        # tshark of trex-05 is the valid one 
        if 'csi-trex-05' not in platform.node():
            print("Can run only at csi-trex-05, skip here")
            return

        cmd = './astf-sim-utl --sfr  --cmd="--sim-mode=28,--sim-arg=0.1" --skip-counter-err'
        ret, out = run_command(os.path.join(CTRexScenario.scripts_path, cmd), timeout = 50)
        if ret:
            print('\nOutput:\n%s' % out)
            raise Exception('Non zero return status of test_astf_sim_utl_sfr_drop (%s)' % ret)

