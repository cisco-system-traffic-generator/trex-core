import sys
import unittest
from inspect import signature

# from trex_stl_lib.api import *
from wireless.trex_wireless_rpc_message import *
from wireless.trex_wireless_worker_rpc import *
from wireless.trex_wireless_worker import *


class WorkerCallTest(unittest.TestCase):
    """Tests methods for the WorkerCall subclasses.
    
    These tests test the existence of the functions on the worker.
    """

    def test_function_well_defined(self):
        """Test that all WorkerCalls defined in 'trex_wireless_worker_rpc' are well defined worker calls in 'trex_wireless_worker'."""
        workercalls_classes = WorkerCall.__subclasses__() # WorkerCall subclasses
        workercalls_names = [wc.NAME for wc in workercalls_classes] # Names of the WorkerCall subclasses
        methods_name_list = [func for func in dir(WirelessWorker) if callable(getattr(WirelessWorker, func))] # all method names of WirelessWorker
        methods_list = [getattr(WirelessWorker, func) for func in methods_name_list] # all methods of WirelessWorker
        remote_calls_list = [method for method in methods_list if hasattr(method, "remote_call") and method.remote_call] # all methods tagged as remote_call of WirelessWorker
        remote_calls_names_list = [method.__name__ for method in remote_calls_list] # all method names tagged as remote_call of WirelessWorker
        
        # check that all WorkerCalls are defined as a WirelessWorker method
        for wc in workercalls_names:
            self.assertTrue(wc in remote_calls_names_list, "WorkerCall {} should be implemented as a remote call in WirelessWorker".format(wc))
        # check that all remote calls in WirelessWorker are defined as a WorkerCall
        for rc in remote_calls_names_list:
            self.assertTrue(rc in workercalls_names, "remote Call {} should be implemnented as a WorkerCall".format(rc))

        # check that all WorkerCalls match in the number of arguments
        for wc in workercalls_classes:
            init_sig_params = signature(wc.__init__).parameters
            wc_num_args = len(init_sig_params) - 1 # minus 1 for the worker parameter in WorkerCall's __init__

            method = getattr(WirelessWorker, wc.NAME)
            wc_method_num_args = len(signature(method).parameters) 

            self.assertEqual(wc_num_args, wc_method_num_args, "WorkerCall and WirelessWorker method '{}''s signature do not match".format(wc.NAME))
