# -*- coding: utf-8 -*-
#
# test_version.py
# Part of ‘python-daemon’, an implementation of PEP 3143.
#
# Copyright © 2008–2015 Ben Finney <ben+python@benfinney.id.au>
#
# This is free software: you may copy, modify, and/or distribute this work
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 3 of that license or any later version.
# No warranty expressed or implied. See the file ‘LICENSE.GPL-3’ for details.

""" Unit test for ‘version’ packaging module. """

from __future__ import (absolute_import, unicode_literals)

import os
import os.path
import io
import errno
import functools
import collections
import textwrap
import json
import tempfile
import distutils.dist
import distutils.cmd
import distutils.errors
import distutils.fancy_getopt
try:
    # Standard library of Python 2.7 and later.
    from io import StringIO
except ImportError:
    # Standard library of Python 2.6 and earlier.
    from StringIO import StringIO

import mock
import testtools
import testscenarios
import docutils
import docutils.writers
import docutils.nodes
import setuptools
import setuptools.command

import version

version.ensure_class_bases_begin_with(
        version.__dict__, str('VersionInfoWriter'), docutils.writers.Writer)
version.ensure_class_bases_begin_with(
        version.__dict__, str('VersionInfoTranslator'),
        docutils.nodes.SparseNodeVisitor)


def make_test_classes_for_ensure_class_bases_begin_with():
    """ Make test classes for use with ‘ensure_class_bases_begin_with’.

        :return: Mapping {`name`: `type`} of the custom types created.

        """

    class quux_metaclass(type):
        def __new__(metaclass, name, bases, namespace):
            return super(quux_metaclass, metaclass).__new__(
                    metaclass, name, bases, namespace)

    class Foo(object):
        __metaclass__ = type

    class Bar(object):
        pass

    class FooInheritingBar(Bar):
        __metaclass__ = type

    class FooWithCustomMetaclass(object):
        __metaclass__ = quux_metaclass

    result = dict(
            (name, value) for (name, value) in locals().items()
            if isinstance(value, type))

    return result

class ensure_class_bases_begin_with_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ensure_class_bases_begin_with’ function. """

    test_classes = make_test_classes_for_ensure_class_bases_begin_with()

    scenarios = [
            ('simple', {
                'test_class': test_classes['Foo'],
                'base_class': test_classes['Bar'],
                }),
            ('custom metaclass', {
                'test_class': test_classes['FooWithCustomMetaclass'],
                'base_class': test_classes['Bar'],
                'expected_metaclass': test_classes['quux_metaclass'],
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(ensure_class_bases_begin_with_TestCase, self).setUp()

        self.class_name = self.test_class.__name__
        self.test_module_namespace = {self.class_name: self.test_class}

        if not hasattr(self, 'expected_metaclass'):
            self.expected_metaclass = type

        patcher_metaclass = mock.patch.object(
            self.test_class, '__metaclass__')
        patcher_metaclass.start()
        self.addCleanup(patcher_metaclass.stop)

        self.fake_new_class = type(object)
        self.test_class.__metaclass__.return_value = (
                self.fake_new_class)

    def test_module_namespace_contains_new_class(self):
        """ Specified module namespace should have new class. """
        version.ensure_class_bases_begin_with(
                self.test_module_namespace, self.class_name, self.base_class)
        self.assertIn(self.fake_new_class, self.test_module_namespace.values())

    def test_calls_metaclass_with_expected_class_name(self):
        """ Should call the metaclass with the expected class name. """
        version.ensure_class_bases_begin_with(
                self.test_module_namespace, self.class_name, self.base_class)
        expected_class_name = self.class_name
        self.test_class.__metaclass__.assert_called_with(
                expected_class_name, mock.ANY, mock.ANY)

    def test_calls_metaclass_with_expected_bases(self):
        """ Should call the metaclass with the expected bases. """
        version.ensure_class_bases_begin_with(
                self.test_module_namespace, self.class_name, self.base_class)
        expected_bases = tuple(
                [self.base_class]
                + list(self.test_class.__bases__))
        self.test_class.__metaclass__.assert_called_with(
                mock.ANY, expected_bases, mock.ANY)

    def test_calls_metaclass_with_expected_namespace(self):
        """ Should call the metaclass with the expected class namespace. """
        version.ensure_class_bases_begin_with(
                self.test_module_namespace, self.class_name, self.base_class)
        expected_namespace = self.test_class.__dict__.copy()
        del expected_namespace['__dict__']
        self.test_class.__metaclass__.assert_called_with(
                mock.ANY, mock.ANY, expected_namespace)


class ensure_class_bases_begin_with_AlreadyHasBase_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ensure_class_bases_begin_with’ function.

        These test cases test the conditions where the class's base is
        already the specified base class.

        """

    test_classes = make_test_classes_for_ensure_class_bases_begin_with()

    scenarios = [
            ('already Bar subclass', {
                'test_class': test_classes['FooInheritingBar'],
                'base_class': test_classes['Bar'],
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(
                ensure_class_bases_begin_with_AlreadyHasBase_TestCase,
                self).setUp()

        self.class_name = self.test_class.__name__
        self.test_module_namespace = {self.class_name: self.test_class}

        patcher_metaclass = mock.patch.object(
            self.test_class, '__metaclass__')
        patcher_metaclass.start()
        self.addCleanup(patcher_metaclass.stop)

    def test_metaclass_not_called(self):
        """ Should not call metaclass to create a new type. """
        version.ensure_class_bases_begin_with(
                self.test_module_namespace, self.class_name, self.base_class)
        self.assertFalse(self.test_class.__metaclass__.called)


class VersionInfoWriter_TestCase(testtools.TestCase):
    """ Test cases for ‘VersionInfoWriter’ class. """

    def setUp(self):
        """ Set up test fixtures. """
        super(VersionInfoWriter_TestCase, self).setUp()

        self.test_instance = version.VersionInfoWriter()

    def test_declares_version_info_support(self):
        """ Should declare support for ‘version_info’. """
        instance = self.test_instance
        expected_support = "version_info"
        result = instance.supports(expected_support)
        self.assertTrue(result)


class VersionInfoWriter_translate_TestCase(testtools.TestCase):
    """ Test cases for ‘VersionInfoWriter.translate’ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(VersionInfoWriter_translate_TestCase, self).setUp()

        patcher_translator = mock.patch.object(
                version, 'VersionInfoTranslator')
        self.mock_class_translator = patcher_translator.start()
        self.addCleanup(patcher_translator.stop)
        self.mock_translator = self.mock_class_translator.return_value

        self.test_instance = version.VersionInfoWriter()
        patcher_document = mock.patch.object(
                self.test_instance, 'document')
        patcher_document.start()
        self.addCleanup(patcher_document.stop)

    def test_creates_translator_with_document(self):
        """ Should create a translator with the writer's document. """
        instance = self.test_instance
        expected_document = self.test_instance.document
        instance.translate()
        self.mock_class_translator.assert_called_with(expected_document)

    def test_calls_document_walkabout_with_translator(self):
        """ Should call document.walkabout with the translator. """
        instance = self.test_instance
        instance.translate()
        instance.document.walkabout.assert_called_with(self.mock_translator)

    def test_output_from_translator_astext(self):
        """ Should have output from translator.astext(). """
        instance = self.test_instance
        instance.translate()
        expected_output = self.mock_translator.astext.return_value
        self.assertEqual(expected_output, instance.output)


class ChangeLogEntry_TestCase(testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry’ class. """

    def setUp(self):
        """ Set up test fixtures. """
        super(ChangeLogEntry_TestCase, self).setUp()

        self.test_instance = version.ChangeLogEntry()

    def test_instantiate(self):
        """ New instance of ‘ChangeLogEntry’ should be created. """
        self.assertIsInstance(
                self.test_instance, version.ChangeLogEntry)

    def test_minimum_zero_arguments(self):
        """ Initialiser should not require any arguments. """
        instance = version.ChangeLogEntry()
        self.assertIsNot(instance, None)


class ChangeLogEntry_release_date_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry.release_date’ attribute. """

    scenarios = [
            ('default', {
                'test_args': {},
                'expected_release_date':
                    version.ChangeLogEntry.default_release_date,
                }),
            ('unknown token', {
                'test_args': {'release_date': "UNKNOWN"},
                'expected_release_date': "UNKNOWN",
                }),
            ('future token', {
                'test_args': {'release_date': "FUTURE"},
                'expected_release_date': "FUTURE",
                }),
            ('2001-01-01', {
                'test_args': {'release_date': "2001-01-01"},
                'expected_release_date': "2001-01-01",
                }),
            ('bogus', {
                'test_args': {'release_date': "b0gUs"},
                'expected_error': ValueError,
                }),
            ]

    def test_has_expected_release_date(self):
        """ Should have default `release_date` attribute. """
        if hasattr(self, 'expected_error'):
            self.assertRaises(
                    self.expected_error,
                    version.ChangeLogEntry, **self.test_args)
        else:
            instance = version.ChangeLogEntry(**self.test_args)
            self.assertEqual(self.expected_release_date, instance.release_date)


class ChangeLogEntry_version_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry.version’ attribute. """

    scenarios = [
            ('default', {
                'test_args': {},
                'expected_version':
                    version.ChangeLogEntry.default_version,
                }),
            ('unknown token', {
                'test_args': {'version': "UNKNOWN"},
                'expected_version': "UNKNOWN",
                }),
            ('0.0', {
                'test_args': {'version': "0.0"},
                'expected_version': "0.0",
                }),
            ]

    def test_has_expected_version(self):
        """ Should have default `version` attribute. """
        instance = version.ChangeLogEntry(**self.test_args)
        self.assertEqual(self.expected_version, instance.version)


class ChangeLogEntry_maintainer_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry.maintainer’ attribute. """

    scenarios = [
            ('default', {
                'test_args': {},
                'expected_maintainer': None,
                }),
            ('person', {
                'test_args': {'maintainer': "Foo Bar <foo.bar@example.org>"},
                'expected_maintainer': "Foo Bar <foo.bar@example.org>",
                }),
            ('bogus', {
                'test_args': {'maintainer': "b0gUs"},
                'expected_error': ValueError,
                }),
            ]

    def test_has_expected_maintainer(self):
        """ Should have default `maintainer` attribute. """
        if hasattr(self, 'expected_error'):
            self.assertRaises(
                    self.expected_error,
                    version.ChangeLogEntry, **self.test_args)
        else:
            instance = version.ChangeLogEntry(**self.test_args)
            self.assertEqual(self.expected_maintainer, instance.maintainer)


class ChangeLogEntry_body_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry.body’ attribute. """

    scenarios = [
            ('default', {
                'test_args': {},
                'expected_body': None,
                }),
            ('simple', {
                'test_args': {'body': "Foo bar baz."},
                'expected_body': "Foo bar baz.",
                }),
            ]

    def test_has_expected_body(self):
        """ Should have default `body` attribute. """
        instance = version.ChangeLogEntry(**self.test_args)
        self.assertEqual(self.expected_body, instance.body)


class ChangeLogEntry_as_version_info_entry_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘ChangeLogEntry.as_version_info_entry’ attribute. """

    scenarios = [
            ('default', {
                'test_args': {},
                'expected_result': collections.OrderedDict([
                    ('release_date', version.ChangeLogEntry.default_release_date),
                    ('version', version.ChangeLogEntry.default_version),
                    ('maintainer', None),
                    ('body', None),
                    ]),
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(ChangeLogEntry_as_version_info_entry_TestCase, self).setUp()

        self.test_instance = version.ChangeLogEntry(**self.test_args)

    def test_returns_result(self):
        """ Should return expected result. """
        result = self.test_instance.as_version_info_entry()
        self.assertEqual(self.expected_result, result)


def make_mock_field_node(field_name, field_body):
    """ Make a mock Docutils field node for tests. """

    mock_field_node = mock.MagicMock(
            name='field', spec=docutils.nodes.field)

    mock_field_name_node = mock.MagicMock(
            name='field_name', spec=docutils.nodes.field_name)
    mock_field_name_node.parent = mock_field_node
    mock_field_name_node.children = [field_name]

    mock_field_body_node = mock.MagicMock(
            name='field_body', spec=docutils.nodes.field_body)
    mock_field_body_node.parent = mock_field_node
    mock_field_body_node.children = [field_body]

    mock_field_node.children = [mock_field_name_node, mock_field_body_node]

    def fake_func_first_child_matching_class(node_class):
        result = None
        node_class_name = node_class.__name__
        for (index, node) in enumerate(mock_field_node.children):
            if node._mock_name == node_class_name:
                result = index
                break
        return result

    mock_field_node.first_child_matching_class.side_effect = (
            fake_func_first_child_matching_class)

    return mock_field_node


class JsonEqual(testtools.matchers.Matcher):
    """ A matcher to compare the value of JSON streams. """

    def __init__(self, expected):
        self.expected_value = expected

    def match(self, content):
        """ Assert the JSON `content` matches the `expected_content`. """
        result = None
        actual_value = json.loads(content.decode('utf-8'))
        if actual_value != self.expected_value:
            result = JsonValueMismatch(self.expected_value, actual_value)
        return result


class JsonValueMismatch(testtools.matchers.Mismatch):
    """ The specified JSON stream does not evaluate to the expected value. """

    def __init__(self, expected, actual):
        self.expected_value = expected
        self.actual_value = actual

    def describe(self):
        """ Emit a text description of this mismatch. """
        expected_json_text = json.dumps(self.expected_value, indent=4)
        actual_json_text = json.dumps(self.actual_value, indent=4)
        text = (
                "\n"
                "reference: {expected}\n"
                "actual: {actual}\n").format(
                    expected=expected_json_text, actual=actual_json_text)
        return text


class changelog_to_version_info_collection_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘changelog_to_version_info_collection’ function. """

    scenarios = [
            ('single entry', {
                'test_input': textwrap.dedent("""\
                    Version 1.0
                    ===========

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_version_info': [
                    {
                        'release_date': "2009-01-01",
                        'version': "1.0",
                        'maintainer': "Foo Bar <foo.bar@example.org>",
                        'body': "* Lorem ipsum dolor sit amet.\n",
                        },
                    ],
                }),
            ('multiple entries', {
                'test_input': textwrap.dedent("""\
                    Version 1.0
                    ===========

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.


                    Version 0.8
                    ===========

                    :Released: 2004-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Donec venenatis nisl aliquam ipsum.


                    Version 0.7.2
                    =============

                    :Released: 2001-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Pellentesque elementum mollis finibus.
                    """),
                'expected_version_info': [
                    {
                        'release_date': "2009-01-01",
                        'version': "1.0",
                        'maintainer': "Foo Bar <foo.bar@example.org>",
                        'body': "* Lorem ipsum dolor sit amet.\n",
                        },
                    {
                        'release_date': "2004-01-01",
                        'version': "0.8",
                        'maintainer': "Foo Bar <foo.bar@example.org>",
                        'body': "* Donec venenatis nisl aliquam ipsum.\n",
                        },
                    {
                        'release_date': "2001-01-01",
                        'version': "0.7.2",
                        'maintainer': "Foo Bar <foo.bar@example.org>",
                        'body': "* Pellentesque elementum mollis finibus.\n",
                        },
                    ],
                }),
            ('trailing comment', {
                'test_input': textwrap.dedent("""\
                    Version NEXT
                    ============

                    :Released: FUTURE
                    :Maintainer:

                    * Lorem ipsum dolor sit amet.

                    ..
                        Vivamus aliquam felis rutrum rutrum dictum.
                    """),
                'expected_version_info': [
                    {
                        'release_date': "FUTURE",
                        'version': "NEXT",
                        'maintainer': "",
                        'body': "* Lorem ipsum dolor sit amet.\n",
                        },
                    ],
                }),
            ('inline comment', {
                'test_input': textwrap.dedent("""\
                    Version NEXT
                    ============

                    :Released: FUTURE
                    :Maintainer:

                    ..
                        Vivamus aliquam felis rutrum rutrum dictum.

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_version_info': [
                    {
                        'release_date': "FUTURE",
                        'version': "NEXT",
                        'maintainer': "",
                        'body': "* Lorem ipsum dolor sit amet.\n",
                        },
                    ],
                }),
            ('unreleased entry', {
                'test_input': textwrap.dedent("""\
                    Version NEXT
                    ============

                    :Released: FUTURE
                    :Maintainer:

                    * Lorem ipsum dolor sit amet.


                    Version 0.8
                    ===========

                    :Released: 2001-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Donec venenatis nisl aliquam ipsum.
                    """),
                'expected_version_info': [
                    {
                        'release_date': "FUTURE",
                        'version': "NEXT",
                        'maintainer': "",
                        'body': "* Lorem ipsum dolor sit amet.\n",
                        },
                    {
                        'release_date': "2001-01-01",
                        'version': "0.8",
                        'maintainer': "Foo Bar <foo.bar@example.org>",
                        'body': "* Donec venenatis nisl aliquam ipsum.\n",
                        },
                    ],
                }),
            ('no section', {
                'test_input': textwrap.dedent("""\
                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_error': version.InvalidFormatError,
                }),
            ('subsection', {
                'test_input': textwrap.dedent("""\
                    Version 1.0
                    ===========

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.

                    Ut ultricies fermentum quam
                    ---------------------------

                    * In commodo magna facilisis in.
                    """),
                'expected_error': version.InvalidFormatError,
                'subsection': True,
                }),
            ('unknown field', {
                'test_input': textwrap.dedent("""\
                    Version 1.0
                    ===========

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>
                    :Favourite: Spam

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_error': version.InvalidFormatError,
                }),
            ('invalid version word', {
                'test_input': textwrap.dedent("""\
                    BoGuS 1.0
                    =========

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_error': version.InvalidFormatError,
                }),
            ('invalid section title', {
                'test_input': textwrap.dedent("""\
                    Lorem Ipsum 1.0
                    ===============

                    :Released: 2009-01-01
                    :Maintainer: Foo Bar <foo.bar@example.org>

                    * Lorem ipsum dolor sit amet.
                    """),
                'expected_error': version.InvalidFormatError,
                }),
            ]

    def test_returns_expected_version_info(self):
        """ Should return expected version info mapping. """
        infile = StringIO(self.test_input)
        if hasattr(self, 'expected_error'):
            self.assertRaises(
                    self.expected_error,
                    version.changelog_to_version_info_collection, infile)
        else:
            result = version.changelog_to_version_info_collection(infile)
            self.assertThat(result, JsonEqual(self.expected_version_info))


try:
    FileNotFoundError
    PermissionError
except NameError:
    # Python 2 uses OSError.
    FileNotFoundError = functools.partial(IOError, errno.ENOENT)
    PermissionError = functools.partial(IOError, errno.EPERM)

fake_version_info = {
        'release_date': "2001-01-01", 'version': "2.0",
        'maintainer': None, 'body': None,
        }

@mock.patch.object(
        version, "get_latest_version", return_value=fake_version_info)
class generate_version_info_from_changelog_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘generate_version_info_from_changelog’ function. """

    fake_open_side_effects = {
            'success': (
                lambda *args, **kwargs: StringIO()),
            'file not found': FileNotFoundError(),
            'permission denied': PermissionError(),
            }

    scenarios = [
            ('simple', {
                'open_scenario': 'success',
                'fake_versions_json': json.dumps([fake_version_info]),
                'expected_result': fake_version_info,
                }),
            ('file not found', {
                'open_scenario': 'file not found',
                'expected_result': {},
                }),
            ('permission denied', {
                'open_scenario': 'permission denied',
                'expected_result': {},
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(generate_version_info_from_changelog_TestCase, self).setUp()

        self.fake_changelog_file_path = tempfile.mktemp()

        def fake_open(filespec, *args, **kwargs):
            if filespec == self.fake_changelog_file_path:
                side_effect = self.fake_open_side_effects[self.open_scenario]
                if callable(side_effect):
                    result = side_effect()
                else:
                    raise side_effect
            else:
                result = StringIO()
            return result

        func_patcher_io_open = mock.patch.object(
                io, "open")
        func_patcher_io_open.start()
        self.addCleanup(func_patcher_io_open.stop)
        io.open.side_effect = fake_open

        self.file_encoding = "utf-8"

        func_patcher_changelog_to_version_info_collection = mock.patch.object(
                version, "changelog_to_version_info_collection")
        func_patcher_changelog_to_version_info_collection.start()
        self.addCleanup(func_patcher_changelog_to_version_info_collection.stop)
        if hasattr(self, 'fake_versions_json'):
            version.changelog_to_version_info_collection.return_value = (
                    self.fake_versions_json.encode(self.file_encoding))

    def test_returns_empty_collection_on_read_error(
            self,
            mock_func_get_latest_version):
        """ Should return empty collection on error reading changelog. """
        test_error = PermissionError("Not for you")
        version.changelog_to_version_info_collection.side_effect = test_error
        result = version.generate_version_info_from_changelog(
                self.fake_changelog_file_path)
        expected_result = {}
        self.assertDictEqual(expected_result, result)

    def test_opens_file_with_expected_encoding(
            self,
            mock_func_get_latest_version):
        """ Should open changelog file in text mode with expected encoding. """
        result = version.generate_version_info_from_changelog(
                self.fake_changelog_file_path)
        expected_file_path = self.fake_changelog_file_path
        expected_open_mode = 'rt'
        expected_encoding = self.file_encoding
        (open_args_positional, open_args_kwargs) = io.open.call_args
        (open_args_filespec, open_args_mode) = open_args_positional[:2]
        open_args_encoding = open_args_kwargs['encoding']
        self.assertEqual(expected_file_path, open_args_filespec)
        self.assertEqual(expected_open_mode, open_args_mode)
        self.assertEqual(expected_encoding, open_args_encoding)

    def test_returns_expected_result(
            self,
            mock_func_get_latest_version):
        """ Should return expected result. """
        result = version.generate_version_info_from_changelog(
                self.fake_changelog_file_path)
        self.assertEqual(self.expected_result, result)


DefaultNoneDict = functools.partial(collections.defaultdict, lambda: None)

class get_latest_version_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘get_latest_version’ function. """

    scenarios = [
            ('simple', {
                'test_versions': [
                    DefaultNoneDict({'release_date': "LATEST"}),
                    ],
                'expected_result': version.ChangeLogEntry.make_ordered_dict(
                    DefaultNoneDict({'release_date': "LATEST"})),
                }),
            ('no versions', {
                'test_versions': [],
                'expected_result': collections.OrderedDict(),
                }),
            ('ordered versions', {
                'test_versions': [
                    DefaultNoneDict({'release_date': "1"}),
                    DefaultNoneDict({'release_date': "2"}),
                    DefaultNoneDict({'release_date': "LATEST"}),
                    ],
                'expected_result': version.ChangeLogEntry.make_ordered_dict(
                    DefaultNoneDict({'release_date': "LATEST"})),
                }),
            ('un-ordered versions', {
                'test_versions': [
                    DefaultNoneDict({'release_date': "2"}),
                    DefaultNoneDict({'release_date': "LATEST"}),
                    DefaultNoneDict({'release_date': "1"}),
                    ],
                'expected_result': version.ChangeLogEntry.make_ordered_dict(
                    DefaultNoneDict({'release_date': "LATEST"})),
                }),
            ]

    def test_returns_expected_result(self):
        """ Should return expected result. """
        result = version.get_latest_version(self.test_versions)
        self.assertDictEqual(self.expected_result, result)


@mock.patch.object(json, "dumps", side_effect=json.dumps)
class serialise_version_info_from_mapping_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘get_latest_version’ function. """

    scenarios = [
            ('simple', {
                'test_version_info': {'foo': "spam"},
                }),
            ]

    for (name, scenario) in scenarios:
        scenario['fake_json_dump'] = json.dumps(scenario['test_version_info'])
        scenario['expected_value'] = scenario['test_version_info']

    def test_passes_specified_object(self, mock_func_json_dumps):
        """ Should pass the specified object to `json.dumps`. """
        result = version.serialise_version_info_from_mapping(
                self.test_version_info)
        mock_func_json_dumps.assert_called_with(
                self.test_version_info, indent=mock.ANY)

    def test_returns_expected_result(self, mock_func_json_dumps):
        """ Should return expected result. """
        mock_func_json_dumps.return_value = self.fake_json_dump
        result = version.serialise_version_info_from_mapping(
                self.test_version_info)
        value = json.loads(result)
        self.assertEqual(self.expected_value, value)


DistributionMetadata_defaults = {
        name: None
        for name in list(collections.OrderedDict.fromkeys(
            distutils.dist.DistributionMetadata._METHOD_BASENAMES))}
FakeDistributionMetadata = collections.namedtuple(
        'FakeDistributionMetadata', DistributionMetadata_defaults.keys())

Distribution_defaults = {
        'metadata': None,
        'version': None,
        'release_date': None,
        'maintainer': None,
        'maintainer_email': None,
        }
FakeDistribution = collections.namedtuple(
        'FakeDistribution', Distribution_defaults.keys())

def make_fake_distribution(
        fields_override=None, metadata_fields_override=None):
    metadata_fields = DistributionMetadata_defaults.copy()
    if metadata_fields_override is not None:
        metadata_fields.update(metadata_fields_override)
    metadata = FakeDistributionMetadata(**metadata_fields)

    fields = Distribution_defaults.copy()
    fields['metadata'] = metadata
    if fields_override is not None:
        fields.update(fields_override)
    distribution = FakeDistribution(**fields)

    return distribution


class get_changelog_path_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘get_changelog_path’ function. """

    default_path = "."
    default_script_filename = "setup.py"

    scenarios = [
            ('simple', {}),
            ('unusual script name', {
                'script_filename': "lorem_ipsum",
                }),
            ('relative script path', {
                'script_directory': "dolor/sit/amet",
                }),
            ('absolute script path', {
                'script_directory': "/dolor/sit/amet",
                }),
            ('specify filename', {
                'changelog_filename': "adipiscing",
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(get_changelog_path_TestCase, self).setUp()

        self.test_distribution = mock.MagicMock(distutils.dist.Distribution)

        if not hasattr(self, 'script_directory'):
            self.script_directory = self.default_path
        if not hasattr(self, 'script_filename'):
            self.script_filename = self.default_script_filename
        self.test_distribution.script_name = os.path.join(
                self.script_directory, self.script_filename)

        changelog_filename = version.changelog_filename
        if hasattr(self, 'changelog_filename'):
            changelog_filename = self.changelog_filename

        self.expected_result = os.path.join(
                self.script_directory, changelog_filename)

    def test_returns_expected_result(self):
        """ Should return expected result. """
        args = {
                'distribution': self.test_distribution,
                }
        if hasattr(self, 'changelog_filename'):
            args.update({'filename': self.changelog_filename})
        result = version.get_changelog_path(**args)
        self.assertEqual(self.expected_result, result)


class WriteVersionInfoCommand_BaseTestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Base class for ‘WriteVersionInfoCommand’ test case classes. """

    def setUp(self):
        """ Set up test fixtures. """
        super(WriteVersionInfoCommand_BaseTestCase, self).setUp()

        fake_distribution_name = self.getUniqueString()

        self.test_distribution = distutils.dist.Distribution()
        self.test_distribution.metadata.name = fake_distribution_name


class WriteVersionInfoCommand_TestCase(WriteVersionInfoCommand_BaseTestCase):
    """ Test cases for ‘WriteVersionInfoCommand’ class. """

    def test_subclass_of_distutils_command(self):
        """ Should be a subclass of ‘distutils.cmd.Command’. """
        instance = version.WriteVersionInfoCommand(self.test_distribution)
        self.assertIsInstance(instance, distutils.cmd.Command)


class WriteVersionInfoCommand_user_options_TestCase(
        WriteVersionInfoCommand_BaseTestCase):
    """ Test cases for ‘WriteVersionInfoCommand.user_options’ attribute. """

    def setUp(self):
        """ Set up test fixtures. """
        super(WriteVersionInfoCommand_user_options_TestCase, self).setUp()

        self.test_instance = version.WriteVersionInfoCommand(
                self.test_distribution)
        self.commandline_parser = distutils.fancy_getopt.FancyGetopt(
                self.test_instance.user_options)

    def test_parses_correctly_as_fancy_getopt(self):
        """ Should parse correctly in ‘FancyGetopt’. """
        self.assertIsInstance(
                self.commandline_parser, distutils.fancy_getopt.FancyGetopt)

    def test_includes_base_class_user_options(self):
        """ Should include base class's user_options. """
        base_command = setuptools.command.egg_info.egg_info
        expected_user_options = base_command.user_options
        self.assertThat(
                set(expected_user_options),
                IsSubset(set(self.test_instance.user_options)))

    def test_has_option_changelog_path(self):
        """ Should have a ‘changelog-path’ option. """
        expected_option_name = "changelog-path="
        result = self.commandline_parser.has_option(expected_option_name)
        self.assertTrue(result)

    def test_has_option_outfile_path(self):
        """ Should have a ‘outfile-path’ option. """
        expected_option_name = "outfile-path="
        result = self.commandline_parser.has_option(expected_option_name)
        self.assertTrue(result)


class WriteVersionInfoCommand_initialize_options_TestCase(
        WriteVersionInfoCommand_BaseTestCase):
    """ Test cases for ‘WriteVersionInfoCommand.initialize_options’ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(
                WriteVersionInfoCommand_initialize_options_TestCase, self
                ).setUp()

        patcher_func_egg_info_initialize_options = mock.patch.object(
                setuptools.command.egg_info.egg_info, "initialize_options")
        patcher_func_egg_info_initialize_options.start()
        self.addCleanup(patcher_func_egg_info_initialize_options.stop)

    def test_calls_base_class_method(self):
        """ Should call base class's ‘initialize_options’ method. """
        instance = version.WriteVersionInfoCommand(self.test_distribution)
        base_command_class = setuptools.command.egg_info.egg_info
        base_command_class.initialize_options.assert_called_with()

    def test_sets_changelog_path_to_none(self):
        """ Should set ‘changelog_path’ attribute to ``None``. """
        instance = version.WriteVersionInfoCommand(self.test_distribution)
        self.assertIs(instance.changelog_path, None)

    def test_sets_outfile_path_to_none(self):
        """ Should set ‘outfile_path’ attribute to ``None``. """
        instance = version.WriteVersionInfoCommand(self.test_distribution)
        self.assertIs(instance.outfile_path, None)


class WriteVersionInfoCommand_finalize_options_TestCase(
        WriteVersionInfoCommand_BaseTestCase):
    """ Test cases for ‘WriteVersionInfoCommand.finalize_options’ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(WriteVersionInfoCommand_finalize_options_TestCase, self).setUp()

        self.test_instance = version.WriteVersionInfoCommand(self.test_distribution)

        patcher_func_egg_info_finalize_options = mock.patch.object(
                setuptools.command.egg_info.egg_info, "finalize_options")
        patcher_func_egg_info_finalize_options.start()
        self.addCleanup(patcher_func_egg_info_finalize_options.stop)

        self.fake_script_dir = self.getUniqueString()
        self.test_distribution.script_name = os.path.join(
                self.fake_script_dir, self.getUniqueString())

        self.fake_egg_dir = self.getUniqueString()
        self.test_instance.egg_info = self.fake_egg_dir

        patcher_func_get_changelog_path = mock.patch.object(
                version, "get_changelog_path")
        patcher_func_get_changelog_path.start()
        self.addCleanup(patcher_func_get_changelog_path.stop)

        self.fake_changelog_path = self.getUniqueString()
        version.get_changelog_path.return_value = self.fake_changelog_path

    def test_calls_base_class_method(self):
        """ Should call base class's ‘finalize_options’ method. """
        base_command_class = setuptools.command.egg_info.egg_info
        self.test_instance.finalize_options()
        base_command_class.finalize_options.assert_called_with()

    def test_sets_force_to_none(self):
        """ Should set ‘force’ attribute to ``None``. """
        self.test_instance.finalize_options()
        self.assertIs(self.test_instance.force, None)

    def test_sets_changelog_path_using_get_changelog_path(self):
        """ Should set ‘changelog_path’ attribute if it was ``None``. """
        self.test_instance.changelog_path = None
        self.test_instance.finalize_options()
        expected_changelog_path = self.fake_changelog_path
        self.assertEqual(expected_changelog_path, self.test_instance.changelog_path)

    def test_leaves_changelog_path_if_already_set(self):
        """ Should leave ‘changelog_path’ attribute set. """
        prior_changelog_path = self.getUniqueString()
        self.test_instance.changelog_path = prior_changelog_path
        self.test_instance.finalize_options()
        expected_changelog_path = prior_changelog_path
        self.assertEqual(expected_changelog_path, self.test_instance.changelog_path)

    def test_sets_outfile_path_to_default(self):
        """ Should set ‘outfile_path’ attribute to default value. """
        fake_version_info_filename = self.getUniqueString()
        with mock.patch.object(
                version, "version_info_filename",
                new=fake_version_info_filename):
            self.test_instance.finalize_options()
        expected_outfile_path = os.path.join(
                self.fake_egg_dir, fake_version_info_filename)
        self.assertEqual(expected_outfile_path, self.test_instance.outfile_path)

    def test_leaves_outfile_path_if_already_set(self):
        """ Should leave ‘outfile_path’ attribute set. """
        prior_outfile_path = self.getUniqueString()
        self.test_instance.outfile_path = prior_outfile_path
        self.test_instance.finalize_options()
        expected_outfile_path = prior_outfile_path
        self.assertEqual(expected_outfile_path, self.test_instance.outfile_path)


class has_changelog_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘has_changelog’ function. """

    fake_os_path_exists_side_effects = {
            'true': (lambda path: True),
            'false': (lambda path: False),
            }

    scenarios = [
            ('no changelog path', {
                'changelog_path': None,
                'expected_result': False,
                }),
            ('changelog exists', {
                'os_path_exists_scenario': 'true',
                'expected_result': True,
                }),
            ('changelog not found', {
                'os_path_exists_scenario': 'false',
                'expected_result': False,
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(has_changelog_TestCase, self).setUp()

        self.test_distribution = distutils.dist.Distribution()
        self.test_command = version.EggInfoCommand(
                self.test_distribution)

        patcher_func_get_changelog_path = mock.patch.object(
                version, "get_changelog_path")
        patcher_func_get_changelog_path.start()
        self.addCleanup(patcher_func_get_changelog_path.stop)

        self.fake_changelog_file_path = self.getUniqueString()
        if hasattr(self, 'changelog_path'):
            self.fake_changelog_file_path = self.changelog_path
        version.get_changelog_path.return_value = self.fake_changelog_file_path
        self.fake_changelog_file = StringIO()

        def fake_os_path_exists(path):
            if path == self.fake_changelog_file_path:
                side_effect = self.fake_os_path_exists_side_effects[
                        self.os_path_exists_scenario]
                if callable(side_effect):
                    result = side_effect(path)
                else:
                    raise side_effect
            else:
                result = False
            return result

        func_patcher_os_path_exists = mock.patch.object(
                os.path, "exists")
        func_patcher_os_path_exists.start()
        self.addCleanup(func_patcher_os_path_exists.stop)
        os.path.exists.side_effect = fake_os_path_exists

    def test_gets_changelog_path_from_distribution(self):
        """ Should call ‘get_changelog_path’ with distribution. """
        result = version.has_changelog(self.test_command)
        version.get_changelog_path.assert_called_with(
                self.test_distribution)

    def test_returns_expected_result(self):
        """ Should be a subclass of ‘distutils.cmd.Command’. """
        result = version.has_changelog(self.test_command)
        self.assertEqual(self.expected_result, result)


@mock.patch.object(version, 'generate_version_info_from_changelog')
@mock.patch.object(version, 'serialise_version_info_from_mapping')
@mock.patch.object(version.EggInfoCommand, "write_file")
class WriteVersionInfoCommand_run_TestCase(
        WriteVersionInfoCommand_BaseTestCase):
    """ Test cases for ‘WriteVersionInfoCommand.run’ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(WriteVersionInfoCommand_run_TestCase, self).setUp()

        self.test_instance = version.WriteVersionInfoCommand(
                self.test_distribution)

        self.fake_changelog_path = self.getUniqueString()
        self.test_instance.changelog_path = self.fake_changelog_path

        self.fake_outfile_path = self.getUniqueString()
        self.test_instance.outfile_path = self.fake_outfile_path

    def test_returns_none(
            self,
            mock_func_egg_info_write_file,
            mock_func_serialise_version_info,
            mock_func_generate_version_info):
        """ Should return ``None``. """
        result = self.test_instance.run()
        self.assertIs(result, None)

    def test_generates_version_info_from_changelog(
            self,
            mock_func_egg_info_write_file,
            mock_func_serialise_version_info,
            mock_func_generate_version_info):
        """ Should generate version info from specified changelog. """
        self.test_instance.run()
        expected_changelog_path = self.test_instance.changelog_path
        mock_func_generate_version_info.assert_called_with(
                expected_changelog_path)

    def test_serialises_version_info_from_mapping(
            self,
            mock_func_egg_info_write_file,
            mock_func_serialise_version_info,
            mock_func_generate_version_info):
        """ Should serialise version info from specified mapping. """
        self.test_instance.run()
        expected_version_info = mock_func_generate_version_info.return_value
        mock_func_serialise_version_info.assert_called_with(
                expected_version_info)

    def test_writes_file_using_command_context(
            self,
            mock_func_egg_info_write_file,
            mock_func_serialise_version_info,
            mock_func_generate_version_info):
        """ Should write the metadata file using the command context. """
        self.test_instance.run()
        expected_content = mock_func_serialise_version_info.return_value
        mock_func_egg_info_write_file.assert_called_with(
                "version info", self.fake_outfile_path, expected_content)


IsSubset = testtools.matchers.MatchesPredicateWithParams(
        set.issubset, "{0} should be a subset of {1}")

class EggInfoCommand_TestCase(testtools.TestCase):
    """ Test cases for ‘EggInfoCommand’ class. """

    def setUp(self):
        """ Set up test fixtures. """
        super(EggInfoCommand_TestCase, self).setUp()

        self.test_distribution = distutils.dist.Distribution()
        self.test_instance = version.EggInfoCommand(self.test_distribution)

    def test_subclass_of_setuptools_egg_info(self):
        """ Should be a subclass of Setuptools ‘egg_info’. """
        self.assertIsInstance(
                self.test_instance, setuptools.command.egg_info.egg_info)

    def test_sub_commands_include_base_class_sub_commands(self):
        """ Should include base class's sub-commands in this sub_commands. """
        base_command = setuptools.command.egg_info.egg_info
        expected_sub_commands = base_command.sub_commands
        self.assertThat(
                set(expected_sub_commands),
                IsSubset(set(self.test_instance.sub_commands)))

    def test_sub_commands_includes_write_version_info_command(self):
        """ Should include sub-command named ‘write_version_info’. """
        commands_by_name = dict(self.test_instance.sub_commands)
        expected_predicate = version.has_changelog
        expected_item = ('write_version_info', expected_predicate)
        self.assertIn(expected_item, commands_by_name.items())


@mock.patch.object(setuptools.command.egg_info.egg_info, "run")
class EggInfoCommand_run_TestCase(testtools.TestCase):
    """ Test cases for ‘EggInfoCommand.run’ method. """

    def setUp(self):
        """ Set up test fixtures. """
        super(EggInfoCommand_run_TestCase, self).setUp()

        self.test_distribution = distutils.dist.Distribution()
        self.test_instance = version.EggInfoCommand(self.test_distribution)

        base_command = setuptools.command.egg_info.egg_info
        patcher_func_egg_info_get_sub_commands = mock.patch.object(
                base_command, "get_sub_commands")
        patcher_func_egg_info_get_sub_commands.start()
        self.addCleanup(patcher_func_egg_info_get_sub_commands.stop)

        patcher_func_egg_info_run_command = mock.patch.object(
                base_command, "run_command")
        patcher_func_egg_info_run_command.start()
        self.addCleanup(patcher_func_egg_info_run_command.stop)

        self.fake_sub_commands = ["spam", "eggs", "beans"]
        base_command.get_sub_commands.return_value = self.fake_sub_commands

    def test_returns_none(self, mock_func_egg_info_run):
        """ Should return ``None``. """
        result = self.test_instance.run()
        self.assertIs(result, None)

    def test_runs_each_command_in_sub_commands(
            self, mock_func_egg_info_run):
        """ Should run each command in ‘self.get_sub_commands()’. """
        base_command = setuptools.command.egg_info.egg_info
        self.test_instance.run()
        expected_calls = [mock.call(name) for name in self.fake_sub_commands]
        base_command.run_command.assert_has_calls(expected_calls)

    def test_calls_base_class_run(self, mock_func_egg_info_run):
        """ Should call base class's ‘run’ method. """
        result = self.test_instance.run()
        mock_func_egg_info_run.assert_called_with()


# Local variables:
# coding: utf-8
# mode: python
# End:
# vim: fileencoding=utf-8 filetype=python :
