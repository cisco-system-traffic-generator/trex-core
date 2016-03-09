import os, sys
import unittest
from trex import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_stl_lib.api import *
import time
from nose.tools import nottest


class CStlGeneral_Test(CTRexGeneral_Test):
    """This class defines the general stateless testcase of the T-Rex traffic generator"""

    #once for all tests under CStlGeneral_Test
    @classmethod
    def setUpClass(cls):
        cls.stl_trex = CTRexScenario.stl_trex

    def setUp(self):
        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if CTRexScenario.stl_init_error:
            self.skip(CTRexScenario.stl_init_error)

    @staticmethod
    def connect(timeout = 20):
        sys.stdout.write('Connecting')
        for i in range(timeout):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                CTRexScenario.stl_trex.connect()
                return
            except:
                time.sleep(1)
        CTRexScenario.stl_trex.connect()

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

        CTRexScenario.stl_init_error = 'Client could not connect'
        self.connect()
        CTRexScenario.stl_init_error = 'Client could not map ports'
        CTRexScenario.stl_ports_map = stl_map_ports(CTRexScenario.stl_trex)
        CTRexScenario.stl_init_error = 'Could not determine bidirectional ports'
        print 'Ports mapping: %s' % CTRexScenario.stl_ports_map
        if not len(CTRexScenario.stl_ports_map['bi']):
            raise STLError('No bidirectional ports')
        CTRexScenario.stl_init_error = None
