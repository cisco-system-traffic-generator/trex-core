#!/usr/bin/env python

__copyright__ = "Copyright 2014"

"""
Name:
     trex_unit_test.py


Description:

    This script creates the functionality to test the performance of the TRex traffic generator
    The tested scenario is a TRex TG directly connected to a Cisco router.

::

    Topology:

       -------                         --------
      |       | Tx---1gig/10gig----Rx |        |
      | TRex  |                       | router |
      |       | Rx---1gig/10gig----Tx |        |
       -------                         --------

"""

import os
import sys
import outer_packages

import nose
from nose.plugins import Plugin
from nose.selector import Selector
import CustomLogger
import misc_methods
from rednose import RedNose
import termstyle
from trex import CTRexScenario
from trex_stf_lib.trex_client import *
from trex_stf_lib.trex_exceptions import *
from trex_stl_lib.api import *
from trex_stl_lib.utils.GAObjClass import GAmanager_Regression
import trex
import socket
from pprint import pprint
import time
from distutils.dir_util import mkpath

# nose overrides

# option to select wanted test by name without file, class etc.
def new_Selector_wantMethod(self, method, orig_Selector_wantMethod = Selector.wantMethod):
    result = orig_Selector_wantMethod(self, method)
    return result and (not CTRexScenario.test or CTRexScenario.test in getattr(method, '__name__', ''))

Selector.wantMethod = new_Selector_wantMethod

def new_Selector_wantFunction(self, function, orig_Selector_wantFunction = Selector.wantFunction):
    result = orig_Selector_wantFunction(self, function)
    return result and (not CTRexScenario.test or CTRexScenario.test in getattr(function, '__name__', ''))

Selector.wantFunction = new_Selector_wantFunction

# override nose's strange representation of setUpClass errors
def __suite_repr__(self):
    if hasattr(self.context, '__module__'): # inside class, setUpClass etc.
        class_repr = nose.suite._strclass(self.context)
    else:                                   # outside of class, setUpModule etc.
        class_repr = nose.suite._strclass(self.__class__)
    return '%s.%s' % (class_repr, getattr(self.context, '__name__', self.context))

nose.suite.ContextSuite.__repr__ = __suite_repr__
nose.suite.ContextSuite.__str__  = __suite_repr__

# /nose overrides

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


def address_to_ip(address):
    for i in range(5):
        try:
            return socket.gethostbyname(address)
        except:
            continue
    return socket.gethostbyname(address)


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
        parser.add_option('--json-verbose', '--json_verbose', action="store_true", default = False,
                            dest="json_verbose",
                            help="Print JSON-RPC commands.")
        parser.add_option('--telnet-verbose', '--telnet_verbose', action="store_true", default = False,
                            dest="telnet_verbose",
                            help="Print telnet commands and responces.")
        parser.add_option('--server-logs', '--server_logs', action="store_true", default = False,
                            dest="server_logs",
                            help="Print server side (TRex and trex_daemon) logs per test.")
        parser.add_option('--kill-running', '--kill_running', action="store_true", default = False,
                            dest="kill_running",
                            help="Kills running TRex process on remote server (useful for regression).")
        parser.add_option('--func', '--functional', action="store_true", default = False,
                            dest="functional",
                            help="Run functional tests.")
        parser.add_option('--stl', '--stateless', action="store_true", default = False,
                            dest="stateless",
                            help="Run stateless tests.")
        parser.add_option('--stf', '--stateful', action="store_true", default = False,
                            dest="stateful",
                            help="Run stateful tests.")
        parser.add_option('--pkg', action="store",
                            dest="pkg",
                            help="Run with given TRex package. Make sure the path available at server machine.")
        parser.add_option('--collect', action="store_true", default = False,
                            dest="collect",
                            help="Alias to --collect-only.")
        parser.add_option('--warmup', action="store_true", default = False,
                            dest="warmup",
                            help="Warm up the system for stateful: run 30 seconds 9k imix test without check of results.")
        parser.add_option('--test-client-package', '--test_client_package', action="store_true", default = False,
                            dest="test_client_package",
                            help="Includes tests of client package.")
        parser.add_option('--long', action="store_true", default = False,
                            dest="long",
                            help="Flag of long tests (stability).")
        parser.add_option('--ga', action="store_true", default = False,
                            dest="ga",
                            help="Flag to send benchmarks to GA.")
        parser.add_option('--no-daemon', action="store_true", default = False,
                            dest="no_daemon",
                            help="Flag that specifies to use running stl server, no need daemons.")
        parser.add_option('--debug-image', action="store_true", default = False,
                            dest="debug_image",
                            help="Flag that specifies to use t-rex-64-debug as TRex executable.")
        parser.add_option('--trex-args', action='store', default = '',
                            dest="trex_args",
                            help="Additional TRex arguments (--no-watchdog etc.).")
        parser.add_option('-t', '--test', action='store', default = '', dest='test',
                            help='Test name to run (without file, class etc.)')


    def configure(self, options, conf):
        self.collect_only   = options.collect_only
        self.functional     = options.functional
        self.stateless      = options.stateless
        self.stateful       = options.stateful
        self.pkg            = options.pkg
        self.json_verbose   = options.json_verbose
        self.telnet_verbose = options.telnet_verbose
        self.no_daemon      = options.no_daemon
        CTRexScenario.test  = options.test
        if self.collect_only or self.functional:
            return
        if CTRexScenario.setup_dir and options.config_path:
            raise Exception('Please either define --cfg or use env. variable SETUP_DIR, not both.')
        if not options.config_path and CTRexScenario.setup_dir:
            options.config_path = CTRexScenario.setup_dir
        if not options.config_path:
            raise Exception('Please specify path to config.yaml using --cfg parameter or env. variable SETUP_DIR')
        options.config_path = options.config_path.rstrip('/')
        CTRexScenario.setup_name = os.path.basename(options.config_path)
        self.configuration = misc_methods.load_complete_config_file(os.path.join(options.config_path, 'config.yaml'))
        self.configuration.trex['trex_name'] = address_to_ip(self.configuration.trex['trex_name']) # translate hostname to ip
        self.benchmark     = misc_methods.load_benchmark_config_file(os.path.join(options.config_path, 'benchmark.yaml'))
        self.enabled       = True
        self.modes         = self.configuration.trex.get('modes', [])
        self.kill_running  = options.kill_running
        self.load_image    = options.load_image
        self.clean_config  = False if options.skip_clean_config else True
        self.server_logs   = options.server_logs
        if options.log_path:
            self.loggerPath = options.log_path
        # initialize CTRexScenario global testing class, to be used by all tests
        CTRexScenario.configuration = self.configuration
        CTRexScenario.no_daemon     = options.no_daemon
        CTRexScenario.benchmark     = self.benchmark
        CTRexScenario.modes         = set(self.modes)
        CTRexScenario.server_logs   = self.server_logs
        CTRexScenario.debug_image   = options.debug_image
        CTRexScenario.json_verbose  = self.json_verbose
        if not self.no_daemon:
            CTRexScenario.trex      = CTRexClient(trex_host   = self.configuration.trex['trex_name'],
                                                  verbose     = self.json_verbose,
                                                  debug_image = options.debug_image,
                                                  trex_args   = options.trex_args)
            if not CTRexScenario.trex.check_master_connectivity():
                print('Could not connect to master daemon')
                sys.exit(-1)
        if options.ga and CTRexScenario.setup_name:
            CTRexScenario.GAManager  = GAmanager_Regression(GoogleID         = 'UA-75220362-3',
                                                            AnalyticsUserID  = CTRexScenario.setup_name,
                                                            QueueSize        = 100,
                                                            Timeout          = 3,  # seconds
                                                            UserPermission   = 1,
                                                            BlockingMode     = 0,
                                                            appName          = 'TRex',
                                                            appVer           = CTRexScenario.trex_version)


    def begin (self):
        client = CTRexScenario.trex
        if self.pkg and not CTRexScenario.is_copied:
            if client.master_daemon.is_trex_daemon_running() and client.get_trex_cmds() and not self.kill_running:
                print("Can't update TRex, it's running")
                sys.exit(-1)
            print('Updating TRex to %s' % self.pkg)
            if not client.master_daemon.update_trex(self.pkg):
                print('Failed updating TRex')
                sys.exit(-1)
            else:
                print('Updated')
            CTRexScenario.is_copied = True
        if self.functional or self.collect_only:
            return
        if not self.no_daemon:
            print('Restarting TRex daemon server')
            res = client.restart_trex_daemon()
            if not res:
                print('Could not restart TRex daemon server')
                sys.exit(-1)
            print('Restarted.')

            if self.kill_running:
                client.kill_all_trexes()
            else:
                if client.get_trex_cmds():
                    print('TRex is already running')
                    sys.exit(-1)

        if 'loopback' not in self.modes:
            CTRexScenario.router_cfg = dict(config_dict      = self.configuration.router,
                                            forceImageReload = self.load_image,
                                            silent_mode      = not self.telnet_verbose,
                                            forceCleanConfig = self.clean_config,
                                            tftp_config_dict = self.configuration.tftp)
        try:
            CustomLogger.setup_custom_logger('TRexLogger', self.loggerPath)
        except AttributeError:
            CustomLogger.setup_custom_logger('TRexLogger')

    def finalize(self, result):
        if self.functional or self.collect_only:
            return
        CTRexScenario.is_init = False
        if self.stateful:
            CTRexScenario.trex = None
        if self.stateless:
            if self.no_daemon:
                if CTRexScenario.stl_trex and CTRexScenario.stl_trex.is_connected():
                    CTRexScenario.stl_trex.disconnect()
            else:
                CTRexScenario.trex.force_kill(False)
            CTRexScenario.stl_trex = None


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
            if CTRexScenario.debug_image:
                setup_info += '\nDebug image: %s' % CTRexScenario.debug_image
                
            with open('%s/report_%s.info' % (CTRexScenario.report_dir, CTRexScenario.setup_name), 'w') as f:
                f.write(setup_info)
    except Exception as err:
        print('Error saving setup info: %s ' % err)


if __name__ == "__main__":

    # setting defaults. By default we run all the test suite
    specific_tests              = False
    CTRexScenario.report_dir    = 'reports'
    need_to_copy                = False
    setup_dir                   = os.getenv('SETUP_DIR', '').rstrip('/')
    CTRexScenario.setup_dir     = check_setup_path(setup_dir)
    CTRexScenario.scripts_path  = get_trex_path()
    if not CTRexScenario.setup_dir:
        CTRexScenario.setup_dir = check_setup_path(os.path.join('setups', setup_dir))


    nose_argv = ['', '-s', '-v', '--exe', '--rednose', '--detailed-errors']
    test_client_package = False
    if '--test-client-package' in sys.argv:
        test_client_package = True

    if '--collect' in sys.argv:
        sys.argv.append('--collect-only')
    if '--collect-only' in sys.argv: # this is a user trying simply to view the available tests. no need xunit.
        CTRexScenario.is_test_list   = True
        xml_arg                      = ''
    else:
        xml_name                     = 'unit_test.xml'
        if CTRexScenario.setup_dir:
            CTRexScenario.setup_name = os.path.basename(CTRexScenario.setup_dir)
            xml_name = 'report_%s.xml' % CTRexScenario.setup_name
        xml_arg= '--xunit-file=%s/%s' % (CTRexScenario.report_dir, xml_name)
        mkpath(CTRexScenario.report_dir)

    sys_args = sys.argv[:]
    for i, arg in enumerate(sys.argv):
        if 'log-path' in arg:
            nose_argv += ['--nologcapture']
        else:
            for tests_type in CTRexScenario.test_types.keys():
                if tests_type in arg:
                    specific_tests = True
                    CTRexScenario.test_types[tests_type].append(arg[arg.find(tests_type):])
                    sys_args.remove(arg)

    if not specific_tests:
        for key in ('--func', '--functional'):
            if key in sys_args:
                CTRexScenario.test_types['functional_tests'].append('functional_tests')
                sys_args.remove(key)
        for key in ('--stf', '--stateful'):
            if key in sys_args:
                CTRexScenario.test_types['stateful_tests'].append('stateful_tests')
                sys_args.remove(key)
        for key in ('--stl', '--stateless'):
            if key in sys_args:
                CTRexScenario.test_types['stateless_tests'].append('stateless_tests')
                sys_args.remove(key)
        # Run all of the tests or just the selected ones
        if not sum([len(x) for x in CTRexScenario.test_types.values()]):
            for key in CTRexScenario.test_types.keys():
                CTRexScenario.test_types[key].append(key)

    nose_argv += sys_args

    addplugins = [RedNose(), CTRexTestConfiguringPlugin()]
    result = True
    try:
        if len(CTRexScenario.test_types['functional_tests']):
            additional_args = ['--func'] + CTRexScenario.test_types['functional_tests']
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_functional.xml')]
            result = nose.run(argv = nose_argv + additional_args, addplugins = addplugins)
        if len(CTRexScenario.test_types['stateful_tests']):
            additional_args = ['--stf']
            if '--warmup' in sys.argv:
                additional_args.append('stateful_tests/trex_imix_test.py:CTRexIMIX_Test.test_warm_up')
            additional_args += CTRexScenario.test_types['stateful_tests']
            if not test_client_package:
                additional_args.extend(['-a', '!client_package'])
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_stateful.xml')]
            result = nose.run(argv = nose_argv + additional_args, addplugins = addplugins) and result
        if len(CTRexScenario.test_types['stateless_tests']):
            additional_args = ['--stl', 'stateless_tests/stl_general_test.py:STLBasic_Test.test_connectivity'] + CTRexScenario.test_types['stateless_tests']
            if not test_client_package:
                additional_args.extend(['-a', '!client_package'])
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_stateless.xml')]
            result = nose.run(argv = nose_argv + additional_args, addplugins = addplugins) and result
    #except Exception as e:
    #    result = False
    #    print(e)
    finally:
        save_setup_info()

    if not CTRexScenario.is_test_list:
        if result == True:
            print(termstyle.green("""
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

        """))
            sys.exit(0)
        else:
            print(termstyle.red("""
           /\_/\ 
          ( o.o ) 
           > ^ < 

This cat is sad, test failed.
        """))
            sys.exit(-1)











