#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command

class STLExamples_Test(CStlGeneral_Test):
    """This class defines the IMIX testcase of the T-Rex traffic generator"""

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        # examples connect by their own
        if self.is_connected():
            CTRexScenario.stl_trex.disconnect()

    @classmethod
    def tearDownClass(cls):
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()

    def test_stl_examples(self):
        examples_dir = '../trex_control_plane/stl/examples'
        examples_to_test = [
                            'stl_imix.py',
                            ]

        for example in examples_to_test:
            return_code, stdout, stderr = run_command("sh -c 'cd %s; %s %s -s %s'" % (examples_dir, sys.executable, example, CTRexScenario.configuration.trex['trex_name']))
            assert return_code == 0, 'example %s failed.\nstdout: %s\nstderr: %s' % (return_code, stdout, stderr)

