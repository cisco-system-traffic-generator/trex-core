#!/router/bin/python

import pkt_bld_general_test
from client_utils.packet_builder import *
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises

class CTRexVM_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        self.vm = CTRexPktBuilder.CTRexVM()

    def test_add_flow_man_inst(self):
        self.vm.add_flow_man_inst("src_ip")
        assert_raises(CTRexPktBuilder.VMVarNameExistsError, self.vm.add_flow_man_inst, "src_ip")
        self.vm.add_flow_man_inst("dst_ip", size=8, operation="inc", init_value=5, max_value=100)
        assert_equal(self.vm.vm_variables["dst_ip"].dump(),
                     {"type": "flow_var",
                      "name": "dst_ip",
                      "size": 8,
                      # "big_endian": True,
                      "op": "inc",
#                     "split_by_core": False,
                      "init_value": 5,
                      "min_value": 1,
                      "max_value": 100})
        assert_raises(CTRexPktBuilder.VMFieldNameError, self.vm.add_flow_man_inst, "src_mac", no_field=1)

    def test_load_flow_man(self):
        vm2 = CTRexPktBuilder.CTRexVM()
        vm2.add_flow_man_inst("dst_ip", size=8, operation="inc", init_value=5, max_value=100)
        self.vm.load_flow_man(vm2.vm_variables["dst_ip"])
        assert_equal(self.vm.vm_variables["dst_ip"].dump(),
                     vm2.vm_variables["dst_ip"].dump())

    def test_set_vm_var_field(self):
        self.vm.add_flow_man_inst("src_ip")
        self.vm.set_vm_var_field("src_ip", "size", 8)
        assert_equal(self.vm.vm_variables["src_ip"].size, 8)
        assert_raises(KeyError, self.vm.set_vm_var_field, "no_var", "no_field", 10)
        assert_raises(CTRexPktBuilder.VMFieldNameError, self.vm.set_vm_var_field, "src_ip", "no_field", 10)
        assert_raises(CTRexPktBuilder.VMFieldValueError, self.vm.set_vm_var_field, "src_ip", "operation", "rand")

    def test_dump(self):
        self.vm.add_flow_man_inst("dst_ip", size=8, operation="inc", init_value=5, max_value=100)
        self.vm.add_flow_man_inst("src_ip", size=8, operation="dec", init_value=10, min_value=2, max_value=100)
        from pprint import pprint
        print ''
        pprint (self.vm.dump())
        assert_equal(self.vm.dump(),
                {'instructions': [{'init_value': 10,
                                   'max_value':  100,
                                   'min_value':   2,
                                   'name': 'src_ip',
                                   'op': 'dec',
                                   'size': 8,
                                   'type': 'flow_var'},
                                  {'init_value': 5,
                                   'max_value': 100,
                                   'min_value': 1,
                                   'name': 'dst_ip',
                                   'op': 'inc',
                                   'size': 8,
                                   'type': 'flow_var'}],
                 'split_by_var': ''}
        )


    def tearDown(self):
        pass


if __name__ == "__main__":
    pass

