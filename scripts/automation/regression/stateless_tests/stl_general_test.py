import os, sys
import unittest
from trex import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_stl_lib.api import *
import time
from nose.tools import nottest

def setUpModule():
    if CTRexScenario.stl_trex.is_connected():
        CStlGeneral_Test.recover_after_trex_210_issue()

class CStlGeneral_Test(CTRexGeneral_Test):
    """This class defines the general stateless testcase of the T-Rex traffic generator"""

    def setUp(self):
        self.stl_trex = CTRexScenario.stl_trex
        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if CTRexScenario.stl_init_error:
            self.skip(CTRexScenario.stl_init_error)

    # workaround of http://trex-tgn.cisco.com/youtrack/issue/trex-210
    @staticmethod
    def recover_after_trex_210_issue():
        for i in range(20): 
            try:
                stl_map_ports(CTRexScenario.stl_trex)
                break
            except:
                CTRexScenario.stl_trex.disconnect()
                time.sleep(0.5)
                CTRexScenario.stl_trex.connect()
        # verify problem is solved
        stl_map_ports(CTRexScenario.stl_trex)

    def connect(self, timeout = 100):
        # need delay and check only because TRex process might be still starting
        sys.stdout.write('Connecting')
        for i in range(timeout):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                self.stl_trex.connect()
                print('')
                return True
            except:
                time.sleep(0.1)
        print('')
        return False

    def map_ports(self, timeout = 100):
        sys.stdout.write('Mapping ports')
        for i in range(timeout):
            sys.stdout.write('.')
            sys.stdout.flush()
            CTRexScenario.stl_ports_map = stl_map_ports(self.stl_trex)
            if self.verify_bidirectional(CTRexScenario.stl_ports_map):
                print('')
                return True
            time.sleep(0.1)
        print('')
        return False

    # verify all the ports are bidirectional
    @staticmethod
    def verify_bidirectional(mapping_dict):
        if len(mapping_dict['unknown']):
            return False
        if len(mapping_dict['bi']) * 2 == len(mapping_dict['map']):
            return True
        return False

    @staticmethod
    def get_port_count():
        return CTRexScenario.stl_trex.get_port_count()

    @staticmethod
    def is_connected():
        return CTRexScenario.stl_trex.is_connected()

class STLBasic_Test(CStlGeneral_Test):
    # will run it first explicitly, check connectivity and configure routing
    @nottest
    def test_connectivity(self):
        if not self.is_loopback:
            CTRexScenario.router.load_clean_config()
            CTRexScenario.router.configure_basic_interfaces()
            CTRexScenario.router.config_pbr(mode = "config")

        err = 'Client could not connect'
        CTRexScenario.stl_init_error = err
        if not self.connect():
            self.fail(err)
        err = 'Client could not map ports'
        CTRexScenario.stl_init_error = err
        if not self.map_ports():
            self.fail(err)
        print('Got ports mapping: %s' % CTRexScenario.stl_ports_map)
        CTRexScenario.stl_init_error = None
