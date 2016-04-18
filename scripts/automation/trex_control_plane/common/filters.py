
def shallow_copy(x):
    return type(x)(x)


class ToggleFilter(object):
    """
    This class provides a "sticky" filter, that works by "toggling" items of the original database on and off.
    """
    def __init__(self, db_ref, show_by_default=True):
        """
        Instantiate a ToggleFilter object

        :parameters:
             db_ref : iterable
                an iterable object (i.e. list, set etc) that would serve as the reference db of the instance.
                Changes in that object will affect the output of ToggleFilter instance.

             show_by_default: bool
                decide if by default all the items are "on", i.e. these items will be presented if no other
                toggling occurred.

                default value : **True**

        """
        self._data = db_ref
        self._toggle_db = set()
        self._filter_method = filter
        self.__set_initial_state(show_by_default)

    def toggle_item(self, item_key):
        """
        Toggle a single item in/out.

        :parameters:
             item_key :
                an item the by its value the filter can decide to toggle or not.
                Example: int, str and so on.

        :return:
            + **True** if item toggled **into** the filtered items
            + **False** if item toggled **out from** the filtered items

        :raises:
            + KeyError, in case if item key is not part of the toggled list and not part of the referenced db.

        """
        if item_key in self._toggle_db:
            self._toggle_db.remove(item_key)
            return False
        elif item_key in self._data:
            self._toggle_db.add(item_key)
            return True
        else:
            raise KeyError("Provided item key isn't a key of the referenced data structure.")

    def toggle_items(self, *args):
        """
        Toggle multiple items in/out with a single call. Each item will be ha.

        :parameters:
             args : iterable
                an iterable object containing all item keys to be toggled in/out

        :return:
            + **True** if all toggled items were toggled **into** the filtered items
            + **False** if at least one of the items was toggled **out from** the filtered items

        :raises:
            + KeyError, in case if ont of the item keys was not part of the toggled list and not part of the referenced db.

        """
        # in python 3, 'map' returns an iterator, so wrapping with 'list' call creates same effect for both python 2 and 3
        return all(list(map(self.toggle_item, args)))

    def filter_items(self):
        """
        Filters the pointed database by showing only the items mapped at toggle_db set.

        :returns:
            Filtered data of the original object.

        """
        return self._filter_method(self.__toggle_filter, self._data)

    # private methods

    def __set_initial_state(self, show_by_default):
        try:
            _ = (x for x in self._data)
            if isinstance(self._data, dict):
                self._filter_method = ToggleFilter.dict_filter
                if show_by_default:
                    self._toggle_db = set(self._data.keys())
                return
            elif isinstance(self._data, list):
                self._filter_method = ToggleFilter.list_filter
            elif isinstance(self._data, set):
                self._filter_method = ToggleFilter.set_filter
            elif isinstance(self._data, tuple):
                self._filter_method = ToggleFilter.tuple_filter
            if show_by_default:
                self._toggle_db = set(shallow_copy(self._data))  # assuming all relevant data with unique identifier
            return
        except TypeError:
            raise TypeError("provided data object is not iterable")

    def __toggle_filter(self, x):
        return (x in self._toggle_db)

    # static utility methods

    @staticmethod
    def dict_filter(function, iterable):
        assert isinstance(iterable, dict)
        return {k: v
                for k,v in iterable.items()
                if function(k)}

    @staticmethod
    def list_filter(function, iterable):
        # in python 3, filter returns an iterator, so wrapping with list creates same effect for both python 2 and 3
        return list(filter(function, iterable))

    @staticmethod
    def set_filter(function, iterable):
        return {x
                for x in iterable
                if function(x)}

    @staticmethod
    def tuple_filter(function, iterable):
        return tuple(filter(function, iterable))


if __name__ == "__main__":
    pass
