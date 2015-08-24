# -*- coding: utf-8 -*-
#
# test/test_metadata.py
# Part of ‘python-daemon’, an implementation of PEP 3143.
#
# Copyright © 2008–2015 Ben Finney <ben+python@benfinney.id.au>
#
# This is free software: you may copy, modify, and/or distribute this work
# under the terms of the Apache License, version 2.0 as published by the
# Apache Software Foundation.
# No warranty expressed or implied. See the file ‘LICENSE.ASF-2’ for details.

""" Unit test for ‘_metadata’ private module.
    """

from __future__ import (absolute_import, unicode_literals)

import sys
import errno
import re
try:
    # Python 3 standard library.
    import urllib.parse as urlparse
except ImportError:
    # Python 2 standard library.
    import urlparse
import functools
import collections
import json

import pkg_resources
import mock
import testtools.helpers
import testtools.matchers
import testscenarios

from . import scaffold
from .scaffold import (basestring, unicode)

import daemon._metadata as metadata


class HasAttribute(testtools.matchers.Matcher):
    """ A matcher to assert an object has a named attribute. """

    def __init__(self, name):
        self.attribute_name = name

    def match(self, instance):
        """ Assert the object `instance` has an attribute named `name`. """
        result = None
        if not testtools.helpers.safe_hasattr(instance, self.attribute_name):
            result = AttributeNotFoundMismatch(instance, self.attribute_name)
        return result


class AttributeNotFoundMismatch(testtools.matchers.Mismatch):
    """ The specified instance does not have the named attribute. """

    def __init__(self, instance, name):
        self.instance = instance
        self.attribute_name = name

    def describe(self):
        """ Emit a text description of this mismatch. """
        text = (
                "{instance!r}"
                " has no attribute named {name!r}").format(
                    instance=self.instance, name=self.attribute_name)
        return text


class metadata_value_TestCase(scaffold.TestCaseWithScenarios):
    """ Test cases for metadata module values. """

    expected_str_attributes = set([
            'version_installed',
            'author',
            'copyright',
            'license',
            'url',
            ])

    scenarios = [
            (name, {'attribute_name': name})
            for name in expected_str_attributes]
    for (name, params) in scenarios:
        if name == 'version_installed':
            # No duck typing, this attribute might be None.
            params['ducktype_attribute_name'] = NotImplemented
            continue
        # Expect an attribute of ‘str’ to test this value.
        params['ducktype_attribute_name'] = 'isdigit'

    def test_module_has_attribute(self):
        """ Metadata should have expected value as a module attribute. """
        self.assertThat(
                metadata, HasAttribute(self.attribute_name))

    def test_module_attribute_has_duck_type(self):
        """ Metadata value should have expected duck-typing attribute. """
        if self.ducktype_attribute_name == NotImplemented:
            self.skipTest("Can't assert this attribute's type")
        instance = getattr(metadata, self.attribute_name)
        self.assertThat(
                instance, HasAttribute(self.ducktype_attribute_name))


class parse_person_field_TestCase(
        testscenarios.WithScenarios, testtools.TestCase):
    """ Test cases for ‘get_latest_version’ function. """

    scenarios = [
            ('simple', {
                'test_person': "Foo Bar <foo.bar@example.com>",
                'expected_result': ("Foo Bar", "foo.bar@example.com"),
                }),
            ('empty', {
                'test_person': "",
                'expected_result': (None, None),
                }),
            ('none', {
                'test_person': None,
                'expected_error': TypeError,
                }),
            ('no email', {
                'test_person': "Foo Bar",
                'expected_result': ("Foo Bar", None),
                }),
            ]

    def test_returns_expected_result(self):
        """ Should return expected result. """
        if hasattr(self, 'expected_error'):
            self.assertRaises(
                    self.expected_error,
                    metadata.parse_person_field, self.test_person)
        else:
            result = metadata.parse_person_field(self.test_person)
            self.assertEqual(self.expected_result, result)


class YearRange_TestCase(scaffold.TestCaseWithScenarios):
    """ Test cases for ‘YearRange’ class. """

    scenarios = [
            ('simple', {
                'begin_year': 1970,
                'end_year': 1979,
                'expected_text': "1970–1979",
                }),
            ('same year', {
                'begin_year': 1970,
                'end_year': 1970,
                'expected_text': "1970",
                }),
            ('no end year', {
                'begin_year': 1970,
                'end_year': None,
                'expected_text': "1970",
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(YearRange_TestCase, self).setUp()

        self.test_instance = metadata.YearRange(
                self.begin_year, self.end_year)

    def test_text_representation_as_expected(self):
        """ Text representation should be as expected. """
        result = unicode(self.test_instance)
        self.assertEqual(result, self.expected_text)


FakeYearRange = collections.namedtuple('FakeYearRange', ['begin', 'end'])

@mock.patch.object(metadata, 'YearRange', new=FakeYearRange)
class make_year_range_TestCase(scaffold.TestCaseWithScenarios):
    """ Test cases for ‘make_year_range’ function. """

    scenarios = [
            ('simple', {
                'begin_year': "1970",
                'end_date': "1979-01-01",
                'expected_range': FakeYearRange(begin=1970, end=1979),
                }),
            ('same year', {
                'begin_year': "1970",
                'end_date': "1970-01-01",
                'expected_range': FakeYearRange(begin=1970, end=1970),
                }),
            ('no end year', {
                'begin_year': "1970",
                'end_date': None,
                'expected_range': FakeYearRange(begin=1970, end=None),
                }),
            ('end date UNKNOWN token', {
                'begin_year': "1970",
                'end_date': "UNKNOWN",
                'expected_range': FakeYearRange(begin=1970, end=None),
                }),
            ('end date FUTURE token', {
                'begin_year': "1970",
                'end_date': "FUTURE",
                'expected_range': FakeYearRange(begin=1970, end=None),
                }),
            ]

    def test_result_matches_expected_range(self):
        """ Result should match expected YearRange. """
        result = metadata.make_year_range(self.begin_year, self.end_date)
        self.assertEqual(result, self.expected_range)


class metadata_content_TestCase(scaffold.TestCase):
    """ Test cases for content of metadata. """

    def test_copyright_formatted_correctly(self):
        """ Copyright statement should be formatted correctly. """
        regex_pattern = (
                "Copyright © "
                "\d{4}" # four-digit year
                "(?:–\d{4})?" # optional range dash and ending four-digit year
                )
        regex_flags = re.UNICODE
        self.assertThat(
                metadata.copyright,
                testtools.matchers.MatchesRegex(regex_pattern, regex_flags))

    def test_author_formatted_correctly(self):
        """ Author information should be formatted correctly. """
        regex_pattern = (
                ".+ " # name
                "<[^>]+>" # email address, in angle brackets
                )
        regex_flags = re.UNICODE
        self.assertThat(
                metadata.author,
                testtools.matchers.MatchesRegex(regex_pattern, regex_flags))

    def test_copyright_contains_author(self):
        """ Copyright information should contain author information. """
        self.assertThat(
                metadata.copyright,
                testtools.matchers.Contains(metadata.author))

    def test_url_parses_correctly(self):
        """ Homepage URL should parse correctly. """
        result = urlparse.urlparse(metadata.url)
        self.assertIsInstance(
                result, urlparse.ParseResult,
                "URL value {url!r} did not parse correctly".format(
                    url=metadata.url))


try:
    FileNotFoundError
except NameError:
    # Python 2 uses IOError.
    FileNotFoundError = functools.partial(IOError, errno.ENOENT)

version_info_filename = "version_info.json"

def fake_func_has_metadata(testcase, resource_name):
    """ Fake the behaviour of ‘pkg_resources.Distribution.has_metadata’. """
    if (
            resource_name != testcase.expected_resource_name
            or not hasattr(testcase, 'test_version_info')):
        return False
    return True


def fake_func_get_metadata(testcase, resource_name):
    """ Fake the behaviour of ‘pkg_resources.Distribution.get_metadata’. """
    if not fake_func_has_metadata(testcase, resource_name):
        error = FileNotFoundError(resource_name)
        raise error
    content = testcase.test_version_info
    return content


def fake_func_get_distribution(testcase, distribution_name):
    """ Fake the behaviour of ‘pkg_resources.get_distribution’. """
    if distribution_name != metadata.distribution_name:
        raise pkg_resources.DistributionNotFound
    if hasattr(testcase, 'get_distribution_error'):
        raise testcase.get_distribution_error
    mock_distribution = testcase.mock_distribution
    mock_distribution.has_metadata.side_effect = functools.partial(
            fake_func_has_metadata, testcase)
    mock_distribution.get_metadata.side_effect = functools.partial(
            fake_func_get_metadata, testcase)
    return mock_distribution


@mock.patch.object(metadata, 'distribution_name', new="mock-dist")
class get_distribution_version_info_TestCase(scaffold.TestCaseWithScenarios):
    """ Test cases for ‘get_distribution_version_info’ function. """

    default_version_info = {
            'release_date': "UNKNOWN",
            'version': "UNKNOWN",
            'maintainer': "UNKNOWN",
            }

    scenarios = [
            ('version 0.0', {
                'test_version_info': json.dumps({
                    'version': "0.0",
                    }),
                'expected_version_info': {'version': "0.0"},
                }),
            ('version 1.0', {
                'test_version_info': json.dumps({
                    'version': "1.0",
                    }),
                'expected_version_info': {'version': "1.0"},
                }),
            ('file lorem_ipsum.json', {
                'version_info_filename': "lorem_ipsum.json",
                'test_version_info': json.dumps({
                    'version': "1.0",
                    }),
                'expected_version_info': {'version': "1.0"},
                }),
            ('not installed', {
                'get_distribution_error': pkg_resources.DistributionNotFound(),
                'expected_version_info': default_version_info,
                }),
            ('no version_info', {
                'expected_version_info': default_version_info,
                }),
            ]

    def setUp(self):
        """ Set up test fixtures. """
        super(get_distribution_version_info_TestCase, self).setUp()

        if hasattr(self, 'expected_resource_name'):
            self.test_args = {'filename': self.expected_resource_name}
        else:
            self.test_args = {}
            self.expected_resource_name = version_info_filename

        self.mock_distribution = mock.MagicMock()
        func_patcher_get_distribution = mock.patch.object(
                pkg_resources, 'get_distribution')
        func_patcher_get_distribution.start()
        self.addCleanup(func_patcher_get_distribution.stop)
        pkg_resources.get_distribution.side_effect = functools.partial(
                fake_func_get_distribution, self)

    def test_requests_installed_distribution(self):
        """ The package distribution should be retrieved. """
        expected_distribution_name = metadata.distribution_name
        version_info = metadata.get_distribution_version_info(**self.test_args)
        pkg_resources.get_distribution.assert_called_with(
                expected_distribution_name)

    def test_requests_specified_filename(self):
        """ The specified metadata resource name should be requested. """
        if hasattr(self, 'get_distribution_error'):
            self.skipTest("No access to distribution")
        version_info = metadata.get_distribution_version_info(**self.test_args)
        self.mock_distribution.has_metadata.assert_called_with(
                self.expected_resource_name)

    def test_result_matches_expected_items(self):
        """ The result should match the expected items. """
        version_info = metadata.get_distribution_version_info(**self.test_args)
        self.assertEqual(self.expected_version_info, version_info)


# Local variables:
# coding: utf-8
# mode: python
# End:
# vim: fileencoding=utf-8 filetype=python :
