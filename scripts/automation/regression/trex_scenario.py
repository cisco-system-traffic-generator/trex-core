#!/router/bin/python

import os
import sys
import subprocess
import misc_methods
import re
import signal
import time
from CProgressDisp import TimedProgressBar
from stateful_tests.tests_exceptions import TRexInUseError
import datetime
import copy
import outer_packages
import yaml

class CTRexScenario:
    modes            = set() # list of modes of this setup: loopback, virtual etc.
    server_logs      = False
    is_test_list     = False
    is_init          = False
    is_stl_init      = False
    trex_crashed     = False
    configuration    = None
    trex             = None
    stl_trex         = None
    astf_trex        = None
    stl_ports_map    = None
    stl_init_error   = None
    astf_init_error  = None
    router           = None
    router_cfg       = None
    daemon_log_lines = 0
    setup_name       = None
    setup_dir        = None
    router_image     = None
    trex_version     = None
    scripts_path     = None
    benchmark        = None
    report_dir       = 'reports'
    # logger         = None
    test_types       = {'functional_tests': [], 'stateful_tests': [], 'stateless_tests': [], 'astf_tests': [], 'wireless_tests': []}
    pkg_updated      = False
    GAManager        = None
    no_daemon        = False
    debug_image      = False
    test             = None
    json_verbose     = False
    elk              = None
    elk_info         = None
    global_cfg       = None
    config_dict      = None

def copy_elk_info ():
   assert(CTRexScenario.elk_info)
   d = copy.deepcopy(CTRexScenario.elk_info);

   timestamp = datetime.datetime.now() - datetime.timedelta(hours=2); # Jerusalem timeZone, Kibana does not have feature to change timezone 
   d['timestamp']=timestamp.strftime("%Y-%m-%d %H:%M:%S")
   return(d)

global_cfg = 'cfg/global_regression_cfg.yaml'
if not os.path.exists(global_cfg):
    raise Exception('Global configuration file %s is missing' % global_cfg)
with open(global_cfg) as f:
    CTRexScenario.global_cfg = yaml.safe_load(f.read())




