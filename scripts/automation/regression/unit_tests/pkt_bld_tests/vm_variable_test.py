#!/router/bin/python

import pkt_bld_general_test
from client_utils.packet_builder import *
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises

class CTRexVMVariable_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
        self.vm_var = CTRexPktBuilder.CTRexVM.CTRexVMFlowVariable("test_var")

    def test_var_name(self):
        assert_equal(self.vm_var.name, "test_var")

#   @raises(CTRexPktBuilder.VMVarNameError)
    def test_set_field(self):
        assert_raises(CTRexPktBuilder.VMFieldNameError, self.vm_var.set_field, "no_field", 10)
        # test for 'size' field
        assert_raises(CTRexPktBuilder.VMFieldTypeError, self.vm_var.set_field, "size", "wrong_type")
        assert_raises(CTRexPktBuilder.VMFieldValueError, self.vm_var.set_field, "size", 10) # 10 is illegal size
        self.vm_var.set_field("size", 8)
        assert_equal(self.vm_var.size, 8)
        # test for 'init_value' field
        assert_raises(CTRexPktBuilder.VMFieldTypeError, self.vm_var.set_field, "init_value", '123') # 123 is wrong type, should be int
        self.vm_var.set_field("init_value", 5)
        assert_equal(self.vm_var.init_value, 5)
        # test for 'operation' field
        assert_raises(CTRexPktBuilder.VMFieldTypeError, self.vm_var.set_field, "operation", 1) # operation is field of type str
        assert_raises(CTRexPktBuilder.VMFieldValueError, self.vm_var.set_field, "operation", "rand") # "rand" is illegal option
        self.vm_var.set_field("operation", "inc")
        assert_equal(self.vm_var.operation, "inc")
        # test for 'split_by_core' field
#       self.vm_var.set_field("split_by_core", 5)
#       assert_equal(self.vm_var.split_by_core, True)

    def test_var_dump (self):
        # set VM variable options
        self.vm_var.set_field("size", 8)
        self.vm_var.set_field("init_value", 5)
        self.vm_var.set_field("operation", "inc")
#       self.vm_var.set_field("split_by_core", False)
        self.vm_var.set_field("max_value", 100)
        assert_equal(self.vm_var.dump(),
                     {"type": "flow_var",
                        "name": "test_var",
                        "size": 8,
                        # "big_endian": True,
                        "op": "inc",
#                       "split_by_core": False,
                        "init_value": "5",
                        "min_value": "1",
                        "max_value": "100"})

    def tearDown(self):
        pass


if __name__ == "__main__":
    pass

