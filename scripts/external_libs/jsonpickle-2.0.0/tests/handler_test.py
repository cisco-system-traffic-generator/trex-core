# -*- coding: utf-8 -*-
#
# Copyright (C) 2013 Jason R. Coombs <jaraco@jaraco.com>
# All rights reserved.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution.

import doctest
import unittest

import jsonpickle
import jsonpickle.handlers


class CustomObject(object):
    "A class to be serialized by a custom handler"

    def __init__(self, name=None, creator=None):
        self.name = name
        self.creator = creator

    def __eq__(self, other):
        return self.name == other.name


class CustomA(CustomObject):
    pass


class CustomB(CustomA):
    pass


class NullHandler(jsonpickle.handlers.BaseHandler):
    def flatten(self, obj, data):
        data['name'] = obj.name
        return data

    def restore(self, obj):
        return CustomObject(obj['name'], creator=type(self))


class DecoratedBase(CustomObject):
    pass


class DecoratedChild(DecoratedBase):
    pass


@jsonpickle.handlers.register(DecoratedBase, base=True)
class DecoratedHandler(NullHandler):
    pass


class SithHandler(jsonpickle.handlers.BaseHandler):
    """(evil) serialization handler that rewrites the entire object"""

    def flatten(self, obj, data):
        data['name'] = obj.name
        data['py/object'] = 'sith.lord'
        return data


class HandlerTestCase(unittest.TestCase):
    def setUp(self):
        jsonpickle.handlers.register(CustomObject, NullHandler)

    def tearDown(self):
        jsonpickle.handlers.unregister(CustomObject)

    def roundtrip(self, ob):
        encoded = jsonpickle.encode(ob)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded, ob)
        return decoded

    def test_custom_handler(self):
        """Ensure that the custom handler is indeed used"""
        expect = CustomObject('jarjar')
        encoded = jsonpickle.encode(expect)
        actual = jsonpickle.decode(encoded)
        self.assertEqual(expect.name, actual.name)
        self.assertTrue(expect.creator is None)
        self.assertTrue(actual.creator is NullHandler)

    def test_references(self):
        """
        Ensure objects handled by a custom handler are properly dereferenced.
        """
        ob = CustomObject()
        # create a dictionary which contains several references to ob
        subject = dict(a=ob, b=ob, c=ob)
        # ensure the subject can be roundtripped
        new_subject = self.roundtrip(subject)
        self.assertEqual(new_subject['a'], new_subject['b'])
        self.assertEqual(new_subject['b'], new_subject['c'])
        self.assertTrue(new_subject['a'] is new_subject['b'])
        self.assertTrue(new_subject['b'] is new_subject['c'])

    def test_invalid_class(self):
        self.assertRaises(TypeError, jsonpickle.handlers.register, 'foo', NullHandler)

    def test_base_handler(self):
        a = CustomA('a')
        self.assertTrue(a.creator is None)
        self.assertTrue(jsonpickle.decode(jsonpickle.encode(a)).creator is None)

        b = CustomB('b')
        self.assertTrue(b.creator is None)
        self.assertTrue(jsonpickle.decode(jsonpickle.encode(b)).creator is None)

        OtherHandler = type('OtherHandler', (NullHandler,), {})
        jsonpickle.handlers.register(CustomA, OtherHandler, base=True)
        self.assertTrue(self.roundtrip(a).creator is OtherHandler)
        self.assertTrue(self.roundtrip(b).creator is OtherHandler)

        SpecializedHandler = type('SpecializedHandler', (NullHandler,), {})
        jsonpickle.handlers.register(CustomB, SpecializedHandler)
        self.assertTrue(self.roundtrip(a).creator is OtherHandler)
        self.assertTrue(self.roundtrip(b).creator is SpecializedHandler)

    def test_decorated_register(self):
        db = DecoratedBase('db')
        dc = DecoratedChild('dc')

        self.assertTrue(self.roundtrip(db).creator is DecoratedHandler)
        self.assertTrue(self.roundtrip(dc).creator is DecoratedHandler)

    def test_custom_handler_can_rewrite_everything(self):
        """Test the low-level pickling structures"""
        jsonpickle.handlers.unregister(CustomObject)
        jsonpickle.handlers.register(CustomObject, SithHandler)
        obj = CustomObject('jarjar')  # secret sith lord jarjar
        pickler = jsonpickle.pickler.Pickler()
        data = pickler.flatten(obj)
        self.assertTrue(isinstance(data, dict))
        self.assertEqual(len(data), 2)
        self.assertEqual('jarjar', data['name'])
        self.assertEqual('sith.lord', data['py/object'])


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(HandlerTestCase))
    suite.addTest(doctest.DocTestSuite(jsonpickle.handlers))
    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='suite')
