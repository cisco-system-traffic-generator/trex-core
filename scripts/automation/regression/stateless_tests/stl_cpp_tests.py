from .stl_general_test import CStlGeneral_Test, CTRexScenario
import os
import re

class STLCPP_Test(CStlGeneral_Test):
    '''Tests related to CPP'''

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        print('')
        self.c = CTRexScenario.stl_trex
        self.tx_port, self.rx_port = CTRexScenario.ports_map['bi'][0]

        port_info = self.c.get_port_info(ports = self.rx_port)[0]
        self.drv_name = port_info['driver']




