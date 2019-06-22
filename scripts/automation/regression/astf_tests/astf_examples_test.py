from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command

class ASTFExamples_Test(CASTFGeneral_Test):
    """Checking examples of ASTF (interactive/trex/examples/astf) """

    def explicitSetUp(self):
        # examples connect by their own
        if self.is_connected():
            self.astf_trex.disconnect()

    def explicitTearDown(self):
        # connect back at end of tests
        if not self.is_connected():
            self.astf_trex.connect()

    def test_astf_prof_examples(self):
        examples_dir = '../trex_control_plane/interactive'
        examples_to_test = [
                            'astf_example',
                            ]

        for example in examples_to_test:
            cmd = "-m trex.examples.astf.{0}".format(example)

            self.explicitSetUp()
            return_code, stdout, stderr = run_command("sh -c 'cd %s; %s %s -s %s'" % (examples_dir, sys.executable, cmd, CTRexScenario.configuration.trex['trex_name']))
            self.explicitTearDown()
            assert return_code == 0, 'example %s failed.\nstdout: %s\nstderr: %s' % (return_code, stdout, stderr)

