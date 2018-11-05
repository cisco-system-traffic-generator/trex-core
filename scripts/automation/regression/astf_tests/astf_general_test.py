import os, sys
import unittest
from trex_scenario import CTRexScenario
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex.astf.api import *
import time
from nose.tools import nottest

class CASTFGeneral_Test(CTRexGeneral_Test):
    """This class defines the general ASTF testcase of the TRex traffic generator"""

    def setUp(self):
        self.astf_trex = CTRexScenario.astf_trex if CTRexScenario.astf_trex else 'mock'
        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if CTRexScenario.astf_init_error:
            self.skip(CTRexScenario.astf_init_error)
        if type(self.astf_trex) is ASTFClient:
            self.astf_trex.reset()

    def connect(self, tries = 10):
        # need delay and check only because TRex process might be still starting
        sys.stdout.write('Connecting')
        err = ''
        for i in range(tries):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                self.astf_trex.connect()
                self.astf_trex.acquire(force = True)
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
                CTRexScenario.astf_trex.remove_all_captures()
                CTRexScenario.ports_map = self.astf_trex.map_ports()
                if self.verify_bidirectional(CTRexScenario.ports_map):
                    print('')
                    return True
            except Exception as e:
                raise
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
        return CTRexScenario.astf_trex.get_port_count()

    @staticmethod
    def is_connected():
        return CTRexScenario.astf_trex.is_connected()


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
        conf = self.configuration.trex
        cores = conf.get('astf_cores', conf.get('trex_cores', 1))
        if self.is_virt_nics and cores > 1:
            raise Exception('Number of cores should be 1 with virtual NICs')
        if not CTRexScenario.no_daemon:
            self.trex.start_astf(c = cores)
        self.astf_trex = ASTFClient(username = 'TRexRegression',
                                    server = conf['trex_name'],
                                    verbose_level = "debug" if CTRexScenario.json_verbose else "none")
        CTRexScenario.astf_trex = self.astf_trex
        sys.stdout.write('done. (%ss)\n' % int(time.time() - start_time))


    def update_elk_obj(self):
        info = self.astf_trex.get_server_system_info()
        setup = CTRexScenario.elk_info['info']['setup']
        setup['drv-name']  = info['ports'][0]['driver']
        setup['nic-ports'] = info['port_count']
        setup['nic-speed'] = str(self.astf_trex.get_port_info(0))

    def get_driver_params(self):
        c = CTRexScenario.astf_trex
        driver = c.any_port.get_formatted_info()['driver']
        return self.get_per_driver_params()[driver]

class ASTFBasic_Test(CASTFGeneral_Test):
    # will run it first explicitly, check connectivity and configure routing
    @nottest
    def test_connectivity(self):
        CTRexScenario.astf_init_error = 'Unknown error'
        if not self.is_loopback:
            try:
                self.config_dut()
            except Exception as e:
                print('')
                CTRexScenario.astf_init_error = 'Could not configure device, err: %s' % e
                self.fail(CTRexScenario.astf_init_error)
                return

        try:
            self.start_trex()
        except Exception as e:
            print('')
            CTRexScenario.astf_init_error = 'Could not start ASTF TRex, err: %s' % e
            self.fail(CTRexScenario.astf_init_error)
            return

        if not self.connect():
            CTRexScenario.astf_init_error = 'Client could not connect'
            self.fail(CTRexScenario.astf_init_error)
            return
        print('Connected')

        if not self.map_ports():
            CTRexScenario.astf_init_error = 'Client could not map ports'
            self.fail(CTRexScenario.astf_init_error)
            return
        print('Got ports mapping: %s' % CTRexScenario.ports_map)

        #update elk const object
        if self.elk:
            self.update_elk_obj()

        CTRexScenario.astf_init_error = None


