# Copyright (C) 2008 John Paulett (john -at- paulett.org)
# Copyright (C) 2009-2021 David Aguilar (davvid -at- gmail.com)
# All rights reserved.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution.
from __future__ import absolute_import, division, unicode_literals
import os
import unittest
import collections

import pytest

import jsonpickle
import jsonpickle.backend
import jsonpickle.handlers
from jsonpickle import compat, tags, util
from jsonpickle.compat import PY2, PY3
from helper import SkippableTest


class Thing(object):
    def __init__(self, name):
        self.name = name
        self.child = None

    def __repr__(self):
        return 'Thing("%s")' % self.name


class Capture(object):
    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs


class ThingWithProps(object):
    def __init__(self, name='', dogs='reliable', monkies='tricksy'):
        self.name = name
        self._critters = (('dogs', dogs), ('monkies', monkies))

    def _get_identity(self):
        keys = [self.dogs, self.monkies, self.name]
        return hash('-'.join([str(key) for key in keys]))

    identity = property(_get_identity)

    def _get_dogs(self):
        return self._critters[0][1]

    dogs = property(_get_dogs)

    def _get_monkies(self):
        return self._critters[1][1]

    monkies = property(_get_monkies)

    def __getstate__(self):
        out = dict(
            __identity__=self.identity,
            nom=self.name,
            dogs=self.dogs,
            monkies=self.monkies,
        )
        return out

    def __setstate__(self, state_dict):
        self._critters = (
            ('dogs', state_dict.get('dogs')),
            ('monkies', state_dict.get('monkies')),
        )
        self.name = state_dict.get('nom', '')
        ident = state_dict.get('__identity__')
        if ident != self.identity:
            raise ValueError('expanded object does not match originial state!')

    def __eq__(self, other):
        return self.identity == other.identity


class UserDict(dict):
    """A user class that inherits from :class:`dict`"""

    def __init__(self, **kwargs):
        dict.__init__(self, **kwargs)
        self.valid = False


class Outer(object):
    class Middle(object):
        class Inner(object):
            pass


class PicklingTestCase(unittest.TestCase):
    def setUp(self):
        self.pickler = jsonpickle.pickler.Pickler()
        self.unpickler = jsonpickle.unpickler.Unpickler()
        self.b85_pickler = jsonpickle.pickler.Pickler(use_base85=True)

    def tearDown(self):
        self.pickler.reset()
        self.unpickler.reset()

    @unittest.skipIf(not PY2, 'Python 2-specific base85 test')
    def test_base85_always_false_on_py2(self):
        self.assertFalse(self.b85_pickler.use_base85)

    @unittest.skipIf(PY2, 'Base85 not supported on Python 2')
    def test_base85_override_py3(self):
        """Ensure the Python 2 check still lets us set use_base85 on Python 3"""
        self.assertTrue(self.b85_pickler.use_base85)

    @unittest.skipIf(PY2, 'Base85 not supported on Python 2')
    def test_bytes_default_base85(self):
        data = os.urandom(16)
        encoded = util.b85encode(data)
        self.assertEqual({tags.B85: encoded}, self.b85_pickler.flatten(data))

    @unittest.skipIf(PY2, 'Base85 not supported on Python 2')
    def test_py3_bytes_base64_default(self):
        data = os.urandom(16)
        encoded = util.b64encode(data)
        self.assertEqual({tags.B64: encoded}, self.pickler.flatten(data))

    @unittest.skipIf(not PY2, 'Python 2-specific base64 test')
    def test_py2_default_base64(self):
        data = os.urandom(16)
        encoded = util.b64encode(data)
        self.assertEqual({tags.B64: encoded}, self.pickler.flatten(data))

    @unittest.skipIf(PY2, 'Base85 not supported on Python 2')
    def test_decode_base85(self):
        pickled = {tags.B85: 'P{Y4;Xv4O{u^=-c'}
        expected = u'P\u00ffth\u00f6\u00f1 3!'.encode('utf-8')
        self.assertEqual(expected, self.unpickler.restore(pickled))

    @unittest.skipIf(PY2, 'Base85 not supported on Python 2')
    def test_base85_still_handles_base64(self):
        pickled = {tags.B64: 'UMO/dGjDtsOxIDMh'}
        expected = u'P\u00ffth\u00f6\u00f1 3!'.encode('utf-8')
        self.assertEqual(expected, self.unpickler.restore(pickled))

    @unittest.skipIf(not PY2, 'Python 2-specific base85 test')
    def test_base85_crashes_py2(self):
        with self.assertRaises(NotImplementedError):
            self.unpickler.restore({tags.B85: 'P{Y4;Xv4O{u^=-c'})

    def test_string(self):
        self.assertEqual('a string', self.pickler.flatten('a string'))
        self.assertEqual('a string', self.unpickler.restore('a string'))

    def test_unicode(self):
        self.assertEqual('a string', self.pickler.flatten('a string'))
        self.assertEqual('a string', self.unpickler.restore('a string'))

    def test_int(self):
        self.assertEqual(3, self.pickler.flatten(3))
        self.assertEqual(3, self.unpickler.restore(3))

    def test_float(self):
        self.assertEqual(3.5, self.pickler.flatten(3.5))
        self.assertEqual(3.5, self.unpickler.restore(3.5))

    def test_boolean(self):
        self.assertTrue(self.pickler.flatten(True))
        self.assertFalse(self.pickler.flatten(False))
        self.assertTrue(self.unpickler.restore(True))
        self.assertFalse(self.unpickler.restore(False))

    def test_none(self):
        self.assertTrue(self.pickler.flatten(None) is None)
        self.assertTrue(self.unpickler.restore(None) is None)

    def test_list(self):
        # multiple types of values
        listA = [1, 35.0, 'value']
        self.assertEqual(listA, self.pickler.flatten(listA))
        self.assertEqual(listA, self.unpickler.restore(listA))
        # nested list
        listB = [40, 40, listA, 6]
        self.assertEqual(listB, self.pickler.flatten(listB))
        self.assertEqual(listB, self.unpickler.restore(listB))
        # 2D list
        listC = [[1, 2], [3, 4]]
        self.assertEqual(listC, self.pickler.flatten(listC))
        self.assertEqual(listC, self.unpickler.restore(listC))
        # empty list
        listD = []
        self.assertEqual(listD, self.pickler.flatten(listD))
        self.assertEqual(listD, self.unpickler.restore(listD))

    def test_set(self):
        setlist = ['orange', 'apple', 'grape']
        setA = set(setlist)

        flattened = self.pickler.flatten(setA)
        for s in setlist:
            self.assertTrue(s in flattened[tags.SET])

        setA_pickle = {tags.SET: setlist}
        self.assertEqual(setA, self.unpickler.restore(setA_pickle))

    def test_dict(self):
        dictA = {'key1': 1.0, 'key2': 20, 'key3': 'thirty', tags.JSON_KEY + '6': 6}
        self.assertEqual(dictA, self.pickler.flatten(dictA))
        self.assertEqual(dictA, self.unpickler.restore(dictA))
        dictB = {}
        self.assertEqual(dictB, self.pickler.flatten(dictB))
        self.assertEqual(dictB, self.unpickler.restore(dictB))

    def test_tuple(self):
        # currently all collections are converted to lists
        tupleA = (4, 16, 32)
        tupleA_pickle = {tags.TUPLE: [4, 16, 32]}
        self.assertEqual(tupleA_pickle, self.pickler.flatten(tupleA))
        self.assertEqual(tupleA, self.unpickler.restore(tupleA_pickle))
        tupleB = (4,)
        tupleB_pickle = {tags.TUPLE: [4]}
        self.assertEqual(tupleB_pickle, self.pickler.flatten(tupleB))
        self.assertEqual(tupleB, self.unpickler.restore(tupleB_pickle))

    def test_tuple_roundtrip(self):
        data = (1, 2, 3)
        newdata = jsonpickle.decode(jsonpickle.encode(data))
        self.assertEqual(data, newdata)

    def test_set_roundtrip(self):
        data = {1, 2, 3}
        newdata = jsonpickle.decode(jsonpickle.encode(data))
        self.assertEqual(data, newdata)

    def test_list_roundtrip(self):
        data = [1, 2, 3]
        newdata = jsonpickle.decode(jsonpickle.encode(data))
        self.assertEqual(data, newdata)

    def test_class(self):
        inst = Thing('test name')
        inst.child = Thing('child name')

        flattened = self.pickler.flatten(inst)
        self.assertEqual('test name', flattened['name'])
        child = flattened['child']
        self.assertEqual('child name', child['name'])

        inflated = self.unpickler.restore(flattened)
        self.assertEqual('test name', inflated.name)
        self.assertTrue(type(inflated) is Thing)
        self.assertEqual('child name', inflated.child.name)
        self.assertTrue(type(inflated.child) is Thing)

    def test_classlist(self):
        array = [Thing('one'), Thing('two'), 'a string']

        flattened = self.pickler.flatten(array)
        self.assertEqual('one', flattened[0]['name'])
        self.assertEqual('two', flattened[1]['name'])
        self.assertEqual('a string', flattened[2])

        inflated = self.unpickler.restore(flattened)
        self.assertEqual('one', inflated[0].name)
        self.assertTrue(type(inflated[0]) is Thing)
        self.assertEqual('two', inflated[1].name)
        self.assertTrue(type(inflated[1]) is Thing)
        self.assertEqual('a string', inflated[2])

    def test_classdict(self):
        dict = {'k1': Thing('one'), 'k2': Thing('two'), 'k3': 3}

        flattened = self.pickler.flatten(dict)
        self.assertEqual('one', flattened['k1']['name'])
        self.assertEqual('two', flattened['k2']['name'])
        self.assertEqual(3, flattened['k3'])

        inflated = self.unpickler.restore(flattened)
        self.assertEqual('one', inflated['k1'].name)
        self.assertTrue(type(inflated['k1']) is Thing)
        self.assertEqual('two', inflated['k2'].name)
        self.assertTrue(type(inflated['k2']) is Thing)
        self.assertEqual(3, inflated['k3'])

    def test_recursive(self):
        """create a recursive structure and test that we can handle it"""
        parent = Thing('parent')
        child = Thing('child')
        child.sibling = Thing('sibling')

        parent.self = parent
        parent.child = child
        parent.child.twin = child
        parent.child.parent = parent
        parent.child.sibling.parent = parent

        cloned = jsonpickle.decode(jsonpickle.encode(parent))

        self.assertEqual(parent.name, cloned.name)
        self.assertEqual(parent.child.name, cloned.child.name)
        self.assertEqual(parent.child.sibling.name, cloned.child.sibling.name)
        self.assertEqual(cloned, cloned.child.parent)
        self.assertEqual(cloned, cloned.child.sibling.parent)
        self.assertEqual(cloned, cloned.child.twin.parent)
        self.assertEqual(cloned.child, cloned.child.twin)

    def test_tuple_notunpicklable(self):
        self.pickler.unpicklable = False

        flattened = self.pickler.flatten(('one', 2, 3))
        self.assertEqual(flattened, ['one', 2, 3])

    def test_set_not_unpicklable(self):
        self.pickler.unpicklable = False

        flattened = self.pickler.flatten({'one', 2, 3})
        self.assertTrue('one' in flattened)
        self.assertTrue(2 in flattened)
        self.assertTrue(3 in flattened)
        self.assertTrue(isinstance(flattened, list))

    def test_thing_with_module(self):
        obj = Thing('with-module')
        obj.themodule = os

        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.themodule, os)

    def test_thing_with_module_safe(self):
        obj = Thing('with-module')
        obj.themodule = os
        flattened = self.pickler.flatten(obj)
        self.unpickler.safe = True
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.themodule, None)

    def test_thing_with_submodule(self):
        from distutils import sysconfig

        obj = Thing('with-submodule')
        obj.submodule = sysconfig

        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.submodule, sysconfig)

    def test_type_reference(self):
        """This test ensures that users can store references to types."""
        obj = Thing('object-with-type-reference')

        # reference the built-in 'object' type
        obj.typeref = object

        flattened = self.pickler.flatten(obj)
        self.assertEqual(flattened['typeref'], {tags.TYPE: 'builtins.object'})

        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.typeref, object)

    def test_class_reference(self):
        """This test ensures that users can store references to classes."""
        obj = Thing('object-with-class-reference')

        # reference the 'Thing' class (not an instance of the class)
        obj.classref = Thing

        flattened = self.pickler.flatten(obj)
        self.assertEqual(flattened['classref'], {tags.TYPE: 'jsonpickle_test.Thing'})

        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.classref, Thing)

    def test_supports_getstate_setstate(self):
        obj = ThingWithProps('object-which-defines-getstate-setstate')
        flattened = self.pickler.flatten(obj)
        self.assertTrue(flattened[tags.STATE].get('__identity__'))
        self.assertTrue(flattened[tags.STATE].get('nom'))
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(obj, inflated)

    def test_references(self):
        obj_a = Thing('foo')
        obj_b = Thing('bar')
        coll = [obj_a, obj_b, obj_b]
        flattened = self.pickler.flatten(coll)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(len(inflated), len(coll))
        for x in range(len(coll)):
            self.assertEqual(repr(coll[x]), repr(inflated[x]))

    def test_references_in_number_keyed_dict(self):
        """
        Make sure a dictionary with numbers as keys and objects as values
        can make the round trip.

        Because JSON must coerce integers to strings in dict keys, the sort
        order may have a tendency to change between pickling and unpickling,
        and this could affect the object references.
        """
        one = Thing('one')
        two = Thing('two')
        twelve = Thing('twelve')
        two.child = twelve
        obj = {
            1: one,
            2: two,
            12: twelve,
        }
        self.assertNotEqual(
            list(sorted(obj.keys())), list(map(int, sorted(map(str, obj.keys()))))
        )
        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(len(inflated), 3)
        self.assertEqual(inflated['12'].name, 'twelve')

    def test_builtin_error(self):
        expect = AssertionError
        json = jsonpickle.encode(expect)
        actual = jsonpickle.decode(json)
        self.assertEqual(expect, actual)
        self.assertTrue(expect is actual)

    def test_builtin_function(self):
        expect = dir
        json = jsonpickle.encode(expect)
        actual = jsonpickle.decode(json)
        self.assertEqual(expect, actual)
        self.assertTrue(expect is actual)

    def test_restore_legacy_builtins(self):
        """
        jsonpickle 0.9.6 and earlier used the Python 2 `__builtin__`
        naming for builtins. Ensure those can be loaded until they're
        no longer supported.
        """
        ae = jsonpickle.decode('{"py/type": "__builtin__.AssertionError"}')
        assert ae is AssertionError
        ae = jsonpickle.decode('{"py/type": "exceptions.AssertionError"}')
        assert ae is AssertionError
        cls = jsonpickle.decode('{"py/type": "__builtin__.int"}')
        assert cls is int


class JSONPickleTestCase(SkippableTest):
    def setUp(self):
        self.obj = Thing('A name')
        self.expected_json = (
            '{"%s": "jsonpickle_test.Thing", "name": "A name", "child": null}'
            % tags.OBJECT
        )

    def test_API_names(self):
        """
        Enforce expected names in main module
        """
        names = list(vars(jsonpickle))
        self.assertIn('pickler', names)
        self.assertIn('unpickler', names)
        self.assertIn('JSONBackend', names)
        self.assertIn('__version__', names)
        self.assertIn('register', names)
        self.assertIn('unregister', names)
        self.assertIn('Pickler', names)
        self.assertIn('Unpickler', names)
        self.assertIn('encode', names)
        self.assertIn('decode', names)

    def test_encode(self):
        expect = self.obj
        pickle = jsonpickle.encode(self.obj)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(expect.name, actual.name)
        self.assertEqual(expect.child, actual.child)

    def test_encode_notunpicklable(self):
        expect = {'name': 'A name', 'child': None}
        pickle = jsonpickle.encode(self.obj, unpicklable=False)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(expect['name'], actual['name'])

    def test_decode(self):
        actual = jsonpickle.decode(self.expected_json)
        self.assertEqual(self.obj.name, actual.name)
        self.assertEqual(type(self.obj), type(actual))

    def test_json(self):
        expect = self.obj
        pickle = jsonpickle.encode(self.obj)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(actual.name, expect.name)
        self.assertEqual(actual.child, expect.child)

        actual = jsonpickle.decode(self.expected_json)
        self.assertEqual(self.obj.name, actual.name)
        self.assertEqual(type(self.obj), type(actual))

    def test_unicode_dict_keys(self):
        if PY2:
            uni = unichr(0x1234)  # noqa
        else:
            uni = chr(0x1234)
        pickle = jsonpickle.encode({uni: uni})
        actual = jsonpickle.decode(pickle)
        self.assertTrue(uni in actual)
        self.assertEqual(actual[uni], uni)

    def test_tuple_dict_keys_default(self):
        """Test that we handle dictionaries with tuples as keys."""
        tuple_dict = {(1, 2): 3, (4, 5): {(7, 8): 9}}
        pickle = jsonpickle.encode(tuple_dict)
        expect = {'(1, 2)': 3, '(4, 5)': {'(7, 8)': 9}}
        actual = jsonpickle.decode(pickle)
        self.assertEqual(expect, actual)

        tuple_dict = {(1, 2): [1, 2]}
        pickle = jsonpickle.encode(tuple_dict)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(actual['(1, 2)'], [1, 2])

    def test_tuple_dict_keys_with_keys_enabled(self):
        """Test that we handle dictionaries with tuples as keys."""
        tuple_dict = {(1, 2): 3, (4, 5): {(7, 8): 9}}
        pickle = jsonpickle.encode(tuple_dict, keys=True)
        expect = tuple_dict
        actual = jsonpickle.decode(pickle, keys=True)
        self.assertEqual(expect, actual)

        tuple_dict = {(1, 2): [1, 2]}
        pickle = jsonpickle.encode(tuple_dict, keys=True)
        actual = jsonpickle.decode(pickle, keys=True)
        self.assertEqual(actual[(1, 2)], [1, 2])

    def test_None_dict_key_default(self):
        # We do string coercion for non-string keys so None becomes 'None'
        expect = {'null': None}
        obj = {None: None}
        pickle = jsonpickle.encode(obj)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(expect, actual)

    def test_None_dict_key_with_keys_enabled(self):
        expect = {None: None}
        obj = {None: None}
        pickle = jsonpickle.encode(obj, keys=True)
        actual = jsonpickle.decode(pickle, keys=True)
        self.assertEqual(expect, actual)

    def test_object_dict_keys(self):
        """Test that we handle random objects as keys."""
        thing = Thing('random')
        pickle = jsonpickle.encode({thing: True})
        actual = jsonpickle.decode(pickle)
        self.assertEqual(actual, {'Thing("random")': True})

    def test_int_dict_keys_defaults(self):
        int_dict = {1000: [1, 2]}
        pickle = jsonpickle.encode(int_dict)
        actual = jsonpickle.decode(pickle)
        self.assertEqual(actual['1000'], [1, 2])

    def test_int_dict_keys_with_keys_enabled(self):
        int_dict = {1000: [1, 2]}
        pickle = jsonpickle.encode(int_dict, keys=True)
        actual = jsonpickle.decode(pickle, keys=True)
        self.assertEqual(actual[1000], [1, 2])

    def test_string_key_requiring_escape_dict_keys_with_keys_enabled(self):
        json_key_dict = {tags.JSON_KEY + '6': [1, 2]}
        pickled = jsonpickle.encode(json_key_dict, keys=True)
        unpickled = jsonpickle.decode(pickled, keys=True)
        self.assertEqual(unpickled[tags.JSON_KEY + '6'], [1, 2])

    def test_string_key_not_requiring_escape_dict_keys_with_keys_enabled(self):
        """test that string keys that do not require escaping are not escaped"""
        str_dict = {'name': [1, 2]}
        pickled = jsonpickle.encode(str_dict, keys=True)
        unpickled = jsonpickle.decode(pickled)
        self.assertTrue('name' in unpickled)

    def test_dict_subclass(self):
        obj = UserDict()
        obj.valid = True
        obj.s = 'string'
        obj.d = 'd_string'
        obj['d'] = {}
        obj['s'] = 'test'
        pickle = jsonpickle.encode(obj)
        actual = jsonpickle.decode(pickle)

        self.assertEqual(type(actual), UserDict)
        self.assertTrue('d' in actual)
        self.assertTrue('s' in actual)
        self.assertTrue(hasattr(actual, 'd'))
        self.assertTrue(hasattr(actual, 's'))
        self.assertTrue(hasattr(actual, 'valid'))
        self.assertEqual(obj['d'], actual['d'])
        self.assertEqual(obj['s'], actual['s'])
        self.assertEqual(obj.d, actual.d)
        self.assertEqual(obj.s, actual.s)
        self.assertEqual(obj.valid, actual.valid)

    def test_list_of_objects(self):
        """Test that objects in lists are referenced correctly"""
        a = Thing('a')
        b = Thing('b')
        pickle = jsonpickle.encode([a, b, b])
        actual = jsonpickle.decode(pickle)
        self.assertEqual(actual[1], actual[2])
        self.assertEqual(type(actual[0]), Thing)
        self.assertEqual(actual[0].name, 'a')
        self.assertEqual(actual[1].name, 'b')
        self.assertEqual(actual[2].name, 'b')

    def test_refs_keys_values(self):
        """Test that objects in dict keys are referenced correctly"""
        j = Thing('random')
        object_dict = {j: j}
        pickle = jsonpickle.encode(object_dict, keys=True)
        actual = jsonpickle.decode(pickle, keys=True)
        self.assertEqual(list(actual.keys()), list(actual.values()))

    def test_object_keys_to_list(self):
        """Test that objects in dict values are referenced correctly"""
        j = Thing('random')
        object_dict = {j: [j, j]}
        pickle = jsonpickle.encode(object_dict, keys=True)
        actual = jsonpickle.decode(pickle, keys=True)
        obj = list(actual.keys())[0]
        self.assertEqual(j.name, obj.name)
        self.assertTrue(obj is actual[obj][0])
        self.assertTrue(obj is actual[obj][1])

    def test_refs_in_objects(self):
        """Test that objects in lists are referenced correctly"""
        a = Thing('a')
        b = Thing('b')
        pickle = jsonpickle.encode([a, b, b])
        actual = jsonpickle.decode(pickle)
        self.assertNotEqual(actual[0], actual[1])
        self.assertEqual(actual[1], actual[2])
        self.assertTrue(actual[1] is actual[2])

    def test_refs_recursive(self):
        """Test that complicated recursive refs work"""

        a = Thing('a')
        a.self_list = [Thing('0'), Thing('1'), Thing('2')]
        a.first = a.self_list[0]
        a.stuff = {a.first: a.first}
        a.morestuff = {a.self_list[1]: a.stuff}

        pickle = jsonpickle.encode(a, keys=True)
        b = jsonpickle.decode(pickle, keys=True)

        item = b.self_list[0]
        self.assertEqual(b.first, item)
        self.assertEqual(b.stuff[b.first], item)
        self.assertEqual(b.morestuff[b.self_list[1]][b.first], item)

    def test_load_backend(self):
        """Test that we can call jsonpickle.load_backend()"""
        jsonpickle.load_backend('simplejson', 'dumps', 'loads', ValueError)
        self.assertTrue(True)

    def test_set_preferred_backend_allows_magic(self):
        """Tests that we can use the pluggable backends magically"""
        backend = 'os.path'
        jsonpickle.load_backend(backend, 'split', 'join', AttributeError)
        jsonpickle.set_preferred_backend(backend)

        slash_hello, world = jsonpickle.encode('/hello/world')
        jsonpickle.remove_backend(backend)

        self.assertEqual(slash_hello, '/hello')
        self.assertEqual(world, 'world')

    def test_load_backend_submodule(self):
        """Test that we can load a submodule as a backend"""
        jsonpickle.load_backend('os.path', 'split', 'join', AttributeError)
        self.assertTrue(
            'os.path' in jsonpickle.json._backend_names
            and 'os.path' in jsonpickle.json._encoders
            and 'os.path' in jsonpickle.json._decoders
            and 'os.path' in jsonpickle.json._encoder_options
            and 'os.path' in jsonpickle.json._decoder_exceptions
        )
        jsonpickle.remove_backend('os.path')

    def _backend_is_partially_loaded(self, backend):
        """Return True if the specified backend is incomplete"""
        return (
            backend in jsonpickle.json._backend_names
            or backend in jsonpickle.json._encoders
            or backend in jsonpickle.json._decoders
            or backend in jsonpickle.json._encoder_options
            or backend in jsonpickle.json._decoder_exceptions
        )

    def test_load_backend_handles_bad_encode(self):
        """Test that we ignore bad encoders"""

        load_backend = jsonpickle.load_backend
        self.assertFalse(load_backend('os.path', 'bad!', 'split', AttributeError))
        self.assertFalse(self._backend_is_partially_loaded('os.path'))

    def test_load_backend_raises_on_bad_decode(self):
        """Test that we ignore bad decoders"""

        load_backend = jsonpickle.load_backend
        self.assertFalse(load_backend('os.path', 'join', 'bad!', AttributeError))
        self.assertFalse(self._backend_is_partially_loaded('os.path'))

    def test_load_backend_handles_bad_loads_exc(self):
        """Test that we ignore bad decoder exceptions"""

        load_backend = jsonpickle.load_backend
        self.assertFalse(load_backend('os.path', 'join', 'split', 'bad!'))
        self.assertFalse(self._backend_is_partially_loaded('os.path'))

    def test_list_item_reference(self):
        thing = Thing('parent')
        thing.child = Thing('child')
        thing.child.refs = [thing]

        encoded = jsonpickle.encode(thing)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(id(decoded.child.refs[0]), id(decoded))

    def test_reference_to_list(self):
        thing = Thing('parent')
        thing.a = [1]
        thing.b = thing.a
        thing.b.append(thing.a)
        thing.b.append([thing.a])

        encoded = jsonpickle.encode(thing)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(decoded.a[0], 1)
        self.assertEqual(decoded.b[0], 1)
        self.assertEqual(id(decoded.a), id(decoded.b))
        self.assertEqual(id(decoded.a), id(decoded.a[1]))
        self.assertEqual(id(decoded.a), id(decoded.a[2][0]))

    def test_make_refs_disabled_list(self):
        obj_a = Thing('foo')
        obj_b = Thing('bar')
        coll = [obj_a, obj_b, obj_b]
        encoded = jsonpickle.encode(coll, make_refs=False)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(len(decoded), 3)
        self.assertTrue(decoded[0] is not decoded[1])
        self.assertTrue(decoded[1] is not decoded[2])

    def test_make_refs_disabled_reference_to_list(self):
        thing = Thing('parent')
        thing.a = [1]
        thing.b = thing.a
        thing.b.append(thing.a)
        thing.b.append([thing.a])

        encoded = jsonpickle.encode(thing, make_refs=False)
        decoded = jsonpickle.decode(encoded)

        assert decoded.a[0] == 1
        assert decoded.b[0] == 1
        assert decoded.a[1][0:3] == '[1,'
        assert decoded.a[2][0][0:3] == '[1,'

    def test_can_serialize_inner_classes(self):
        class InnerScope(object):
            """Private class visible to this method only"""

            def __init__(self, name):
                self.name = name

        obj = InnerScope('test')
        encoded = jsonpickle.encode(obj)
        # Single class
        decoded = jsonpickle.decode(encoded, classes=InnerScope)
        self._test_inner_class(InnerScope, obj, decoded)
        # List of classes
        decoded = jsonpickle.decode(encoded, classes=[InnerScope])
        self._test_inner_class(InnerScope, obj, decoded)
        # Tuple of classes
        decoded = jsonpickle.decode(encoded, classes=(InnerScope,))
        self._test_inner_class(InnerScope, obj, decoded)
        # Set of classes
        decoded = jsonpickle.decode(encoded, classes={InnerScope})
        self._test_inner_class(InnerScope, obj, decoded)

    def _test_inner_class(self, InnerScope, obj, decoded):
        self.assertTrue(isinstance(obj, InnerScope))
        self.assertEqual(decoded.name, obj.name)

    def test_can_serialize_nested_classes(self):
        if PY2:
            return self.skip('Serialization of nested classes requires ' 'Python >= 3')

        middle = Outer.Middle
        inner = Outer.Middle.Inner
        encoded_middle = jsonpickle.encode(middle)
        encoded_inner = jsonpickle.encode(inner)

        decoded_middle = jsonpickle.decode(encoded_middle)
        decoded_inner = jsonpickle.decode(encoded_inner)

        self.assertTrue(isinstance(decoded_middle, type))
        self.assertTrue(isinstance(decoded_inner, type))
        self.assertEqual(decoded_middle, middle)
        self.assertEqual(decoded_inner, inner)

    def test_can_serialize_nested_class_objects(self):
        if PY2:
            return self.skip('Serialization of nested classes requires ' 'Python >= 3')

        middle_obj = Outer.Middle()
        middle_obj.attribute = 5
        inner_obj = Outer.Middle.Inner()
        inner_obj.attribute = 6
        encoded_middle_obj = jsonpickle.encode(middle_obj)
        encoded_inner_obj = jsonpickle.encode(inner_obj)

        decoded_middle_obj = jsonpickle.decode(encoded_middle_obj)
        decoded_inner_obj = jsonpickle.decode(encoded_inner_obj)

        self.assertTrue(isinstance(decoded_middle_obj, Outer.Middle))
        self.assertTrue(isinstance(decoded_inner_obj, Outer.Middle.Inner))
        self.assertEqual(decoded_middle_obj.attribute, middle_obj.attribute)
        self.assertEqual(decoded_inner_obj.attribute, inner_obj.attribute)


class PicklableNamedTuple(object):
    """
    A picklable namedtuple wrapper, to demonstrate the need
    for protocol 2 compatibility. Yes, this is contrived in
    its use of new, but it demonstrates the issue.
    """

    def __new__(cls, propnames, vals):
        # it's necessary to use the correct class name for class resolution
        # classes that fake their own names may never be unpicklable
        ntuple = collections.namedtuple(cls.__name__, propnames)
        ntuple.__getnewargs__ = lambda self: (propnames, vals)
        instance = ntuple.__new__(ntuple, *vals)
        return instance


class PicklableNamedTupleEx(object):
    """
    A picklable namedtuple wrapper, to demonstrate the need
    for protocol 4 compatibility. Yes, this is contrived in
    its use of new, but it demonstrates the issue.
    """

    def __getnewargs__(self):
        raise NotImplementedError("This class needs __getnewargs_ex__")

    def __new__(cls, newargs=__getnewargs__, **kwargs):
        # it's necessary to use the correct class name for class resolution
        # classes that fake their own names may never be unpicklable
        ntuple = collections.namedtuple(cls.__name__, sorted(kwargs.keys()))
        ntuple.__getnewargs_ex__ = lambda self: ((), kwargs)
        ntuple.__getnewargs__ = newargs
        instance = ntuple.__new__(ntuple, *[b for a, b in sorted(kwargs.items())])
        return instance


class PickleProtocol2Thing(object):
    def __init__(self, *args):
        self.args = args

    def __getnewargs__(self):
        return self.args

    def __eq__(self, other):
        """
        Make PickleProtocol2Thing('slotmagic') ==
             PickleProtocol2Thing('slotmagic')
        """
        if self.__dict__ == other.__dict__ and dir(self) == dir(other):
            for prop in dir(self):
                selfprop = getattr(self, prop)
                if not callable(selfprop) and prop[0] != '_':
                    if selfprop != getattr(other, prop):
                        return False
            return True
        else:
            return False


# these two instances are used below and in tests
slotmagic = PickleProtocol2Thing('slotmagic')
dictmagic = PickleProtocol2Thing('dictmagic')


class PickleProtocol2GetState(PickleProtocol2Thing):
    def __new__(cls, *args):
        instance = super(PickleProtocol2GetState, cls).__new__(cls)
        instance.newargs = args
        return instance

    def __getstate__(self):
        return 'I am magic'


class PickleProtocol2GetStateDict(PickleProtocol2Thing):
    def __getstate__(self):
        return {'magic': True}


class PickleProtocol2GetStateSlots(PickleProtocol2Thing):
    def __getstate__(self):
        return (None, {'slotmagic': slotmagic})


class PickleProtocol2GetStateSlotsDict(PickleProtocol2Thing):
    def __getstate__(self):
        return ({'dictmagic': dictmagic}, {'slotmagic': slotmagic})


class PickleProtocol2GetSetState(PickleProtocol2GetState):
    def __setstate__(self, state):
        """
        Contrived example, easy to test
        """
        if state == "I am magic":
            self.magic = True
        else:
            self.magic = False


class PickleProtocol2ChildThing(object):
    def __init__(self, child):
        self.child = child

    def __getnewargs__(self):
        return ([self.child],)


class PickleProtocol2ReduceString(object):
    def __reduce__(self):
        return __name__ + '.slotmagic'


class PickleProtocol2ReduceExString(object):
    def __reduce__(self):
        assert False, "Should not be here"

    def __reduce_ex__(self, n):
        return __name__ + '.slotmagic'


class PickleProtocol2ReduceTuple(object):
    def __init__(self, argval, optional=None):
        self.argval = argval
        self.optional = optional

    def __reduce__(self):
        return (
            PickleProtocol2ReduceTuple,  # callable
            ('yam', 1),  # args
            None,  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


@compat.iterator
class ReducibleIterator(object):
    def __iter__(self):
        return self

    def __next__(self):
        raise StopIteration()

    def __reduce__(self):
        return ReducibleIterator, ()


def protocol_2_reduce_tuple_func(*args):
    return PickleProtocol2ReduceTupleFunc(*args)


class PickleProtocol2ReduceTupleFunc(object):
    def __init__(self, argval, optional=None):
        self.argval = argval
        self.optional = optional

    def __reduce__(self):
        return (
            protocol_2_reduce_tuple_func,  # callable
            ('yam', 1),  # args
            None,  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


def __newobj__(lol, fail):
    """
    newobj is special-cased, such that it is not actually called
    """


class PickleProtocol2ReduceNewobj(PickleProtocol2ReduceTupleFunc):
    def __new__(cls, *args):
        inst = super(cls, cls).__new__(cls)
        inst.newargs = args
        return inst

    def __reduce__(self):
        return (
            __newobj__,  # callable
            (PickleProtocol2ReduceNewobj, 'yam', 1),  # args
            None,  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


class PickleProtocol2ReduceTupleState(PickleProtocol2ReduceTuple):
    def __reduce__(self):
        return (
            PickleProtocol2ReduceTuple,  # callable
            ('yam', 1),  # args
            {'foo': 1},  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


class PickleProtocol2ReduceTupleSetState(PickleProtocol2ReduceTuple):
    def __setstate__(self, state):
        self.bar = state['foo']

    def __reduce__(self):
        return (
            type(self),  # callable
            ('yam', 1),  # args
            {'foo': 1},  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


class PickleProtocol2ReduceTupleStateSlots(object):
    __slots__ = ('argval', 'optional', 'foo')

    def __init__(self, argval, optional=None):
        self.argval = argval
        self.optional = optional

    def __reduce__(self):
        return (
            PickleProtocol2ReduceTuple,  # callable
            ('yam', 1),  # args
            {'foo': 1},  # state
            iter([]),  # listitems
            iter([]),  # dictitems
        )


class PickleProtocol2ReduceListitemsAppend(object):
    def __init__(self):
        self.inner = []

    def __reduce__(self):
        return (
            PickleProtocol2ReduceListitemsAppend,  # callable
            (),  # args
            {},  # state
            iter(['foo', 'bar']),  # listitems
            iter([]),  # dictitems
        )

    def append(self, item):
        self.inner.append(item)


class PickleProtocol2ReduceListitemsExtend(object):
    def __init__(self):
        self.inner = []

    def __reduce__(self):
        return (
            PickleProtocol2ReduceListitemsAppend,  # callable
            (),  # args
            {},  # state
            iter(['foo', 'bar']),  # listitems
            iter([]),  # dictitems
        )

    def extend(self, items):
        self.inner.exend(items)


class PickleProtocol2ReduceDictitems(object):
    def __init__(self):
        self.inner = {}

    def __reduce__(self):
        return (
            PickleProtocol2ReduceDictitems,  # callable
            (),  # args
            {},  # state
            [],  # listitems
            iter(zip(['foo', 'bar'], ['foo', 'bar'])),  # dictitems
        )

    def __setitem__(self, k, v):
        return self.inner.__setitem__(k, v)


class PickleProtocol2Classic:
    def __init__(self, foo):
        self.foo = foo


class PickleProtocol2ClassicInitargs:
    def __init__(self, foo, bar=None):
        self.foo = foo
        if bar:
            self.bar = bar

    def __getinitargs__(self):
        return ('choo', 'choo')


class PicklingProtocol4TestCase(unittest.TestCase):
    def test_pickle_newargs_ex(self):
        """
        Ensure we can pickle and unpickle an object whose class needs arguments
        to __new__ and get back the same typle
        """
        instance = PicklableNamedTupleEx(**{'a': 'b', 'n': 2})
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(instance, decoded)

    def test_validate_reconstruct_by_newargs_ex(self):
        """
        Ensure that the exemplar tuple's __getnewargs_ex__ works
        This is necessary to know whether the breakage exists
        in jsonpickle or not
        """
        instance = PicklableNamedTupleEx(**{'a': 'b', 'n': 2})
        args, kwargs = instance.__getnewargs_ex__()
        newinstance = PicklableNamedTupleEx.__new__(
            PicklableNamedTupleEx, *args, **kwargs
        )
        self.assertEqual(instance, newinstance)

    def test_references(self):
        shared = Thing('shared')
        instance = PicklableNamedTupleEx(**{'a': shared, 'n': shared})
        child = Thing('child')
        shared.child = child
        child.child = instance

        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(decoded[0], decoded[1])
        self.assertTrue(decoded[0] is decoded[1])
        self.assertTrue(decoded.a is decoded.n)
        self.assertEqual(decoded.a.name, 'shared')
        self.assertEqual(decoded.a.child.name, 'child')
        self.assertTrue(decoded.a.child.child is decoded)
        self.assertTrue(decoded.n.child.child is decoded)
        self.assertTrue(decoded.a.child is decoded.n.child)

        self.assertEqual(decoded.__class__.__name__, PicklableNamedTupleEx.__name__)
        # TODO the class itself looks just like the real class, but it's
        # actually a reconstruction; PicklableNamedTupleEx is not type(decoded).
        self.assertFalse(decoded.__class__ is PicklableNamedTupleEx)


class PicklingProtocol2TestCase(SkippableTest):
    def test_classic_init_has_args(self):
        """
        Test unpickling a classic instance whose init takes args,
        has no __getinitargs__
        Because classic only exists under 2, skipped if PY3
        """
        if PY3:
            return self.skip('No classic classes in PY3')
        instance = PickleProtocol2Classic(3)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.foo, 3)

    def test_getinitargs(self):
        """
        Test __getinitargs__ with classic instance

        Because classic only exists under 2, skipped if PY3
        """
        if PY3:
            return self.skip('No classic classes in PY3')
        instance = PickleProtocol2ClassicInitargs(3)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.bar, 'choo')

    def test_reduce_complex_num(self):
        instance = 5j
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded, instance)

    def test_reduce_complex_zero(self):
        instance = 0j
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded, instance)

    def test_reduce_dictitems(self):
        'Test reduce with dictitems set (as a generator)'
        instance = PickleProtocol2ReduceDictitems()
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.inner, {'foo': 'foo', 'bar': 'bar'})

    def test_reduce_listitems_extend(self):
        'Test reduce with listitems set (as a generator), yielding single items'
        instance = PickleProtocol2ReduceListitemsExtend()
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.inner, ['foo', 'bar'])

    def test_reduce_listitems_append(self):
        'Test reduce with listitems set (as a generator), yielding single items'
        instance = PickleProtocol2ReduceListitemsAppend()
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.inner, ['foo', 'bar'])

    def test_reduce_state_setstate(self):
        """Test reduce with the optional state argument set,
        on an object with a __setstate__"""

        instance = PickleProtocol2ReduceTupleSetState(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.argval, 'yam')
        self.assertEqual(decoded.optional, 1)
        self.assertEqual(decoded.bar, 1)
        self.assertFalse(hasattr(decoded, 'foo'))

    def test_reduce_state_no_dict(self):
        """Test reduce with the optional state argument set,
        on an object with no __dict__, and no __setstate__"""

        instance = PickleProtocol2ReduceTupleStateSlots(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.argval, 'yam')
        self.assertEqual(decoded.optional, 1)
        self.assertEqual(decoded.foo, 1)

    def test_reduce_state_dict(self):
        """Test reduce with the optional state argument set,
        on an object with a __dict__, and no __setstate__"""

        instance = PickleProtocol2ReduceTupleState(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.argval, 'yam')
        self.assertEqual(decoded.optional, 1)
        self.assertEqual(decoded.foo, 1)

    def test_reduce_basic(self):
        """Test reduce with only callable and args"""

        instance = PickleProtocol2ReduceTuple(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.argval, 'yam')
        self.assertEqual(decoded.optional, 1)

    def test_reduce_basic_func(self):
        """Test reduce with only callable and args

        callable is a module-level function

        """
        instance = PickleProtocol2ReduceTupleFunc(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.argval, 'yam')
        self.assertEqual(decoded.optional, 1)

    def test_reduce_newobj(self):
        """Test reduce with callable called __newobj__

        ensures special-case behaviour

        """
        instance = PickleProtocol2ReduceNewobj(5)
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.newargs, ('yam', 1))

    def test_reduce_iter(self):
        instance = iter('123')
        self.assertTrue(util.is_iterator(instance))
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(next(decoded), '1')
        self.assertEqual(next(decoded), '2')
        self.assertEqual(next(decoded), '3')

    def test_reduce_iterable(self):
        """
        An reducible object should be pickleable even if
        it is also iterable.
        """
        instance = ReducibleIterator()
        self.assertTrue(util.is_iterator(instance))
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertIsInstance(decoded, ReducibleIterator)

    def test_reduce_string(self):
        """
        Ensure json pickle will accept the redirection to another object when
        __reduce__ returns a string
        """
        instance = PickleProtocol2ReduceString()
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded, slotmagic)

    def test_reduce_ex_string(self):
        """
        Ensure json pickle will accept the redirection to another object when
        __reduce_ex__ returns a string

        ALSO tests that __reduce_ex__ is called in preference to __reduce__
        """
        instance = PickleProtocol2ReduceExString()
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded, slotmagic)

    def test_pickle_newargs(self):
        """
        Ensure we can pickle and unpickle an object whose class needs arguments
        to __new__ and get back the same typle
        """
        instance = PicklableNamedTuple(('a', 'b'), (1, 2))
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(instance, decoded)

    def test_validate_reconstruct_by_newargs(self):
        """
        Ensure that the exemplar tuple's __getnewargs__ works
        This is necessary to know whether the breakage exists
        in jsonpickle or not
        """
        instance = PicklableNamedTuple(('a', 'b'), (1, 2))
        newinstance = PicklableNamedTuple.__new__(
            PicklableNamedTuple, *(instance.__getnewargs__())
        )
        self.assertEqual(instance, newinstance)

    def test_getnewargs_priority(self):
        """
        Ensure newargs are used before py/state when decoding
        (As per PEP 307, classes are not supposed to implement
        all three magic methods)
        """
        instance = PickleProtocol2GetState('whatevs')
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.newargs, ('whatevs',))

    def test_restore_dict_state(self):
        """
        Ensure that if getstate returns a dict, and there is no custom
        __setstate__, the dict is used as a source of variables to restore
        """
        instance = PickleProtocol2GetStateDict('whatevs')
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertTrue(decoded.magic)

    def test_restore_slots_state(self):
        """
        Ensure that if getstate returns a 2-tuple with a dict in the second
        position, and there is no custom __setstate__, the dict is used as a
        source of variables to restore
        """
        instance = PickleProtocol2GetStateSlots('whatevs')
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded.slotmagic.__dict__, slotmagic.__dict__)
        self.assertEqual(decoded.slotmagic, slotmagic)

    def test_restore_slots_dict_state(self):
        """
        Ensure that if getstate returns a 2-tuple with a dict in both positions,
        and there is no custom __setstate__, the dicts are used as a source of
        variables to restore
        """
        instance = PickleProtocol2GetStateSlotsDict('whatevs')
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(
            PickleProtocol2Thing('slotmagic'), PickleProtocol2Thing('slotmagic')
        )
        self.assertEqual(decoded.slotmagic.__dict__, slotmagic.__dict__)
        self.assertEqual(decoded.slotmagic, slotmagic)
        self.assertEqual(decoded.dictmagic, dictmagic)

    def test_setstate(self):
        """
        Ensure output of getstate is passed to setstate
        """
        instance = PickleProtocol2GetSetState('whatevs')
        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)
        self.assertTrue(decoded.magic)

    def test_handles_nested_objects(self):
        child = PickleProtocol2Thing(None)
        instance = PickleProtocol2Thing(child, child)

        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)

        self.assertEqual(PickleProtocol2Thing, decoded.__class__)
        self.assertEqual(PickleProtocol2Thing, decoded.args[0].__class__)
        self.assertEqual(PickleProtocol2Thing, decoded.args[1].__class__)
        self.assertTrue(decoded.args[0] is decoded.args[1])

    def test_cyclical_objects(self, use_tuple=True):
        child = Capture(None)
        instance = Capture(child, child)
        # create a cycle
        if use_tuple:
            child.args = (instance,)
        else:
            child.args = [instance]

        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)

        # Ensure the right objects were constructed
        self.assertEqual(Capture, decoded.__class__)
        self.assertEqual(Capture, decoded.args[0].__class__)
        self.assertEqual(Capture, decoded.args[1].__class__)
        self.assertEqual(Capture, decoded.args[0].args[0].__class__)
        self.assertEqual(Capture, decoded.args[1].args[0].__class__)

        # It's turtles all the way down
        self.assertEqual(
            Capture,
            decoded.args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .args[0]
            .__class__,
        )
        # Ensure that references are properly constructed
        self.assertTrue(decoded.args[0] is decoded.args[1])
        self.assertTrue(decoded is decoded.args[0].args[0])
        self.assertTrue(decoded is decoded.args[1].args[0])
        self.assertTrue(decoded.args[0] is decoded.args[0].args[0].args[0])
        self.assertTrue(decoded.args[0] is decoded.args[1].args[0].args[0])

    def test_cyclical_objects_list(self):
        self.test_cyclical_objects(use_tuple=False)

    def test_handles_cyclical_objects_in_lists(self):
        child = PickleProtocol2ChildThing(None)
        instance = PickleProtocol2ChildThing([child, child])
        child.child = instance  # create a cycle

        encoded = jsonpickle.encode(instance)
        decoded = jsonpickle.decode(encoded)

        self.assertTrue(decoded is decoded.child[0].child)
        self.assertTrue(decoded is decoded.child[1].child)

    def test_cyclical_objects_unpickleable_false(self, use_tuple=True):
        child = Capture(None)
        instance = Capture(child, child)
        # create a cycle
        if use_tuple:
            child.args = (instance,)
        else:
            child.args = [instance]
        encoded = jsonpickle.encode(instance, unpicklable=False)
        decoded = jsonpickle.decode(encoded)

        self.assertTrue(isinstance(decoded, dict))
        self.assertTrue('args' in decoded)
        self.assertTrue('kwargs' in decoded)

        # Tuple is lost via json
        args = decoded['args']
        self.assertTrue(isinstance(args, list))

        # Get the children
        self.assertEqual(len(args), 2)
        decoded_child0 = args[0]
        decoded_child1 = args[1]
        # Circular references become None
        assert decoded_child0 == {'args': [None], 'kwargs': {}}
        assert decoded_child1 == {'args': [None], 'kwargs': {}}

    def test_cyclical_objects_unpickleable_false_list(self):
        self.test_cyclical_objects_unpickleable_false(use_tuple=False)


def test_dict_references_are_preserved():
    data = {}
    actual = jsonpickle.decode(jsonpickle.encode([data, data]))
    assert isinstance(actual, list)
    assert isinstance(actual[0], dict)
    assert isinstance(actual[1], dict)
    assert actual[0] is actual[1]


def test_repeat_objects_are_expanded():
    """Ensure that all objects are present in the json output"""
    # When references are disabled we should create expanded copies
    # of any object that appears more than once in the object stream.
    alice = Thing('alice')
    bob = Thing('bob')
    alice.child = bob

    car = Thing('car')
    car.driver = alice
    car.owner = alice
    car.passengers = [alice, bob]

    pickler = jsonpickle.Pickler(make_refs=False)
    flattened = pickler.flatten(car)

    assert flattened['name'] == 'car'
    assert flattened['driver']['name'] == 'alice'
    assert flattened['owner']['name'] == 'alice'
    assert flattened['passengers'][0]['name'] == 'alice'
    assert flattened['passengers'][1]['name'] == 'bob'
    assert flattened['driver']['child']['name'] == 'bob'
    assert flattened['passengers'][0]['child']['name'] == 'bob'


if __name__ == '__main__':
    pytest.main([__file__])
