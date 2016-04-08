#!/router/bin/python

import functional_general_test
#HACK FIX ME START
import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.abspath(os.path.join(CURRENT_PATH, '../../trex_control_plane/common')))
#HACK FIX ME END
import filters
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import assert_true, assert_false
from nose.tools import raises


class ToggleFilter_Test(functional_general_test.CGeneralFunctional_Test):

    def setUp(self):
        self.list_db = [1, 2, 3, 4, 5]
        self.set_db = {1, 2, 3, 4, 5}
        self.tuple_db = (1, 2, 3, 4, 5)
        self.dict_db  = {str(x): x**2
                         for x in range(5)}

    def test_init_with_dict(self):
        toggle_filter = filters.ToggleFilter(self.dict_db)
        assert_equal(toggle_filter._toggle_db, set(self.dict_db.keys()))
        assert_equal(toggle_filter.filter_items(), self.dict_db)


    def test_init_with_list(self):
        toggle_filter = filters.ToggleFilter(self.list_db)
        assert_equal(toggle_filter._toggle_db, set(self.list_db))
        assert_equal(toggle_filter.filter_items(), self.list_db)

    def test_init_with_set(self):
        toggle_filter = filters.ToggleFilter(self.set_db)
        assert_equal(toggle_filter._toggle_db, self.set_db)
        assert_equal(toggle_filter.filter_items(), self.set_db)

    def test_init_with_tuple(self):
        toggle_filter = filters.ToggleFilter(self.tuple_db)
        assert_equal(toggle_filter._toggle_db, set(self.tuple_db))
        assert_equal(toggle_filter.filter_items(), self.tuple_db)

    @raises(TypeError)
    def test_init_with_non_iterable(self):
        toggle_filter = filters.ToggleFilter(15)

    def test_dict_toggeling(self):
        toggle_filter = filters.ToggleFilter(self.dict_db)
        assert_false(toggle_filter.toggle_item("3"))
        assert_equal(toggle_filter._toggle_db, {'0', '1', '2', '4'})
        assert_true(toggle_filter.toggle_item("3"))
        assert_equal(toggle_filter._toggle_db, {'0', '1', '2', '3', '4'})
        assert_false(toggle_filter.toggle_item("2"))
        assert_false(toggle_filter.toggle_item("4"))
        self.dict_db.update({'5': 25, '6': 36})
        assert_true(toggle_filter.toggle_item("6"))

        assert_equal(toggle_filter.filter_items(), {'0': 0, '1': 1, '3': 9, '6': 36})

        del self.dict_db['1']
        assert_equal(toggle_filter.filter_items(), {'0': 0, '3': 9, '6': 36})

    def test_dict_toggeling_negative(self):
        toggle_filter = filters.ToggleFilter(self.dict_db)
        assert_raises(KeyError, toggle_filter.toggle_item, "100")

    def test_list_toggeling(self):
        toggle_filter = filters.ToggleFilter(self.list_db)
        assert_false(toggle_filter.toggle_item(3))
        assert_equal(toggle_filter._toggle_db, {1, 2, 4, 5})
        assert_true(toggle_filter.toggle_item(3))
        assert_equal(toggle_filter._toggle_db, {1, 2, 3, 4, 5})
        assert_false(toggle_filter.toggle_item(2))
        assert_false(toggle_filter.toggle_item(4))
        self.list_db.extend([6 ,7])
        assert_true(toggle_filter.toggle_item(6))

        assert_equal(toggle_filter.filter_items(), [1, 3 , 5, 6])

        self.list_db.remove(1)
        assert_equal(toggle_filter.filter_items(), [3, 5, 6])

    def test_list_toggeliing_negative(self):
        toggle_filter = filters.ToggleFilter(self.list_db)
        assert_raises(KeyError, toggle_filter.toggle_item, 10)

    def test_toggle_multiple_items(self):
        toggle_filter = filters.ToggleFilter(self.list_db)
        assert_false(toggle_filter.toggle_items(1, 3, 5))
        assert_equal(toggle_filter._toggle_db, {2, 4})
        assert_true(toggle_filter.toggle_items(1, 5))
        assert_equal(toggle_filter._toggle_db, {1, 2, 4, 5})

    def test_dont_show_after_init(self):
        toggle_filter = filters.ToggleFilter(self.list_db, show_by_default = False)
        assert_equal(toggle_filter._toggle_db, set())
        assert_equal(toggle_filter.filter_items(), [])


    def tearDown(self):
        pass
