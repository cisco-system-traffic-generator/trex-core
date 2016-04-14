#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from misc_methods import run_command
from nose.plugins.attrib import attr


@attr('client_package')
class CTRexClientPKG_Test(CStlGeneral_Test):
    """This class tests TRex client package"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        self.unzip_client_package()

    def run_client_package_stf_example(self, python_version):
        commands = [
                    'cd %s' % CTRexScenario.scripts_path,
                    'source find_python.sh --%s' % python_version,
                    'which $PYTHON',
                    'cd trex_client/stl/examples',
                    '$PYTHON stl_imix.py -s %s' % self.configuration.trex['trex_name'],
                   ]
        return_code, _, stderr = run_command("bash -ce '%s'" % '; '.join(commands))
        if return_code:
            self.fail('Error in running stf_example using %s: %s' % (python_version, stderr))

    def test_client_python2(self):
        self.run_client_package_stf_example(python_version = 'python2')

    def test_client_python3(self):
        self.run_client_package_stf_example(python_version = 'python3')
