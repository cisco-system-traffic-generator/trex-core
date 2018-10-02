import os
import sys
import inspect

from ..utils.text_opts import format_text

# add here any object that needs to be available in the API
__all__ = ["TRexError", "TRexArgumentError", "TRexTypeError","TRexTimeoutError","TRexConsoleError","TRexConsoleNoAction"]



'''
full_tb - traceback at __init__ of exception
err_tb - traceback at print of exception (could lead to different path)
the idea is to show full_tb path starting from start of err_tb
'''
def remove_common_prefix(full_tb, err_tb):
    if not err_tb:
        return full_tb
    for index, frame in enumerate(full_tb):
        if full_tb[index][0] is err_tb[0][0]:
            return full_tb[index:]
    return full_tb

# basic error for API
class TRexError(Exception):
    def __init__ (self, msg):
        super(TRexError, self).__init__(msg)
        self.msg = str(msg)
        self.tb = inspect.stack()[1:]
        self.tb.reverse()

    def brief (self):
        return self.msg

    __str__ = brief

    def get_tb(self):
        err_tb = inspect.trace()
        short_tb = remove_common_prefix(self.tb, err_tb)

        if not short_tb:
            return ''

        s = format_text("\n******\n", 'bold')
        s += format_text("\nException stack (most recent call last):\n\n", 'underline')

        for i, tb_tuple in enumerate(short_tb):
            fname, lineno, func = os.path.basename(tb_tuple[1]), tb_tuple[2], tb_tuple[4][0].strip()
            s += "#{:<2}    {:<50} - '{}'\n".format(len(short_tb) - i - 1, format_text(fname, 'bold') + ':' + format_text(lineno, 'bold'), format_text(func, 'bold'))
        return s

    def full(self):
        s = self.get_tb()
        s += format_text('\nSummary error message:\n\n', 'underline')
        s += format_text(self.msg + '\n', 'bold')
        return s


# raised when argument value is not valid for operation
class TRexArgumentError(TRexError):
    def __init__ (self, name, got, valid_values = None, extended = None):
        msg = "Argument: '{0}' invalid value: '{1}'".format(name, got)
        if valid_values:
            msg += " - valid values are '{0}'".format(valid_values)

        if extended:
            msg += "\n{0}".format(extended)
        super(TRexArgumentError, self).__init__(msg)


# raised when argument type is not valid for operation
class TRexTypeError(TRexError):
    def __init__ (self, arg_name, arg_type, valid_types):
        msg = "Argument: '%s' invalid type: '%s', expecting type(s): %s." % (arg_name, arg_type.__name__,
            [t.__name__ for t in valid_types] if isinstance(valid_types, tuple) else valid_types.__name__)
        super(TRexTypeError, self).__init__(msg)


# raised when timeout occurs
class TRexTimeoutError(TRexError):
    def __init__ (self, timeout):
        msg = "Timeout: operation took more than '{0}' seconds".format(timeout)
        super(TRexTimeoutError, self).__init__(msg)


class TRexConsoleError(TRexError):
    def __init__ (self, msg):
        TRexError.__init__(self, msg)


class TRexConsoleNoAction(TRexError):
    def __init__ (self):
        TRexError.__init__(self, '')

