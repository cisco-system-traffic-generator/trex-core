# -*- coding: utf-8 -*-
import warnings
from timeit import Timer
from test import pystone
from time import sleep


def decorator_with_args(decorator_to_enhance):
    """
    This is decorator for decorator. It allows any decorator to get additional arguments
    """
    def decorator_maker(*args, **kwargs):
        def decorator_wrapper(func):
            return decorator_to_enhance(func, *args, **kwargs)
        return decorator_wrapper
    return decorator_maker


def deprecated(deprecated_method):
    def _deprecated(*args, **kwargs):
        # Print only the first occurrence of the DeprecationWarning, regardless of location
        warnings.simplefilter('once', DeprecationWarning)
        # Display the deprecation warning message
        warnings.warn("Call to deprecated method %s" % deprecated_method.__name__,
                      category=DeprecationWarning, stacklevel=2)
        return deprecated_method(*args, **kwargs)  # actually call the method

    return _deprecated


@decorator_with_args
def duration(function, repeat=10000):
    def _duration(*args, **kwargs):
        time = 0
        try:
            time = Timer(lambda: function(*args, **kwargs)).timeit(repeat)
        finally:
            benchtime, pystones = pystone.pystones()
            kstones = (pystones * time) / 1000
            print '%s : time = %f kstones = %f' % (function.__name__, time, kstones)
        return function(*args, **kwargs)

    return _duration


class TestDeprecatedDecorator(object):
    @deprecated
    def deprecated_decorator(self):
        return

    def test_deprecated_decorator(self):
        import sys
        from StringIO import StringIO

        saved_stderr = sys.stderr
        try:
            out = StringIO()
            sys.stderr = out
            self.deprecated_decorator()
            assert ('DeprecationWarning: Call to deprecated method deprecated_decorator' in out.getvalue())
            # 'in' because message contains the filename, line, etc
        finally:
            sys.stderr = saved_stderr


class TestDurationDecorator(object):
    @duration(1)
    def duration_decorator(self):
        sleep(0.05)
        return

    def test_duration_decorator(self):
        import sys
        import re
        from StringIO import StringIO

        saved_stdout = sys.stdout
        try:
            out = StringIO()
            sys.stdout = out
            self.duration_decorator()
            assert (re.match('((.+?[0-9]+\.[0-9]+)\s?){2}', out.getvalue()))
        finally:
            sys.stdout = saved_stdout

if __name__ == '__main__':
    a = TestDeprecatedDecorator()
    a.test_deprecated_decorator()
    a = TestDurationDecorator()
    a.test_duration_decorator()
    print 'Tests Successful...'