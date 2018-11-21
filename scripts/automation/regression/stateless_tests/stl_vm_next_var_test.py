#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys
import glob
from nose.tools import nottest


class VmNextVar_Test(CStlGeneral_Test):
    """ Tests for next_var in VM vars """
    cores_verified = False

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.ports_map['bi'][0]

        self.c.connect()
        self.c.reset(ports = [self.tx_port, self.rx_port])

        port_info = self.c.get_port_info(ports = self.rx_port)[0]

        self.base_pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        self.c.clear_stats()

        
    def cleanup (self):
        self.c.remove_all_captures()
        self.c.reset(ports = [self.tx_port, self.rx_port])


    def test_next_var_negative(self):

        try:
            vm = STLVM()
            vm.var(name='simple', min_value=1, max_value=3, op='inc', size=4, step=1, next_var='simple')
            
        except CTRexPacketBuildException as e:
            assert e.message == "Self loops are forbidden."

        try:
            vm = STLScVmRaw( [ STLVmFlowVar('ip_src', min_value='10.0.0.1', max_value='10.0.0.255',
                                            size=4, step=1, op='inc', next_var='ip_src')])
        except CTRexPacketBuildException as e:
            assert e.message == "Self loops are forbidden."

        try:
            vm = STLVM()
            vm.var(name='simple', min_value=1, max_value=3, op='random', size=4, next_var='also_simple')
            vm.var(name='also_simple', min_value=1, max_value=4, op='inc', size=4)
        except CTRexPacketBuildException as e:
            assert "If next_var is defined then op can't be random." in e.message

        try:
            vm = STLVM()
            vm.var(name='first', min_value=1, max_value=4, op='inc', size=4, next_var='second')
            vm.var(name='second', min_value=1, max_value=4, op='inc', size=4, next_var='first')
            pkt = STLPktBuilder(pkt=self.base_pkt, vm=vm)
        except CTRexPacketBuildException as e:
            assert e.message == "Loops are forbidden for dependent variables"

        try:
            vm = STLVM()
            vm.var(name='first', min_value=1, max_value=4, op='inc', size=4, next_var='second')
            vm.var(name='middle', min_value=1, max_value=4, op='inc', size=4)
            vm.var(name='second', min_value=1, max_value=4, op='inc', size=4, next_var='first')
            pkt = STLPktBuilder(pkt=self.base_pkt, vm=vm)
        except CTRexPacketBuildException as e:
            assert e.message == "Loops are forbidden for dependent variables"

        try:
            vm = STLVM()
            vm.var(name='first', min_value=1, max_value=4, op='inc', size=4, next_var='third')
            vm.var(name='second', min_value=1, max_value=4, op='inc', size=4, next_var='third')
            vm.var(name='third', min_value=1, max_value=4, op='inc', size=4)
            pkt = STLPktBuilder(pkt=self.base_pkt, vm=vm)
        except CTRexPacketBuildException as e:
            assert "is pointed by two vars" in e.message

        try:
            vm = STLVM()
            vm.var(name='first', min_value=1, max_value=4, op='inc', size=4, next_var='third')
            vm.var(name='second', min_value=1, max_value=4, op='inc', size=4, next_var='third')
            vm.var(name='third', min_value=1, max_value=4, op='inc', size=4, next_var='first')
            pkt = STLPktBuilder(pkt=self.base_pkt, vm=vm)
        except CTRexPacketBuildException as e:
            assert "is pointed by two vars" in e.message

    def test_next_var_order(self):
        vm1 = STLVM()
        vm1.var(name='ip_src', min_value='10.0.0.1', max_value='10.0.0.255',
                                        size=4, step=1, op='inc')
        vm1.write(fv_name='ip_src', pkt_offset='IP.src' )
        vm1.fix_chksum(offset='IP')
        vm1.var(name='ip_dst', min_value='8.0.0.1', max_value='8.0.0.10', size=4, op='inc', next_var='ip_src')
        pkt = STLPktBuilder(pkt=self.base_pkt, vm=vm1)
        json = pkt.to_json()
        first_inst = json['vm']['instructions'][0]
        second_inst = json['vm']['instructions'][1]
        third_inst = json['vm']['instructions'][2]
        assert first_inst['type'] == 'flow_var'
        assert first_inst['name'] == 'ip_dst'
        assert first_inst['next_var'] == 'ip_src'
        assert second_inst['type'] == 'flow_var'
        assert second_inst['name'] == 'ip_src'
        assert third_inst['type'] == 'write_flow_var'

        vm2 = STLVM()
        vm2.var(name='var1', min_value = ord('a'), max_value = ord('c'), size = 2, step = 1, op = 'inc', next_var = 'var3')
        vm2.write(fv_name='var1', pkt_offset=42)
        vm2.var(name='var2', min_value = ord('a'), max_value = ord('b'), size = 2, step = 1, op = 'inc')
        vm2.write(fv_name='var2', pkt_offset=43)
        vm2.var(name='var3', min_value = ord('a'), max_value = ord('d'), size = 2, step = 1, op = 'inc')
        vm2.write(fv_name='var3', pkt_offset=44)
        pkt2 = STLPktBuilder(pkt=self.base_pkt, vm=vm2)
        json2 = pkt2.to_json()
        first_inst_vm2 = json2['vm']['instructions'][0]
        second_inst_vm2 = json2['vm']['instructions'][1]
        third_inst_vm2 = json2['vm']['instructions'][2]
        assert first_inst_vm2['type'] == 'flow_var'
        assert first_inst_vm2['name'] == 'var1'
        assert second_inst_vm2['type'] == 'flow_var'
        assert second_inst_vm2['name'] == 'var3'
        assert third_inst_vm2['type'] == 'flow_var'
        assert third_inst_vm2['name'] == 'var2'
        fourth_inst_vm2  = json2['vm']['instructions'][3]
        assert fourth_inst_vm2['type'] == 'write_flow_var'
        assert fourth_inst_vm2['name'] == 'var1'
