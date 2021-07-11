"""
This example demonstrates how to add a custom handler to serialize
Qt's QPointF class jsonpickle using PyQt4.

"""
import sys
import unittest

from PyQt4 import QtCore

import jsonpickle
from jsonpickle import handlers

text_type = eval('unicode') if str is bytes else str


class QReduceHandler(handlers.BaseHandler):
    def flatten(self, obj, data):
        pickler = self.context
        if not pickler.unpicklable:
            return text_type(obj)
        flatten = pickler.flatten
        data['__reduce__'] = [flatten(i, reset=False) for i in obj.__reduce__()[1]]
        return data

    def restore(self, data):
        unpickler = self.context
        restore = unpickler.restore
        reduced = [restore(i, reset=False) for i in data['__reduce__']]
        modulename = reduced[0]
        classname = reduced[1]
        args = reduced[2]
        __import__(modulename)
        module = sys.modules[modulename]
        factory = getattr(module, classname)

        return factory(*args)


handlers.register(QtCore.QPointF, QReduceHandler)


class QtTestCase(unittest.TestCase):
    def test_QPointF_roundtrip(self):
        expect = QtCore.QPointF(1.0, 2.0)
        json = jsonpickle.encode(expect)
        actual = jsonpickle.decode(json)
        self.assertEqual(expect, actual)


def suite(self):
    suite = unittest.TestSuite()
    suite.addSuite(unittest.makeSuite(QtTestCase))
    return suite


if __name__ == '__main__':
    unittest.main()
