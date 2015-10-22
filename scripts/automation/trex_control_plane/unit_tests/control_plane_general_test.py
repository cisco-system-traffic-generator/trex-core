#!/router/bin/python

__copyright__ = "Copyright 2015"

"""
Name:
     control_plane_general_test.py


Description:

    This script creates the functionality to test the performance of the TRex traffic generator control plane.
    The scenarios assumes a WORKING server is listening and processing the requests.

::

    Topology:

       --------                         --------
      |        |                       |        |
      | Client | <-----JSON-RPC------> | Server |
      |        |                       |        |
       --------                         --------

"""
from nose.plugins import Plugin
# import misc_methods
import sys
import os
# from CPlatformUnderTest import *
# from CPlatform import *
import termstyle
import threading
from common.trex_exceptions import *
from Client.trex_client import CTRexClient
# import Client.outer_packages
# import Client.trex_client

TREX_SERVER = None

class CTRexCP():
    trex_server = None

def setUpModule(module):
    pass

def tearDownModule(module):
    pass


class CControlPlaneGeneral_Test(object):#(unittest.TestCase):
    """This class defines the general testcase of the control plane service"""
    def __init__ (self):
        self.trex_server_name = 'csi-kiwi-02'
        self.trex             = CTRexClient(self.trex_server_name)
        pass

    def setUp(self):
        # initialize server connection for single client
        # self.server = CTRexClient(self.trex_server)
        pass

    ########################################################################
    ####                DO NOT ADD TESTS TO THIS FILE                   ####
    ####    Added tests here will held once for EVERY test sub-class    ####
    ########################################################################

    def tearDown(self):
        pass

    def check_for_trex_crash(self):
        pass
