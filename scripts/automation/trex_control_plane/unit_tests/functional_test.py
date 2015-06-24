#!/router/bin/python
from control_plane_general_test import CControlPlaneGeneral_Test
from Client.trex_client import CTRexClient

import socket
from nose.tools import assert_raises, assert_equal, assert_not_equal
from common.trex_status_e import TRexStatus
from common.trex_exceptions import *
from enum import Enum
import time


class CTRexStartStop_Test(CControlPlaneGeneral_Test):
    def __init__(self):
        super(CTRexStartStop_Test, self).__init__()
        self.valid_start_params = dict( c = 4, 
            m = 1.1,
            d = 100,
            f = 'avl/sfr_delay_10_1g.yaml', 
            nc = True,
            p = True,
            l = 1000)

    def setUp(self):
        pass

    def test_mandatory_param_error(self):
        start_params = dict( c = 4, 
            m = 1.1,
            d = 70,
            # f = 'avl/sfr_delay_10_1g.yaml',   <-- f (mandatory) is not provided on purpose
            nc = True,
            p = True,
            l = 1000)

        assert_raises(TypeError, self.trex.start_trex, **start_params)
        
    def test_parameter_name_error(self):
        ret = self.trex.start_trex( c = 4, 
            wrong_key = 1.1,                # <----- This key does not exists in T-Rex API
            d = 70,
            f = 'avl/sfr_delay_10_1g.yaml',
            nc = True,
            p = True,
            l = 1000)
        
        time.sleep(5)

        # check for failure status 
        run_status = self.trex.get_running_status()
        assert isinstance(run_status, dict)
        assert_equal (run_status['state'], TRexStatus.Idle )
        assert_equal (run_status['verbose'], "T-Rex run failed due to wrong input parameters, or due to reachability issues.")
        assert_raises(TRexError, self.trex.get_running_info)

    def test_too_early_sample(self):
        ret = self.trex.start_trex(**self.valid_start_params)

        assert ret==True
        # issue get_running_info() too soon, without any(!) sleep
        run_status = self.trex.get_running_status()
        assert isinstance(run_status, dict)
        assert_equal (run_status['state'], TRexStatus.Starting )
        assert_raises(TRexWarning, self.trex.get_running_info)

        ret = self.trex.stop_trex()
        assert ret==True    # make sure stop succeeded
        assert self.trex.is_running() == False

    def test_start_sampling_on_time(self):
        ret = self.trex.start_trex(**self.valid_start_params)
        assert ret==True
        time.sleep(6)
        
        run_status = self.trex.get_running_status()
        assert isinstance(run_status, dict)
        assert_equal (run_status['state'], TRexStatus.Running )

        run_info = self.trex.get_running_info()
        assert isinstance(run_info, dict)
        ret = self.trex.stop_trex()
        assert ret==True    # make sure stop succeeded
        assert self.trex.is_running() == False

    def test_start_more_than_once_same_user(self):
        assert self.trex.is_running() == False                  # first, make sure T-Rex is not running
        ret = self.trex.start_trex(**self.valid_start_params)   # start 1st T-Rex run
        assert ret == True                                      # make sure 1st run submitted successfuly
        # time.sleep(1)
        assert_raises(TRexInUseError, self.trex.start_trex, **self.valid_start_params)  # try to start T-Rex again

        ret = self.trex.stop_trex()
        assert ret==True    # make sure stop succeeded
        assert self.trex.is_running() == False

    def test_start_more_than_once_different_users(self):
        assert self.trex.is_running() == False                  # first, make sure T-Rex is not running
        ret = self.trex.start_trex(**self.valid_start_params)   # start 1st T-Rex run
        assert ret == True                                      # make sure 1st run submitted successfuly
        # time.sleep(1)
        
        tmp_trex = CTRexClient(self.trex_server_name)           # initialize another client connecting same server
        assert_raises(TRexInUseError, tmp_trex.start_trex, **self.valid_start_params)  # try to start T-Rex again

        ret = self.trex.stop_trex()
        assert ret==True    # make sure stop succeeded
        assert self.trex.is_running() == False

    def test_simultaneous_sampling(self):
        assert self.trex.is_running() == False                  # first, make sure T-Rex is not running
        tmp_trex = CTRexClient(self.trex_server_name)           # initialize another client connecting same server
        ret = self.trex.start_trex(**self.valid_start_params)   # start T-Rex run
        assert ret == True                                      # make sure 1st run submitted successfuly

        time.sleep(6)
        # now, sample server from both clients
        while (self.trex.is_running()):
            info_1 = self.trex.get_running_info()
            info_2 = tmp_trex.get_running_info()

            # make sure samples are consistent
            if self.trex.get_result_obj().is_valid_hist():
                assert tmp_trex.get_result_obj().is_valid_hist() == True
            if self.trex.get_result_obj().is_done_warmup():
                assert tmp_trex.get_result_obj().is_done_warmup() == True
            # except TRexError as inst: # T-Rex might have stopped between is_running result and get_running_info() call
            #     # hence, ingore that case
            #     break   

        assert self.trex.is_running() == False

    def test_fast_toggling(self):
        assert self.trex.is_running() == False  
        for i in range(20):
            ret = self.trex.start_trex(**self.valid_start_params)   # start T-Rex run
            assert ret == True
            assert self.trex.is_running() == False  # we expect the status to be 'Starting'
            ret = self.trex.stop_trex()
            assert ret == True
            assert self.trex.is_running() == False
        pass


    def tearDown(self):
        pass

class CBasicQuery_Test(CControlPlaneGeneral_Test):
    def __init__(self):
    	super(CBasicQuery_Test, self).__init__()
    	pass

    def setUp(self):
        pass

    def test_is_running(self):
        assert self.trex.is_running() == False
        

    def tearDown(self):
        pass
