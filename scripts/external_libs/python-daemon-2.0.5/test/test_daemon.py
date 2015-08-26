# -*- coding: utf-8 -*-
#
# test/test_daemon.py
# Part of ‘python-daemon’, an implementation of PEP 3143.
#
# Copyright © 2008–2015 Ben Finney <ben+python@benfinney.id.au>
#
# This is free software: you may copy, modify, and/or distribute this work
# under the terms of the Apache License, version 2.0 as published by the
# Apache Software Foundation.
# No warranty expressed or implied. See the file ‘LICENSE.ASF-2’ for details.

""" Unit test for ‘daemon’ module.
    """

from __future__ import (absolute_import, unicode_literals)

import os
import sys
import tempfile
import resource
import errno
import signal
import socket
from types import ModuleType
import collections
import functools
try:
    # Standard library of Python 2.7 and later.
    from io import StringIO
except ImportError:
    # Standard library of Python 2.6 and earlier.
    from StringIO import StringIO

import mock

from . import scaffold
from .scaffold import (basestring, unicode)
from .test_pidfile import (
        FakeFileDescriptorStringIO,
        setup_pidfile_fixtures,
        )

import daemon


class ModuleExceptions_TestCase(scaffold.Exception_TestCase):
    """ Test cases for module exception classes. """

    scenarios = scaffold.make_exception_scenarios([
            ('daemon.daemon.DaemonError', dict(
                exc_type = daemon.daemon.DaemonError,
                min_args = 1,
                types = [Exception],
                )),
            ('daemon.daemon.DaemonOSEnvironmentError', dict(
                exc_type = daemon.daemon.DaemonOSEnvironmentError,
                min_args = 1,
                types = [daemon.daemon.DaemonError, OSError],
                )),
            ('daemon.daemon.DaemonProcessDetachError', dict(
                exc_type = daemon.daemon.DaemonProcessDetachError,
                min_args = 1,
                types = [daemon.daemon.DaemonError, OSError],
                )),
            ])


def setup_daemon_context_fixtures(testcase):
    """ Set up common test fixtures for DaemonContext test case.

        :param testcase: A ``TestCase`` instance to decorate.
        :return: ``None``.

        Decorate the `testcase` with fixtures for tests involving
        `DaemonContext`.

        """
    setup_streams_fixtures(testcase)

    setup_pidfile_fixtures(testcase)

    testcase.fake_pidfile_path = tempfile.mktemp()
    testcase.mock_pidlockfile = mock.MagicMock()
    testcase.mock_pidlockfile.path = testcase.fake_pidfile_path

    testcase.daemon_context_args = dict(
            stdin=testcase.stream_files_by_name['stdin'],
            stdout=testcase.stream_files_by_name['stdout'],
            stderr=testcase.stream_files_by_name['stderr'],
            )
    testcase.test_instance = daemon.DaemonContext(
            **testcase.daemon_context_args)

fake_default_signal_map = object()

@mock.patch.object(
        daemon.daemon, "is_detach_process_context_required",
        new=(lambda: True))
@mock.patch.object(
        daemon.daemon, "make_default_signal_map",
        new=(lambda: fake_default_signal_map))
@mock.patch.object(os, "setgid", new=(lambda x: object()))
@mock.patch.object(os, "setuid", new=(lambda x: object()))
class DaemonContext_BaseTestCase(scaffold.TestCase):
    """ Base class for DaemonContext test case classes. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_BaseTestCase, self).setUp()

        setup_daemon_context_fixtures(self)


class DaemonContext_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext class. """

    def test_instantiate(self):
        """ New instance of DaemonContext should be created. """
        self.assertIsInstance(
                self.test_instance, daemon.daemon.DaemonContext)

    def test_minimum_zero_arguments(self):
        """ Initialiser should not require any arguments. """
        instance = daemon.daemon.DaemonContext()
        self.assertIsNot(instance, None)

    def test_has_specified_chroot_directory(self):
        """ Should have specified chroot_directory option. """
        args = dict(
                chroot_directory=object(),
                )
        expected_directory = args['chroot_directory']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_directory, instance.chroot_directory)

    def test_has_specified_working_directory(self):
        """ Should have specified working_directory option. """
        args = dict(
                working_directory=object(),
                )
        expected_directory = args['working_directory']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_directory, instance.working_directory)

    def test_has_default_working_directory(self):
        """ Should have default working_directory option. """
        args = dict()
        expected_directory = "/"
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_directory, instance.working_directory)

    def test_has_specified_creation_mask(self):
        """ Should have specified umask option. """
        args = dict(
                umask=object(),
                )
        expected_mask = args['umask']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_mask, instance.umask)

    def test_has_default_creation_mask(self):
        """ Should have default umask option. """
        args = dict()
        expected_mask = 0
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_mask, instance.umask)

    def test_has_specified_uid(self):
        """ Should have specified uid option. """
        args = dict(
                uid=object(),
                )
        expected_id = args['uid']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_id, instance.uid)

    def test_has_derived_uid(self):
        """ Should have uid option derived from process. """
        args = dict()
        expected_id = os.getuid()
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_id, instance.uid)

    def test_has_specified_gid(self):
        """ Should have specified gid option. """
        args = dict(
                gid=object(),
                )
        expected_id = args['gid']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_id, instance.gid)

    def test_has_derived_gid(self):
        """ Should have gid option derived from process. """
        args = dict()
        expected_id = os.getgid()
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_id, instance.gid)

    def test_has_specified_detach_process(self):
        """ Should have specified detach_process option. """
        args = dict(
                detach_process=object(),
                )
        expected_value = args['detach_process']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_value, instance.detach_process)

    def test_has_derived_detach_process(self):
        """ Should have detach_process option derived from environment. """
        args = dict()
        func = daemon.daemon.is_detach_process_context_required
        expected_value = func()
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_value, instance.detach_process)

    def test_has_specified_files_preserve(self):
        """ Should have specified files_preserve option. """
        args = dict(
                files_preserve=object(),
                )
        expected_files_preserve = args['files_preserve']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_files_preserve, instance.files_preserve)

    def test_has_specified_pidfile(self):
        """ Should have the specified pidfile. """
        args = dict(
                pidfile=object(),
                )
        expected_pidfile = args['pidfile']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_pidfile, instance.pidfile)

    def test_has_specified_stdin(self):
        """ Should have specified stdin option. """
        args = dict(
                stdin=object(),
                )
        expected_file = args['stdin']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_file, instance.stdin)

    def test_has_specified_stdout(self):
        """ Should have specified stdout option. """
        args = dict(
                stdout=object(),
                )
        expected_file = args['stdout']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_file, instance.stdout)

    def test_has_specified_stderr(self):
        """ Should have specified stderr option. """
        args = dict(
                stderr=object(),
                )
        expected_file = args['stderr']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_file, instance.stderr)

    def test_has_specified_signal_map(self):
        """ Should have specified signal_map option. """
        args = dict(
                signal_map=object(),
                )
        expected_signal_map = args['signal_map']
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_signal_map, instance.signal_map)

    def test_has_derived_signal_map(self):
        """ Should have signal_map option derived from system. """
        args = dict()
        expected_signal_map = daemon.daemon.make_default_signal_map()
        instance = daemon.daemon.DaemonContext(**args)
        self.assertEqual(expected_signal_map, instance.signal_map)


class DaemonContext_is_open_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.is_open property. """

    def test_begin_false(self):
        """ Initial value of is_open should be False. """
        instance = self.test_instance
        self.assertEqual(False, instance.is_open)

    def test_write_fails(self):
        """ Writing to is_open should fail. """
        instance = self.test_instance
        self.assertRaises(
                AttributeError,
                setattr, instance, 'is_open', object())


class DaemonContext_open_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.open method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_open_TestCase, self).setUp()

        self.test_instance._is_open = False

        self.mock_module_daemon = mock.MagicMock()
        daemon_func_patchers = dict(
                (func_name, mock.patch.object(
                    daemon.daemon, func_name))
                for func_name in [
                    "detach_process_context",
                    "change_working_directory",
                    "change_root_directory",
                    "change_file_creation_mask",
                    "change_process_owner",
                    "prevent_core_dump",
                    "close_all_open_files",
                    "redirect_stream",
                    "set_signal_handlers",
                    "register_atexit_function",
                    ])
        for (func_name, patcher) in daemon_func_patchers.items():
            mock_func = patcher.start()
            self.addCleanup(patcher.stop)
            self.mock_module_daemon.attach_mock(mock_func, func_name)

        self.mock_module_daemon.attach_mock(mock.Mock(), 'DaemonContext')

        self.test_files_preserve_fds = object()
        self.test_signal_handler_map = object()
        daemoncontext_method_return_values = {
                '_get_exclude_file_descriptors':
                    self.test_files_preserve_fds,
                '_make_signal_handler_map':
                    self.test_signal_handler_map,
                }
        daemoncontext_func_patchers = dict(
                (func_name, mock.patch.object(
                    daemon.daemon.DaemonContext,
                    func_name,
                    return_value=return_value))
                for (func_name, return_value) in
                    daemoncontext_method_return_values.items())
        for (func_name, patcher) in daemoncontext_func_patchers.items():
            mock_func = patcher.start()
            self.addCleanup(patcher.stop)
            self.mock_module_daemon.DaemonContext.attach_mock(
                    mock_func, func_name)

    def test_performs_steps_in_expected_sequence(self):
        """ Should perform daemonisation steps in expected sequence. """
        instance = self.test_instance
        instance.chroot_directory = object()
        instance.detach_process = True
        instance.pidfile = self.mock_pidlockfile
        self.mock_module_daemon.attach_mock(
                self.mock_pidlockfile, 'pidlockfile')
        expected_calls = [
                mock.call.change_root_directory(mock.ANY),
                mock.call.prevent_core_dump(),
                mock.call.change_file_creation_mask(mock.ANY),
                mock.call.change_working_directory(mock.ANY),
                mock.call.change_process_owner(mock.ANY, mock.ANY),
                mock.call.detach_process_context(),
                mock.call.DaemonContext._make_signal_handler_map(),
                mock.call.set_signal_handlers(mock.ANY),
                mock.call.DaemonContext._get_exclude_file_descriptors(),
                mock.call.close_all_open_files(exclude=mock.ANY),
                mock.call.redirect_stream(mock.ANY, mock.ANY),
                mock.call.redirect_stream(mock.ANY, mock.ANY),
                mock.call.redirect_stream(mock.ANY, mock.ANY),
                mock.call.pidlockfile.__enter__(),
                mock.call.register_atexit_function(mock.ANY),
                ]
        instance.open()
        self.mock_module_daemon.assert_has_calls(expected_calls)

    def test_returns_immediately_if_is_open(self):
        """ Should return immediately if is_open property is true. """
        instance = self.test_instance
        instance._is_open = True
        instance.open()
        self.assertEqual(0, len(self.mock_module_daemon.mock_calls))

    def test_changes_root_directory_to_chroot_directory(self):
        """ Should change root directory to `chroot_directory` option. """
        instance = self.test_instance
        chroot_directory = object()
        instance.chroot_directory = chroot_directory
        instance.open()
        self.mock_module_daemon.change_root_directory.assert_called_with(
                chroot_directory)

    def test_omits_chroot_if_no_chroot_directory(self):
        """ Should omit changing root directory if no `chroot_directory`. """
        instance = self.test_instance
        instance.chroot_directory = None
        instance.open()
        self.assertFalse(self.mock_module_daemon.change_root_directory.called)

    def test_prevents_core_dump(self):
        """ Should request prevention of core dumps. """
        instance = self.test_instance
        instance.open()
        self.mock_module_daemon.prevent_core_dump.assert_called_with()

    def test_omits_prevent_core_dump_if_prevent_core_false(self):
        """ Should omit preventing core dumps if `prevent_core` is false. """
        instance = self.test_instance
        instance.prevent_core = False
        instance.open()
        self.assertFalse(self.mock_module_daemon.prevent_core_dump.called)

    def test_closes_open_files(self):
        """ Should close all open files, excluding `files_preserve`. """
        instance = self.test_instance
        expected_exclude = self.test_files_preserve_fds
        instance.open()
        self.mock_module_daemon.close_all_open_files.assert_called_with(
                exclude=expected_exclude)

    def test_changes_directory_to_working_directory(self):
        """ Should change current directory to `working_directory` option. """
        instance = self.test_instance
        working_directory = object()
        instance.working_directory = working_directory
        instance.open()
        self.mock_module_daemon.change_working_directory.assert_called_with(
                working_directory)

    def test_changes_creation_mask_to_umask(self):
        """ Should change file creation mask to `umask` option. """
        instance = self.test_instance
        umask = object()
        instance.umask = umask
        instance.open()
        self.mock_module_daemon.change_file_creation_mask.assert_called_with(
                umask)

    def test_changes_owner_to_specified_uid_and_gid(self):
        """ Should change process UID and GID to `uid` and `gid` options. """
        instance = self.test_instance
        uid = object()
        gid = object()
        instance.uid = uid
        instance.gid = gid
        instance.open()
        self.mock_module_daemon.change_process_owner.assert_called_with(
                uid, gid)

    def test_detaches_process_context(self):
        """ Should request detach of process context. """
        instance = self.test_instance
        instance.open()
        self.mock_module_daemon.detach_process_context.assert_called_with()

    def test_omits_process_detach_if_not_required(self):
        """ Should omit detach of process context if not required. """
        instance = self.test_instance
        instance.detach_process = False
        instance.open()
        self.assertFalse(self.mock_module_daemon.detach_process_context.called)

    def test_sets_signal_handlers_from_signal_map(self):
        """ Should set signal handlers according to `signal_map`. """
        instance = self.test_instance
        instance.signal_map = object()
        expected_signal_handler_map = self.test_signal_handler_map
        instance.open()
        self.mock_module_daemon.set_signal_handlers.assert_called_with(
                expected_signal_handler_map)

    def test_redirects_standard_streams(self):
        """ Should request redirection of standard stream files. """
        instance = self.test_instance
        (system_stdin, system_stdout, system_stderr) = (
                sys.stdin, sys.stdout, sys.stderr)
        (target_stdin, target_stdout, target_stderr) = (
                self.stream_files_by_name[name]
                for name in ['stdin', 'stdout', 'stderr'])
        expected_calls = [
                mock.call(system_stdin, target_stdin),
                mock.call(system_stdout, target_stdout),
                mock.call(system_stderr, target_stderr),
                ]
        instance.open()
        self.mock_module_daemon.redirect_stream.assert_has_calls(
                expected_calls, any_order=True)

    def test_enters_pidfile_context(self):
        """ Should enter the PID file context manager. """
        instance = self.test_instance
        instance.pidfile = self.mock_pidlockfile
        instance.open()
        self.mock_pidlockfile.__enter__.assert_called_with()

    def test_sets_is_open_true(self):
        """ Should set the `is_open` property to True. """
        instance = self.test_instance
        instance.open()
        self.assertEqual(True, instance.is_open)

    def test_registers_close_method_for_atexit(self):
        """ Should register the `close` method for atexit processing. """
        instance = self.test_instance
        close_method = instance.close
        instance.open()
        self.mock_module_daemon.register_atexit_function.assert_called_with(
                close_method)


class DaemonContext_close_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.close method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_close_TestCase, self).setUp()

        self.test_instance._is_open = True

    def test_returns_immediately_if_not_is_open(self):
        """ Should return immediately if is_open property is false. """
        instance = self.test_instance
        instance._is_open = False
        instance.pidfile = object()
        instance.close()
        self.assertFalse(self.mock_pidlockfile.__exit__.called)

    def test_exits_pidfile_context(self):
        """ Should exit the PID file context manager. """
        instance = self.test_instance
        instance.pidfile = self.mock_pidlockfile
        instance.close()
        self.mock_pidlockfile.__exit__.assert_called_with(None, None, None)

    def test_returns_none(self):
        """ Should return None. """
        instance = self.test_instance
        expected_result = None
        result = instance.close()
        self.assertIs(result, expected_result)

    def test_sets_is_open_false(self):
        """ Should set the `is_open` property to False. """
        instance = self.test_instance
        instance.close()
        self.assertEqual(False, instance.is_open)


@mock.patch.object(daemon.daemon.DaemonContext, "open")
class DaemonContext_context_manager_enter_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.__enter__ method. """

    def test_opens_daemon_context(self, mock_func_daemoncontext_open):
        """ Should open the DaemonContext. """
        instance = self.test_instance
        instance.__enter__()
        mock_func_daemoncontext_open.assert_called_with()

    def test_returns_self_instance(self, mock_func_daemoncontext_open):
        """ Should return DaemonContext instance. """
        instance = self.test_instance
        expected_result = instance
        result = instance.__enter__()
        self.assertIs(result, expected_result)


@mock.patch.object(daemon.daemon.DaemonContext, "close")
class DaemonContext_context_manager_exit_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.__exit__ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_context_manager_exit_TestCase, self).setUp()

        self.test_args = dict(
                exc_type=object(),
                exc_value=object(),
                traceback=object(),
                )

    def test_closes_daemon_context(self, mock_func_daemoncontext_close):
        """ Should close the DaemonContext. """
        instance = self.test_instance
        args = self.test_args
        instance.__exit__(**args)
        mock_func_daemoncontext_close.assert_called_with()

    def test_returns_none(self, mock_func_daemoncontext_close):
        """ Should return None, indicating exception was not handled. """
        instance = self.test_instance
        args = self.test_args
        expected_result = None
        result = instance.__exit__(**args)
        self.assertIs(result, expected_result)


class DaemonContext_terminate_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext.terminate method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_terminate_TestCase, self).setUp()

        self.test_signal = signal.SIGTERM
        self.test_frame = None
        self.test_args = (self.test_signal, self.test_frame)

    def test_raises_system_exit(self):
        """ Should raise SystemExit. """
        instance = self.test_instance
        args = self.test_args
        expected_exception = SystemExit
        self.assertRaises(
                expected_exception,
                instance.terminate, *args)

    def test_exception_message_contains_signal_number(self):
        """ Should raise exception with a message containing signal number. """
        instance = self.test_instance
        args = self.test_args
        signal_number = self.test_signal
        expected_exception = SystemExit
        exc = self.assertRaises(
                expected_exception,
                instance.terminate, *args)
        self.assertIn(unicode(signal_number), unicode(exc))


class DaemonContext_get_exclude_file_descriptors_TestCase(
        DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext._get_exclude_file_descriptors function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(
                DaemonContext_get_exclude_file_descriptors_TestCase,
                self).setUp()

        self.test_files = {
                2: FakeFileDescriptorStringIO(),
                5: 5,
                11: FakeFileDescriptorStringIO(),
                17: None,
                23: FakeFileDescriptorStringIO(),
                37: 37,
                42: FakeFileDescriptorStringIO(),
                }
        for (fileno, item) in self.test_files.items():
            if hasattr(item, '_fileno'):
                item._fileno = fileno
        self.test_file_descriptors = set(
                fd for (fd, item) in self.test_files.items()
                if item is not None)
        self.test_file_descriptors.update(
                self.stream_files_by_name[name].fileno()
                for name in ['stdin', 'stdout', 'stderr']
                )

    def test_returns_expected_file_descriptors(self):
        """ Should return expected set of file descriptors. """
        instance = self.test_instance
        instance.files_preserve = list(self.test_files.values())
        expected_result = self.test_file_descriptors
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_returns_stream_redirects_if_no_files_preserve(self):
        """ Should return only stream redirects if no files_preserve. """
        instance = self.test_instance
        instance.files_preserve = None
        expected_result = set(
                stream.fileno()
                for stream in self.stream_files_by_name.values())
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_returns_empty_set_if_no_files(self):
        """ Should return empty set if no file options. """
        instance = self.test_instance
        for name in ['files_preserve', 'stdin', 'stdout', 'stderr']:
            setattr(instance, name, None)
        expected_result = set()
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_omits_non_file_streams(self):
        """ Should omit non-file stream attributes. """
        instance = self.test_instance
        instance.files_preserve = list(self.test_files.values())
        stream_files = self.stream_files_by_name
        expected_result = self.test_file_descriptors.copy()
        for (pseudo_stream_name, pseudo_stream) in stream_files.items():
            test_non_file_object = object()
            setattr(instance, pseudo_stream_name, test_non_file_object)
            stream_fd = pseudo_stream.fileno()
            expected_result.discard(stream_fd)
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_includes_verbatim_streams_without_file_descriptor(self):
        """ Should include verbatim any stream without a file descriptor. """
        instance = self.test_instance
        instance.files_preserve = list(self.test_files.values())
        stream_files = self.stream_files_by_name
        mock_fileno_method = mock.MagicMock(
                spec=sys.__stdin__.fileno,
                side_effect=ValueError)
        expected_result = self.test_file_descriptors.copy()
        for (pseudo_stream_name, pseudo_stream) in stream_files.items():
            test_non_fd_stream = StringIO()
            if not hasattr(test_non_fd_stream, 'fileno'):
                # Python < 3 StringIO doesn't have ‘fileno’ at all.
                # Add a method which raises an exception.
                test_non_fd_stream.fileno = mock_fileno_method
            setattr(instance, pseudo_stream_name, test_non_fd_stream)
            stream_fd = pseudo_stream.fileno()
            expected_result.discard(stream_fd)
            expected_result.add(test_non_fd_stream)
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_omits_none_streams(self):
        """ Should omit any stream attribute which is None. """
        instance = self.test_instance
        instance.files_preserve = list(self.test_files.values())
        stream_files = self.stream_files_by_name
        expected_result = self.test_file_descriptors.copy()
        for (pseudo_stream_name, pseudo_stream) in stream_files.items():
            setattr(instance, pseudo_stream_name, None)
            stream_fd = pseudo_stream.fileno()
            expected_result.discard(stream_fd)
        result = instance._get_exclude_file_descriptors()
        self.assertEqual(expected_result, result)


class DaemonContext_make_signal_handler_TestCase(DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext._make_signal_handler function. """

    def test_returns_ignore_for_none(self):
        """ Should return SIG_IGN when None handler specified. """
        instance = self.test_instance
        target = None
        expected_result = signal.SIG_IGN
        result = instance._make_signal_handler(target)
        self.assertEqual(expected_result, result)

    def test_returns_method_for_name(self):
        """ Should return method of DaemonContext when name specified. """
        instance = self.test_instance
        target = 'terminate'
        expected_result = instance.terminate
        result = instance._make_signal_handler(target)
        self.assertEqual(expected_result, result)

    def test_raises_error_for_unknown_name(self):
        """ Should raise AttributeError for unknown method name. """
        instance = self.test_instance
        target = 'b0gUs'
        expected_error = AttributeError
        self.assertRaises(
                expected_error,
                instance._make_signal_handler, target)

    def test_returns_object_for_object(self):
        """ Should return same object for any other object. """
        instance = self.test_instance
        target = object()
        expected_result = target
        result = instance._make_signal_handler(target)
        self.assertEqual(expected_result, result)


class DaemonContext_make_signal_handler_map_TestCase(
        DaemonContext_BaseTestCase):
    """ Test cases for DaemonContext._make_signal_handler_map function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(DaemonContext_make_signal_handler_map_TestCase, self).setUp()

        self.test_instance.signal_map = {
                object(): object(),
                object(): object(),
                object(): object(),
                }

        self.test_signal_handlers = dict(
                (key, object())
                for key in self.test_instance.signal_map.values())
        self.test_signal_handler_map = dict(
                (key, self.test_signal_handlers[target])
                for (key, target) in self.test_instance.signal_map.items())

        def fake_make_signal_handler(target):
            return self.test_signal_handlers[target]

        func_patcher_make_signal_handler = mock.patch.object(
                daemon.daemon.DaemonContext, "_make_signal_handler",
                side_effect=fake_make_signal_handler)
        self.mock_func_make_signal_handler = (
                func_patcher_make_signal_handler.start())
        self.addCleanup(func_patcher_make_signal_handler.stop)

    def test_returns_constructed_signal_handler_items(self):
        """ Should return items as constructed via make_signal_handler. """
        instance = self.test_instance
        expected_result = self.test_signal_handler_map
        result = instance._make_signal_handler_map()
        self.assertEqual(expected_result, result)


try:
    FileNotFoundError
except NameError:
    # Python 2 uses IOError.
    FileNotFoundError = functools.partial(IOError, errno.ENOENT)


@mock.patch.object(os, "chdir")
class change_working_directory_TestCase(scaffold.TestCase):
    """ Test cases for change_working_directory function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(change_working_directory_TestCase, self).setUp()

        self.test_directory = object()
        self.test_args = dict(
                directory=self.test_directory,
                )

    def test_changes_working_directory_to_specified_directory(
            self,
            mock_func_os_chdir):
        """ Should change working directory to specified directory. """
        args = self.test_args
        directory = self.test_directory
        daemon.daemon.change_working_directory(**args)
        mock_func_os_chdir.assert_called_with(directory)

    def test_raises_daemon_error_on_os_error(
            self,
            mock_func_os_chdir):
        """ Should raise a DaemonError on receiving an IOError. """
        args = self.test_args
        test_error = FileNotFoundError("No such directory")
        mock_func_os_chdir.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_working_directory, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_error_message_contains_original_error_message(
            self,
            mock_func_os_chdir):
        """ Should raise a DaemonError with original message. """
        args = self.test_args
        test_error = FileNotFoundError("No such directory")
        mock_func_os_chdir.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_working_directory, **args)
        self.assertIn(unicode(test_error), unicode(exc))


@mock.patch.object(os, "chroot")
@mock.patch.object(os, "chdir")
class change_root_directory_TestCase(scaffold.TestCase):
    """ Test cases for change_root_directory function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(change_root_directory_TestCase, self).setUp()

        self.test_directory = object()
        self.test_args = dict(
                directory=self.test_directory,
                )

    def test_changes_working_directory_to_specified_directory(
            self,
            mock_func_os_chdir, mock_func_os_chroot):
        """ Should change working directory to specified directory. """
        args = self.test_args
        directory = self.test_directory
        daemon.daemon.change_root_directory(**args)
        mock_func_os_chdir.assert_called_with(directory)

    def test_changes_root_directory_to_specified_directory(
            self,
            mock_func_os_chdir, mock_func_os_chroot):
        """ Should change root directory to specified directory. """
        args = self.test_args
        directory = self.test_directory
        daemon.daemon.change_root_directory(**args)
        mock_func_os_chroot.assert_called_with(directory)

    def test_raises_daemon_error_on_os_error_from_chdir(
            self,
            mock_func_os_chdir, mock_func_os_chroot):
        """ Should raise a DaemonError on receiving an IOError from chdir. """
        args = self.test_args
        test_error = FileNotFoundError("No such directory")
        mock_func_os_chdir.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_root_directory, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_raises_daemon_error_on_os_error_from_chroot(
            self,
            mock_func_os_chdir, mock_func_os_chroot):
        """ Should raise a DaemonError on receiving an OSError from chroot. """
        args = self.test_args
        test_error = OSError(errno.EPERM, "No chroot for you!")
        mock_func_os_chroot.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_root_directory, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_error_message_contains_original_error_message(
            self,
            mock_func_os_chdir, mock_func_os_chroot):
        """ Should raise a DaemonError with original message. """
        args = self.test_args
        test_error = FileNotFoundError("No such directory")
        mock_func_os_chdir.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_root_directory, **args)
        self.assertIn(unicode(test_error), unicode(exc))


@mock.patch.object(os, "umask")
class change_file_creation_mask_TestCase(scaffold.TestCase):
    """ Test cases for change_file_creation_mask function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(change_file_creation_mask_TestCase, self).setUp()

        self.test_mask = object()
        self.test_args = dict(
                mask=self.test_mask,
                )

    def test_changes_umask_to_specified_mask(self, mock_func_os_umask):
        """ Should change working directory to specified directory. """
        args = self.test_args
        mask = self.test_mask
        daemon.daemon.change_file_creation_mask(**args)
        mock_func_os_umask.assert_called_with(mask)

    def test_raises_daemon_error_on_os_error_from_chdir(
            self,
            mock_func_os_umask):
        """ Should raise a DaemonError on receiving an OSError from umask. """
        args = self.test_args
        test_error = OSError(errno.EINVAL, "Whatchoo talkin' 'bout?")
        mock_func_os_umask.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_file_creation_mask, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_error_message_contains_original_error_message(
            self,
            mock_func_os_umask):
        """ Should raise a DaemonError with original message. """
        args = self.test_args
        test_error = FileNotFoundError("No such directory")
        mock_func_os_umask.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_file_creation_mask, **args)
        self.assertIn(unicode(test_error), unicode(exc))


@mock.patch.object(os, "setgid")
@mock.patch.object(os, "setuid")
class change_process_owner_TestCase(scaffold.TestCase):
    """ Test cases for change_process_owner function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(change_process_owner_TestCase, self).setUp()

        self.test_uid = object()
        self.test_gid = object()
        self.test_args = dict(
                uid=self.test_uid,
                gid=self.test_gid,
                )

    def test_changes_gid_and_uid_in_order(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should change process GID and UID in correct order.

            Since the process requires appropriate privilege to use
            either of `setuid` or `setgid`, changing the UID must be
            done last.

            """
        args = self.test_args
        daemon.daemon.change_process_owner(**args)
        mock_func_os_setuid.assert_called()
        mock_func_os_setgid.assert_called()

    def test_changes_group_id_to_gid(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should change process GID to specified value. """
        args = self.test_args
        gid = self.test_gid
        daemon.daemon.change_process_owner(**args)
        mock_func_os_setgid.assert_called(gid)

    def test_changes_user_id_to_uid(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should change process UID to specified value. """
        args = self.test_args
        uid = self.test_uid
        daemon.daemon.change_process_owner(**args)
        mock_func_os_setuid.assert_called(uid)

    def test_raises_daemon_error_on_os_error_from_setgid(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should raise a DaemonError on receiving an OSError from setgid. """
        args = self.test_args
        test_error = OSError(errno.EPERM, "No switching for you!")
        mock_func_os_setgid.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_process_owner, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_raises_daemon_error_on_os_error_from_setuid(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should raise a DaemonError on receiving an OSError from setuid. """
        args = self.test_args
        test_error = OSError(errno.EPERM, "No switching for you!")
        mock_func_os_setuid.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_process_owner, **args)
        self.assertEqual(test_error, exc.__cause__)

    def test_error_message_contains_original_error_message(
            self,
            mock_func_os_setuid, mock_func_os_setgid):
        """ Should raise a DaemonError with original message. """
        args = self.test_args
        test_error = OSError(errno.EINVAL, "Whatchoo talkin' 'bout?")
        mock_func_os_setuid.side_effect = test_error
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.change_process_owner, **args)
        self.assertIn(unicode(test_error), unicode(exc))


RLimitResult = collections.namedtuple('RLimitResult', ['soft', 'hard'])

fake_RLIMIT_CORE = object()

@mock.patch.object(resource, "RLIMIT_CORE", new=fake_RLIMIT_CORE)
@mock.patch.object(resource, "setrlimit", side_effect=(lambda x, y: None))
@mock.patch.object(resource, "getrlimit", side_effect=(lambda x: None))
class prevent_core_dump_TestCase(scaffold.TestCase):
    """ Test cases for prevent_core_dump function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(prevent_core_dump_TestCase, self).setUp()

    def test_sets_core_limit_to_zero(
            self,
            mock_func_resource_getrlimit, mock_func_resource_setrlimit):
        """ Should set the RLIMIT_CORE resource to zero. """
        expected_resource = fake_RLIMIT_CORE
        expected_limit = tuple(RLimitResult(soft=0, hard=0))
        daemon.daemon.prevent_core_dump()
        mock_func_resource_getrlimit.assert_called_with(expected_resource)
        mock_func_resource_setrlimit.assert_called_with(
                expected_resource, expected_limit)

    def test_raises_error_when_no_core_resource(
            self,
            mock_func_resource_getrlimit, mock_func_resource_setrlimit):
        """ Should raise DaemonError if no RLIMIT_CORE resource. """
        test_error = ValueError("Bogus platform doesn't have RLIMIT_CORE")
        def fake_getrlimit(res):
            if res == resource.RLIMIT_CORE:
                raise test_error
            else:
                return None
        mock_func_resource_getrlimit.side_effect = fake_getrlimit
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.prevent_core_dump)
        self.assertEqual(test_error, exc.__cause__)


@mock.patch.object(os, "close")
class close_file_descriptor_if_open_TestCase(scaffold.TestCase):
    """ Test cases for close_file_descriptor_if_open function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(close_file_descriptor_if_open_TestCase, self).setUp()

        self.fake_fd = 274

    def test_requests_file_descriptor_close(self, mock_func_os_close):
        """ Should request close of file descriptor. """
        fd = self.fake_fd
        daemon.daemon.close_file_descriptor_if_open(fd)
        mock_func_os_close.assert_called_with(fd)

    def test_ignores_badfd_error_on_close(self, mock_func_os_close):
        """ Should ignore OSError EBADF when closing. """
        fd = self.fake_fd
        test_error = OSError(errno.EBADF, "Bad file descriptor")
        def fake_os_close(fd):
            raise test_error
        mock_func_os_close.side_effect = fake_os_close
        daemon.daemon.close_file_descriptor_if_open(fd)
        mock_func_os_close.assert_called_with(fd)

    def test_raises_error_if_oserror_on_close(self, mock_func_os_close):
        """ Should raise DaemonError if an OSError occurs when closing. """
        fd = self.fake_fd
        test_error = OSError(object(), "Unexpected error")
        def fake_os_close(fd):
            raise test_error
        mock_func_os_close.side_effect = fake_os_close
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.close_file_descriptor_if_open, fd)
        self.assertEqual(test_error, exc.__cause__)

    def test_raises_error_if_ioerror_on_close(self, mock_func_os_close):
        """ Should raise DaemonError if an IOError occurs when closing. """
        fd = self.fake_fd
        test_error = IOError(object(), "Unexpected error")
        def fake_os_close(fd):
            raise test_error
        mock_func_os_close.side_effect = fake_os_close
        expected_error = daemon.daemon.DaemonOSEnvironmentError
        exc = self.assertRaises(
                expected_error,
                daemon.daemon.close_file_descriptor_if_open, fd)
        self.assertEqual(test_error, exc.__cause__)


class maxfd_TestCase(scaffold.TestCase):
    """ Test cases for module MAXFD constant. """

    def test_positive(self):
        """ Should be a positive number. """
        maxfd = daemon.daemon.MAXFD
        self.assertTrue(maxfd > 0)

    def test_integer(self):
        """ Should be an integer. """
        maxfd = daemon.daemon.MAXFD
        self.assertEqual(int(maxfd), maxfd)

    def test_reasonably_high(self):
        """ Should be reasonably high for default open files limit.

            If the system reports a limit of “infinity” on maximum
            file descriptors, we still need a finite number in order
            to close “all” of them. Ensure this is reasonably high
            to catch most use cases.

            """
        expected_minimum = 2048
        maxfd = daemon.daemon.MAXFD
        self.assertTrue(
                expected_minimum <= maxfd,
                msg=(
                    "MAXFD should be at least {minimum!r}"
                    " (got {maxfd!r})".format(
                        minimum=expected_minimum, maxfd=maxfd)))


fake_default_maxfd = 8
fake_RLIMIT_NOFILE = object()
fake_RLIM_INFINITY = object()
fake_rlimit_nofile_large = 2468

def fake_getrlimit_nofile_soft_infinity(resource):
    result = RLimitResult(soft=fake_RLIM_INFINITY, hard=object())
    if resource != fake_RLIMIT_NOFILE:
        result = NotImplemented
    return result

def fake_getrlimit_nofile_hard_infinity(resource):
    result = RLimitResult(soft=object(), hard=fake_RLIM_INFINITY)
    if resource != fake_RLIMIT_NOFILE:
        result = NotImplemented
    return result

def fake_getrlimit_nofile_hard_large(resource):
    result = RLimitResult(soft=object(), hard=fake_rlimit_nofile_large)
    if resource != fake_RLIMIT_NOFILE:
        result = NotImplemented
    return result

@mock.patch.object(daemon.daemon, "MAXFD", new=fake_default_maxfd)
@mock.patch.object(resource, "RLIMIT_NOFILE", new=fake_RLIMIT_NOFILE)
@mock.patch.object(resource, "RLIM_INFINITY", new=fake_RLIM_INFINITY)
@mock.patch.object(
        resource, "getrlimit",
        side_effect=fake_getrlimit_nofile_hard_large)
class get_maximum_file_descriptors_TestCase(scaffold.TestCase):
    """ Test cases for get_maximum_file_descriptors function. """

    def test_returns_system_hard_limit(self, mock_func_resource_getrlimit):
        """ Should return process hard limit on number of files. """
        expected_result = fake_rlimit_nofile_large
        result = daemon.daemon.get_maximum_file_descriptors()
        self.assertEqual(expected_result, result)

    def test_returns_module_default_if_hard_limit_infinity(
            self, mock_func_resource_getrlimit):
        """ Should return module MAXFD if hard limit is infinity. """
        mock_func_resource_getrlimit.side_effect = (
                fake_getrlimit_nofile_hard_infinity)
        expected_result = fake_default_maxfd
        result = daemon.daemon.get_maximum_file_descriptors()
        self.assertEqual(expected_result, result)


def fake_get_maximum_file_descriptors():
    return fake_default_maxfd

@mock.patch.object(resource, "RLIMIT_NOFILE", new=fake_RLIMIT_NOFILE)
@mock.patch.object(resource, "RLIM_INFINITY", new=fake_RLIM_INFINITY)
@mock.patch.object(
        resource, "getrlimit",
        new=fake_getrlimit_nofile_soft_infinity)
@mock.patch.object(
        daemon.daemon, "get_maximum_file_descriptors",
        new=fake_get_maximum_file_descriptors)
@mock.patch.object(daemon.daemon, "close_file_descriptor_if_open")
class close_all_open_files_TestCase(scaffold.TestCase):
    """ Test cases for close_all_open_files function. """

    def test_requests_all_open_files_to_close(
            self, mock_func_close_file_descriptor_if_open):
        """ Should request close of all open files. """
        expected_file_descriptors = range(fake_default_maxfd)
        expected_calls = [
                mock.call(fd) for fd in expected_file_descriptors]
        daemon.daemon.close_all_open_files()
        mock_func_close_file_descriptor_if_open.assert_has_calls(
                expected_calls, any_order=True)

    def test_requests_all_but_excluded_files_to_close(
            self, mock_func_close_file_descriptor_if_open):
        """ Should request close of all open files but those excluded. """
        test_exclude = set([3, 7])
        args = dict(
                exclude=test_exclude,
                )
        expected_file_descriptors = set(
                fd for fd in range(fake_default_maxfd)
                if fd not in test_exclude)
        expected_calls = [
                mock.call(fd) for fd in expected_file_descriptors]
        daemon.daemon.close_all_open_files(**args)
        mock_func_close_file_descriptor_if_open.assert_has_calls(
                expected_calls, any_order=True)


class detach_process_context_TestCase(scaffold.TestCase):
    """ Test cases for detach_process_context function. """

    class FakeOSExit(SystemExit):
        """ Fake exception raised for os._exit(). """

    def setUp(self):
        """ Set up test fixtures. """
        super(detach_process_context_TestCase, self).setUp()

        self.mock_module_os = mock.MagicMock(wraps=os)

        fake_pids = [0, 0]
        func_patcher_os_fork = mock.patch.object(
                os, "fork",
                side_effect=iter(fake_pids))
        self.mock_func_os_fork = func_patcher_os_fork.start()
        self.addCleanup(func_patcher_os_fork.stop)
        self.mock_module_os.attach_mock(self.mock_func_os_fork, "fork")

        func_patcher_os_setsid = mock.patch.object(os, "setsid")
        self.mock_func_os_setsid = func_patcher_os_setsid.start()
        self.addCleanup(func_patcher_os_setsid.stop)
        self.mock_module_os.attach_mock(self.mock_func_os_setsid, "setsid")

        def raise_os_exit(status=None):
            raise self.FakeOSExit(status)

        func_patcher_os_force_exit = mock.patch.object(
                os, "_exit",
                side_effect=raise_os_exit)
        self.mock_func_os_force_exit = func_patcher_os_force_exit.start()
        self.addCleanup(func_patcher_os_force_exit.stop)
        self.mock_module_os.attach_mock(self.mock_func_os_force_exit, "_exit")

    def test_parent_exits(self):
        """ Parent process should exit. """
        parent_pid = 23
        self.mock_func_os_fork.side_effect = iter([parent_pid])
        self.assertRaises(
                self.FakeOSExit,
                daemon.daemon.detach_process_context)
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                mock.call._exit(0),
                ])

    def test_first_fork_error_raises_error(self):
        """ Error on first fork should raise DaemonProcessDetachError. """
        fork_errno = 13
        fork_strerror = "Bad stuff happened"
        test_error = OSError(fork_errno, fork_strerror)
        test_pids_iter = iter([test_error])

        def fake_fork():
            next_item = next(test_pids_iter)
            if isinstance(next_item, Exception):
                raise next_item
            else:
                return next_item

        self.mock_func_os_fork.side_effect = fake_fork
        exc = self.assertRaises(
                daemon.daemon.DaemonProcessDetachError,
                daemon.daemon.detach_process_context)
        self.assertEqual(test_error, exc.__cause__)
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                ])

    def test_child_starts_new_process_group(self):
        """ Child should start new process group. """
        daemon.daemon.detach_process_context()
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                mock.call.setsid(),
                ])

    def test_child_forks_next_parent_exits(self):
        """ Child should fork, then exit if parent. """
        fake_pids = [0, 42]
        self.mock_func_os_fork.side_effect = iter(fake_pids)
        self.assertRaises(
                self.FakeOSExit,
                daemon.daemon.detach_process_context)
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                mock.call.setsid(),
                mock.call.fork(),
                mock.call._exit(0),
                ])

    def test_second_fork_error_reports_to_stderr(self):
        """ Error on second fork should cause report to stderr. """
        fork_errno = 17
        fork_strerror = "Nasty stuff happened"
        test_error = OSError(fork_errno, fork_strerror)
        test_pids_iter = iter([0, test_error])

        def fake_fork():
            next_item = next(test_pids_iter)
            if isinstance(next_item, Exception):
                raise next_item
            else:
                return next_item

        self.mock_func_os_fork.side_effect = fake_fork
        exc = self.assertRaises(
                daemon.daemon.DaemonProcessDetachError,
                daemon.daemon.detach_process_context)
        self.assertEqual(test_error, exc.__cause__)
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                mock.call.setsid(),
                mock.call.fork(),
                ])

    def test_child_forks_next_child_continues(self):
        """ Child should fork, then continue if child. """
        daemon.daemon.detach_process_context()
        self.mock_module_os.assert_has_calls([
                mock.call.fork(),
                mock.call.setsid(),
                mock.call.fork(),
                ])


@mock.patch("os.getppid", return_value=765)
class is_process_started_by_init_TestCase(scaffold.TestCase):
    """ Test cases for is_process_started_by_init function. """

    def test_returns_false_by_default(self, mock_func_os_getppid):
        """ Should return False under normal circumstances. """
        expected_result = False
        result = daemon.daemon.is_process_started_by_init()
        self.assertIs(result, expected_result)

    def test_returns_true_if_parent_process_is_init(
            self, mock_func_os_getppid):
        """ Should return True if parent process is `init`. """
        init_pid = 1
        mock_func_os_getppid.return_value = init_pid
        expected_result = True
        result = daemon.daemon.is_process_started_by_init()
        self.assertIs(result, expected_result)


class is_socket_TestCase(scaffold.TestCase):
    """ Test cases for is_socket function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(is_socket_TestCase, self).setUp()

        def fake_getsockopt(level, optname, buflen=None):
            result = object()
            if optname is socket.SO_TYPE:
                result = socket.SOCK_RAW
            return result

        self.fake_socket_getsockopt_func = fake_getsockopt

        self.fake_socket_error = socket.error(
                errno.ENOTSOCK,
                "Socket operation on non-socket")

        self.mock_socket = mock.MagicMock(spec=socket.socket)
        self.mock_socket.getsockopt.side_effect = self.fake_socket_error

        def fake_socket_fromfd(fd, family, type, proto=None):
            return self.mock_socket

        func_patcher_socket_fromfd = mock.patch.object(
                socket, "fromfd",
                side_effect=fake_socket_fromfd)
        func_patcher_socket_fromfd.start()
        self.addCleanup(func_patcher_socket_fromfd.stop)

    def test_returns_false_by_default(self):
        """ Should return False under normal circumstances. """
        test_fd = 23
        expected_result = False
        result = daemon.daemon.is_socket(test_fd)
        self.assertIs(result, expected_result)

    def test_returns_true_if_stdin_is_socket(self):
        """ Should return True if `stdin` is a socket. """
        test_fd = 23
        getsockopt = self.mock_socket.getsockopt
        getsockopt.side_effect = self.fake_socket_getsockopt_func
        expected_result = True
        result = daemon.daemon.is_socket(test_fd)
        self.assertIs(result, expected_result)

    def test_returns_false_if_stdin_socket_raises_error(self):
        """ Should return True if `stdin` is a socket and raises error. """
        test_fd = 23
        getsockopt = self.mock_socket.getsockopt
        getsockopt.side_effect = socket.error(
                object(), "Weird socket stuff")
        expected_result = True
        result = daemon.daemon.is_socket(test_fd)
        self.assertIs(result, expected_result)


class is_process_started_by_superserver_TestCase(scaffold.TestCase):
    """ Test cases for is_process_started_by_superserver function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(is_process_started_by_superserver_TestCase, self).setUp()

        def fake_is_socket(fd):
            if sys.__stdin__.fileno() == fd:
                result = self.fake_stdin_is_socket_func()
            else:
                result = False
            return result

        self.fake_stdin_is_socket_func = (lambda: False)

        func_patcher_is_socket = mock.patch.object(
                daemon.daemon, "is_socket",
                side_effect=fake_is_socket)
        func_patcher_is_socket.start()
        self.addCleanup(func_patcher_is_socket.stop)

    def test_returns_false_by_default(self):
        """ Should return False under normal circumstances. """
        expected_result = False
        result = daemon.daemon.is_process_started_by_superserver()
        self.assertIs(result, expected_result)

    def test_returns_true_if_stdin_is_socket(self):
        """ Should return True if `stdin` is a socket. """
        self.fake_stdin_is_socket_func = (lambda: True)
        expected_result = True
        result = daemon.daemon.is_process_started_by_superserver()
        self.assertIs(result, expected_result)


@mock.patch.object(
        daemon.daemon, "is_process_started_by_superserver",
        return_value=False)
@mock.patch.object(
        daemon.daemon, "is_process_started_by_init",
        return_value=False)
class is_detach_process_context_required_TestCase(scaffold.TestCase):
    """ Test cases for is_detach_process_context_required function. """

    def test_returns_true_by_default(
            self,
            mock_func_is_process_started_by_init,
            mock_func_is_process_started_by_superserver):
        """ Should return True under normal circumstances. """
        expected_result = True
        result = daemon.daemon.is_detach_process_context_required()
        self.assertIs(result, expected_result)

    def test_returns_false_if_started_by_init(
            self,
            mock_func_is_process_started_by_init,
            mock_func_is_process_started_by_superserver):
        """ Should return False if current process started by init. """
        mock_func_is_process_started_by_init.return_value = True
        expected_result = False
        result = daemon.daemon.is_detach_process_context_required()
        self.assertIs(result, expected_result)

    def test_returns_true_if_started_by_superserver(
            self,
            mock_func_is_process_started_by_init,
            mock_func_is_process_started_by_superserver):
        """ Should return False if current process started by superserver. """
        mock_func_is_process_started_by_superserver.return_value = True
        expected_result = False
        result = daemon.daemon.is_detach_process_context_required()
        self.assertIs(result, expected_result)


def setup_streams_fixtures(testcase):
    """ Set up common test fixtures for standard streams. """
    testcase.stream_file_paths = dict(
            stdin=tempfile.mktemp(),
            stdout=tempfile.mktemp(),
            stderr=tempfile.mktemp(),
            )

    testcase.stream_files_by_name = dict(
            (name, FakeFileDescriptorStringIO())
            for name in ['stdin', 'stdout', 'stderr']
            )

    testcase.stream_files_by_path = dict(
            (testcase.stream_file_paths[name],
                testcase.stream_files_by_name[name])
            for name in ['stdin', 'stdout', 'stderr']
            )


@mock.patch.object(os, "dup2")
class redirect_stream_TestCase(scaffold.TestCase):
    """ Test cases for redirect_stream function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(redirect_stream_TestCase, self).setUp()

        self.test_system_stream = FakeFileDescriptorStringIO()
        self.test_target_stream = FakeFileDescriptorStringIO()
        self.test_null_file = FakeFileDescriptorStringIO()

        def fake_os_open(path, flag, mode=None):
            if path == os.devnull:
                result = self.test_null_file.fileno()
            else:
                raise FileNotFoundError("No such file", path)
            return result

        func_patcher_os_open = mock.patch.object(
                os, "open",
                side_effect=fake_os_open)
        self.mock_func_os_open = func_patcher_os_open.start()
        self.addCleanup(func_patcher_os_open.stop)

    def test_duplicates_target_file_descriptor(
            self, mock_func_os_dup2):
        """ Should duplicate file descriptor from target to system stream. """
        system_stream = self.test_system_stream
        system_fileno = system_stream.fileno()
        target_stream = self.test_target_stream
        target_fileno = target_stream.fileno()
        daemon.daemon.redirect_stream(system_stream, target_stream)
        mock_func_os_dup2.assert_called_with(target_fileno, system_fileno)

    def test_duplicates_null_file_descriptor_by_default(
            self, mock_func_os_dup2):
        """ Should by default duplicate the null file to the system stream. """
        system_stream = self.test_system_stream
        system_fileno = system_stream.fileno()
        target_stream = None
        null_path = os.devnull
        null_flag = os.O_RDWR
        null_file = self.test_null_file
        null_fileno = null_file.fileno()
        daemon.daemon.redirect_stream(system_stream, target_stream)
        self.mock_func_os_open.assert_called_with(null_path, null_flag)
        mock_func_os_dup2.assert_called_with(null_fileno, system_fileno)


class make_default_signal_map_TestCase(scaffold.TestCase):
    """ Test cases for make_default_signal_map function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(make_default_signal_map_TestCase, self).setUp()

        # Use whatever default string type this Python version needs.
        signal_module_name = str('signal')
        self.fake_signal_module = ModuleType(signal_module_name)

        fake_signal_names = [
                'SIGHUP',
                'SIGCLD',
                'SIGSEGV',
                'SIGTSTP',
                'SIGTTIN',
                'SIGTTOU',
                'SIGTERM',
                ]
        for name in fake_signal_names:
            setattr(self.fake_signal_module, name, object())

        module_patcher_signal = mock.patch.object(
                daemon.daemon, "signal", new=self.fake_signal_module)
        module_patcher_signal.start()
        self.addCleanup(module_patcher_signal.stop)

        default_signal_map_by_name = {
                'SIGTSTP': None,
                'SIGTTIN': None,
                'SIGTTOU': None,
                'SIGTERM': 'terminate',
                }
        self.default_signal_map = dict(
                (getattr(self.fake_signal_module, name), target)
                for (name, target) in default_signal_map_by_name.items())

    def test_returns_constructed_signal_map(self):
        """ Should return map per default. """
        expected_result = self.default_signal_map
        result = daemon.daemon.make_default_signal_map()
        self.assertEqual(expected_result, result)

    def test_returns_signal_map_with_only_ids_in_signal_module(self):
        """ Should return map with only signals in the `signal` module.

            The `signal` module is documented to only define those
            signals which exist on the running system. Therefore the
            default map should not contain any signals which are not
            defined in the `signal` module.

            """
        del(self.default_signal_map[self.fake_signal_module.SIGTTOU])
        del(self.fake_signal_module.SIGTTOU)
        expected_result = self.default_signal_map
        result = daemon.daemon.make_default_signal_map()
        self.assertEqual(expected_result, result)


@mock.patch.object(daemon.daemon.signal, "signal")
class set_signal_handlers_TestCase(scaffold.TestCase):
    """ Test cases for set_signal_handlers function. """

    def setUp(self):
        """ Set up test fixtures. """
        super(set_signal_handlers_TestCase, self).setUp()

        self.signal_handler_map = {
                signal.SIGQUIT: object(),
                signal.SIGSEGV: object(),
                signal.SIGINT: object(),
                }

    def test_sets_signal_handler_for_each_item(self, mock_func_signal_signal):
        """ Should set signal handler for each item in map. """
        signal_handler_map = self.signal_handler_map
        expected_calls = [
                mock.call(signal_number, handler)
                for (signal_number, handler) in signal_handler_map.items()]
        daemon.daemon.set_signal_handlers(signal_handler_map)
        self.assertEquals(expected_calls, mock_func_signal_signal.mock_calls)


@mock.patch.object(daemon.daemon.atexit, "register")
class register_atexit_function_TestCase(scaffold.TestCase):
    """ Test cases for register_atexit_function function. """

    def test_registers_function_for_atexit_processing(
            self, mock_func_atexit_register):
        """ Should register specified function for atexit processing. """
        func = object()
        daemon.daemon.register_atexit_function(func)
        mock_func_atexit_register.assert_called_with(func)


# Local variables:
# coding: utf-8
# mode: python
# End:
# vim: fileencoding=utf-8 filetype=python :
