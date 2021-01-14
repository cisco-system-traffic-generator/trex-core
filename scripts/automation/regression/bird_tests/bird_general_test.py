import os, sys
import unittest
from trex_scenario import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_stl_lib.api import *
import time
from nose.tools import nottest

class CBirdGeneral_Test(CTRexGeneral_Test):
    """This class defines the general stateless testcase of the TRex traffic generator"""

    def setUp(self):
        if not self.bird_trex:
            self.bird_trex = STLClient(username = 'TRexRegression',
                                      server = self.configuration.trex['trex_name'],
                                      verbose_level = "debug" if CTRexScenario.json_verbose else "none")
            CTRexScenario.bird_trex = self.bird_trex

        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if not self.is_bird:
            self.skip('Can run only with bird mode, server on linux based stack connected to a router')
        if CTRexScenario.bird_init_error:
            self.skip(CTRexScenario.bird_init_error)

    def connect(self, tries = 10):
        # need delay and check only because TRex process might be still starting
        sys.stdout.write('Connecting')
        err = ''
        for i in range(tries):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                self.bird_trex.connect()
                self.bird_trex.acquire(force = True)
                self.bird_trex.stop()
                print('')
                return True
            except Exception as e:
                err = e
                time.sleep(0.5)
        print('')
        print('Error connecting: %s' % err)
        return False

    @staticmethod
    def get_port_count():
        return CTRexScenario.bird_trex.get_port_count()

    @staticmethod
    def is_connected():
        return CTRexScenario.bird_trex.is_connected()


    def config_dut(self):
        sys.stdout.flush()
        if not CTRexScenario.router_cfg['no_dut_config']:
            sys.stdout.write('Configuring DUT... ')
            start_time = time.time()
            CTRexScenario.router.load_clean_config()
            CTRexScenario.router.configure_basic_interfaces()
            sys.stdout.write('done. (%ss)\n' % int(time.time() - start_time))


    def start_trex(self):
        sys.stdout.write('Starting TRex... ')
        start_time = time.time()
        cores = self.configuration.trex.get('trex_cores', 1)
        if not CTRexScenario.no_daemon:
            self.trex.start_stateless(c = cores, bird_server = True, i = True, software = True, no_scapy = True, cfg = CTRexScenario.bird_config_file)
        sys.stdout.write('done. (%ss)\n' % int(time.time() - start_time))


    def update_elk_obj(self):
        bird_info = self.bird_trex.get_server_system_info()
        setup = CTRexScenario.elk_info['info']['setup']
        setup['drv-name']  = bird_info['ports'][0]['driver']
        setup['nic-ports'] = bird_info['port_count']
        setup['nic-speed'] = str(self.bird_trex.get_port_info(0))

    def get_driver_params(self):
        c = CTRexScenario.bird_trex
        driver = c.any_port.get_formatted_info()['driver']
        return self.get_per_driver_params()[driver]


class BirdBasic_Test(CBirdGeneral_Test):
    def setUp(self):
        try:
            CBirdGeneral_Test.setUp(self)
        except Exception as e:
            CTRexScenario.bird_init_error = 'First setUp error: %s' % e
            raise


    # will run it first explicitly, check connectivity and configure routing
    @nottest
    def test_connectivity(self):
        print('')
        CTRexScenario.bird_init_error = 'Unknown error'
        if not self.is_loopback:
            try:
                self.config_dut()
            except Exception as e:
                CTRexScenario.bird_init_error = 'Could not configure device, err: %s' % e
                self.fail(CTRexScenario.bird_init_error)
                return
            print('Configured DUT')

        try:
            self.start_trex()
        except Exception as e:
            CTRexScenario.bird_init_error = 'Could not start stateless TRex, err: %s' % e
            self.fail(CTRexScenario.bird_init_error)
            return
        print('Started TRex')

        if not self.connect():
            CTRexScenario.bird_init_error = 'Client could not connect'
            self.fail(CTRexScenario.bird_init_error)
            return
        print('Connected')

        #update elk const object 
        if self.elk:
            self.update_elk_obj()

        CTRexScenario.bird_init_error = None


