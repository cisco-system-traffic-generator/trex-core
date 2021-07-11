# -*- coding: utf-8 -*-
#
# This document is free and open-source software, subject to the OSI-approved
# BSD license below.
#
# Copyright (c) 2014 Alexis Petrounias <www.petrounias.org>,
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# * Neither the name of the author nor the names of its contributors may be used
# to endorse or promote products derived from this software without specific
# prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
Unit tests for collections
"""
from __future__ import absolute_import, division, print_function, unicode_literals
from collections import OrderedDict, defaultdict
import sys

import pytest

import jsonpickle
from jsonpickle.compat import PY2

__status__ = "Stable"
__version__ = "1.0.0"
__maintainer__ = "Alexis Petrounias <www.petrounias.org>"
__author__ = "Alexis Petrounias <www.petrounias.org>"


"""
Classes featuring various collections used by
:mod:`restorable_collections_tests` unit tests; these classes must be
module-level (including the default dictionary factory method) so that they
can be pickled.

Tests restorable collections by creating pickled structures featuring
no cycles, self cycles, and mutual cycles, for all supported dictionary and
set wrappers. Python bult-in dictionaries and sets are also tested with
expectation of failure via raising exceptions.

"""


class Group(object):
    def __init__(self, name):
        super(Group, self).__init__()
        self.name = name
        self.elements = []

    def __repr__(self):
        return 'Group({})'.format(self.name)


class C(object):
    def __init__(self, v):
        super(C, self).__init__()
        self.v = v
        self.plain = dict()
        self.plain_ordered = OrderedDict()
        self.plain_default = defaultdict(c_factory)

    def add(self, key, value):
        self.plain[key] = (key, value)
        self.plain_ordered[key] = (key, value)
        self.plain_default[key] = (key, value)

    def __hash__(self):
        # return id(self)
        # the original __hash__() below means the hash can change after being
        # stored, which JP does not support on Python 2.
        if PY2:
            return id(self)
        return hash(self.v) if hasattr(self, 'v') else id(self)

    def __repr__(self):
        return 'C({})'.format(self.v)


def c_factory():
    return (C(0), '_')


class D(object):
    def __init__(self, v):
        super(D, self).__init__()
        self.v = v
        self.plain = set()

    def add(self, item):
        self.plain.add(item)

    def __hash__(self):
        if PY2:
            return id(self)
        return hash(self.v) if hasattr(self, 'v') else id(self)

    def __repr__(self):
        return 'D({})'.format(self.v)


def pickle_and_unpickle(obj):
    encoded = jsonpickle.encode(obj, keys=True)
    return jsonpickle.decode(encoded, keys=True)


def test_dict_no_cycle():
    g = Group('group')
    c1 = C(42)
    g.elements.append(c1)
    c2 = C(67)
    g.elements.append(c2)
    c1.add(c2, 'a')  # points to c2, which does not point to anything

    assert c2 in c1.plain
    assert c2 in c1.plain_ordered
    assert c2 in c1.plain_default

    gu = pickle_and_unpickle(g)
    c1u = gu.elements[0]
    c2u = gu.elements[1]

    # check existence of keys directly
    assert c2u in c1u.plain.keys()
    assert c2u in c1u.plain_ordered.keys()
    assert c2u in c1u.plain_default.keys()

    # check direct key-based lookup
    assert c2u == c1u.plain[c2u][0]
    assert c2u == c1u.plain_ordered[c2u][0]
    assert c2u == c1u.plain_default[c2u][0]

    # check key lookup with key directly from keys()
    plain_keys = list(c1u.plain.keys())
    ordered_keys = list(c1u.plain_ordered.keys())
    default_keys = list(c1u.plain_default.keys())
    assert c2u == c1u.plain[plain_keys[0]][0]
    assert c2u == c1u.plain_ordered[ordered_keys[0]][0]
    assert c2u == c1u.plain_default[default_keys[0]][0]
    assert c2u == c1u.plain[plain_keys[0]][0]
    assert c2u == c1u.plain_ordered[ordered_keys[0]][0]
    assert c2u == c1u.plain_default[default_keys[0]][0]


def test_dict_self_cycle():
    g = Group('group')
    c1 = C(42)
    g.elements.append(c1)
    c2 = C(67)
    g.elements.append(c2)
    c1.add(c1, 'a')  # cycle to itself
    c1.add(c2, 'b')  # c2 does not point to itself nor c1

    assert c1 in c1.plain
    assert c1 in c1.plain_ordered
    assert c1 in c1.plain_default

    gu = pickle_and_unpickle(g)
    c1u = gu.elements[0]
    c2u = gu.elements[1]

    # check existence of keys directly
    # key c1u
    assert c1u in list(c1u.plain.keys())
    assert c1u in list(c1u.plain_ordered.keys())
    assert c1u in list(c1u.plain_default.keys())

    # key c2u
    assert c2u in list(c1u.plain.keys())
    assert c2u in list(c1u.plain_ordered.keys())
    assert c2u in list(c1u.plain_default.keys())

    # check direct key-based lookup

    # key c1u
    assert 42 == c1u.plain[c1u][0].v
    assert 42 == c1u.plain_ordered[c1u][0].v

    assert len(c1u.plain_default) == 2
    assert 42 == c1u.plain_default[c1u][0].v
    # No new entries were created.
    assert len(c1u.plain_default) == 2

    # key c2u
    # succeeds because c2u does not have a cycle to itself
    assert c2u == c1u.plain[c2u][0]
    # succeeds because c2u does not have a cycle to itself
    assert c2u == c1u.plain_ordered[c2u][0]
    # succeeds because c2u does not have a cycle to itself
    assert c2u == c1u.plain_default[c2u][0]
    assert c2u == c1u.plain[c2u][0]
    assert c2u == c1u.plain_ordered[c2u][0]
    assert c2u == c1u.plain_default[c2u][0]

    # check key lookup with key directly from keys()
    # key c1u
    plain_keys = list(c1u.plain.keys())
    ordered_keys = list(c1u.plain_ordered.keys())
    default_keys = list(c1u.plain_default.keys())
    if PY2:
        value = 67
    else:
        value = 42
    assert value == c1u.plain[plain_keys[0]][0].v
    42 == c1u.plain_ordered[ordered_keys[0]][0].v
    42 == c1u.plain_default[default_keys[0]][0].v

    # key c2u
    # succeeds because c2u does not have a cycle to itself
    if PY2:
        key = c1u
    else:
        key = c2u
    assert key == c1u.plain[plain_keys[1]][0]
    # succeeds because c2u does not have a cycle to itself
    assert c2u == c1u.plain_ordered[ordered_keys[1]][0]
    assert 67 == c1u.plain_default[default_keys[1]][0].v


def test_dict_mutual_cycle():
    g = Group('group')
    c1 = C(42)
    g.elements.append(c1)
    c2 = C(67)
    g.elements.append(c2)

    c1.add(c2, 'a')  # points to c2, which points to c1, forming cycle
    c2.add(c1, 'a')  # points to c1 in order to form cycle

    assert c2 in c1.plain
    assert c2 in c1.plain_ordered
    assert c2 in c1.plain_default

    assert c1 in c2.plain
    assert c1 in c2.plain_ordered
    assert c1 in c2.plain_default

    gu = pickle_and_unpickle(g)
    c1u = gu.elements[0]
    c2u = gu.elements[1]

    # check existence of keys directly
    # key c2u
    assert c2u in c1u.plain.keys()
    assert c2u in c1u.plain_ordered.keys()
    assert c2u in c1u.plain_default.keys()

    # key c1u
    assert c1u in list(c2u.plain.keys())
    assert c1u in list(c2u.plain_ordered.keys())
    assert c1u in list(c2u.plain_default.keys())

    # check direct key-based lookup
    # key c2u, succeed because c2u added to c1u after __setstate__
    assert c2u == c1u.plain[c2u][0]
    assert c2u == c1u.plain_ordered[c2u][0]
    assert c2u == c1u.plain_default[c2u][0]

    # key c1u
    assert isinstance(c2u.plain[c1u][0], C)
    assert 42 == c2u.plain[c1u][0].v
    assert isinstance(c2u.plain_ordered[c1u][0], C)

    # succeeds
    assert len(c2u.plain_default) == 1
    assert 42 == c2u.plain_default[c1u][0].v
    # Ensure that no new entry is created by verifying that the length
    # has not changed.
    assert len(c2u.plain_default) == 1

    # check key lookup with key directly from keys()

    # key c2u, succeed because c2u added to c1u after __setstate__
    plain_keys = list(c1u.plain.keys())
    ordered_keys = list(c1u.plain_ordered.keys())
    default_keys = list(c1u.plain_default.keys())
    assert c2u == c1u.plain[plain_keys[0]][0]
    assert c2u == c1u.plain_ordered[ordered_keys[0]][0]
    assert c2u == c1u.plain_default[default_keys[0]][0]

    # key c1u
    plain_keys = list(c2u.plain.keys())
    ordered_keys = list(c2u.plain_ordered.keys())
    default_keys = list(c2u.plain_default.keys())
    assert 42 == c2u.plain[plain_keys[0]][0].v
    assert 42 == c2u.plain_ordered[ordered_keys[0]][0].v
    assert 42 == c2u.plain_default[default_keys[0]][0].v
    assert isinstance(c2u.plain[plain_keys[0]], tuple)
    assert isinstance(c2u.plain_ordered[ordered_keys[0]], tuple)


def test_set_no_cycle():
    g = Group('group')
    d1 = D(42)
    g.elements.append(d1)
    d2 = D(67)
    g.elements.append(d2)
    d1.add(d2)  # d1 points to d1, d2 does not point to anything, no cycles

    assert d2 in d1.plain

    gu = pickle_and_unpickle(g)
    d1u = gu.elements[0]
    d2u = gu.elements[1]

    # check element directly
    assert d2u in d1u.plain
    # check element taken from elements
    assert list(d1u.plain)[0] in d1u.plain


def test_set_self_cycle():
    g = Group('group')
    d1 = D(42)
    g.elements.append(d1)
    d2 = D(67)
    g.elements.append(d2)
    d1.add(d1)  # cycle to itself
    d1.add(d2)  # d1 also points to d2, but d2 does not point to d1

    assert d1 in d1.plain

    gu = pickle_and_unpickle(g)
    d1u = gu.elements[0]
    d2u = gu.elements[1]
    assert d2u is not None

    # check element directly
    assert d1u in d1u.plain
    # check element taken from elements
    assert list(d1u.plain)[0] in d1u.plain
    # succeeds because d2u added to d1u after __setstate__
    assert list(d1u.plain)[1] in d1u.plain


def test_set_mutual_cycle():
    g = Group('group')
    d1 = D(42)
    g.elements.append(d1)
    d2 = D(67)
    g.elements.append(d2)
    d1.add(d2)  # points to d2, which points to d1, forming cycle
    d2.add(d1)  # points to d1 in order to form cycle

    assert d2 in d1.plain
    assert d1 in d2.plain

    gu = pickle_and_unpickle(g)
    d1u = gu.elements[0]
    d2u = gu.elements[1]

    # check element directly
    # succeeds because d2u added to d1u after __setstate__
    assert d2u in d1u.plain
    assert d1u in d2u.plain
    # check element taken from elements
    # succeeds because d2u added to d1u after __setstate__
    assert list(d1u.plain)[0] in d1u.plain
    assert list(d2u.plain)[0] in d2u.plain


if __name__ == '__main__':
    pytest.main([__file__] + sys.argv[1:])
