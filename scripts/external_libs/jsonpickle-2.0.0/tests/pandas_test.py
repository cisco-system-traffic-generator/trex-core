from __future__ import absolute_import, division, unicode_literals
import datetime

import pytest

try:
    import pandas as pd
    import numpy as np
    from pandas.testing import assert_series_equal
    from pandas.testing import assert_frame_equal
    from pandas.testing import assert_index_equal
except ImportError:
    pytest.skip('numpy is not available', allow_module_level=True)


import jsonpickle
import jsonpickle.ext.pandas


@pytest.fixture(scope='module', autouse=True)
def pandas_extension():
    """Initialize the numpy extension for this test module"""
    jsonpickle.ext.pandas.register_handlers()
    yield  # control to the test function.
    jsonpickle.ext.pandas.unregister_handlers()


def roundtrip(obj):
    return jsonpickle.decode(jsonpickle.encode(obj))


def test_series_roundtrip():
    ser = pd.Series(
        {
            'an_int': np.int_(1),
            'a_float': np.float_(2.5),
            'a_nan': np.nan,
            'a_minus_inf': -np.inf,
            'an_inf': np.inf,
            'a_str': np.str_('foo'),
            'a_unicode': np.unicode_('bar'),
            'date': np.datetime64('2014-01-01'),
            'complex': np.complex_(1 - 2j),
            # TODO: the following dtypes are not currently supported.
            # 'object': np.object_({'a': 'b'}),
        }
    )
    decoded_ser = roundtrip(ser)
    assert_series_equal(decoded_ser, ser)


def test_dataframe_roundtrip():
    df = pd.DataFrame(
        {
            'an_int': np.int_([1, 2, 3]),
            'a_float': np.float_([2.5, 3.5, 4.5]),
            'a_nan': np.array([np.nan] * 3),
            'a_minus_inf': np.array([-np.inf] * 3),
            'an_inf': np.array([np.inf] * 3),
            'a_str': np.str_('foo'),
            'a_unicode': np.unicode_('bar'),
            'date': np.array([np.datetime64('2014-01-01')] * 3),
            'complex': np.complex_([1 - 2j, 2 - 1.2j, 3 - 1.3j]),
            # TODO: the following dtypes are not currently supported.
            # 'object': np.object_([{'a': 'b'}]*3),
        }
    )
    decoded_df = roundtrip(df)
    assert_frame_equal(decoded_df, df)


def test_multindex_dataframe_roundtrip():
    df = pd.DataFrame(
        {
            'idx_lvl0': ['a', 'b', 'c'],
            'idx_lvl1': np.int_([1, 1, 2]),
            'an_int': np.int_([1, 2, 3]),
            'a_float': np.float_([2.5, 3.5, 4.5]),
            'a_nan': np.array([np.nan] * 3),
            'a_minus_inf': np.array([-np.inf] * 3),
            'an_inf': np.array([np.inf] * 3),
            'a_str': np.str_('foo'),
            'a_unicode': np.unicode_('bar'),
        }
    )
    df = df.set_index(['idx_lvl0', 'idx_lvl1'])

    decoded_df = roundtrip(df)
    assert_frame_equal(decoded_df, df)


def test_dataframe_with_interval_index_roundtrip():
    df = pd.DataFrame(
        {'a': [1, 2], 'b': [3, 4]}, index=pd.IntervalIndex.from_breaks([1, 2, 4])
    )

    decoded_df = roundtrip(df)
    assert_frame_equal(decoded_df, df)


def test_index_roundtrip():
    idx = pd.Index(range(5, 10))
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_datetime_index_roundtrip():
    idx = pd.date_range(start='2019-01-01', end='2019-02-01', freq='D')
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_ragged_datetime_index_roundtrip():
    idx = pd.DatetimeIndex(['2019-01-01', '2019-01-02', '2019-01-05'])
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_timedelta_index_roundtrip():
    idx = pd.timedelta_range(start='1 day', periods=4, closed='right')
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_period_index_roundtrip():
    idx = pd.period_range(start='2017-01-01', end='2018-01-01', freq='M')
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_int64_index_roundtrip():
    idx = pd.Int64Index([-1, 0, 3, 4])
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_uint64_index_roundtrip():
    idx = pd.UInt64Index([0, 3, 4])
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_float64_index_roundtrip():
    idx = pd.Float64Index([0.1, 3.7, 4.2])
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_interval_index_roundtrip():
    idx = pd.IntervalIndex.from_breaks(range(5))
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_datetime_interval_index_roundtrip():
    idx = pd.IntervalIndex.from_breaks(pd.date_range('2019-01-01', '2019-01-10'))
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_multi_index_roundtrip():
    idx = pd.MultiIndex.from_product(((1, 2, 3), ('a', 'b')))
    decoded_idx = roundtrip(idx)
    assert_index_equal(decoded_idx, idx)


def test_timestamp_roundtrip():
    obj = pd.Timestamp('2019-01-01')
    decoded_obj = roundtrip(obj)
    assert decoded_obj == obj


def test_period_roundtrip():
    obj = pd.Timestamp('2019-01-01')
    decoded_obj = roundtrip(obj)
    assert decoded_obj == obj


def test_interval_roundtrip():
    obj = pd.Interval(2, 4, closed=str('left'))
    decoded_obj = roundtrip(obj)
    assert decoded_obj == obj


def test_b64():
    """Test the binary encoding"""
    # array of substantial size is stored as b64
    a = np.random.rand(20, 10)
    index = ['Row' + str(i) for i in range(1, a.shape[0] + 1)]
    columns = ['Col' + str(i) for i in range(1, a.shape[1] + 1)]
    df = pd.DataFrame(a, index=index, columns=columns)
    decoded_df = roundtrip(df)
    assert_frame_equal(decoded_df, df)


def test_series_list_index():
    """Test pandas using series with a list index"""
    expect = pd.Series(0, index=[1, 2, 3])
    actual = roundtrip(expect)

    assert expect.values[0] == actual.values[0]
    assert 0 == actual.values[0]

    assert expect.index[0] == actual.index[0]
    assert expect.index[1] == actual.index[1]
    assert expect.index[2] == actual.index[2]


def test_series_multi_index():
    """Test pandas using series with a multi-index"""
    expect = pd.Series(0, index=[[1], [2], [3]])
    actual = roundtrip(expect)

    assert expect.values[0] == actual.values[0]
    assert 0 == actual.values[0]

    assert expect.index[0] == actual.index[0]
    assert expect.index[0][0] == actual.index[0][0]
    assert expect.index[0][1] == actual.index[0][1]
    assert expect.index[0][2] == actual.index[0][2]


def test_series_multi_index_strings():
    """Test multi-index with strings"""
    lets = ['A', 'B', 'C']
    nums = ['1', '2', '3']
    midx = pd.MultiIndex.from_product([lets, nums])
    expect = pd.Series(0, index=midx)
    actual = roundtrip(expect)

    assert expect.values[0] == actual.values[0]
    assert 0 == actual.values[0]

    assert expect.index[0] == actual.index[0]
    assert expect.index[1] == actual.index[1]
    assert expect.index[2] == actual.index[2]
    assert expect.index[3] == actual.index[3]
    assert expect.index[4] == actual.index[4]
    assert expect.index[5] == actual.index[5]
    assert expect.index[6] == actual.index[6]
    assert expect.index[7] == actual.index[7]
    assert expect.index[8] == actual.index[8]

    assert ('A', '1') == actual.index[0]
    assert ('A', '2') == actual.index[1]
    assert ('A', '3') == actual.index[2]
    assert ('B', '1') == actual.index[3]
    assert ('B', '2') == actual.index[4]
    assert ('B', '3') == actual.index[5]
    assert ('C', '1') == actual.index[6]
    assert ('C', '2') == actual.index[7]
    assert ('C', '3') == actual.index[8]


def test_dataframe_with_timedelta64_dtype():
    data_frame = pd.DataFrame(
        {
            'Start': [
                '2020/12/14 00:00:01',
                '2020/12/14 00:00:04',
                '2020/12/14 00:00:06',
            ],
            'End': [
                '2020/12/14 00:00:04',
                '2020/12/14 00:00:06',
                '2020/12/14 00:00:09',
            ],
        }
    )
    data_frame['Start'] = pd.to_datetime(data_frame['Start'])
    data_frame['End'] = pd.to_datetime(data_frame['End'])
    data_frame['Duration'] = data_frame['End'] - data_frame['Start']

    encoded = jsonpickle.encode(data_frame)
    actual = jsonpickle.decode(encoded)

    assert isinstance(actual, pd.DataFrame)
    assert data_frame['Start'][0] == actual['Start'][0]
    assert data_frame['Start'][1] == actual['Start'][1]
    assert data_frame['Start'][2] == actual['Start'][2]
    assert data_frame['End'][0] == actual['End'][0]
    assert data_frame['End'][1] == actual['End'][1]
    assert data_frame['End'][2] == actual['End'][2]
    assert isinstance(actual['Duration'][0], datetime.timedelta)
    assert isinstance(actual['Duration'][1], datetime.timedelta)
    assert isinstance(actual['Duration'][2], datetime.timedelta)
    assert data_frame['Duration'][0] == actual['Duration'][0]
    assert data_frame['Duration'][1] == actual['Duration'][1]
    assert data_frame['Duration'][2] == actual['Duration'][2]


def test_multilevel_columns():
    iterables = [['inj', 'prod'], ['hourly', 'cumulative']]
    names = ['first', 'second']
    # transform it to tuples
    columns = pd.MultiIndex.from_product(iterables, names=names)
    # build a multi-index from it
    data_frame = pd.DataFrame(
        np.random.randn(3, 4), index=['A', 'B', 'C'], columns=columns
    )
    encoded = jsonpickle.encode(data_frame)
    cloned_data_frame = jsonpickle.decode(encoded)
    assert isinstance(cloned_data_frame, pd.DataFrame)
    assert data_frame.columns.names == cloned_data_frame.columns.names
    assert_frame_equal(data_frame, cloned_data_frame)


if __name__ == '__main__':
    pytest.main([__file__])
