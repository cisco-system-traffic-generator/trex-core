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
import datetime
import nose
from nose.plugins import Plugin
from nose.plugins.xunit import escape_cdata
from nose.selector import Selector
from nose.exc import SkipTest
from nose.pyversion import force_unicode, format_exception
import CustomLogger
import misc_methods
from rednose import RedNose
import termstyle
from trex import CTRexScenario
from trex_stf_lib.trex_client import *
from trex_stf_lib.trex_exceptions import *
from trex_stl_lib.api import *
from trex_stl_lib.utils.GAObjClass import GAmanager_Regression
import trex_elk
import trex
import socket
from pprint import pprint
import time
from distutils.dir_util import mkpath
import re
from io import StringIO
from argparse import ArgumentParser



TEST_ID = re.compile(r'^(.*?)(\(.*\))$')

def id_split(idval):
    m = TEST_ID.match(idval)
    if m:
        name, fargs = m.groups()
        head, tail = name.rsplit(".", 1)
        return [head, tail+fargs]
    else:
        return idval.rsplit(".", 1)

# nose overrides

# option to select wanted test by name without file, class etc.
def new_Selector_wantMethod(self, method, orig_Selector_wantMethod = Selector.wantMethod):
    result = orig_Selector_wantMethod(self, method)
    return result and (not CTRexScenario.test or list(filter(lambda t: t in getattr(method, '__name__', ''), CTRexScenario.test.split(','))))

Selector.wantMethod = new_Selector_wantMethod

def new_Selector_wantFunction(self, function, orig_Selector_wantFunction = Selector.wantFunction):
    result = orig_Selector_wantFunction(self, function)
    return result and (not CTRexScenario.test or list(filter(lambda t: t in getattr(function, '__name__', ''), CTRexScenario.test.split(','))))

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
nose.case.Test.shortDescription  = lambda *a, **k: None

# /nose overrides

def fatal(txt):
    print(txt)
    sys.exit(1)

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
        fatal('Could not determine trex_under_test folder, try setting env.var. TREX_UNDER_TEST')
    return latest_build_path


def address_to_ip(address):
    for i in range(5):
        try:
            return socket.gethostbyname(address)
        except:
            continue
    return socket.gethostbyname(address)


class TRexTee(object):
    def __init__(self, encoding, *args):
        self._encoding = encoding
        self._streams = args

    def write(self, data):
        data = force_unicode(data, self._encoding)
        for s in self._streams:
            s.write(data)

    def writelines(self, lines):
        for line in lines:
            self.write(line)

    def flush(self):
        for s in self._streams:
            s.flush()

    def isatty(self):
        return False


class CTRexTestConfiguringPlugin(Plugin):
    encoding = 'UTF-8'

    def __init__(self):
        super(CTRexTestConfiguringPlugin, self).__init__()
        self._capture_stack = []
        self._currentStdout = None
        self._currentStderr = None

    def _timeTaken(self):
        if hasattr(self, '_timer'):
            taken = time.time() - self._timer
        else:
            # test died before it ran (probably error in setup())
            # or success/failure added before test started probably 
            # due to custom TestResult munging
            taken = 0.0
        return taken

    def _startCapture(self):
        self._capture_stack.append((sys.stdout, sys.stderr))
        self._currentStdout = StringIO()
        self._currentStderr = StringIO()
        sys.stdout = TRexTee(self.encoding, self._currentStdout, sys.stdout)
        sys.stderr = TRexTee(self.encoding, self._currentStderr, sys.stderr)

    def startContext(self, context):
        self._startCapture()

    def stopContext(self, context):
        self._endCapture()

    def beforeTest(self, test):
        self._timer = time.time()
        self._startCapture()

    def _endCapture(self):
        if self._capture_stack:
            sys.stdout, sys.stderr = self._capture_stack.pop()

    def afterTest(self, test):
        self._endCapture()
        self._currentStdout = None
        self._currentStderr = None

    def _getCapturedStdout(self):
        if self._currentStdout:
            value = self._currentStdout.getvalue()
            if value:
                return '<system-out><![CDATA[%s]]></system-out>' % escape_cdata(
                        value)
        return ''

    def _getCapturedStderr(self):
        if self._currentStderr:
            value = self._currentStderr.getvalue()
            if value:
                return '<system-err><![CDATA[%s]]></system-err>' % escape_cdata(
                        value)
        return ''

    def addError(self, test, err, capt=None):
        elk = CTRexScenario.elk 
        if elk:
            taken = self._timeTaken()
            id = test.id()
            tb = format_exception(err, self.encoding)
            err_msg="TB : \n"+tb+"\n\n STDOUT:"+self._getCapturedStdout()+self._getCapturedStderr();
            name=id_split(id)[-1]
            elk_obj = trex.copy_elk_info ()
            elk_obj['test']={ 
                       "name"   : name,
                       "name_key"   : name,
                       "name_full"  : id,
                       "type"  : self.get_operation_mode (),
                       "duration_sec"  : taken,
                       "result" :  "ERROR",
                       "stdout" : err_msg,
            };
            #pprint(elk_obj['test']);
            elk.reg.push_data(elk_obj)



    def addFailure(self, test, err, capt=None, tb_info=None):
        elk = CTRexScenario.elk 
        if elk:
            taken = self._timeTaken()
            tb = format_exception(err, self.encoding)
            id = test.id()
            err_msg="TB : \n"+tb+"\n\n STDOUT:"+ self._getCapturedStdout()+self._getCapturedStderr();
            name=id_split(id)[-1]
            elk_obj = trex.copy_elk_info ()
            elk_obj['test']={ 
                       "name"   : name,
                       "name_key"   : name,
                       "name_full"  : id,
                        "type"  : self.get_operation_mode (),
                        "duration_sec"  : taken,
                        "result" :  "FAILURE",
                        "stdout" : err_msg,
            };
            #pprint(elk_obj['test']);
            elk.reg.push_data(elk_obj)



    def addSuccess(self, test, capt=None):
        elk = CTRexScenario.elk 
        if elk:
            taken = self._timeTaken()
            id = test.id()
            name=id_split(id)[-1]
            elk_obj = trex.copy_elk_info ()
            elk_obj['test']={ 
                       "name"   : name,
                       "name_key"   : name,
                       "name_full"  : id,
                       "type"  : self.get_operation_mode (),
                       "duration_sec"  : taken,
                       "result" :  "PASS",
                       "stdout" : "",
            };
            #pprint(elk_obj['test']);
            elk.reg.push_data(elk_obj)



    def get_operation_mode (self):
        if self.stateful:
            return('stateful');
        return('stateless');

      


##### option/configure

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
        parser.add_option('--pkg', type = str,
                            help="Run with given TRex package. Make sure the path available at server machine. Implies --restart-daemon.")
        parser.add_option('--restart-daemon', action="store_true", default = False,
                            help="Flag that specifies to restart daemon. Implied by --pkg.")
        parser.add_option('--collect', action="store_true", default = False,
                            help="Alias to --collect-only.")
        parser.add_option('--warmup', action="store_true", default = False,
                            help="Warm up the system for stateful: run 30 seconds 9k imix test without check of results.")
        parser.add_option('--test-client-package', '--test_client_package', action="store_true", default = False,
                            help="Includes tests of client package.")
        parser.add_option('--long', action="store_true", default = False,
                            help="Flag of long tests (stability).")
        parser.add_option('--ga', action="store_true", default = False,
                            help="Flag to send benchmarks to GA.")
        parser.add_option('--no-daemon', action="store_true", default = False,
                            dest="no_daemon",
                            help="Flag that specifies to use running stl server, no need daemons.")
        parser.add_option('--debug-image', action="store_true", default = False,
                            help="Flag that specifies to use t-rex-64-debug as TRex executable.")
        parser.add_option('--trex-args', default = '',
                            help="Additional TRex arguments (--no-watchdog etc.).")
        parser.add_option('-t', '--test', type = str,
                            help = 'Test name to run (without file, class etc.). Can choose several names splitted by comma.')
        parser.add_option('--no-dut-config', action = 'store_true',
                            help = 'Skip the config of DUT to save time. Implies --skip-clean.')


    def configure(self, options, conf):
        self.collect_only   = options.collect_only
        self.functional     = options.functional
        self.stateless      = options.stateless
        self.stateful       = options.stateful
        self.pkg            = options.pkg
        self.restart_daemon = options.restart_daemon
        self.json_verbose   = options.json_verbose
        self.telnet_verbose = options.telnet_verbose
        self.no_daemon      = options.no_daemon
        CTRexScenario.test  = options.test
        if self.no_daemon and (self.pkg or self.restart_daemon):
            fatal('You have specified both --no-daemon and either --pkg or --restart-daemon at same time.')
        if self.no_daemon and self.stateful :
            fatal("Can't run stateful without daemon.")
        if self.collect_only or self.functional:
            return
        self.enabled       = True
        self.kill_running  = options.kill_running
        self.load_image    = options.load_image
        self.clean_config  = False if options.skip_clean_config else True
        self.no_dut_config = options.no_dut_config
        if self.no_dut_config:
            self.clean_config = False
        self.server_logs   = options.server_logs
        if options.log_path:
            self.loggerPath = options.log_path
        # initialize CTRexScenario global testing class, to be used by all tests
        CTRexScenario.no_daemon     = options.no_daemon
        CTRexScenario.server_logs   = self.server_logs
        CTRexScenario.debug_image   = options.debug_image
        CTRexScenario.json_verbose  = self.json_verbose
        additional_args             = CTRexScenario.configuration.trex.get('trex_add_args', '')
        if not self.no_daemon:
            CTRexScenario.trex      = CTRexClient(trex_host   = CTRexScenario.configuration.trex['trex_name'],
                                                  verbose     = self.json_verbose,
                                                  debug_image = options.debug_image,
                                                  trex_args   = options.trex_args + ' ' + additional_args)

        if self.pkg or self.restart_daemon:
            if not CTRexScenario.trex.check_master_connectivity():
                fatal('Could not connect to master daemon')
        if options.ga and CTRexScenario.setup_name and not (CTRexScenario.GAManager and CTRexScenario.elk):
            CTRexScenario.GAManager  = GAmanager_Regression(
                    GoogleID         = CTRexScenario.global_cfg['google']['id'],
                    AnalyticsUserID  = CTRexScenario.setup_name,
                    QueueSize        = CTRexScenario.global_cfg['google']['queue_size'],
                    Timeout          = CTRexScenario.global_cfg['google']['timeout'],  # seconds
                    UserPermission   = 1,
                    BlockingMode     = CTRexScenario.global_cfg['google']['blocking'],
                    appName          = 'TRex',
                    appVer           = CTRexScenario.trex_version)

            CTRexScenario.elk = trex_elk.TRexEs(
                    CTRexScenario.global_cfg['elk']['server'],
                    CTRexScenario.global_cfg['elk']['port']);
            self.set_cont_elk_info ()

    def set_cont_elk_info (self):
        elk_info={}
        timestamp = datetime.datetime.now() - datetime.timedelta(hours=2); # need to update this
        info = {};


        img={}
        img['sha'] = "v2.14"                #TBD
        img['build_time'] = timestamp.strftime("%Y-%m-%d %H:%M:%S")
        img['version'] = "v2.14"           #TBD need to fix  
        img['formal'] = False

        setup = {}

        setup['distro'] = 'None'            #TBD 'Ubunto14.03'
        setup['kernel'] = 'None'           #TBD '2.6.12'
        setup['baremetal'] = True          #TBD
        setup['hypervisor'] = 'None'       #TBD
        setup['name'] = CTRexScenario.setup_name

        setup['cpu-sockets'] = 0           #TBD  2
        setup['cores'] = 0                 #TBD 16
        setup['cpu-speed'] = -1            #TBD 3.5

        setup['dut'] = 'None'             #TBD 'loopback'
        setup['drv-name'] = 'None'         #TBD 'mlx5'
        setup['nic-ports'] = 0             #TBD 2
        setup['total-nic-ports'] = 0       #TBD 2
        setup['nic-speed'] = "None"      #"40GbE" TBD



        info['image'] = img
        info['setup'] = setup

        elk_info['info'] =info;

        elk_info['timestamp'] = timestamp.strftime("%Y-%m-%d %H:%M:%S")  # need to update it
        elk_info['build_id'] = os.environ.get('BUILD_NUM')
        elk_info['scenario'] = os.environ.get('SCENARIO')

        CTRexScenario.elk_info = elk_info

    def _update_trex(self, timeout = 600):
        client = CTRexScenario.trex
        if client.master_daemon.is_trex_daemon_running() and client.get_trex_cmds() and not self.kill_running:
            fatal("Can't update TRex, it's running. Consider adding --kill-running flag.")

        ret, out, err = misc_methods.run_command('sha1sum -b %s' % self.pkg)
        if ret:
            fatal('Could not calculate sha1 of package. Got: %s' % [ret, out, err])
        sha1 = out.strip().split()[0]
        if client.master_daemon.get_package_sha1() == sha1:
            print('Server is up to date with package: %s' % self.pkg)
            CTRexScenario.pkg_updated = True
            return

        print('Updating TRex to: %s' % self.pkg)
        client.master_daemon.update_trex(self.pkg)
        sys.stdout.write('Waiting for update to finish')
        sys.stdout.flush()
        start_time = time.time()
        while True:
            if time.time() > start_time + timeout:
                fatal(' timeout of %ss while updating TRex.' % timeout)
            sys.stdout.write('.')
            sys.stdout.flush()
            time.sleep(1)
            if not client.master_daemon.is_updating():
                print(' finished.')
                break

        master_pkg_sha1 = client.master_daemon.get_package_sha1()
        if master_pkg_sha1 == sha1:
            print('Hash matches needed package, success.')
            CTRexScenario.pkg_updated = True
            return
        else:
            fatal('Hash does not match (%s), stuck with old package.' % master_pkg_sha1)

    def begin (self):
        client = CTRexScenario.trex
        if self.pkg and not CTRexScenario.pkg_updated:
            self._update_trex()
        if self.functional or self.collect_only:
            return
        if self.pkg or self.restart_daemon:
            print('Restarting TRex daemon server')
            res = client.restart_trex_daemon(tries = 5)
            if not res:
                fatal('Could not restart TRex daemon server')
            print('Restarted.')

        if not self.no_daemon:
            if self.kill_running:
                client.kill_all_trexes()
            elif client.get_trex_cmds():
                fatal('TRex is already running. Use --kill-running flag to kill it.')
            try:
                client.check_server_connectivity()
            except Exception as e:
                fatal(e)


        if 'loopback' not in CTRexScenario.modes:
            CTRexScenario.router_cfg = dict(config_dict      = CTRexScenario.configuration.router,
                                            forceImageReload = self.load_image,
                                            silent_mode      = not self.telnet_verbose,
                                            forceCleanConfig = self.clean_config,
                                            no_dut_config    = self.no_dut_config,
                                            tftp_config_dict = CTRexScenario.configuration.tftp)
        try:
            CustomLogger.setup_custom_logger('TRexLogger', self.loggerPath)
        except AttributeError:
            CustomLogger.setup_custom_logger('TRexLogger')

    def finalize(self, result):
        while self._capture_stack:
            self._endCapture()

        if self.functional or self.collect_only:
            return
        #CTRexScenario.is_init = False
        if CTRexScenario.trex:
            CTRexScenario.trex.master_daemon.save_coredump()
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


    nose_argv = ['', '-s', '-v', '--exe', '--rednose', '--nologcapture']
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

    cfg_plugin = CTRexTestConfiguringPlugin()
    parser = ArgumentParser(add_help = False)
    parser.add_option = parser.add_argument
    cfg_plugin.options(parser)
    options, _ = parser.parse_known_args(sys.argv)
    if options.stateless or options.stateful:
        if CTRexScenario.setup_dir and options.config_path:
            fatal('Please either define --cfg or use env. variable SETUP_DIR, not both.')
        if not options.config_path and CTRexScenario.setup_dir:
            options.config_path = CTRexScenario.setup_dir
        if not options.config_path:
            fatal('Please specify path to config.yaml using --cfg parameter or env. variable SETUP_DIR')
        options.config_path = options.config_path.rstrip('/')
        CTRexScenario.setup_name = os.path.basename(options.config_path)
        CTRexScenario.configuration = misc_methods.load_complete_config_file(os.path.join(options.config_path, 'config.yaml'))
        CTRexScenario.config_dict = misc_methods.load_object_config_file(os.path.join(options.config_path, 'config.yaml'))
        CTRexScenario.configuration.trex['trex_name'] = address_to_ip(CTRexScenario.configuration.trex['trex_name']) # translate hostname to ip
        CTRexScenario.benchmark     = misc_methods.load_benchmark_config_file(os.path.join(options.config_path, 'benchmark.yaml'))
        CTRexScenario.modes         = set(CTRexScenario.configuration.trex.get('modes', []))

    is_wlc = 'wlc' in CTRexScenario.modes
    addplugins = [RedNose(), cfg_plugin]
    result = True
    try:
        if CTRexScenario.test_types['functional_tests']:
            additional_args = ['--func'] + CTRexScenario.test_types['functional_tests']
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_functional.xml')]
            result = nose.run(argv = nose_argv + additional_args, addplugins = addplugins)
        if CTRexScenario.test_types['stateless_tests']:
            if is_wlc:
                additional_args = ['--stl', '-a', 'wlc'] + CTRexScenario.test_types['stateless_tests']
            else:
                additional_args = ['--stl', 'stateless_tests/stl_general_test.py:STLBasic_Test.test_connectivity'] + CTRexScenario.test_types['stateless_tests']
                if not test_client_package:
                    additional_args.extend(['-a', '!client_package'])
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_stateless.xml')]
            result = nose.run(argv = nose_argv + additional_args, addplugins = addplugins) and result
        if CTRexScenario.test_types['stateful_tests'] and not is_wlc:
            additional_args = ['--stf']
            if '--warmup' in sys.argv:
                additional_args.append('stateful_tests/trex_imix_test.py:CTRexIMIX_Test.test_warm_up')
            additional_args += CTRexScenario.test_types['stateful_tests']
            if not test_client_package:
                additional_args.extend(['-a', '!client_package'])
            if xml_arg:
                additional_args += ['--with-xunit', xml_arg.replace('.xml', '_stateful.xml')]
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











