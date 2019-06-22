import sys
import os
import unittest
import unittest.mock
from unittest.mock import patch
import time
import logging
import pickle

# from trex_stl_lib.api import *
from wireless.trex_wireless_rpc_message import *

class RPCExceptionReportTest(unittest.TestCase):
    """Test methods for the RPCExceptionReport class."""

    def test_serialization(self):
        """Test correct serialization of the class."""
        report = RPCExceptionReport(Exception("test_serialization"))

        pickled = pickle.dumps(report)
        reconstructed = pickle.loads(pickled)

        self.assertEqual(str(report), str(reconstructed))

    def test_ValueError(self):
        """Test correct instantiation of the class with a ValueError."""
        report = RPCExceptionReport(Exception("test_serialization"))

        try:
            raise ValueError("test")
        except Exception as e:
            report = RPCExceptionReport(e)
        self.assertTrue("test" in str(report))
        self.assertTrue("ValueError" in str(report))

# class WorkerCallTest(unittest.TestCase):
#     """Tests methods for the WorkerCall class.
    
#     These tests test the sending of Remote Calls by the WirelessManager to the WirelessWorker.
#     They check that the protocol is respected.
#     The remote calls messages must be of the form ('cmd', command_name, id, args).
#     IDs of the messages are overwridden for testing purposes.
#     """
#     def setUp(self):
#         self.worker=None
#         pass

#     def test_command_stop(self):
#         """Tests 'stop' call."""
#         self.assertEqual(WorkerCall_stop(self.worker).call(id=3), ("cmd", "stop", 3, ()))

#     def test_command_create_aps(self):
#         """Tests 'create_aps' call."""
#         self.assertEqual(WorkerCall_create_aps(self.worker, "port_layer_cfg", "clients").call(id=1), ("cmd", "create_aps", 1, ("port_layer_cfg", "clients")))

#     def test_command_create_clients(self):
#         """Tests 'create_clients' call."""
#         self.assertEqual(WorkerCall_create_clients(self.worker, "clients").call(id=2), ("cmd", "create_clients", 2, ("clients",)))
    
#     def test_command_join_aps_no_max_concurrent(self):
#         """Tests 'join_aps' call when max_concurrent is not specified."""
#         self.assertEqual(WorkerCall_join_aps(self.worker, "ap_ids").call(id=2), ("cmd", "join_aps", 2, ("ap_ids", float('inf'))))

#     def test_command_join_aps_max_concurrent(self):
#         """Tests 'join_aps' call when max_concurrent is specified."""
#         self.assertEqual(WorkerCall_join_aps(self.worker, "ap_ids", max_concurrent=42).call(id=2), ("cmd", "join_aps", 2, ("ap_ids", 42)))


#     def test_command_join_clients_no_max_concurrent(self):
#         """Tests 'join_clients' call when max_concurrent is not specified."""
#         self.assertEqual(WorkerCall_join_clients(self.worker, "client_ids").call(id=2), ("cmd", "join_clients", 2, ("client_ids", float('inf'))))

#     def test_command_join_clients_max_concurrent(self):
#         """Tests 'join_clients' call when max_concurrent is specified."""
#         self.assertEqual(WorkerCall_join_clients(self.worker, "client_ids", max_concurrent=42).call(id=2), ("cmd", "join_clients", 2, ("client_ids", 42)))


# class WorkerRepsonseTest(unittest.TestCase):
#     """Tests methods for the RPCResponse class.""" 

#     def test_response_call_message_success(self):
#         """Tests 'call' method of a RPCResponse when response is successful."""
#         id = 42
#         code = RPCResponse.SUCCESS
#         ret = 'return values'
#         resp = RPCResponse(id=id, code=code, ret=ret)
#         self.assertEqual(resp.call, ("resp", id, code, ret))

#     def test_response_call_message_error(self):
#         """Tests 'call' method of a RPCResponse when response is error."""
#         id = 404
#         code = RPCResponse.ERROR
#         ret = 'return values'
#         resp = RPCResponse(id=id, code=code, ret=ret)
#         self.assertEqual(resp.call, ("resp", id, code, ret))


if __name__ == '__main__':
    unittest.main()
