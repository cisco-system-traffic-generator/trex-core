#!/router/bin/python
from .astf_general_test import CASTFGeneral_Test, CTRexScenario
from misc_methods import run_command
from nose.plugins.attrib import attr

@attr('client_package')
class CTRexClientPKG_Test(CASTFGeneral_Test):
    """This class tests TRex client package"""

    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        # examples connect by their own
        if CTRexScenario.astf_trex.is_connected():
            CTRexScenario.astf_trex.disconnect()
        CASTFGeneral_Test.unzip_client_package()

    def tearDown(self):
        # connect back at end of tests
        if not CTRexScenario.astf_trex.is_connected():
            CTRexScenario.astf_trex.connect()
        CASTFGeneral_Test.tearDown(self)

    def run_client_package_astf_example(self, python_version):
        commands = [
                    'cd %s' % CTRexScenario.scripts_path,
                    'source find_python.sh --python%s' % python_version,
                    'which $PYTHON',
                    'cd trex_client/interactive',
                    '$PYTHON -m trex.examples.astf.astf_example -s %s' % self.configuration.trex['trex_name'],
                   ]
        return_code, stdout, stderr = run_command("bash -ce '%s'" % '; '.join(commands))
        if return_code:
            self.fail('Error in running stf_example using Python %s: %s' % (python_version, [return_code, stdout, stderr]))

    def test_client_python2(self):
        self.run_client_package_astf_example(python_version = 2)

    def test_client_python3(self):
        self.run_client_package_astf_example(python_version = 3)
