"""Test serializing pymongo bson structures"""

import datetime
import json
import pickle
import unittest

import jsonpickle

from helper import SkippableTest

bson = None


class Object(object):
    def __init__(self, offset):
        self.offset = datetime.timedelta(offset)

    def __getinitargs__(self):
        return (self.offset,)


class BSONTestCase(SkippableTest):
    def setUp(self):
        global bson
        try:
            bson = __import__('bson.tz_util')
            self.should_skip = False
        except ImportError:
            self.should_skip = True

    def test_FixedOffsetSerializable(self):
        if self.should_skip:
            return self.skip('bson is not installed')
        fo = bson.tz_util.FixedOffset(-60 * 5, 'EST')
        serialized = jsonpickle.dumps(fo)
        restored = jsonpickle.loads(serialized)
        self.assertEqual(vars(restored), vars(fo))

    def test_timedelta(self):
        if self.should_skip:
            return self.skip('bson is not installed')
        td = datetime.timedelta(-1, 68400)
        serialized = jsonpickle.dumps(td)
        restored = jsonpickle.loads(serialized)
        self.assertEqual(restored, td)

    def test_stdlib_pickle(self):
        if self.should_skip:
            return self.skip('bson is not installed')
        fo = bson.tz_util.FixedOffset(-60 * 5, 'EST')
        serialized = pickle.dumps(fo)
        restored = pickle.loads(serialized)
        self.assertEqual(vars(restored), vars(fo))

    def test_nested_objects(self):
        if self.should_skip:
            return self.skip('bson is not installed')
        o = Object(99)
        serialized = jsonpickle.dumps(o)
        restored = jsonpickle.loads(serialized)
        self.assertEqual(restored.offset, datetime.timedelta(99))

    def test_datetime_with_fixed_offset(self):
        if self.should_skip:
            return self.skip('bson is not installed')
        fo = bson.tz_util.FixedOffset(-60 * 5, 'EST')
        dt = datetime.datetime.now().replace(tzinfo=fo)
        serialized = jsonpickle.dumps(dt)
        restored = jsonpickle.loads(serialized)
        self.assertEqual(restored, dt)

    def test_datetime_with_fixed_offset_incremental(self):
        """Test creating an Unpickler and incrementally encoding"""
        if self.should_skip:
            return self.skip('bson is not installed')
        obj = datetime.datetime(2019, 1, 29, 18, 9, 8, 826000, tzinfo=bson.tz_util.utc)
        doc = jsonpickle.dumps(obj)

        # Restore the json using a custom unpickler context.
        unpickler = jsonpickle.unpickler.Unpickler()
        jsonpickle.loads(doc, context=unpickler)

        # Incrementally restore using the same context
        clone = json.loads(doc, object_hook=lambda x: unpickler.restore(x, reset=False))

        self.assertEqual(obj.tzinfo.__reduce__(), clone.tzinfo.__reduce__())


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(BSONTestCase, 'test'))
    return suite


if __name__ == '__main__':
    unittest.main()
