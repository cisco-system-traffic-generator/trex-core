# Copyright (C) 2008 John Paulett (john -at- paulett.org)
# Copyright (C) 2009-2018 David Aguilar (davvid -at- gmail.com)
# All rights reserved.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution.

from __future__ import absolute_import, division, unicode_literals
import unittest
import doctest
import time

from jsonpickle import compat
from jsonpickle import util


class Thing(object):
    def __init__(self, name):
        self.name = name
        self.child = None


class DictSubclass(dict):
    pass


class ListSubclass(list):
    pass


class MethodTestClass(object):
    @staticmethod
    def static_method():
        pass

    @classmethod
    def class_method(cls):
        pass

    def bound_method(self):
        pass

    variable = None


class MethodTestSubclass(MethodTestClass):
    pass


class MethodTestOldStyle:
    def bound_method(self):
        pass


class UtilTestCase(unittest.TestCase):
    @unittest.skipIf(not compat.PY2, 'Python 2-specific Base85 test')
    def test_b85encode_crashes_on_python2(self):
        with self.assertRaises(NotImplementedError):
            util.b85encode(b'')

    @unittest.skipIf(not compat.PY2, 'Python 2-specific Base85 test')
    def test_b85decode_crashes_on_python2(self):
        with self.assertRaises(NotImplementedError):
            util.b85decode(u'RC2?pb0AN3baKO~')

    def test_is_primitive_int(self):
        self.assertTrue(util.is_primitive(0))
        self.assertTrue(util.is_primitive(3))
        self.assertTrue(util.is_primitive(-3))

    def test_is_primitive_float(self):
        self.assertTrue(util.is_primitive(0))
        self.assertTrue(util.is_primitive(3.5))
        self.assertTrue(util.is_primitive(-3.5))
        self.assertTrue(util.is_primitive(float(3)))

    def test_is_primitive_long(self):
        self.assertTrue(util.is_primitive(2 ** 64))

    def test_is_primitive_bool(self):
        self.assertTrue(util.is_primitive(True))
        self.assertTrue(util.is_primitive(False))

    def test_is_primitive_None(self):
        self.assertTrue(util.is_primitive(None))

    def test_is_primitive_bytes(self):
        self.assertFalse(util.is_primitive(b'hello'))
        self.assertFalse(util.is_primitive('foo'.encode('utf-8')))
        self.assertTrue(util.is_primitive('foo'))

    def test_is_primitive_unicode(self):
        self.assertTrue(util.is_primitive(compat.ustr('')))
        self.assertTrue(util.is_primitive('hello'))

    def test_is_primitive_list(self):
        self.assertFalse(util.is_primitive([]))
        self.assertFalse(util.is_primitive([4, 4]))

    def test_is_primitive_dict(self):
        self.assertFalse(util.is_primitive({'key': 'value'}))
        self.assertFalse(util.is_primitive({}))

    def test_is_primitive_tuple(self):
        self.assertFalse(util.is_primitive((1, 3)))
        self.assertFalse(util.is_primitive((1,)))

    def test_is_primitive_set(self):
        self.assertFalse(util.is_primitive({1, 3}))

    def test_is_primitive_object(self):
        self.assertFalse(util.is_primitive(Thing('test')))

    def test_is_list_list(self):
        self.assertTrue(util.is_list([1, 2]))

    def test_is_list_set(self):
        self.assertTrue(util.is_set({1, 2}))

    def test_is_list_tuple(self):
        self.assertTrue(util.is_tuple((1, 2)))

    def test_is_list_dict(self):
        self.assertFalse(util.is_list({'key': 'value'}))
        self.assertFalse(util.is_set({'key': 'value'}))
        self.assertFalse(util.is_tuple({'key': 'value'}))

    def test_is_list_other(self):
        self.assertFalse(util.is_list(1))
        self.assertFalse(util.is_set(1))
        self.assertFalse(util.is_tuple(1))

    def test_is_sequence_various(self):
        self.assertTrue(util.is_sequence([]))
        self.assertTrue(util.is_sequence(tuple()))
        self.assertTrue(util.is_sequence(set()))

    def test_is_dictionary_dict(self):
        self.assertTrue(util.is_dictionary({}))

    def test_is_dicitonary_sequences(self):
        self.assertFalse(util.is_dictionary([]))
        self.assertFalse(util.is_dictionary(set()))

    def test_is_dictionary_tuple(self):
        self.assertFalse(util.is_dictionary(tuple()))

    def test_is_dictionary_primitive(self):
        self.assertFalse(util.is_dictionary(int()))
        self.assertFalse(util.is_dictionary(None))
        self.assertFalse(util.is_dictionary(str()))

    def test_is_dictionary_subclass_dict(self):
        self.assertFalse(util.is_dictionary_subclass({}))

    def test_is_dictionary_subclass_subclass(self):
        self.assertTrue(util.is_dictionary_subclass(DictSubclass()))

    def test_is_sequence_subclass_subclass(self):
        self.assertTrue(util.is_sequence_subclass(ListSubclass()))

    def test_is_sequence_subclass_list(self):
        self.assertFalse(util.is_sequence_subclass([]))

    def test_is_noncomplex_time_struct(self):
        t = time.struct_time('123456789')
        self.assertTrue(util.is_noncomplex(t))

    def test_is_noncomplex_other(self):
        self.assertFalse(util.is_noncomplex('a'))

    def test_is_function_builtins(self):
        self.assertTrue(util.is_function(globals))

    def test_is_function_lambda(self):
        self.assertTrue(util.is_function(lambda: False))

    def test_is_function_instance_method(self):
        class Foo(object):
            def method(self):
                pass

            @staticmethod
            def staticmethod():
                pass

            @classmethod
            def classmethod(cls):
                pass

        f = Foo()
        self.assertTrue(util.is_function(f.method))
        self.assertTrue(util.is_function(f.staticmethod))
        self.assertTrue(util.is_function(f.classmethod))

    def test_itemgetter(self):
        expect = '0'
        actual = util.itemgetter((0, 'zero'))
        self.assertEqual(expect, actual)

    def test_has_method(self):
        instance = MethodTestClass()
        x = 1
        has_method = util.has_method

        # no attribute
        self.assertFalse(has_method(instance, 'foo'))

        # builtin method type
        self.assertFalse(has_method(int, '__getnewargs__'))
        self.assertTrue(has_method(x, '__getnewargs__'))

        # staticmethod
        self.assertTrue(has_method(instance, 'static_method'))
        self.assertTrue(has_method(MethodTestClass, 'static_method'))

        # not a method
        self.assertFalse(has_method(instance, 'variable'))
        self.assertFalse(has_method(MethodTestClass, 'variable'))

        # classmethod
        self.assertTrue(has_method(instance, 'class_method'))
        self.assertTrue(has_method(MethodTestClass, 'class_method'))

        # bound method
        self.assertTrue(has_method(instance, 'bound_method'))
        self.assertFalse(has_method(MethodTestClass, 'bound_method'))

        # subclass bound method
        sub_instance = MethodTestSubclass()
        self.assertTrue(has_method(sub_instance, 'bound_method'))
        self.assertFalse(has_method(MethodTestSubclass, 'bound_method'))

        # old style object
        old_instance = MethodTestOldStyle()
        self.assertTrue(has_method(old_instance, 'bound_method'))
        self.assertFalse(has_method(MethodTestOldStyle, 'bound_method'))


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(UtilTestCase))
    suite.addTest(doctest.DocTestSuite(util))
    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='suite')
