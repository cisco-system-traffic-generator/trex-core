#!/usr/bin/python

'''
Usage:

    nums = SimpleEnum('nums', ['zero', 'one', 'two'])
        or:
    nums = SimpleEnum('nums', 'zero one two')

    one = nums.one

    print(one.name)
    print(one.value)

output:

    one
    1

'''


try: # P2
    string_types = (str, unicode)
    int_types = (int, long)
except: # P3
    string_types = (bytes, str)
    int_types = (int,)


class _SimpleEnumVar(object):
    def __init__(self, class_name, name, val):
        self._name = name
        self._val = val
        self._class_name = class_name

    @property
    def value(self):
        return self._val

    @property
    def name(self):
        return self._name

    def __str__(self):
        return '%s.%s' % (self._class_name, self._name)

    __repr__ = __str__


class SimpleEnum(object):
    def __init__(self, class_name, names_list):
        assert names_list, 'names_list should not be empty'
        if type(names_list) in string_types:
            names_list = names_list.strip().split()
        assert type(class_name) in string_types, 'class_name should be string'
        assert type(names_list) is list, 'names_list should be list or string with names separated by spaces'
        self._class_name = class_name
        self._dict = {}
        for i, name in enumerate(names_list):
            assert type(name) in string_types, 'names_list should include strings'
            var = _SimpleEnumVar(class_name, name, i)
            self._dict[i] = var
            self._dict[name] = var

    def __call__(self, val):
        if val not in self._dict:
            raise Exception("Value '%s' is not part of enum '%s'" % (val, self._class_name))
        return self._dict[val]

    __getattr__ = __call__

    def __str__(self):
        return '%s(%s)' % (self._class_name, ', '.join(['%s: %s' % (self._dict[val].name, val) for val in range(len(self._dict) // 2)]))

    __repr__ = __str__


if __name__ == '__main__':
    nums = SimpleEnum('nums', ['zero', 'one', 'two'])
    print(nums)

    one1 = nums.one
    print(one1.name)
    print(one1.value)

    one2 = nums(1)
    print(one2.name)
    print(one2.value)

    try:
        nums.three
    except Exception as e:
        print(e)


