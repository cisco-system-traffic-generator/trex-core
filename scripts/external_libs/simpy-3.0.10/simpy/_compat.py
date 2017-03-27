"""
Compatibility helpers for older Python versions.

"""
import sys


PY2 = sys.version_info[0] == 2


if PY2:  # NOQA
    # Python 2.x does not report exception chains.  To emulate the behaviour of
    # Python 3 traceback.format_exception and traceback.print_exception are
    # overwritten with the custom functions.  The original functions
    # are stored in _format_exception and _print_exception.
    import traceback
    from collections import deque

    _print_exception = traceback.print_exception
    _format_exception = traceback.format_exception

    def print_exception(etype, value, tb, limit=None, file=None):
        if file is None:
            file = sys.stderr

        # Build the exception chain.
        chain = deque()
        cause = value
        while True:
            cause = cause.__dict__.get('__cause__', None)
            if cause is None:
                break
            chain.appendleft(cause)

        # Print the exception chain.
        for cause in chain:
            _print_exception(type(cause), cause,
                             cause.__dict__.get('__traceback__', None),
                             limit, file)
            traceback._print(file, '\nThe above exception was the direct '
                                   'cause of the following exception:\n')

        _print_exception(etype, value, tb, limit, file)

    traceback.print_exception = print_exception

    def format_exception(etype, value, tb, limit=None):
        # Build the exception chain.
        chain = deque()
        cause = value
        while True:
            cause = cause.__dict__.get('__cause__', None)
            if cause is None:
                break
            chain.appendleft(cause)

        # Format the exception chain.
        lines = []
        for cause in chain:
            lines.extend(_format_exception(
                type(cause), cause, cause.__dict__.get('__traceback__', None),
                limit))
            lines.append('\nThe above exception was the direct '
                         'cause of the following exception:\n\n')

        lines.extend(_format_exception(etype, value, tb, limit))

        return lines

    traceback.format_exception = format_exception

    sys.excepthook = print_exception
