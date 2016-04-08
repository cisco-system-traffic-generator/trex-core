#!/router/bin/python

import functional_general_test
#HACK FIX ME START
import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.abspath(os.path.join(CURRENT_PATH, '../../trex_control_plane/common')))
#HACK FIX ME END
import filters
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises


class ToggleFilter_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        pass

    def test_ipv4_gen(self):
        pass

    def tearDown(self):
        pass
