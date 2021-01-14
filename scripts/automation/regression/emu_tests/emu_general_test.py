import os
import sys

from nose.tools import nottest
from stateful_tests.trex_general_test import CTRexGeneral_Test
from trex_scenario import CTRexScenario
from trex_stl_lib.api import *


class CEmuGeneral_Test(CTRexGeneral_Test):
    """This class defines the general EMU testcase of the TRex traffic generator"""

    def setUp(self):
        """ Set up the basics for a general EMU test. Override the parent setUp function."""
        if not self.emu_trex:
            # Create an STL client, since we run the TRex in interactive STL mode for EMU.
            self.emu_trex = STLClient(username = 'TRexRegression',
                                      server = self.configuration.trex['trex_name'],
                                      verbose_level = "debug" if CTRexScenario.json_verbose else "none")
            CTRexScenario.emu_trex = self.emu_trex

        CTRexGeneral_Test.setUp(self)
        # check basic requirements, should be verified at test_connectivity, here only skip test
        if not self.is_emu:
            self.skip('Can run only with Emu mode, server must be connected to a router!')
        if CTRexScenario.emu_init_error:
            self.skip(CTRexScenario.emu_init_error)

    def connect(self, tries = 10):
        """ Connect to the TRex server"""
        # need to delay and check only because TRex process might be still starting
        sys.stdout.write('Connecting')
        err = ''
        for i in range(tries):
            try:
                sys.stdout.write('.')
                sys.stdout.flush()
                self.emu_trex.connect() # using the STL client
                self.emu_trex.acquire(force = True)
                self.emu_trex.stop()
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
        """Get the port count of the server"""
        return CTRexScenario.emu_trex.get_port_count()

    @staticmethod
    def is_connected():
        """Is the client successfully connected to the server?"""
        return CTRexScenario.emu_trex.is_connected()

    def config_dut(self):
        """Configure the DUT (Router in our case) which we use for testing the EMU 
        functionality prior to the test"""
        sys.stdout.flush()
        if not CTRexScenario.router_cfg['no_dut_config']:
            sys.stdout.write('Configuring DUT... ')
            start_time = time.time()
            CTRexScenario.router.load_clean_config()
            CTRexScenario.router.configure_basic_interfaces()
            sys.stdout.write('Done. (%ss)\n' % int(time.time() - start_time))

    def start_trex(self):
        """Start the TRex server in stateless mode with the required parameters for a successful Emu connection"""
        sys.stdout.write('Starting TRex with Emu... ')
        start_time = time.time()
        cores = self.configuration.trex.get('trex_cores', 1)
        if not CTRexScenario.no_daemon:
            self.trex.start_stateless(c = cores, emu = True, i = True, software = True, no_scapy = True, cfg = CTRexScenario.emu_config_file)
        sys.stdout.write('Done. (%ss)\n' % int(time.time() - start_time))

    def update_elk_obj(self):
        emu_info = self.emu_trex.get_server_system_info()
        setup = CTRexScenario.elk_info['info']['setup']
        setup['drv-name']  = emu_info['ports'][0]['driver']
        setup['nic-ports'] = emu_info['port_count']
        setup['nic-speed'] = str(self.emu_trex.get_port_info(0))

    @classmethod
    def get_driver_params(cls):
        """Get the driver parameters."""
        c = CTRexScenario.emu_trex
        driver = c.any_port.get_formatted_info()['driver']
        return cls.get_per_driver_params()[driver]

class EmuBasic_Test(CEmuGeneral_Test):

    def setUp(self):
        """Set up for the basic test"""
        try:
            CEmuGeneral_Test.setUp(self)
        except Exception as e:
            CTRexScenario.emu_init_error = 'First setUp error: %s' % e
            raise

    # will run it first explicitly, check connectivity and configure routing
    @nottest # don't count this as a test.
    def test_connectivity(self):
        """This is run at the beginning, needs to be called explicitly. It verifies that the router is connected
        properly"""
        print('')
        CTRexScenario.emu_init_error = 'Unknown error'
        if not self.is_loopback:
            # Try to configure the DUT device.
            try:
                self.config_dut()
            except Exception as e:
                CTRexScenario.emu_init_error = 'Could not configure the DUT device, err: %s' % e
                self.fail(CTRexScenario.emu_init_error)
                return
            print('Configured DUT')

        # The DUT device is configured, let's attempt to start TRex.
        try:
            self.start_trex()
        except Exception as e:
            CTRexScenario.emu_init_error = 'Could not start TRex, err: %s' % e
            self.fail(CTRexScenario.emu_init_error)
            return
        print('Started TRex')

        # Let's attempt to connect to the TRex server we just started.
        if not self.connect():
            CTRexScenario.emu_init_error = 'Client could not connect to the server.'
            self.fail(CTRexScenario.emu_init_error)
            return
        print('Connected')

        #update elk const object 
        if self.elk:
            self.update_elk_obj()

        CTRexScenario.emu_init_error = None
