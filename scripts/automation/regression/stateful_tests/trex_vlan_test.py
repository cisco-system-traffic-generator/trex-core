#!/router/bin/python
from .trex_general_test import CTRexGeneral_Test, CTRexScenario
from .tests_exceptions import *
import time
from CPlatform import CStaticRouteConfig

class CTRexVlan_Test(CTRexGeneral_Test):#(unittest.TestCase):
    """This class defines test for vlan platform configutation file"""
    def __init__(self, *args, **kwargs):
        super(CTRexVlan_Test, self).__init__(*args, **kwargs)
        self.unsupported_modes = ['loopback'] # We test on routers

    def setUp(self):
        super(CTRexVlan_Test, self).setUp() # launch super test class setUp process

    def test_platform_cfg_vlan(self):
        if self.is_loopback:
            self.skip('Not relevant on loopback')

        if not CTRexScenario.router_cfg['no_dut_config']:
            self.router.configure_basic_interfaces(vlan = True)
            self.router.config_pbr(mode = "config", vlan = True)

        mult = self.get_benchmark_param('multiplier')
        core = self.get_benchmark_param('cores')

        self.create_vlan_conf_file()

        ret = self.trex.start_trex (
            c = core,
            m = mult,
            d = 60,
            f = 'cap2/dns.yaml',
            cfg = '/tmp/trex_files/trex_cfg_vlan.yaml',
            l = 100,
            limit_ports = 4)

        trex_res = self.trex.sample_to_run_finish()
        print("\nLATEST RESULT OBJECT:")
        print(trex_res)
        self.check_general_scenario_results(trex_res, check_latency = True)

    def tearDown(self):
        pass

    # Add vlan to platform config file
    # write the result to new config file in /tmp, to be used by the test
    def create_vlan_conf_file(self):

        remote_cfg_file = self.trex.get_trex_config()
        out = open("/tmp/trex_cfg_vlan.yaml", "w")
        insert_vlan = False
        vlan_val_1 = "100"
        vlan_val_2 = "200"
        vlan_val = vlan_val_1

        for line in remote_cfg_file.split("\n"):
            out.write (line + "\n")
            # when we see "port_info" start inserting, until port_info section end
            if "port_info" in line.lstrip():
                num_space_port_info = len(line) - len(line.lstrip())
                insert_vlan = True
                continue

            if insert_vlan:
                num_space = len(line) - len(line.lstrip())
                if num_space == num_space_port_info:
                    insert_vlan = False
                    continue
                if len(line.lstrip()) > 0 and line.lstrip()[0] == '-':
                    # insert vlan for each port (every time a section start with '-'
                    num_space = len(line) - len(line.lstrip(' -'))
                    # need to insert in the same indentation as other lines in the section
                    out.write(num_space* " " + "vlan: " + vlan_val + "\n")
                    if vlan_val == vlan_val_1:
                        vlan_val = vlan_val_2                            
                    else:
                        vlan_val = vlan_val_1
        out.close()
        self.trex.push_files("/tmp/trex_cfg_vlan.yaml");
                        
if __name__ == "__main__":
    pass
