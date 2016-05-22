#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from misc_methods import run_command
from nose.plugins.attrib import attr
from time import sleep


def setUpModule():
    # examples connect by their own
    CStlGeneral_Test.unzip_client_package()
    if CTRexScenario.stl_trex.is_connected():
        CTRexScenario.stl_trex.disconnect()
        sleep(3)

def tearDownModule():
    # connect back at end of tests
    if not CTRexScenario.stl_trex.is_connected():
        CTRexScenario.stl_trex.connect()


@attr('client_package')
class CTRexClientPKG_Test(CStlGeneral_Test):
    """This class tests TRex client package"""

    def run_client_package_stl_example(self, python_version):
        commands = [
                    'cd %s' % CTRexScenario.scripts_path,
                    'source find_python.sh --%s' % python_version,
                    'which $PYTHON',
                    'cd trex_client/stl/examples',
                    '$PYTHON stl_imix.py -s %s' % self.configuration.trex['trex_name'],
                   ]
        return_code, stdout, stderr = run_command("bash -ce '%s'" % '; '.join(commands))
        if return_code:
            self.fail('Error in running stf_example using %s: %s' % (python_version, [return_code, stdout, stderr]))

    def test_client_python2(self):
        self.run_client_package_stl_example(python_version = 'python2')

    def test_client_python3(self):
        self.run_client_package_stl_example(python_version = 'python3')
