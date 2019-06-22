import sys
import unittest
from inspect import signature

# from trex_stl_lib.api import *
from wireless.trex_wireless_rpc_message import *
from wireless.trex_wireless_traffic_handler_rpc import *
from wireless.trex_wireless_traffic_handler import *


class TrafficHandlerCallTest(unittest.TestCase):
    """Tests methods for the TrafficHandlerCall subclasses.
    
    These tests test the existence of the functions on the traffic_handler.
    """

    def test_function_well_defined(self):
        """Test that all TrafficHandlerCalls defined in 'trex_wireless_traffic_handler_rpc' are well defined traffic_handler calls in 'trex_wireless_traffic_handler'."""
        traffic_handlercalls_classes = TrafficHandlerCall.__subclasses__() # TrafficHandlerCall subclasses
        traffic_handlercalls_names = [wc.NAME for wc in traffic_handlercalls_classes] # Names of the TrafficHandlerCall subclasses
        methods_name_list = [func for func in dir(TrafficHandler) if callable(getattr(TrafficHandler, func))] # all method names of TrafficHandler
        methods_list = [getattr(TrafficHandler, func) for func in methods_name_list] # all methods of TrafficHandler
        remote_calls_list = [method for method in methods_list if hasattr(method, "remote_call") and method.remote_call] # all methods tagged as remote_call of TrafficHandler
        remote_calls_names_list = [method.__name__ for method in remote_calls_list] # all method names tagged as remote_call of TrafficHandler
        
        # check that all TrafficHandlerCalls are defined as a TrafficHandler method
        for wc in traffic_handlercalls_names:
            self.assertTrue(wc in remote_calls_names_list, "TrafficHandlerCall {} should be implemented as a remote call in TrafficHandler".format(wc))
        # check that all remote calls in TrafficHandler are defined as a TrafficHandlerCall
        for rc in remote_calls_names_list:
            self.assertTrue(rc in traffic_handlercalls_names, "remote Call {} should be implemnented as a TrafficHandlerCall".format(rc))

        # check that all TrafficHandlerCalls match in the number of arguments
        for wc in traffic_handlercalls_classes:
            init_sig_params = signature(wc.__init__).parameters
            wc_num_args = len(init_sig_params)

            method = getattr(TrafficHandler, wc.NAME)
            wc_method_num_args = len(signature(method).parameters) 

            self.assertEqual(wc_num_args, wc_method_num_args, "TrafficHandlerCall and TrafficHandler method '{}''s signature do not match".format(wc.NAME))
