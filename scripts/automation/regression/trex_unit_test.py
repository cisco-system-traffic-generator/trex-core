#!/usr/bin/env python

__copyright__ = "Copyright 2014"

"""
Name:
     trex_unit_test.py


Description:

    This script creates the functionality to test the performance of the T-Rex traffic generator
    The tested scenario is a T-Rex TG directly connected to a Cisco router.

::

    Topology:

       -------                         --------
      |       | Tx---1gig/10gig----Rx |        |
      | T-Rex |                       | router |
      |       | Rx---1gig/10gig----Tx |        |
       -------                         --------

"""

import os
import sys
import outer_packages
import nose
from nose.plugins import Plugin
import logging
import CustomLogger
import misc_methods
from rednose import RedNose
import termstyle
from unit_tests.trex_general_test import CTRexScenario
from client.trex_client import *
from common.trex_exceptions import *
import trex
import socket
from pprint import pprint
import subprocess
import re

def check_trex_path(trex_path):
    if os.path.isfile('%s/trex_daemon_server' % trex_path):
        return os.path.abspath(trex_path)

def check_setup_path(setup_path):
    if os.path.isfile('%s/config.yaml' % setup_path):
        return os.path.abspath(setup_path)


def get_trex_path():
    latest_build_path = check_trex_path(os.getenv('TREX_UNDER_TEST'))    # TREX_UNDER_TEST is env var pointing to <trex-core>/scripts
    if not latest_build_path:
        latest_build_path = check_trex_path(os.path.join(os.pardir, os.pardir))
    if not latest_build_path:
        raise Exception('Could not determine trex_under_test folder, try setting env.var. TREX_UNDER_TEST')
    return latest_build_path

def _start_stop_trex_remote_server(trex_data, command):
    # start t-rex server as daemon process
    # subprocess.call(["/usr/bin/python", "trex_daemon_server", "restart"], cwd = trex_latest_build)
    misc_methods.run_remote_command(trex_data['trex_name'],
                                    trex_data['trex_password'],
                                    command)

def start_trex_remote_server(trex_data, kill_running = False):
    if kill_running:
        (return_code, stdout, stderr) = misc_methods.run_remote_command(trex_data['trex_name'],
                                    trex_data['trex_password'],
                                    'ps -u root --format comm,pid,cmd | grep t-rex-64')
        if stdout:
            for process in stdout.split('\n'):
                try:
                    proc_name, pid, full_cmd = re.split('\s+', process, maxsplit=2)
                    if proc_name.find('t-rex-64') >= 0:
                        print 'Killing remote process: %s' % full_cmd
                        misc_methods.run_remote_command(trex_data['trex_name'],
                                        trex_data['trex_password'],
                                        'kill %s' % pid)
                except:
                    continue

    _start_stop_trex_remote_server(trex_data, DAEMON_START_COMMAND)

def stop_trex_remote_server(trex_data):
    _start_stop_trex_remote_server(trex_data, DAEMON_STOP_COMMAND)

class CTRexTestConfiguringPlugin(Plugin):
    def options(self, parser, env = os.environ):
        super(CTRexTestConfiguringPlugin, self).options(parser, env)
        parser.add_option('--cfg', '--trex-scenario-config', action='store',
                            dest='config_path',
                            help='Specify path to folder with config.yaml and benchmark.yaml')
        parser.add_option('--skip-clean', '--skip_clean', action='store_true',
                            dest='skip_clean_config',
                            help='Skip the clean configuration replace on the platform.')
        parser.add_option('--load-image', '--load_image', action='store_true', default = False,
                            dest='load_image',
                            help='Install image specified in config file on router.')
        parser.add_option('--log-path', '--log_path', action='store',
                            dest='log_path',
                            help='Specify path for the tests` log to be saved at. Once applied, logs capturing by nose will be disabled.') # Default is CURRENT/WORKING/PATH/trex_log/trex_log.log')
        parser.add_option('--verbose-mode', '--verbose_mode', action="store_true", default = False,
                            dest="verbose_mode", 
                            help="Print RPC command and router commands.")
        parser.add_option('--server-logs', '--server_logs', action="store_true", default = False,
                            dest="server_logs", 
                            help="Print server side (TRex and trex_daemon) logs per test.")
        parser.add_option('--kill-running', '--kill_running', action="store_true", default = False,
                            dest="kill_running", 
                            help="Kills running TRex process on remote server (useful for regression).")
        parser.add_option('--functional', action="store_true", default = False,
                            dest="functional", 
                            help="Don't connect to remote server for runnning daemon (For functional tests).")

    def configure(self, options, conf):
        self.functional = options.functional
        self.collect_only = options.collect_only
        if self.functional or self.collect_only:
            return
        if CTRexScenario.setup_dir and options.config_path:
            raise Exception('Please either define --cfg or use env. variable SETUP_DIR, not both.')
        if not options.config_path and CTRexScenario.setup_dir:
            options.config_path = CTRexScenario.setup_dir
        if options.config_path:
            self.configuration = misc_methods.load_complete_config_file(os.path.join(options.config_path, 'config.yaml'))
            self.benchmark = misc_methods.load_benchmark_config_file(os.path.join(options.config_path, 'benchmark.yaml'))
            self.enabled = True
        else:
            raise Exception('Please specify path to config.yaml using --cfg parameter or env. variable SETUP_DIR')
        self.modes         = self.configuration.trex.get('modes', [])
        self.kill_running  = options.kill_running
        self.load_image    = options.load_image
        self.verbose_mode  = options.verbose_mode
        self.clean_config  = False if options.skip_clean_config else True
        self.server_logs   = options.server_logs
        if options.log_path:
            self.loggerPath = options.log_path

    def begin (self):
        if self.functional or self.collect_only:
            return
        # initialize CTRexScenario global testing class, to be used by all tests
        CTRexScenario.configuration = self.configuration
        CTRexScenario.benchmark     = self.benchmark
        CTRexScenario.modes         = set(self.modes)
        CTRexScenario.server_logs   = self.server_logs

        # launch TRex daemon on relevant setup
        start_trex_remote_server(self.configuration.trex, self.kill_running)
        CTRexScenario.trex          = CTRexClient(trex_host = self.configuration.trex['trex_name'], verbose = self.verbose_mode)

        if 'loopback' not in self.modes:
            CTRexScenario.router_cfg    = dict( config_dict  = self.configuration.router, 
                                                                        forceImageReload = self.load_image, 
                                                                        silent_mode      = not self.verbose_mode,
                                                                        forceCleanConfig = self.clean_config,
                                                                        tftp_config_dict = self.configuration.tftp )
        try:
            CustomLogger.setup_custom_logger('TRexLogger', self.loggerPath)
        except AttributeError:
            CustomLogger.setup_custom_logger('TRexLogger')
    
    def finalize(self, result):
        if self.functional or self.collect_only:
            return
        CTRexScenario.is_init       = False
        stop_trex_remote_server(self.configuration.trex)


def save_setup_info():
    try:
        if CTRexScenario.setup_name and CTRexScenario.trex_version:
            setup_info = ''
            for key, value in CTRexScenario.trex_version.items():
                setup_info += '{0:8}: {1}\n'.format(key, value)
            cfg = CTRexScenario.configuration
            setup_info += 'Server: %s, Modes: %s' % (cfg.trex.get('trex_name'), cfg.trex.get('modes'))
            if cfg.router:
                setup_info += '\nRouter: Model: %s, Image: %s' % (cfg.router.get('model'), CTRexScenario.router_image)
            with open('%s/report_%s.info' % (CTRexScenario.report_dir, CTRexScenario.setup_name), 'w') as f:
                f.write(setup_info)
    except Exception as err:
        print 'Error saving setup info: %s ' % err


def set_report_dir (report_dir):
    if not os.path.exists(report_dir):
        os.mkdir(report_dir)


if __name__ == "__main__":
    
    # setting defaults. By default we run all the test suite
    specific_tests              = False
    disableLogCapture           = False
    long_test                   = False
    xml_name                    = 'unit_test.xml'
    CTRexScenario.report_dir    = 'reports'
    CTRexScenario.scripts_path  = get_trex_path()
    DAEMON_STOP_COMMAND         = 'cd %s; ./trex_daemon_server stop; sleep 1; ./trex_daemon_server stop;' % CTRexScenario.scripts_path
    DAEMON_START_COMMAND        = DAEMON_STOP_COMMAND + 'sleep 1; rm /var/log/trex/trex_daemon_server.log; ./trex_daemon_server start; sleep 2; ./trex_daemon_server show'
    setup_dir                   = os.getenv('SETUP_DIR', '').rstrip('/')
    CTRexScenario.setup_dir     = check_setup_path(setup_dir)
    if not CTRexScenario.setup_dir:
        CTRexScenario.setup_dir = check_setup_path(os.path.join('setups', setup_dir))
    
    if CTRexScenario.setup_dir:
        CTRexScenario.setup_name = os.path.basename(CTRexScenario.setup_dir)
        xml_name =  'report_%s.xml' % CTRexScenario.setup_name

    nose_argv = ['', '-s', '-v', '--exe', '--rednose', '--detailed-errors']
    if '--collect-only' in sys.argv: # this is a user trying simply to view the available tests. no need xunit.
        CTRexScenario.is_test_list = True
    else:
        nose_argv += ['--with-xunit', '--xunit-file=%s/%s' % (CTRexScenario.report_dir, xml_name)]
        set_report_dir(CTRexScenario.report_dir)

    for i, arg in enumerate(sys.argv):
        if 'unit_tests/' in arg:
            specific_tests = True
            sys.argv[i] = arg[arg.find('unit_tests/'):]
        if 'log-path' in arg:
            disableLogCapture = True

    nose_argv += sys.argv

    # Run all of the unit tests or just the selected ones
    if not specific_tests:
        if '--functional' in sys.argv:
            nose_argv += ['unit_tests/functional_tests']
        else:
            nose_argv += ['unit_tests']
    if disableLogCapture:
        nose_argv += ['--nologcapture']

    try:
        config_plugin = CTRexTestConfiguringPlugin()
        red_nose = RedNose()
        try:
            result = nose.run(argv = nose_argv, addplugins = [red_nose, config_plugin])
        except socket.error:    # handle consecutive tests exception, try once again
            print "TRex connectivity error identified. Possibly due to consecutive nightly runs.\nRetrying..."
            result = nose.run(argv = nose_argv, addplugins = [red_nose, config_plugin])
        finally:
            save_setup_info()

        if (result == True and not CTRexScenario.is_test_list):
            print termstyle.green("""
                     ..::''''::..
                   .;''        ``;.
                  ::    ::  ::    ::
                 ::     ::  ::     ::
                 ::     ::  ::     ::
                 :: .:' ::  :: `:. ::
                 ::  :          :  ::
                  :: `:.      .:' ::
                   `;..``::::''..;'
                     ``::,,,,::''

                   ___  ___   __________
                  / _ \/ _ | / __/ __/ /
                 / ___/ __ |_\ \_\ \/_/ 
                /_/  /_/ |_/___/___(_)  

            """)
            sys.exit(0)
        else:
            sys.exit(-1)
    
    finally:
        pass
        
        
    



                        

