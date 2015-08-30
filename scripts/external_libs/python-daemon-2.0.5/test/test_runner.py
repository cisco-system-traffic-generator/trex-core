# -*- coding: utf-8 -*-
#
# test/test_runner.py
# Part of ‘python-daemon’, an implementation of PEP 3143.
#
# Copyright © 2009–2015 Ben Finney <ben+python@benfinney.id.au>
#
# This is free software: you may copy, modify, and/or distribute this work
# under the terms of the Apache License, version 2.0 as published by the
# Apache Software Foundation.
# No warranty expressed or implied. See the file ‘LICENSE.ASF-2’ for details.

""" Unit test for ‘runner’ module.
    """

from __future__ import (absolute_import, unicode_literals)

try:
    # Python 3 standard library.
    import builtins
except ImportError:
    # Python 2 standard library.
    import __builtin__ as builtins
import os
import os.path
import sys
import tempfile
import errno
import signal
import functools

import lockfile
import mock
import testtools

from . import scaffold
from .scaffold import (basestring, unicode)
from .test_pidfile import (
        FakeFileDescriptorStringIO,
        setup_pidfile_fixtures,
        make_pidlockfile_scenarios,
        apply_lockfile_method_mocks,
        )
from .test_daemon import (
        setup_streams_fixtures,
        )

import daemon.daemon
import daemon.runner
import daemon.pidfile


class ModuleExceptions_TestCase(scaffold.Exception_TestCase):
    """ Test cases for module exception classes. """

    scenarios = scaffold.make_exception_scenarios([
            ('daemon.runner.DaemonRunnerError', dict(
                exc_type = daemon.runner.DaemonRunnerError,
                min_args = 1,
                types = [Exception],
                )),
            ('daemon.runner.DaemonRunnerInvalidActionError', dict(
                exc_type = daemon.runner.DaemonRunnerInvalidActionError,
                min_args = 1,
                types = [daemon.runner.DaemonRunnerError, ValueError],
                )),
            ('daemon.runner.DaemonRunnerStartFailureError', dict(
                exc_type = daemon.runner.DaemonRunnerStartFailureError,
                min_args = 1,
                types = [daemon.runner.DaemonRunnerError, RuntimeError],
                )),
            ('daemon.runner.DaemonRunnerStopFailureError', dict(
                exc_type = daemon.runner.DaemonRunnerStopFailureError,
                min_args = 1,
                types = [daemon.runner.DaemonRunnerError, RuntimeError],
                )),
            ])


def make_runner_scenarios():
    """ Make a collection of scenarios for testing `DaemonRunner` instances.

        :return: A collection of scenarios for tests involving
            `DaemonRunner` instances.

        The collection is a mapping from scenario name to a dictionary of
        scenario attributes.

        """

    pidlockfile_scenarios = make_pidlockfile_scenarios()

    scenarios = {
            'simple': {
                'pidlockfile_scenario_name': 'simple',
                },
            'pidfile-locked': {
                'pidlockfile_scenario_name': 'exist-other-pid-locked',
                },
            }

    for scenario in scenarios.values():
        if 'pidlockfile_scenario_name' in scenario:
            pidlockfile_scenario = pidlockfile_scenarios.pop(
                    scenario['pidlockfile_scenario_name'])
        scenario['pid'] = pidlockfile_scenario['pid']
        scenario['pidfile_path'] = pidlockfile_scenario['pidfile_path']
        scenario['pidfile_timeout'] = 23
        scenario['pidlockfile_scenario'] = pidlockfile_scenario

    return scenarios


def set_runner_scenario(testcase, scenario_name):
    """ Set the DaemonRunner test scenario for the test case.

        :param testcase: The `TestCase` instance to decorate.
        :param scenario_name: The name of the scenario to use.

        Set the `DaemonRunner` test scenario name and decorate the
        `testcase` with the corresponding scenario fixtures.

        """
    scenarios = testcase.runner_scenarios
    testcase.scenario = scenarios[scenario_name]
    apply_lockfile_method_mocks(
            testcase.mock_runner_lockfile,
            testcase,
            testcase.scenario['pidlockfile_scenario'])


def setup_runner_fixtures(testcase):
    """ Set up common fixtures for `DaemonRunner` test cases.

        :param testcase: A `TestCase` instance to decorate.

        Decorate the `testcase` with attributes to be fixtures for tests
        involving `DaemonRunner` instances.

        """
    setup_pidfile_fixtures(testcase)
    setup_streams_fixtures(testcase)

    testcase.runner_scenarios = make_runner_scenarios()

    patcher_stderr = mock.patch.object(
            sys, "stderr",
            new=FakeFileDescriptorStringIO())
    testcase.fake_stderr = patcher_stderr.start()
    testcase.addCleanup(patcher_stderr.stop)

    simple_scenario = testcase.runner_scenarios['simple']

    testcase.mock_runner_lockfile = mock.MagicMock(
            spec=daemon.pidfile.TimeoutPIDLockFile)
    apply_lockfile_method_mocks(
            testcase.mock_runner_lockfile,
            testcase,
            simple_scenario['pidlockfile_scenario'])
    testcase.mock_runner_lockfile.path = simple_scenario['pidfile_path']

    patcher_lockfile_class = mock.patch.object(
            daemon.pidfile, "TimeoutPIDLockFile",
            return_value=testcase.mock_runner_lockfile)
    patcher_lockfile_class.start()
    testcase.addCleanup(patcher_lockfile_class.stop)

    class TestApp(object):

        def __init__(self):
            self.stdin_path = testcase.stream_file_paths['stdin']
            self.stdout_path = testcase.stream_file_paths['stdout']
            self.stderr_path = testcase.stream_file_paths['stderr']
            self.pidfile_path = simple_scenario['pidfile_path']
            self.pidfile_timeout = simple_scenario['pidfile_timeout']

        run = mock.MagicMock(name="TestApp.run")

    testcase.TestApp = TestApp

    patcher_runner_daemoncontext = mock.patch.object(
            daemon.runner, "DaemonContext", autospec=True)
    patcher_runner_daemoncontext.start()
    testcase.addCleanup(patcher_runner_daemoncontext.stop)

    testcase.test_app = testcase.TestApp()

    testcase.test_program_name = "bazprog"
    testcase.test_program_path = os.path.join(
            "/foo/bar", testcase.test_program_name)
    testcase.valid_argv_params = {
            'start': [testcase.test_program_path, 'start'],
            'stop': [testcase.test_program_path, 'stop'],
            'restart': [testcase.test_program_path, 'restart'],
            }

    def fake_open(filename, mode=None, buffering=None):
        if filename in testcase.stream_files_by_path:
            result = testcase.stream_files_by_path[filename]
        else:
            result = FakeFileDescriptorStringIO()
        result.mode = mode
        result.buffering = buffering
        return result

    mock_open = mock.mock_open()
    mock_open.side_effect = fake_open

    func_patcher_builtin_open = mock.patch.object(
            builtins, "open",
            new=mock_open)
    func_patcher_builtin_open.start()
    testcase.addCleanup(func_patcher_builtin_open.stop)

    func_patcher_os_kill = mock.patch.object(os, "kill")
    func_patcher_os_kill.start()
    testcase.addCleanup(func_patcher_os_kill.stop)

    patcher_sys_argv = mock.patch.object(
            sys, "argv",
            new=testcase.valid_argv_params['start'])
    patcher_sys_argv.start()
    testcase.addCleanup(patcher_sys_argv.stop)

    testcase.test_instance = daemon.runner.DaemonRunner(testcase.test_app)

    testcase.scenario = NotImplemented


class DaemonRunner_BaseTestCase(scaffold.TestCase):
    """ Base class for DaemonRunner test case classes. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_BaseTestCase, self).setUp()

        setup_runner_fixtures(self)
        set_runner_scenario(self, 'simple')


class DaemonRunner_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner class. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_TestCase, self).setUp()

        func_patcher_parse_args = mock.patch.object(
                daemon.runner.DaemonRunner, "parse_args")
        func_patcher_parse_args.start()
        self.addCleanup(func_patcher_parse_args.stop)

        # Create a new instance now with our custom patches.
        self.test_instance = daemon.runner.DaemonRunner(self.test_app)

    def test_instantiate(self):
        """ New instance of DaemonRunner should be created. """
        self.assertIsInstance(self.test_instance, daemon.runner.DaemonRunner)

    def test_parses_commandline_args(self):
        """ Should parse commandline arguments. """
        self.test_instance.parse_args.assert_called_with()

    def test_has_specified_app(self):
        """ Should have specified application object. """
        self.assertIs(self.test_app, self.test_instance.app)

    def test_sets_pidfile_none_when_pidfile_path_is_none(self):
        """ Should set ‘pidfile’ to ‘None’ when ‘pidfile_path’ is ‘None’. """
        pidfile_path = None
        self.test_app.pidfile_path = pidfile_path
        expected_pidfile = None
        instance = daemon.runner.DaemonRunner(self.test_app)
        self.assertIs(expected_pidfile, instance.pidfile)

    def test_error_when_pidfile_path_not_string(self):
        """ Should raise ValueError when PID file path not a string. """
        pidfile_path = object()
        self.test_app.pidfile_path = pidfile_path
        expected_error = ValueError
        self.assertRaises(
                expected_error,
                daemon.runner.DaemonRunner, self.test_app)

    def test_error_when_pidfile_path_not_absolute(self):
        """ Should raise ValueError when PID file path not absolute. """
        pidfile_path = "foo/bar.pid"
        self.test_app.pidfile_path = pidfile_path
        expected_error = ValueError
        self.assertRaises(
                expected_error,
                daemon.runner.DaemonRunner, self.test_app)

    def test_creates_lock_with_specified_parameters(self):
        """ Should create a TimeoutPIDLockFile with specified params. """
        pidfile_path = self.scenario['pidfile_path']
        pidfile_timeout = self.scenario['pidfile_timeout']
        daemon.pidfile.TimeoutPIDLockFile.assert_called_with(
                pidfile_path, pidfile_timeout)

    def test_has_created_pidfile(self):
        """ Should have new PID lock file as `pidfile` attribute. """
        expected_pidfile = self.mock_runner_lockfile
        instance = self.test_instance
        self.assertIs(
                expected_pidfile, instance.pidfile)

    def test_daemon_context_has_created_pidfile(self):
        """ DaemonContext component should have new PID lock file. """
        expected_pidfile = self.mock_runner_lockfile
        daemon_context = self.test_instance.daemon_context
        self.assertIs(
                expected_pidfile, daemon_context.pidfile)

    def test_daemon_context_has_specified_stdin_stream(self):
        """ DaemonContext component should have specified stdin file. """
        test_app = self.test_app
        expected_file = self.stream_files_by_name['stdin']
        daemon_context = self.test_instance.daemon_context
        self.assertEqual(expected_file, daemon_context.stdin)

    def test_daemon_context_has_stdin_in_read_mode(self):
        """ DaemonContext component should open stdin file for read. """
        expected_mode = 'rt'
        daemon_context = self.test_instance.daemon_context
        self.assertIn(expected_mode, daemon_context.stdin.mode)

    def test_daemon_context_has_specified_stdout_stream(self):
        """ DaemonContext component should have specified stdout file. """
        test_app = self.test_app
        expected_file = self.stream_files_by_name['stdout']
        daemon_context = self.test_instance.daemon_context
        self.assertEqual(expected_file, daemon_context.stdout)

    def test_daemon_context_has_stdout_in_append_mode(self):
        """ DaemonContext component should open stdout file for append. """
        expected_mode = 'w+t'
        daemon_context = self.test_instance.daemon_context
        self.assertIn(expected_mode, daemon_context.stdout.mode)

    def test_daemon_context_has_specified_stderr_stream(self):
        """ DaemonContext component should have specified stderr file. """
        test_app = self.test_app
        expected_file = self.stream_files_by_name['stderr']
        daemon_context = self.test_instance.daemon_context
        self.assertEqual(expected_file, daemon_context.stderr)

    def test_daemon_context_has_stderr_in_append_mode(self):
        """ DaemonContext component should open stderr file for append. """
        expected_mode = 'w+t'
        daemon_context = self.test_instance.daemon_context
        self.assertIn(expected_mode, daemon_context.stderr.mode)

    def test_daemon_context_has_stderr_with_no_buffering(self):
        """ DaemonContext component should open stderr file unbuffered. """
        expected_buffering = 0
        daemon_context = self.test_instance.daemon_context
        self.assertEqual(
                expected_buffering, daemon_context.stderr.buffering)


class DaemonRunner_usage_exit_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.usage_exit method. """

    def test_raises_system_exit(self):
        """ Should raise SystemExit exception. """
        instance = self.test_instance
        argv = [self.test_program_path]
        self.assertRaises(
                SystemExit,
                instance._usage_exit, argv)

    def test_message_follows_conventional_format(self):
        """ Should emit a conventional usage message. """
        instance = self.test_instance
        argv = [self.test_program_path]
        expected_stderr_output = """\
                usage: {progname} ...
                """.format(
                    progname=self.test_program_name)
        self.assertRaises(
                SystemExit,
                instance._usage_exit, argv)
        self.assertOutputCheckerMatch(
                expected_stderr_output, self.fake_stderr.getvalue())


class DaemonRunner_parse_args_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.parse_args method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_parse_args_TestCase, self).setUp()

        func_patcher_usage_exit = mock.patch.object(
                daemon.runner.DaemonRunner, "_usage_exit",
                side_effect=NotImplementedError)
        func_patcher_usage_exit.start()
        self.addCleanup(func_patcher_usage_exit.stop)

    def test_emits_usage_message_if_insufficient_args(self):
        """ Should emit a usage message and exit if too few arguments. """
        instance = self.test_instance
        argv = [self.test_program_path]
        exc = self.assertRaises(
                NotImplementedError,
                instance.parse_args, argv)
        daemon.runner.DaemonRunner._usage_exit.assert_called_with(argv)

    def test_emits_usage_message_if_unknown_action_arg(self):
        """ Should emit a usage message and exit if unknown action. """
        instance = self.test_instance
        progname = self.test_program_name
        argv = [self.test_program_path, 'bogus']
        exc = self.assertRaises(
                NotImplementedError,
                instance.parse_args, argv)
        daemon.runner.DaemonRunner._usage_exit.assert_called_with(argv)

    def test_should_parse_system_argv_by_default(self):
        """ Should parse sys.argv by default. """
        instance = self.test_instance
        expected_action = 'start'
        argv = self.valid_argv_params['start']
        with mock.patch.object(sys, "argv", new=argv):
            instance.parse_args()
        self.assertEqual(expected_action, instance.action)

    def test_sets_action_from_first_argument(self):
        """ Should set action from first commandline argument. """
        instance = self.test_instance
        for name, argv in self.valid_argv_params.items():
            expected_action = name
            instance.parse_args(argv)
            self.assertEqual(expected_action, instance.action)


try:
    ProcessLookupError
except NameError:
    # Python 2 uses OSError.
    ProcessLookupError = functools.partial(OSError, errno.ESRCH)

class DaemonRunner_do_action_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.do_action method. """

    def test_raises_error_if_unknown_action(self):
        """ Should emit a usage message and exit if action is unknown. """
        instance = self.test_instance
        instance.action = 'bogus'
        expected_error = daemon.runner.DaemonRunnerInvalidActionError
        self.assertRaises(
                expected_error,
                instance.do_action)


class DaemonRunner_do_action_start_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.do_action method, action 'start'. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_do_action_start_TestCase, self).setUp()

        self.test_instance.action = 'start'

    def test_raises_error_if_pidfile_locked(self):
        """ Should raise error if PID file is locked. """

        instance = self.test_instance
        instance.daemon_context.open.side_effect = lockfile.AlreadyLocked
        pidfile_path = self.scenario['pidfile_path']
        expected_error = daemon.runner.DaemonRunnerStartFailureError
        expected_message_content = pidfile_path
        exc = self.assertRaises(
                expected_error,
                instance.do_action)
        self.assertIn(expected_message_content, unicode(exc))

    def test_breaks_lock_if_no_such_process(self):
        """ Should request breaking lock if PID file process is not running. """
        set_runner_scenario(self, 'pidfile-locked')
        instance = self.test_instance
        self.mock_runner_lockfile.read_pid.return_value = (
                self.scenario['pidlockfile_scenario']['pidfile_pid'])
        pidfile_path = self.scenario['pidfile_path']
        test_pid = self.scenario['pidlockfile_scenario']['pidfile_pid']
        expected_signal = signal.SIG_DFL
        test_error = ProcessLookupError("Not running")
        os.kill.side_effect = test_error
        instance.do_action()
        os.kill.assert_called_with(test_pid, expected_signal)
        self.mock_runner_lockfile.break_lock.assert_called_with()

    def test_requests_daemon_context_open(self):
        """ Should request the daemon context to open. """
        instance = self.test_instance
        instance.do_action()
        instance.daemon_context.open.assert_called_with()

    def test_emits_start_message_to_stderr(self):
        """ Should emit start message to stderr. """
        instance = self.test_instance
        expected_stderr = """\
                started with pid {pid:d}
                """.format(
                    pid=self.scenario['pid'])
        instance.do_action()
        self.assertOutputCheckerMatch(
                expected_stderr, self.fake_stderr.getvalue())

    def test_requests_app_run(self):
        """ Should request the application to run. """
        instance = self.test_instance
        instance.do_action()
        self.test_app.run.assert_called_with()


class DaemonRunner_do_action_stop_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.do_action method, action 'stop'. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_do_action_stop_TestCase, self).setUp()

        set_runner_scenario(self, 'pidfile-locked')

        self.test_instance.action = 'stop'

        self.mock_runner_lockfile.is_locked.return_value = True
        self.mock_runner_lockfile.i_am_locking.return_value = False
        self.mock_runner_lockfile.read_pid.return_value = (
                self.scenario['pidlockfile_scenario']['pidfile_pid'])

    def test_raises_error_if_pidfile_not_locked(self):
        """ Should raise error if PID file is not locked. """
        set_runner_scenario(self, 'simple')
        instance = self.test_instance
        self.mock_runner_lockfile.is_locked.return_value = False
        self.mock_runner_lockfile.i_am_locking.return_value = False
        self.mock_runner_lockfile.read_pid.return_value = (
                self.scenario['pidlockfile_scenario']['pidfile_pid'])
        pidfile_path = self.scenario['pidfile_path']
        expected_error = daemon.runner.DaemonRunnerStopFailureError
        expected_message_content = pidfile_path
        exc = self.assertRaises(
                expected_error,
                instance.do_action)
        self.assertIn(expected_message_content, unicode(exc))

    def test_breaks_lock_if_pidfile_stale(self):
        """ Should break lock if PID file is stale. """
        instance = self.test_instance
        pidfile_path = self.scenario['pidfile_path']
        test_pid = self.scenario['pidlockfile_scenario']['pidfile_pid']
        expected_signal = signal.SIG_DFL
        test_error = OSError(errno.ESRCH, "Not running")
        os.kill.side_effect = test_error
        instance.do_action()
        self.mock_runner_lockfile.break_lock.assert_called_with()

    def test_sends_terminate_signal_to_process_from_pidfile(self):
        """ Should send SIGTERM to the daemon process. """
        instance = self.test_instance
        test_pid = self.scenario['pidlockfile_scenario']['pidfile_pid']
        expected_signal = signal.SIGTERM
        instance.do_action()
        os.kill.assert_called_with(test_pid, expected_signal)

    def test_raises_error_if_cannot_send_signal_to_process(self):
        """ Should raise error if cannot send signal to daemon process. """
        instance = self.test_instance
        test_pid = self.scenario['pidlockfile_scenario']['pidfile_pid']
        pidfile_path = self.scenario['pidfile_path']
        test_error = OSError(errno.EPERM, "Nice try")
        os.kill.side_effect = test_error
        expected_error = daemon.runner.DaemonRunnerStopFailureError
        expected_message_content = unicode(test_pid)
        exc = self.assertRaises(
                expected_error,
                instance.do_action)
        self.assertIn(expected_message_content, unicode(exc))


@mock.patch.object(daemon.runner.DaemonRunner, "_start")
@mock.patch.object(daemon.runner.DaemonRunner, "_stop")
class DaemonRunner_do_action_restart_TestCase(DaemonRunner_BaseTestCase):
    """ Test cases for DaemonRunner.do_action method, action 'restart'. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonRunner_do_action_restart_TestCase, self).setUp()

        set_runner_scenario(self, 'pidfile-locked')

        self.test_instance.action = 'restart'

    def test_requests_stop_then_start(
            self,
            mock_func_daemonrunner_start, mock_func_daemonrunner_stop):
        """ Should request stop, then start. """
        instance = self.test_instance
        instance.do_action()
        mock_func_daemonrunner_start.assert_called_with()
        mock_func_daemonrunner_stop.assert_called_with()


@mock.patch.object(sys, "stderr")
class emit_message_TestCase(scaffold.TestCase):
    """ Test cases for ‘emit_message’ function. """

    def test_writes_specified_message_to_stream(self, mock_stderr):
        """ Should write specified message to stream. """
        test_message = self.getUniqueString()
        expected_content = "{message}\n".format(message=test_message)
        daemon.runner.emit_message(test_message, stream=mock_stderr)
        mock_stderr.write.assert_called_with(expected_content)

    def test_writes_to_specified_stream(self, mock_stderr):
        """ Should write message to specified stream. """
        test_message = self.getUniqueString()
        mock_stream = mock.MagicMock()
        daemon.runner.emit_message(test_message, stream=mock_stream)
        mock_stream.write.assert_called_with(mock.ANY)

    def test_writes_to_stderr_by_default(self, mock_stderr):
        """ Should write message to ‘sys.stderr’ by default. """
        test_message = self.getUniqueString()
        daemon.runner.emit_message(test_message)
        mock_stderr.write.assert_called_with(mock.ANY)


class is_pidfile_stale_TestCase(scaffold.TestCase):
    """ Test cases for ‘is_pidfile_stale’ function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(is_pidfile_stale_TestCase, self).setUp()

        func_patcher_os_kill = mock.patch.object(os, "kill")
        func_patcher_os_kill.start()
        self.addCleanup(func_patcher_os_kill.stop)
        os.kill.return_value = None

        self.test_pid = self.getUniqueInteger()
        self.test_pidfile = mock.MagicMock(daemon.pidfile.TimeoutPIDLockFile)
        self.test_pidfile.read_pid.return_value = self.test_pid

    def test_returns_false_if_no_pid_in_file(self):
        """ Should return False if the pidfile contains no PID. """
        self.test_pidfile.read_pid.return_value = None
        expected_result = False
        result = daemon.runner.is_pidfile_stale(self.test_pidfile)
        self.assertEqual(expected_result, result)

    def test_returns_false_if_process_exists(self):
        """ Should return False if the process with its PID exists. """
        expected_result = False
        result = daemon.runner.is_pidfile_stale(self.test_pidfile)
        self.assertEqual(expected_result, result)

    def test_returns_true_if_process_does_not_exist(self):
        """ Should return True if the process does not exist. """
        test_error = ProcessLookupError("No such process")
        del os.kill.return_value
        os.kill.side_effect = test_error
        expected_result = True
        result = daemon.runner.is_pidfile_stale(self.test_pidfile)
        self.assertEqual(expected_result, result)


# Local variables:
# coding: utf-8
# mode: python
# End:
# vim: fileencoding=utf-8 filetype=python :
