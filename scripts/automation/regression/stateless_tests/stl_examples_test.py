#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
import os, sys
from misc_methods import run_command


class STLExamples_Test(CStlGeneral_Test):
    """This class defines the IMIX testcase of the TRex traffic generator"""

    def explicitSetUp(self):
        # examples connect by their own
        if self.is_connected():
            CTRexScenario.stl_trex.disconnect()

    def explicitTearDown(self):
        # connect back at end of tests
        if not self.is_connected():
            self.stl_trex.connect()

    def test_stl_examples(self):
        #Work around for trex-405. Remove when it is resolved
        rx_port = CTRexScenario.stl_ports_map['bi'][0]
        port_info = CTRexScenario.stl_trex.get_port_info(ports = rx_port)[0]
        drv_name = port_info['driver']
        if drv_name == 'net_mlx5' and 'VM' in self.modes:
            self.skip('Can not run on mlx VM currently - see trex-405 for details')

        examples_dir = '../trex_control_plane/stl/examples'
        examples_to_test = [
                            'stl_imix.py',
                            ]

        for example in examples_to_test:
            self.explicitSetUp()
            return_code, stdout, stderr = run_command("sh -c 'cd %s; %s %s -s %s'" % (examples_dir, sys.executable, example, CTRexScenario.configuration.trex['trex_name']))
            self.explicitTearDown()
            assert return_code == 0, 'example %s failed.\nstdout: %s\nstderr: %s' % (return_code, stdout, stderr)

