#!/router/bin/python

from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises
from nose.plugins.skip import SkipTest
from nose.plugins.attrib import attr
from trex_scenario import CTRexScenario
import os

generated_path = os.path.join(CTRexScenario.scripts_path, 'generated')
if not os.path.exists(generated_path):
    os.makedirs(generated_path)

class CGeneralFunctional_Test(object): 

    def setUp(self):
        pass


    def tearDown(self):
        pass

    # skip running of the test, counts as 'passed' but prints 'skipped'
    def skip(self, message = 'Unknown reason'):
        print('Skip: %s' % message)
        self.skipping = True
        raise SkipTest(message)


# classes/tests with this attribute will run only at night
#@attr('nightly')
#class CNightly_Test(CGeneralFunctional_Test):
#    def test_nightly(self):
#        pass

