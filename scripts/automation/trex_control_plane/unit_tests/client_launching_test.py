#!/router/bin/python
from control_plane_general_test import CControlPlaneGeneral_Test
from Client.trex_client import CTRexClient

import socket
from nose.tools import assert_raises


class CClientLaunching_Test(CControlPlaneGeneral_Test):
    def __init__(self):
    	super(CClientLaunching_Test, self).__init__()
    	pass

    def setUp(self):
        pass

    def test_wrong_hostname(self):
        # self.tmp_server = CTRexClient('some-invalid-hostname')
        assert_raises (socket.gaierror, CTRexClient, 'some-invalid-hostname' )

    # perform this test only if server is down, but server machine is up
    def test_refused_connection(self):
        assert_raises (socket.error, CTRexClient, 'trex-dan')       # Assuming 'trex-dan' server is down! otherwise test fails
        

    def test_verbose_mode(self):
        tmp_client = CTRexClient(self.trex_server_name, verbose = True)
        pass

    def tearDown(self):
        pass
