import os
import sys
import traceback

from .utils.text_opts import *

try:
    basestring
except NameError:
    basestring = str

# basic error for API
class STLError(Exception):
    def __init__ (self, msg):
        self.msg = str(msg)
        self.tb = traceback.extract_stack()

    def __str__ (self):

        s = format_text("\n******\n", 'bold')
        s += format_text('\nSummary error report:\n\n', 'underline')
        s += format_text(self.msg + '\n', 'bold')
        
        s += format_text("\nFull error report:\n\n", 'underline')
        
        for line in reversed(self.tb[:-1]):
            fname, lineno, func, src = os.path.split(line[0])[1], line[1], line[2], line[3]
            s += "         {:<50} - '{}'\n".format(format_text(fname, 'bold') + ':' + format_text(lineno, 'bold'), format_text(src.strip(), 'bold'))

        return s

    def brief (self):
        return self.msg


# raised when the client state is invalid for operation
class STLStateError(STLError):
    def __init__ (self, op, state):
        self.msg = "Operation '{0}' is not valid while '{1}'".format(op, state)
        self.tb = traceback.extract_stack()

# port state error
class STLPortStateError(STLError):
    def __init__ (self, port, op, state):
        self.msg = "Operation '{0}' on port(s) '{1}' is not valid while port(s) '{2}'".format(op, port, state)
        self.tb = traceback.extract_stack()

# raised when argument value is not valid for operation
class STLArgumentError(STLError):
    def __init__ (self, name, got, valid_values = None, extended = None):
        self.tb = traceback.extract_stack()
        self.msg = "Argument: '{0}' invalid value: '{1}'".format(name, got)
        if valid_values:
            self.msg += " - valid values are '{0}'".format(valid_values)

        if extended:
            self.msg += "\n{0}".format(extended)

# raised when argument type is not valid for operation
class STLTypeError(STLError):
    def __init__ (self, arg_name, arg_type, valid_types):
        self.tb = traceback.extract_stack()
        self.msg = "Argument: '%s' invalid type: '%s', expecting type(s): %s." % (arg_name, arg_type.__name__,
            [t.__name__ for t in valid_types] if isinstance(valid_types, tuple) else valid_types.__name__)

# raised when timeout occurs
class STLTimeoutError(STLError):
    def __init__ (self, timeout):
        self.tb = traceback.extract_stack()
        self.msg = "Timeout: operation took more than '{0}' seconds".format(timeout)


