import sys

from nose.tools import nottest
from stateless_tests.stl_general_test import CStlGeneral_Test
from trex_scenario import CTRexScenario
from trex_stl_lib.api import *


class CStlSoftwareGeneral_Test(CStlGeneral_Test):

    def start_trex(self):
        """Override the method that starts the server so we can start the TRex server in stateless software mode"""
        sys.stdout.write('Starting TRex in STL Software mode... ')
        start_time = time.time()
        cores = self.configuration.trex.get('trex_cores', 1)
        if not CTRexScenario.no_daemon:
            self.trex.start_stateless(c=cores, i=True, software=True)
        sys.stdout.write('Done. (%ss)\n' % int(time.time() - start_time))


class StlSoftwareBasic_Test(CStlSoftwareGeneral_Test):

    def setUp(self):
        try:
            CStlSoftwareGeneral_Test.setUp(self)
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
