
from collections import namedtuple
from .utils.text_opts import *
from .trex_stl_exceptions import *
import types

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params', 'api_class'])
TupleRC    = namedtuple('RCT', ['rc', 'data', 'is_warn'])

class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# simple class to represent complex return value
class RC():

    def __init__ (self, rc = None, data = None, is_warn = False):
        self.rc_list = []

        if (rc != None):
            self.rc_list.append(TupleRC(rc, data, is_warn))

    def __nonzero__ (self):
        return self.good()

    def __bool__ (self):
        return self.good()

    def add (self, rc):
        self.rc_list += rc.rc_list

    def good (self):
        return all([x.rc for x in self.rc_list])

    def bad (self):
        return not self.good()

    def warn (self):
        return any([x.is_warn for x in self.rc_list])

    def data (self):
        d = [x.data if x.rc else "" for x in self.rc_list]
        return (d if len(d) != 1 else d[0])

    def err (self):
        e = [x.data if not x.rc else "" for x in self.rc_list]
        return (e if len(e) != 1 else e[0])

    def __str__ (self):
        s = ""
        for x in self.rc_list:
            if x.data:
                s += format_text("\n{0}".format(x.data), 'bold')
        return s

    def __iter__(self):
        return self.rc_list.__iter__()


    def prn_func (self, msg, newline = True):
        if newline:
            print(msg)
        else:
            print(msg),

    def annotate (self, log_func = None, desc = None, show_status = True):

        if not log_func:
            log_func = self.prn_func

        if desc:
            log_func(format_text('\n{:<60}'.format(desc), 'bold'), newline = False)
        else:
            log_func("")

        if self.bad():
            # print all the errors
            print("")
            for x in self.rc_list:
                if not x.rc:
                    log_func(format_text("\n{0}".format(x.data), 'bold'))

            print("")
            if show_status:
                log_func(format_text("[FAILED]\n", 'red', 'bold'))


        else:
            if show_status:
                log_func(format_text("[SUCCESS]\n", 'green', 'bold'))


def RC_OK(data = ""):
    return RC(True, data)

def RC_ERR (err):
    return RC(False, err)

def RC_WARN (warn):
    return RC(True, warn, is_warn = True)

try:
    long
    long_exists = True
except:
    long_exists = False

def is_integer(arg):
    if type(arg) is int:
        return True
    if long_exists and type(arg) is long:
        return True
    return False

# validate type of arg
# example1: validate_type('somearg', somearg, [int, long])
# example2: validate_type('another_arg', another_arg, str)
def validate_type(arg_name, arg, valid_types):
    if long_exists:
        if valid_types is int:
            valid_types = (int, long)
        elif type(valid_types) is list and int in valid_types and long not in valid_types:
            valid_types.append(long)
    if type(valid_types) is list:
        valid_types = tuple(valid_types)
    if (type(valid_types) is type or                        # single type, not array of types
            type(valid_types) is tuple or                   # several valid types as tuple
                type(valid_types) is types.ClassType):      # old style class
        if isinstance(arg, valid_types):
            return
        raise STLTypeError(arg_name, type(arg), valid_types)
    else:
        raise STLError('validate_type: valid_types should be type or list or tuple of types')

# throws STLError if not exactly one argument is present
def verify_exclusive_arg (args_list):
    if not (len(list(filter(lambda x: x is not None, args_list))) == 1):
        raise STLError('exactly one parameter from {0} should be provided'.format(args_list))


# shows as 'N/A', but does not let any compares for user to not mistake in automation
class StatNotAvailable(object):
    def __init__(self, stat_name):
        self.stat_name = stat_name

    def __repr__(self, *args, **kwargs):
        return 'N/A'

    def __cmp__(self, *args, **kwargs):
        raise Exception("Stat '%s' not available at this setup" % self.stat_name)

