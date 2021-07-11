from __future__ import absolute_import, division, unicode_literals
import datetime
import warnings

import pytest

try:
    import numpy as np
    import numpy.testing as npt
    from numpy.compat import asbytes
    from numpy.testing import assert_equal
except ImportError:
    pytest.skip('numpy is not available', allow_module_level=True)

import jsonpickle
import jsonpickle.ext.numpy
from jsonpickle import handlers
from jsonpickle.compat import PY2


@pytest.fixture(scope='module', autouse=True)
def numpy_extension():
    """Initialize the numpy extension for this test module"""
    jsonpickle.ext.numpy.register_handlers()
    yield  # control to the test function.
    jsonpickle.ext.numpy.unregister_handlers()


def roundtrip(obj):
    return jsonpickle.decode(jsonpickle.encode(obj))


def test_dtype_roundtrip():
    dtypes = [
        np.int,
        np.float,
        np.complex,
        np.int32,
        np.str,
        np.object,
        np.unicode,
        np.dtype(np.void),
        np.dtype(np.int32),
        np.dtype(np.float32),
        np.dtype('f4,i4,f2,i1'),
        np.dtype(('f4', 'i4'), ('f2', 'i1')),
        np.dtype('1i4', align=True),
        np.dtype('M8[7D]'),
        np.dtype(
            {
                'names': ['f0', 'f1', 'f2'],
                'formats': ['<u4', '<u2', '<u2'],
                'offsets': [0, 0, 2],
            },
            align=True,
        ),
        np.dtype([('f0', 'i4'), ('f2', 'i1')]),
        np.dtype(
            [
                (
                    'top',
                    [
                        ('tiles', ('>f4', (64, 64)), (1,)),
                        ('rtile', '>f4', (64, 36)),
                    ],
                    (3,),
                ),
                (
                    'bottom',
                    [
                        ('bleft', ('>f4', (8, 64)), (1,)),
                        ('bright', '>f4', (8, 36)),
                    ],
                ),
            ]
        ),
    ]

    for dtype in dtypes:
        encoded = jsonpickle.encode(dtype)
        decoded = jsonpickle.decode(encoded)
        assert dtype == decoded


def test_generic_roundtrip():
    values = [
        np.int_(1),
        np.int32(-2),
        np.float_(2.5),
        np.nan,
        -np.inf,
        np.inf,
        np.datetime64('2014-01-01'),
        np.str_('foo'),
        np.unicode_('bar'),
        np.object_({'a': 'b'}),
        np.complex_(1 - 2j),
    ]
    for value in values:
        decoded = roundtrip(value)
        assert_equal(decoded, value)
        assert isinstance(decoded, type(value))


def test_ndarray_roundtrip():
    arrays = [
        np.random.random((10, 20)),
        np.array([[True, False, True]]),
        np.array(['foo', 'bar']),
        np.array(['baz'.encode('utf-8')]),
        np.array(['2010', 'NaT', '2030']).astype('M'),
        np.rec.array(asbytes('abcdefg') * 100, formats='i2,a3,i4', shape=3),
        np.rec.array(
            [
                (1, 11, 'a'),
                (2, 22, 'b'),
                (3, 33, 'c'),
                (4, 44, 'd'),
                (5, 55, 'ex'),
                (6, 66, 'f'),
                (7, 77, 'g'),
            ],
            formats='u1,f4,a1',
        ),
        np.array(['1960-03-12', datetime.date(1960, 3, 12)], dtype='M8[D]'),
        np.array([0, 1, -1, np.inf, -np.inf, np.nan], dtype='f2'),
    ]

    if not PY2:
        arrays.extend(
            [
                np.rec.array(
                    [('NGC1001', 11), ('NGC1002', 1.0), ('NGC1003', 1.0)],
                    dtype=[('target', 'S20'), ('V_mag', 'f4')],
                )
            ]
        )
    for array in arrays:
        decoded = roundtrip(array)
        assert_equal(decoded, array)
        assert decoded.dtype == array.dtype


def test_shapes_containing_zeroes():
    """Test shapes which cannot be represented as nested lists"""
    expect = np.eye(3)[3:]
    actual = roundtrip(expect)
    npt.assert_array_equal(expect, actual)


def test_accuracy():
    """Test the accuracy of the string representation"""
    expect = np.random.randn(3, 3)
    actual = roundtrip(expect)
    npt.assert_array_equal(expect, actual)


def test_b64():
    """Test the binary encoding"""
    # Array of substantial size is stored as b64.
    expect = np.random.rand(10, 10)
    actual = roundtrip(expect)
    npt.assert_array_equal(expect, actual)


def test_views():
    """Test views under serialization"""
    rng = np.arange(20)  # a range of an array
    view = rng[10:]  # a view referencing a portion of an array
    data = [rng, view]

    actual = roundtrip(data)
    actual[0][15] = -1
    assert actual[1][5] == -1


def test_strides():
    """Test non-standard strides and offsets"""
    arr = np.eye(3)
    view = arr[1:, 1:]
    assert view.base is arr

    data = [arr, view]
    actual = roundtrip(data)

    # test that the deserialized arrays indeed view the same memory
    new_arr, new_view = actual
    new_arr[1, 2] = -1
    assert new_view[0, 1] == -1
    assert new_view.base is new_arr


def test_weird_arrays():
    """Test references to arrays that do not effectively own their memory"""
    a = np.arange(9)
    b = a[5:]
    a.strides = 1

    # this is kinda fishy; a has overlapping memory, _a does not
    if PY2:
        warn_count = 0
    else:
        warn_count = 1
    with warnings.catch_warnings(record=True) as w:
        _a = roundtrip(a)
        assert len(w) == warn_count
        npt.assert_array_equal(a, _a)

    # this also requires a deepcopy to work
    with warnings.catch_warnings(record=True) as w:
        _a, _b = roundtrip([a, b])
        assert len(w) == warn_count
        npt.assert_array_equal(a, _a)
        npt.assert_array_equal(b, _b)


def test_transpose():
    """test handling of non-c-contiguous memory layout"""
    # simple case; view a c-contiguous array
    a = np.arange(9).reshape(3, 3)
    b = a[1:, 1:]
    assert b.base is a.base
    _a, _b = roundtrip([a, b])
    assert _b.base is _a.base
    npt.assert_array_equal(a, _a)
    npt.assert_array_equal(b, _b)

    # a and b both view the same contiguous array
    a = np.arange(9).reshape(3, 3).T
    b = a[1:, 1:]
    assert b.base is a.base
    _a, _b = roundtrip([a, b])
    assert _b.base is _a.base
    npt.assert_array_equal(a, _a)
    npt.assert_array_equal(b, _b)

    # view an f-contiguous array
    a = a.copy()
    a.strides = a.strides[::-1]
    b = a[1:, 1:]
    assert b.base is a
    _a, _b = roundtrip([a, b])
    assert _b.base is _a
    npt.assert_array_equal(a, _a)
    npt.assert_array_equal(b, _b)

    # now a.data.contiguous is False; we have to make a deepcopy to make
    # this work note that this is a pretty contrived example though!
    a = np.arange(8).reshape(2, 2, 2).copy()
    a.strides = a.strides[0], a.strides[2], a.strides[1]
    b = a[1:, 1:]
    assert b.base is a

    if PY2:
        warn_count = 0
    else:
        warn_count = 1
    with warnings.catch_warnings(record=True) as w:
        _a, _b = roundtrip([a, b])
        assert len(w) == warn_count
        npt.assert_array_equal(a, _a)
        npt.assert_array_equal(b, _b)


def test_fortran_base():
    """Test a base array in fortran order"""
    a = np.asfortranarray(np.arange(100).reshape((10, 10)))
    _a = roundtrip(a)
    npt.assert_array_equal(a, _a)


def test_buffer():
    """test behavior with memoryviews which are not ndarrays"""
    bstring = 'abcdefgh'.encode('utf-8')
    a = np.frombuffer(bstring, dtype=np.byte)
    if PY2:
        warn_count = 0
    else:
        warn_count = 1
    with warnings.catch_warnings(record=True) as w:
        _a = roundtrip(a)
        npt.assert_array_equal(a, _a)
        assert len(w) == warn_count


def test_as_strided():
    """Test the result of as_strided()

    as_strided() returns an object that implements the array interface but
    is not an ndarray.

    """
    if PY2:
        warn_count = 0
    else:
        warn_count = 1
    a = np.arange(10)
    b = np.lib.stride_tricks.as_strided(a, shape=(5,), strides=(a.dtype.itemsize * 2,))
    data = [a, b]

    with warnings.catch_warnings(record=True) as w:
        # as_strided returns a DummyArray object, which we can not
        # currently serialize correctly FIXME: would be neat to add
        # support for all objects implementing the __array_interface__
        _data = roundtrip(data)
        assert len(w) == warn_count

    # as we were warned, deserialized result is no longer a view
    _data[0][0] = -1
    assert _data[1][0] == 0


def test_immutable():
    """test that immutability flag is copied correctly"""
    a = np.arange(10)
    a.flags.writeable = False
    _a = roundtrip(a)
    with pytest.raises(ValueError):
        _a[0] = 0


def test_byteorder():
    """Test the byteorder for text and binary encodings"""
    # small arr is stored as text
    a = np.arange(10).newbyteorder()
    b = a[:].newbyteorder()
    _a, _b = roundtrip([a, b])
    npt.assert_array_equal(a, _a)
    npt.assert_array_equal(b, _b)

    # bigger arr is stored as binary
    a = np.arange(100).newbyteorder()
    b = a[:].newbyteorder()
    _a, _b = roundtrip([a, b])
    npt.assert_array_equal(a, _a)
    npt.assert_array_equal(b, _b)


def test_zero_dimensional_array():
    expect = np.array(0.0)
    actual = roundtrip(expect)
    npt.assert_array_equal(expect, actual)


def test_nested_data_list_of_dict_with_list_keys():
    """Ensure we can handle numpy arrays within a nested structure"""
    expect = [{'key': [np.array(0)]}]
    actual = roundtrip(expect)
    npt.assert_array_equal(expect[0]['key'][0], actual[0]['key'][0])

    expect = [{'key': [np.array([1.0])]}]
    actual = roundtrip(expect)
    npt.assert_array_equal(expect[0]['key'][0], actual[0]['key'][0])


def test_size_threshold_None():
    handler = jsonpickle.ext.numpy.NumpyNDArrayHandlerView(size_threshold=None)
    handlers.registry.unregister(np.ndarray)
    handlers.registry.register(np.ndarray, handler, base=True)
    expect = np.array([0, 1])
    actual = roundtrip(expect)
    npt.assert_array_equal(expect, actual)


def test_ndarray_dtype_object():
    a = np.array(['F' + str(i) for i in range(30)], dtype=np.object)
    buf = jsonpickle.encode(a)
    # This is critical for reproducing the numpy segfault issue when
    # restoring ndarray of dtype object.
    del a
    expect = np.array(['F' + str(i) for i in range(30)], dtype=np.object)
    actual = jsonpickle.decode(buf)
    npt.assert_array_equal(expect, actual)


def test_np_random():
    """Ensure random.random() arrays can be serialized"""
    obj = np.random.random(100)
    encoded = jsonpickle.encode(obj)
    clone = jsonpickle.decode(encoded)
    assert 100 == len(clone)
    for idx, (expect, actual) in enumerate(zip(obj, clone)):
        assert expect == actual


if __name__ == '__main__':
    pytest.main([__file__])
