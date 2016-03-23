#!/router/bin/python

from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises
import sys
import outer_packages


class CGeneralPktBld_Test(object): 
    def __init__(self):
        pass

    @staticmethod
    def print_packet(pkt_obj):
        print("\nGenerated packet:\n{}".format(repr(pkt_obj)))


    def setUp(self):
        pass


    def tearDown(self):
        pass

if __name__ == "__main__":
    pass
