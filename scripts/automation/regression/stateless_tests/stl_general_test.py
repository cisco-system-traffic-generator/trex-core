import os, sys
import unittest
from trex_scenario import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_stl_lib.api import *
import time
from nose.tools import nottest

class CStlGeneral_Test(CTRexGeneral_Test):
    """This class defines the general stateless testcase of the TRex traffic generator"""

    def setUp(self):
        if not self.stl_trex:
            self.stl_trex = STLClient(username = 'TRexRegression',
                                      server = self.configuration.trex['trex_name'],
                                      verbose_level = "debug" if CTRexScenario.json_verbose else "none")
            CTRexScenario.stl_trex = self.stl_trex

        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if CTRexScenario.stl_init_error:
            self.skip(CTRexScenario.stl_init_error)

    def connect(self, tries = 10):
        # need delay and check only because TRex process might be still starting
        sys.stdout.write('Connecting')
        err = ''
        for i in range(tries):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                self.stl_trex.connect()
                self.stl_trex.acquire(force = True)
                print('')
                return True
            except Exception as e:
                err = e
                time.sleep(0.5)
        print('')
        print('Error connecting: %s' % err)
        return False

    def map_ports(self, tries = 10):
        sys.stdout.write('Mapping ports')
        for i in range(tries):
            sys.stdout.write('.')
            sys.stdout.flush()
            try:
                CTRexScenario.stl_trex.remove_all_captures()
                CTRexScenario.ports_map = self.stl_trex.map_ports()
                if self.verify_bidirectional(CTRexScenario.ports_map):
                    print('')
                    return True
            except Exception as e:
                print('\nException during mapping: %s' % e)
                return False
            time.sleep(0.5)
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


    def config_dut(self):
        sys.stdout.flush()
        if not CTRexScenario.router_cfg['no_dut_config']:
            sys.stdout.write('Configuring DUT... ')
            start_time = time.time()
            if CTRexScenario.router_cfg['forceCleanConfig']:
                CTRexScenario.router.load_clean_config()
            CTRexScenario.router.configure_basic_interfaces()
            CTRexScenario.router.config_pbr(mode = "config")
            CTRexScenario.router.config_ipv6_pbr(mode = "config")
            sys.stdout.write('done. (%ss)\n' % int(time.time() - start_time))


    def start_trex(self):
        sys.stdout.write('Starting TRex... ')
        start_time = time.time()
        cores = self.configuration.trex.get('trex_cores', 1)
        if self.is_virt_nics and cores > 1:
            raise Exception('Number of cores should be 1 with virtual NICs')
        if not CTRexScenario.no_daemon:
            self.trex.start_stateless(c = cores)
        sys.stdout.write('done. (%ss)\n' % int(time.time() - start_time))


    def update_elk_obj(self):
        stl_info = self.stl_trex.get_server_system_info()
        setup = CTRexScenario.elk_info['info']['setup']
        setup['drv-name']  = stl_info['ports'][0]['driver']
        setup['nic-ports'] = stl_info['port_count']
        setup['nic-speed'] = str(self.stl_trex.get_port_info(0))

    def get_driver_params(self):
        c = CTRexScenario.stl_trex
        driver = c.any_port.get_formatted_info()['driver']
        return self.get_per_driver_params()[driver]


class STLBasic_Test(CStlGeneral_Test):
    def setUp(self):
        try:
            CStlGeneral_Test.setUp(self)
        except Exception as e:
            CTRexScenario.stl_init_error = 'First setUp error: %s' % e
            raise


    # will run it first explicitly, check connectivity and configure routing
    @nottest
    def test_connectivity(self):
        print('')
        CTRexScenario.stl_init_error = 'Unknown error'
        if not self.is_loopback:
            try:
                self.config_dut()
            except Exception as e:
                CTRexScenario.stl_init_error = 'Could not configure device, err: %s' % e
                self.fail(CTRexScenario.stl_init_error)
                return
            print('Configured DUT')

        try:
            self.start_trex()
        except Exception as e:
            CTRexScenario.stl_init_error = 'Could not start stateless TRex, err: %s' % e
            self.fail(CTRexScenario.stl_init_error)
            return
        print('Started TRex')

        if not self.connect():
            CTRexScenario.stl_init_error = 'Client could not connect'
            self.fail(CTRexScenario.stl_init_error)
            return
        print('Connected')

        if not self.map_ports():
            CTRexScenario.stl_init_error = 'Client could not map ports'
            self.fail(CTRexScenario.stl_init_error)
            return
        print('Got ports mapping: %s' % CTRexScenario.ports_map)

        #update elk const object 
        if self.elk:
            self.update_elk_obj()

        CTRexScenario.stl_init_error = None


