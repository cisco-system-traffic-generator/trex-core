from __future__ import absolute_import, division, unicode_literals
import array
import enum
import collections
import datetime
import decimal
import re
import threading
import pytz

import jsonpickle
from jsonpickle import compat
from jsonpickle import handlers
from jsonpickle import tags
from jsonpickle import util
from jsonpickle.compat import queue, PY2, PY3_ORDERED_DICT

import pytest

from helper import SkippableTest


class Thing(object):
    def __init__(self, name):
        self.name = name
        self.child = None


class DictSubclass(dict):
    name = 'Test'


class ListSubclass(list):
    pass


class BrokenReprThing(Thing):
    def __repr__(self):
        raise Exception('%s has a broken repr' % self.name)

    def __str__(self):
        return '<BrokenReprThing "%s">' % self.name


class GetstateDict(dict):
    def __init__(self, name, **kwargs):
        dict.__init__(self, **kwargs)
        self.name = name
        self.active = False

    def __getstate__(self):
        return (self.name, dict(self.items()))

    def __setstate__(self, state):
        self.name, vals = state
        self.update(vals)
        self.active = True


class GetstateOnly(object):
    def __init__(self, a=1, b=2):
        self.a = a
        self.b = b

    def __getstate__(self):
        return [self.a, self.b]


class GetstateReturnsList(object):
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def __getstate__(self):
        return [self.x, self.y]

    def __setstate__(self, state):
        self.x, self.y = state[0], state[1]


class GetstateRecursesInfintely(object):
    def __getstate__(self):
        return GetstateRecursesInfintely()


class ListSubclassWithInit(list):
    def __init__(self, attr):
        self.attr = attr
        super(ListSubclassWithInit, self).__init__()


NamedTuple = collections.namedtuple('NamedTuple', 'a, b, c')


class ObjWithJsonPickleRepr(object):
    def __init__(self):
        self.data = {'a': self}

    def __repr__(self):
        return jsonpickle.encode(self)


class OldStyleClass:
    pass


class SetSubclass(set):
    pass


class ThingWithFunctionRefs(object):
    def __init__(self):
        self.fn = func


def func(x):
    return x


class ThingWithQueue(object):
    def __init__(self):
        self.child_1 = queue.Queue()
        self.child_2 = queue.Queue()
        self.childref_1 = self.child_1
        self.childref_2 = self.child_2


class ThingWithSlots(object):

    __slots__ = ('a', 'b')

    def __init__(self, a, b):
        self.a = a
        self.b = b


class ThingWithInheritedSlots(ThingWithSlots):

    __slots__ = ('c',)

    def __init__(self, a, b, c):
        ThingWithSlots.__init__(self, a, b)
        self.c = c


class ThingWithIterableSlots(object):

    __slots__ = iter('ab')

    def __init__(self, a, b):
        self.a = a
        self.b = b


class ThingWithStringSlots(object):
    __slots__ = 'ab'

    def __init__(self, a, b):
        self.ab = a + b


class ThingWithSelfAsDefaultFactory(collections.defaultdict):
    """defaultdict subclass that uses itself as its default factory"""

    def __init__(self):
        self.default_factory = self

    def __call__(self):
        return self.__class__()


class ThingWithClassAsDefaultFactory(collections.defaultdict):
    """defaultdict subclass that uses its class as its default factory"""

    def __init__(self):
        self.default_factory = self.__class__

    def __call__(self):
        return self.__class__()


class IntEnumTest(enum.IntEnum):
    X = 1
    Y = 2


class StringEnumTest(enum.Enum):
    A = 'a'
    B = 'b'


class SubEnum(enum.Enum):
    a = 1
    b = 2


class EnumClass(object):
    def __init__(self):
        self.enum_a = SubEnum.a
        self.enum_b = SubEnum.b


class MessageTypes(enum.Enum):
    STATUS = ('STATUS',)
    CONTROL = 'CONTROL'


class MessageStatus(enum.Enum):
    OK = ('OK',)
    ERROR = 'ERROR'


class MessageCommands(enum.Enum):
    STATUS_ALL = 'STATUS_ALL'


class Message(object):
    def __init__(self, message_type, command, status=None, body=None):
        self.message_type = MessageTypes(message_type)
        if command:
            self.command = MessageCommands(command)
        if status:
            self.status = MessageStatus(status)
        if body:
            self.body = body


class ThingWithTimedeltaAttribute(object):
    def __init__(self, offset):
        self.offset = datetime.timedelta(offset)

    def __getinitargs__(self):
        return (self.offset,)


class FailSafeTestCase(SkippableTest):
    class BadClass(object):
        def __getstate__(self):
            raise ValueError('Intentional error')

    good = 'good'

    to_pickle = [BadClass(), good]

    def test_no_error(self):
        encoded = jsonpickle.encode(self.to_pickle, fail_safe=lambda e: None)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded[0], None)
        self.assertEqual(decoded[1], 'good')

    def test_error_recorded(self):
        exceptions = []

        def recorder(exception):
            exceptions.append(exception)

        jsonpickle.encode(self.to_pickle, fail_safe=recorder)
        self.assertEqual(len(exceptions), 1)
        self.assertTrue(isinstance(exceptions[0], Exception))

    def test_custom_err_msg(self):
        CUSTOM_ERR_MSG = 'custom err msg'
        encoded = jsonpickle.encode(self.to_pickle, fail_safe=lambda e: CUSTOM_ERR_MSG)
        decoded = jsonpickle.decode(encoded)
        self.assertEqual(decoded[0], CUSTOM_ERR_MSG)


class IntKeysObject(object):
    def __init__(self):
        self.data = {0: 0}

    def __getstate__(self):
        return self.__dict__


class ExceptionWithArguments(Exception):
    def __init__(self, value):
        super(ExceptionWithArguments, self).__init__('test')
        self.value = value


class AdvancedObjectsTestCase(SkippableTest):
    def setUp(self):
        self.pickler = jsonpickle.pickler.Pickler()
        self.unpickler = jsonpickle.unpickler.Unpickler()

    def tearDown(self):
        self.pickler.reset()
        self.unpickler.reset()

    def test_list_subclass(self):
        obj = ListSubclass()
        obj.extend([1, 2, 3])
        flattened = self.pickler.flatten(obj)
        self.assertTrue(tags.OBJECT in flattened)
        self.assertTrue(tags.SEQ in flattened)
        self.assertEqual(len(flattened[tags.SEQ]), 3)
        for v in obj:
            self.assertTrue(v in flattened[tags.SEQ])
        restored = self.unpickler.restore(flattened)
        self.assertEqual(type(restored), ListSubclass)
        self.assertEqual(restored, obj)

    def test_list_subclass_with_init(self):
        obj = ListSubclassWithInit('foo')
        self.assertEqual(obj.attr, 'foo')
        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(type(inflated), ListSubclassWithInit)

    def test_list_subclass_with_data(self):
        obj = ListSubclass()
        obj.extend([1, 2, 3])
        data = SetSubclass([1, 2, 3])
        obj.data = data
        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        self.assertEqual(restored, obj)
        self.assertEqual(type(restored.data), SetSubclass)
        self.assertEqual(restored.data, data)

    def test_set_subclass(self):
        obj = SetSubclass([1, 2, 3])
        flattened = self.pickler.flatten(obj)
        self.assertTrue(tags.OBJECT in flattened)
        self.assertTrue(tags.SEQ in flattened)
        self.assertEqual(len(flattened[tags.SEQ]), 3)
        for v in obj:
            self.assertTrue(v in flattened[tags.SEQ])
        restored = self.unpickler.restore(flattened)
        self.assertEqual(type(restored), SetSubclass)
        self.assertEqual(restored, obj)

    def test_set_subclass_with_data(self):
        obj = SetSubclass([1, 2, 3])
        data = ListSubclass()
        data.extend([1, 2, 3])
        obj.data = data
        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        self.assertEqual(restored.data.__class__, ListSubclass)
        self.assertEqual(restored.data, data)

    def test_decimal(self):
        obj = decimal.Decimal('0.5')
        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertTrue(isinstance(inflated, decimal.Decimal))

    def test_oldstyleclass(self):
        obj = OldStyleClass()
        obj.value = 1234

        flattened = self.pickler.flatten(obj)
        self.assertEqual(1234, flattened['value'])

        inflated = self.unpickler.restore(flattened)
        self.assertEqual(1234, inflated.value)

    def test_dictsubclass(self):
        obj = DictSubclass()
        obj['key1'] = 1

        expect = {
            tags.OBJECT: 'object_test.DictSubclass',
            'key1': 1,
            '__dict__': {},
        }
        flattened = self.pickler.flatten(obj)
        self.assertEqual(expect, flattened)

        inflated = self.unpickler.restore(flattened)
        self.assertEqual(type(inflated), DictSubclass)
        self.assertEqual(1, inflated['key1'])
        self.assertEqual(inflated.name, 'Test')

    def test_dictsubclass_notunpickable(self):
        self.pickler.unpicklable = False

        obj = DictSubclass()
        obj['key1'] = 1

        flattened = self.pickler.flatten(obj)
        self.assertEqual(1, flattened['key1'])
        self.assertFalse(tags.OBJECT in flattened)

        inflated = self.unpickler.restore(flattened)
        self.assertEqual(1, inflated['key1'])

    def test_getstate_dict_subclass_structure(self):
        obj = GetstateDict('test')
        obj['key1'] = 1

        flattened = self.pickler.flatten(obj)
        self.assertTrue(tags.OBJECT in flattened)
        self.assertEqual('object_test.GetstateDict', flattened[tags.OBJECT])
        self.assertTrue(tags.STATE in flattened)
        self.assertTrue(tags.TUPLE in flattened[tags.STATE])
        self.assertEqual(['test', {'key1': 1}], flattened[tags.STATE][tags.TUPLE])

    def test_getstate_dict_subclass_roundtrip_simple(self):
        obj = GetstateDict('test')
        obj['key1'] = 1

        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)

        self.assertEqual(1, inflated['key1'])
        self.assertEqual(inflated.name, 'test')

    def test_getstate_dict_subclass_roundtrip_cyclical(self):
        obj = GetstateDict('test')
        obj['key1'] = 1

        # The "name" field of obj2 points to obj (reference)
        obj2 = GetstateDict(obj)
        # The "obj2" key in obj points to obj2 (cyclical reference)
        obj['obj2'] = obj2

        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)

        # The dict must be preserved
        self.assertEqual(1, inflated['key1'])

        # __getstate__/__setstate__ must have been run
        self.assertEqual(inflated.name, 'test')
        self.assertEqual(inflated.active, True)
        self.assertEqual(inflated['obj2'].active, True)

        # The reference must be preserved
        self.assertTrue(inflated is inflated['obj2'].name)

    def test_getstate_list_simple(self):
        obj = GetstateReturnsList(1, 2)
        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated.x, 1)
        self.assertEqual(inflated.y, 2)

    def test_getstate_list_inside_list(self):
        obj1 = GetstateReturnsList(1, 2)
        obj2 = GetstateReturnsList(3, 4)
        obj = [obj1, obj2]
        flattened = self.pickler.flatten(obj)
        inflated = self.unpickler.restore(flattened)
        self.assertEqual(inflated[0].x, 1)
        self.assertEqual(inflated[0].y, 2)
        self.assertEqual(inflated[1].x, 3)
        self.assertEqual(inflated[1].y, 4)

    def test_getstate_with_getstate_only(self):
        obj = GetstateOnly()
        a = obj.a = 'this object implements'
        b = obj.b = '__getstate__ but not __setstate__'
        expect = [a, b]
        flat = self.pickler.flatten(obj)
        actual = flat[tags.STATE]
        self.assertEqual(expect, actual)
        restored = self.unpickler.restore(flat)
        self.assertEqual(expect, restored)

    def test_thing_with_queue(self):
        obj = ThingWithQueue()
        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        self.assertEqual(type(restored.child_1), type(queue.Queue()))
        self.assertEqual(type(restored.child_2), type(queue.Queue()))
        # Check references
        self.assertTrue(restored.child_1 is restored.childref_1)
        self.assertTrue(restored.child_2 is restored.childref_2)

    def test_thing_with_func(self):
        obj = ThingWithFunctionRefs()
        obj.ref = obj
        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        self.assertTrue(restored.fn is obj.fn)

        expect = 'success'
        actual1 = restored.fn(expect)
        self.assertEqual(expect, actual1)
        self.assertTrue(restored is restored.ref)

    def test_thing_with_compiled_regex(self):
        rgx = re.compile(r'(.*)(cat)')
        obj = Thing(rgx)

        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        match = restored.name.match('fatcat')
        self.assertEqual('fat', match.group(1))
        self.assertEqual('cat', match.group(2))

    def test_base_object_roundrip(self):
        roundtrip = self.unpickler.restore(self.pickler.flatten(object()))
        self.assertEqual(type(roundtrip), object)

    def test_enum34(self):
        restore = self.unpickler.restore
        flatten = self.pickler.flatten

        def roundtrip(obj):
            return restore(flatten(obj))

        self.assertTrue(roundtrip(IntEnumTest.X) is IntEnumTest.X)
        self.assertTrue(roundtrip(IntEnumTest) is IntEnumTest)

        self.assertTrue(roundtrip(StringEnumTest.A) is StringEnumTest.A)
        self.assertTrue(roundtrip(StringEnumTest) is StringEnumTest)

    def test_bytes_unicode(self):
        b1 = b'foo'
        b2 = b'foo\xff'
        u1 = 'foo'

        # unicode strings get encoded/decoded as is
        encoded = self.pickler.flatten(u1)
        self.assertTrue(encoded == u1)
        self.assertTrue(isinstance(encoded, compat.ustr))
        decoded = self.unpickler.restore(encoded)
        self.assertTrue(decoded == u1)
        self.assertTrue(isinstance(decoded, compat.ustr))

        # bytestrings are wrapped in PY3 but in PY2 we try to decode first
        encoded = self.pickler.flatten(b1)
        if PY2:
            self.assertEqual(encoded, u1)
            self.assertTrue(isinstance(encoded, compat.ustr))
        else:
            self.assertNotEqual(encoded, u1)
            encoded_ustr = util.b64encode(b'foo')
            self.assertEqual({tags.B64: encoded_ustr}, encoded)
            self.assertTrue(isinstance(encoded[tags.B64], compat.ustr))
        decoded = self.unpickler.restore(encoded)
        self.assertTrue(decoded == b1)
        if PY2:
            self.assertTrue(isinstance(decoded, compat.ustr))
        else:
            self.assertTrue(isinstance(decoded, bytes))

        # bytestrings that we can't decode to UTF-8 will always be wrapped
        encoded = self.pickler.flatten(b2)
        self.assertNotEqual(encoded, b2)
        encoded_ustr = util.b64encode(b'foo\xff')
        self.assertEqual({tags.B64: encoded_ustr}, encoded)
        self.assertTrue(isinstance(encoded[tags.B64], compat.ustr))
        decoded = self.unpickler.restore(encoded)
        self.assertEqual(decoded, b2)
        self.assertTrue(isinstance(decoded, bytes))

    def test_backcompat_bytes_quoted_printable(self):
        """Test decoding bytes objects from older jsonpickle versions"""

        b1 = b'foo'
        b2 = b'foo\xff'

        # older versions of jsonpickle used a quoted-printable encoding
        expect = b1
        actual = self.unpickler.restore({tags.BYTES: 'foo'})
        self.assertEqual(expect, actual)

        expect = b2
        actual = self.unpickler.restore({tags.BYTES: 'foo=FF'})
        self.assertEqual(expect, actual)

    def test_nested_objects(self):
        obj = ThingWithTimedeltaAttribute(99)
        flattened = self.pickler.flatten(obj)
        restored = self.unpickler.restore(flattened)
        self.assertEqual(restored.offset, datetime.timedelta(99))

    def test_threading_lock(self):
        obj = Thing('lock')
        obj.lock = threading.Lock()
        lock_class = obj.lock.__class__
        # Roundtrip and make sure we get a lock object.
        json = self.pickler.flatten(obj)
        clone = self.unpickler.restore(json)
        self.assertTrue(isinstance(clone.lock, lock_class))
        self.assertFalse(clone.lock.locked())

        # Serializing a locked lock should create a locked clone.
        self.assertTrue(obj.lock.acquire())
        json = self.pickler.flatten(obj)
        obj.lock.release()
        # Restore the locked lock state.
        clone = self.unpickler.restore(json)
        self.assertTrue(clone.lock.locked())
        clone.lock.release()

    def _test_array_roundtrip(self, obj):
        """Roundtrip an array and test invariants"""
        json = self.pickler.flatten(obj)
        clone = self.unpickler.restore(json)
        self.assertTrue(isinstance(clone, array.array))
        self.assertEqual(obj.typecode, clone.typecode)
        self.assertEqual(len(obj), len(clone))
        for j, k in zip(obj, clone):
            self.assertEqual(j, k)
        self.assertEqual(obj, clone)

    def test_array_handler_numeric(self):
        """Test numeric array.array typecodes that work in Python2+3"""
        typecodes = ('b', 'B', 'h', 'H', 'i', 'I', 'l', 'L', 'f', 'd')
        for typecode in typecodes:
            obj = array.array(typecode, (1, 2, 3))
            self._test_array_roundtrip(obj)

    def test_array_handler_python2(self):
        """Python2 allows the "c" byte/char typecode"""
        if PY2:
            obj = array.array('c', bytes('abcd'))
            self._test_array_roundtrip(obj)

    def test_exceptions_with_arguments(self):
        """Ensure that we can roundtrip Exceptions that take arguments"""
        obj = ExceptionWithArguments('example')
        json = self.pickler.flatten(obj)
        clone = self.unpickler.restore(json)
        self.assertEqual(obj.value, clone.value)
        self.assertEqual(obj.args, clone.args)


def test_getstate_does_not_recurse_infinitely():
    """Serialize an object with a __getstate__ that recurses forever"""
    obj = GetstateRecursesInfintely()
    pickler = jsonpickle.pickler.Pickler(max_depth=5)
    actual = pickler.flatten(obj)
    assert isinstance(actual, dict)
    assert tags.OBJECT in actual


def test_defaultdict_roundtrip():
    """Make sure we can handle collections.defaultdict(list)"""
    # setup
    defaultdict = collections.defaultdict
    defdict = defaultdict(list)
    defdict['a'] = 1
    defdict['b'].append(2)
    defdict['c'] = defaultdict(dict)
    # jsonpickle work your magic
    encoded = jsonpickle.encode(defdict)
    newdefdict = jsonpickle.decode(encoded)
    # jsonpickle never fails
    assert newdefdict['a'] == 1
    assert newdefdict['b'] == [2]
    assert type(newdefdict['c']) == defaultdict
    assert defdict.default_factory == list
    assert newdefdict.default_factory == list


def test_defaultdict_roundtrip_simple_lambda():
    """Make sure we can handle defaultdict(lambda: defaultdict(int))"""
    # setup a sparse collections.defaultdict with simple lambdas
    defaultdict = collections.defaultdict
    defdict = defaultdict(lambda: defaultdict(int))
    defdict[0] = 'zero'
    defdict[1] = defaultdict(lambda: defaultdict(dict))
    defdict[1][0] = 'zero'
    # roundtrip
    encoded = jsonpickle.encode(defdict, keys=True)
    newdefdict = jsonpickle.decode(encoded, keys=True)
    assert newdefdict[0] == 'zero'
    assert type(newdefdict[1]) == defaultdict
    assert newdefdict[1][0] == 'zero'
    assert newdefdict[1][1] == {}  # inner defaultdict
    assert newdefdict[2][0] == 0  # outer defaultdict
    assert type(newdefdict[3]) == defaultdict
    # outer-most defaultdict
    assert newdefdict[3].default_factory == int


def test_defaultdict_roundtrip_simple_lambda2():
    """Serialize a defaultdict that contains a lambda"""
    defaultdict = collections.defaultdict
    payload = {'a': defaultdict(lambda: 0)}
    defdict = defaultdict(lambda: 0, payload)
    # roundtrip
    encoded = jsonpickle.encode(defdict, keys=True)
    decoded = jsonpickle.decode(encoded, keys=True)
    assert type(decoded) == defaultdict
    assert type(decoded['a']) == defaultdict


def test_defaultdict_and_things_roundtrip_simple_lambda():
    """Serialize a default dict that contains a lambda and objects"""
    thing = Thing('a')
    defaultdict = collections.defaultdict
    defdict = defaultdict(lambda: 0)
    obj = [defdict, thing, thing]
    # roundtrip
    encoded = jsonpickle.encode(obj, keys=True)
    decoded = jsonpickle.decode(encoded, keys=True)
    assert decoded[0].default_factory() == 0
    assert decoded[1] is decoded[2]


def test_defaultdict_subclass_with_self_as_default_factory():
    """Serialize a defaultdict subclass with self as its default factory"""
    cls = ThingWithSelfAsDefaultFactory
    tree = cls()
    newtree = _test_defaultdict_tree(tree, cls)
    assert type(newtree['A'].default_factory) == cls
    assert newtree.default_factory is newtree
    assert newtree['A'].default_factory is newtree['A']
    assert newtree['Z'].default_factory is newtree['Z']


def test_defaultdict_subclass_with_class_as_default_factory():
    """Serialize a defaultdict with a class as its default factory"""
    cls = ThingWithClassAsDefaultFactory
    tree = cls()
    newtree = _test_defaultdict_tree(tree, cls)
    assert newtree.default_factory is cls
    assert newtree['A'].default_factory is cls
    assert newtree['Z'].default_factory is cls


def _test_defaultdict_tree(tree, cls):
    tree['A']['B'] = 1
    tree['A']['C'] = 2
    # roundtrip
    encoded = jsonpickle.encode(tree)
    newtree = jsonpickle.decode(encoded)
    # make sure we didn't lose anything
    assert type(newtree) == cls
    assert type(newtree['A']) == cls
    assert newtree['A']['B'] == 1
    assert newtree['A']['C'] == 2
    # ensure that the resulting default_factory is callable and creates
    # a new instance of cls.
    assert type(newtree['A'].default_factory()) == cls
    # we've never seen 'D' before so the reconstructed defaultdict tree
    # should create an instance of cls.
    assert type(newtree['A']['D']) == cls
    # ensure that proxies do not escape into user code
    assert type(newtree.default_factory) != jsonpickle.unpickler._Proxy
    assert type(newtree['A'].default_factory) != jsonpickle.unpickler._Proxy
    assert type(newtree['A']['Z'].default_factory) != jsonpickle.unpickler._Proxy
    return newtree


def test_posix_stat_result():
    """Serialize a posix.stat() result"""
    try:
        import posix
    except ImportError:
        return
    expect = posix.stat(__file__)
    encoded = jsonpickle.encode(expect)
    actual = jsonpickle.decode(encoded)
    assert expect == actual


def test_repr_using_jsonpickle():
    """Serialize an object that uses jsonpickle in its __repr__ definition"""
    thing = ObjWithJsonPickleRepr()
    thing.child = ObjWithJsonPickleRepr()
    thing.child.parent = thing
    encoded = jsonpickle.encode(thing)
    decoded = jsonpickle.decode(encoded)
    assert id(decoded) == id(decoded.child.parent)


def test_broken_repr_dict_key():
    """Tests that we can pickle dictionaries with keys that have
    broken __repr__ implementations.
    """
    br = BrokenReprThing('test')
    obj = {br: True}
    pickler = jsonpickle.pickler.Pickler()
    flattened = pickler.flatten(obj)
    assert '<BrokenReprThing "test">' in flattened
    assert flattened['<BrokenReprThing "test">']


def test_ordered_dict_python3():
    """Ensure that we preserve dict order on python3"""
    if not PY3_ORDERED_DICT:
        return
    # Python3.6+ preserves dict order.
    obj = {'z': 'Z', 'x': 'X', 'y': 'Y'}
    clone = jsonpickle.decode(jsonpickle.encode(obj))
    expect = ['z', 'x', 'y']
    actual = list(clone.keys())
    assert expect == actual


def test_ordered_dict():
    """Serialize an OrderedDict"""
    d = collections.OrderedDict([('c', 3), ('a', 1), ('b', 2)])
    encoded = jsonpickle.encode(d)
    decoded = jsonpickle.decode(encoded)
    assert d == decoded


def test_ordered_dict_unpicklable():
    """Serialize an OrderedDict with unpicklable=False"""
    d = collections.OrderedDict([('c', 3), ('a', 1), ('b', 2)])
    encoded = jsonpickle.encode(d, unpicklable=False)
    decoded = jsonpickle.decode(encoded)
    assert d == decoded


def test_ordered_dict_reduces():
    """Ensure that OrderedDict is reduce()-able"""
    d = collections.OrderedDict([('c', 3), ('a', 1), ('b', 2)])
    has_reduce, has_reduce_ex = util.has_reduce(d)
    assert util.is_reducible(d)
    assert has_reduce or has_reduce_ex


def test_int_keys_in_object_with_getstate_only():
    """Serialize objects with dict keys that implement __getstate__ only"""
    obj = IntKeysObject()
    encoded = jsonpickle.encode(obj, keys=True)
    decoded = jsonpickle.decode(encoded, keys=True)
    assert obj.data == decoded.data


def test_ordered_dict_int_keys():
    """Serialize dicts with int keys and OrderedDict values"""
    d = {
        1: collections.OrderedDict([(2, -2), (3, -3)]),
        4: collections.OrderedDict([(5, -5), (6, -6)]),
    }
    encoded = jsonpickle.encode(d, keys=True)
    decoded = jsonpickle.decode(encoded, keys=True)
    assert isinstance(decoded[1], collections.OrderedDict)
    assert isinstance(decoded[4], collections.OrderedDict)
    assert -2 == decoded[1][2]
    assert -3 == decoded[1][3]
    assert -5 == decoded[4][5]
    assert -6 == decoded[4][6]
    assert d == decoded


def test_ordered_dict_nested():
    """Serialize nested dicts with OrderedDict values"""
    bottom = collections.OrderedDict([('z', 1), ('a', 2)])
    middle = collections.OrderedDict([('c', bottom)])
    top = collections.OrderedDict([('b', middle)])
    encoded = jsonpickle.encode(top)
    decoded = jsonpickle.decode(encoded)
    assert top == decoded
    # test unpicklable=False
    encoded = jsonpickle.encode(top, unpicklable=False)
    decoded = jsonpickle.decode(encoded)
    assert top == decoded


def test_deque_roundtrip():
    """Make sure we can handle collections.deque"""
    old_deque = collections.deque([0, 1, 2], maxlen=5)
    encoded = jsonpickle.encode(old_deque)
    new_deque = jsonpickle.decode(encoded)
    assert encoded != 'nil'
    assert old_deque[0] == 0
    assert new_deque[0] == 0
    assert old_deque[1] == 1
    assert new_deque[1] == 1
    assert old_deque[2] == 2
    assert new_deque[2] == 2
    assert old_deque.maxlen == 5
    assert new_deque.maxlen == 5


def test_namedtuple_roundtrip():
    """Serialize a NamedTuple"""
    old_nt = NamedTuple(0, 1, 2)
    encoded = jsonpickle.encode(old_nt)
    new_nt = jsonpickle.decode(encoded)
    assert type(old_nt) == type(new_nt)
    assert old_nt is not new_nt
    assert old_nt.a == new_nt.a
    assert old_nt.b == new_nt.b
    assert old_nt.c == new_nt.c
    assert old_nt[0] == new_nt[0]
    assert old_nt[1] == new_nt[1]
    assert old_nt[2] == new_nt[2]


def test_counter_roundtrip():
    counter = collections.Counter({1: 2})
    encoded = jsonpickle.encode(counter)
    decoded = jsonpickle.decode(encoded)
    assert type(decoded) is collections.Counter
    # the integer key becomes a string when keys=False
    assert decoded.get('1') == 2


def test_counter_roundtrip_with_keys():
    counter = collections.Counter({1: 2})
    encoded = jsonpickle.encode(counter, keys=True)
    decoded = jsonpickle.decode(encoded, keys=True)
    assert type(decoded) is collections.Counter
    assert decoded.get(1) == 2


issue281 = pytest.mark.xfail(
    'sys.version_info >= (3, 8)',
    reason='https://github.com/jsonpickle/jsonpickle/issues/281',
)


@issue281
def test_list_with_fd():
    """Serialize a list with an file descriptor"""
    fd = open(__file__, 'r')
    fd.close()
    obj = [fd]
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert [None] == newobj


@issue281
def test_thing_with_fd():
    """Serialize an object with a file descriptor"""
    fd = open(__file__, 'r')
    fd.close()
    obj = Thing(fd)
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.name is None


@issue281
def test_dict_with_fd():
    """Serialize a dict with a file descriptor"""
    fd = open(__file__, 'r')
    fd.close()
    obj = {'fd': fd}
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj['fd'] is None


def test_thing_with_lamda():
    """Serialize an object with a lambda"""
    obj = Thing(lambda: True)
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert not hasattr(newobj, 'name')


def test_newstyleslots():
    """Serialize an object with new-style slots"""
    obj = ThingWithSlots(True, False)
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a
    assert not newobj.b


def test_newstyleslots_inherited():
    """Serialize an object with inherited new-style slots"""
    obj = ThingWithInheritedSlots(True, False, None)
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a
    assert not newobj.b
    assert newobj.c is None


def test_newstyleslots_inherited_deleted_attr():
    """Serialize an object with inherited and deleted new-style slots"""
    obj = ThingWithInheritedSlots(True, False, None)
    del obj.c
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a
    assert not newobj.b
    assert not hasattr(newobj, 'c')


def test_newstyleslots_with_children():
    """Serialize an object with slots containing objects"""
    obj = ThingWithSlots(Thing('a'), Thing('b'))
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a.name == 'a'
    assert newobj.b.name == 'b'


def test_newstyleslots_with_children_inherited():
    """Serialize an object with inherited slots containing objects"""
    obj = ThingWithInheritedSlots(Thing('a'), Thing('b'), Thing('c'))
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a.name == 'a'
    assert newobj.b.name == 'b'
    assert newobj.c.name == 'c'


def test_newstyleslots_iterable():
    """Seriazlie an object with iterable slots"""
    obj = ThingWithIterableSlots('alpha', 'bravo')
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.a == 'alpha'
    assert newobj.b == 'bravo'


def test_newstyleslots_string_slot():
    """Serialize an object with string slots"""
    obj = ThingWithStringSlots('a', 'b')
    jsonstr = jsonpickle.encode(obj)
    newobj = jsonpickle.decode(jsonstr)
    assert newobj.ab == 'ab'


def test_enum34_nested():
    """Serialize enums as nested member variables in an object"""
    ec = EnumClass()
    encoded = jsonpickle.encode(ec)
    decoded = jsonpickle.decode(encoded)
    assert ec.enum_a == decoded.enum_a
    assert ec.enum_b == decoded.enum_b


def test_enum_references():
    """Serialize duplicate enums so that reference IDs are used"""
    a = IntEnumTest.X
    b = IntEnumTest.X
    enums_list = [a, b]
    encoded = jsonpickle.encode(enums_list)
    decoded = jsonpickle.decode(encoded)
    assert enums_list == decoded


def test_enum_unpicklable():
    """Serialize enums when unpicklable=False is specified"""
    obj = Message(MessageTypes.STATUS, MessageCommands.STATUS_ALL)
    encoded = jsonpickle.encode(obj, unpicklable=False)
    decoded = jsonpickle.decode(encoded)
    assert 'message_type' in decoded
    assert 'command' in decoded


def test_enum_int_key_and_value():
    """Serialize Integer enums as dict keys and values"""
    thing = Thing('test')
    value = IntEnumTest.X
    value2 = IntEnumTest.Y
    expect = {
        '__first__': thing,
        'thing': thing,
        value: value,
        value2: value2,
    }
    string = jsonpickle.encode(expect, keys=True)
    actual = jsonpickle.decode(string, keys=True)
    assert 'test' == actual['__first__'].name
    assert value == actual[value]
    assert value2 == actual[value2]

    actual_first = actual['__first__']
    actual_thing = actual['thing']
    assert actual_first is actual_thing


def test_enum_string_key_and_value():
    """Encode enums dict keys and values"""
    thing = Thing('test')
    value = StringEnumTest.A
    value2 = StringEnumTest.B
    expect = {
        value: value,
        '__first__': thing,
        value2: value2,
    }
    string = jsonpickle.encode(expect, keys=True)
    actual = jsonpickle.decode(string, keys=True)
    assert 'test' == actual['__first__'].name
    assert value == actual[value]
    assert value2 == actual[value2]


def test_multiple_string_enums_when_make_refs_is_false():
    """Enums do not create cycles when make_refs=False"""
    # The make_refs=False code path will fallback to repr() when encoding
    # objects that it believes introduce a cycle.  It does this to break
    # out of what would be infinite recursion during traversal.
    # This test ensures that enums do not trigger cycles and are properly
    # encoded under make_refs=False.
    expect = {
        'a': StringEnumTest.A,
        'aa': StringEnumTest.A,
    }
    string = jsonpickle.encode(expect, make_refs=False)
    actual = jsonpickle.decode(string)
    assert expect == actual


# Test classes for ExternalHandlerTestCase
class Mixin(object):
    def ok(self):
        return True


class UnicodeMixin(str, Mixin):
    def __add__(self, rhs):
        obj = super(UnicodeMixin, self).__add__(rhs)
        return UnicodeMixin(obj)


class UnicodeMixinHandler(handlers.BaseHandler):
    def flatten(self, obj, data):
        data['value'] = obj
        return data

    def restore(self, obj):
        return UnicodeMixin(obj['value'])


def test_unicode_mixin():
    obj = UnicodeMixin('test')
    assert isinstance(obj, UnicodeMixin)
    assert obj == 'test'

    # Encode into JSON
    handlers.register(UnicodeMixin, UnicodeMixinHandler)
    content = jsonpickle.encode(obj)

    # Resurrect from JSON
    new_obj = jsonpickle.decode(content)
    handlers.unregister(UnicodeMixin)

    new_obj += ' passed'

    assert new_obj == 'test passed'
    assert isinstance(new_obj, UnicodeMixin)
    assert new_obj.ok()


def test_datetime_with_tz_copies_refs():
    """Ensure that we create copies of referenced objects"""
    d0 = datetime.datetime(2020, 5, 5, 5, 5, 5, 5, tzinfo=pytz.UTC)
    d1 = datetime.datetime(2020, 5, 5, 5, 5, 5, 5, tzinfo=pytz.UTC)
    obj = [d0, d1]
    encoded = jsonpickle.encode(obj, make_refs=False)
    decoded = jsonpickle.decode(encoded)
    assert len(decoded) == 2
    assert decoded[0] == d0
    assert decoded[1] == d1


if __name__ == '__main__':
    pytest.main([__file__])
