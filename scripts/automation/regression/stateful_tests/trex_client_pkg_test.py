#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test, CTRexScenario
from misc_methods import run_command
from nose.plugins.attrib import attr


@attr('client_package')
class CTRexClientPKG_Test(CTRexGeneral_Test):
    """This class tests TRex client package"""

    def setUp(self):
        CTRexGeneral_Test.setUp(self)
        if not self.is_loopback and not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces()
            self.router.config_pbr(mode = 'config')
        self.unzip_client_package()

    def run_client_package_stf_example(self, python_version):
        commands = [
                    'cd %s' % CTRexScenario.scripts_path,
                    'source find_python.sh --%s' % python_version,
                    'which $PYTHON',
                    'cd trex_client/stf/examples',
                    '$PYTHON stf_example.py -s %s' % self.configuration.trex['trex_name'],
                   ]
        return_code, _, stderr = run_command("bash -ce '%s'" % '; '.join(commands))
        if return_code:
            self.fail('Error in running stf_example using %s: %s' % (python_version, stderr))

    def test_client_python2(self):
        self.run_client_package_stf_example(python_version = 'python2')

    def test_client_python3(self):
        self.run_client_package_stf_example(python_version = 'python3')
