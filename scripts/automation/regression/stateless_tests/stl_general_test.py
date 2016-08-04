import os, sys
import unittest
from trex import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_stl_lib.api import *
import time
from nose.tools import nottest

class CStlGeneral_Test(CTRexGeneral_Test):
    """This class defines the general stateless testcase of the TRex traffic generator"""

    def setUp(self):
        self.stl_trex = CTRexScenario.stl_trex
        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if CTRexScenario.stl_init_error:
            self.skip(CTRexScenario.stl_init_error)

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
            try:
                if CTRexScenario.router_cfg['forceCleanConfig']:
                    CTRexScenario.router.load_clean_config()
                CTRexScenario.router.configure_basic_interfaces()
                CTRexScenario.router.config_pbr(mode = "config")
                CTRexScenario.router.config_ipv6_pbr(mode = "config")
            except Exception as e:
                CTRexScenario.stl_init_error = 'Could not configure device, err: %s' % e
                self.fail(CTRexScenario.stl_init_error)
                return
        if not self.connect():
            CTRexScenario.stl_init_error = 'Client could not connect'
            self.fail(CTRexScenario.stl_init_error)
            return
        print('Connected')
        if not self.map_ports():
            CTRexScenario.stl_init_error = 'Client could not map ports'
            self.fail(CTRexScenario.stl_init_error)
            return
        print('Got ports mapping: %s' % CTRexScenario.stl_ports_map)
