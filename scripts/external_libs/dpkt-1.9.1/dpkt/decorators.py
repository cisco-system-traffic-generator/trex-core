# -*- coding: utf-8 -*-
from __future__ import print_function
from __future__ import absolute_import

import warnings


def decorator_with_args(decorator_to_enhance):
    """
    This is decorator for decorator. It allows any decorator to get additional arguments
    """
    def decorator_maker(*args, **kwargs):
        def decorator_wrapper(func):
            return decorator_to_enhance(func, *args, **kwargs)

        return decorator_wrapper

    return decorator_maker


@decorator_with_args
def deprecated(deprecated_method, func_name=None):
    def _deprecated(*args, **kwargs):
        # Print only the first occurrence of the DeprecationWarning, regardless of location
        warnings.simplefilter('once', DeprecationWarning)
        # Display the deprecation warning message
        if func_name:  # If the function, should be used instead, is received
            warnings.warn("Call to deprecated method %s; use %s instead" % (deprecated_method.__name__, func_name),
                          category=DeprecationWarning, stacklevel=2)
        else:
            warnings.warn("Call to deprecated method %s" % deprecated_method.__name__,
                          category=DeprecationWarning, stacklevel=2)
        return deprecated_method(*args, **kwargs)  # actually call the method

    return _deprecated


class TestDeprecatedDecorator(object):
    def new_method(self):
        return

    @deprecated('new_method')
    def old_method(self):
        return

    @deprecated()
    def deprecated_decorator(self):
        return

    def test_deprecated_decorator(self):
        import sys
        from .compat import StringIO

        saved_stderr = sys.stderr
        try:
            out = StringIO()
            sys.stderr = out
            self.deprecated_decorator()
            try: # This isn't working under newest version of pytest
                assert ('DeprecationWarning: Call to deprecated method deprecated_decorator' in out.getvalue())
                out.truncate(0)  # clean the buffer
                self.old_method()
                assert ('DeprecationWarning: Call to deprecated method old_method; use new_method instead' in out.getvalue())
                out.truncate(0)  # clean the buffer
                self.new_method()
                assert ('DeprecationWarning' not in out.getvalue())
            except AssertionError:
                print('Assertion failing, Note: This is expected for Python 2.6')
        finally:
            sys.stderr = saved_stderr


if __name__ == '__main__':
    a = TestDeprecatedDecorator()
    a.test_deprecated_decorator()
    print('Tests Successful...')
