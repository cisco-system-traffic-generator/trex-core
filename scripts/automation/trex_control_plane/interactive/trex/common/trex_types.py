
from collections import namedtuple, OrderedDict
from ..utils.text_opts import format_text
from .trex_exceptions import TRexError, TRexTypeError
import types

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params', 'api_class'])
TupleRC    = namedtuple('RCT', ['rc', 'data', 'is_warn', 'errno'])


try:
    basestring = basestring
except NameError:
    basestring = str


__all__ = ["RC", "RC_OK", "RC_ERR", "RC_WARN", "listify", "listify_if_int", "validate_type", "is_integer", "basestring", "LRU_cache"]


class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# simple class to represent complex return value
class RC():

    def __init__ (self, rc = None, data = None, is_warn = False, errno = 0):
        self.rc_list = []

        if (rc != None):
            self.rc_list.append(TupleRC(rc, data, is_warn, errno))

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

    def errno(self):
        en = [x.errno if not x.rc else "" for x in self.rc_list]
        return (en if len(en) != 1 else en[0])

    def __str__ (self):
        if self.good():
            s = ""
            for x in self.rc_list:
                if x.data:
                    s += format_text("\n{0}".format(x.data), 'bold')
            return s
        else:
            show_count = 10
            err_list = []
            err_count = 0
            for x in filter(None, listify(self.err())):
                err_count += 1
                if len(err_list) < show_count:
                    err_list.append(format_text(x, 'bold'))
            s = ''
            if err_count > show_count:
                s += format_text('Occurred %s errors, showing first %s:\n' % (err_count, show_count), 'bold')
            s += '\n'.join(err_list)
            return s

    def __iter__(self):
        # make sure every item in the iteration is an RC by itself
        for item in self.rc_list:
            rc = RC()
            rc.rc_list = [item]
            yield rc
        #return self.rc_list.__iter__()


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

def RC_ERR (err = "", errno = 0):
    return RC(False, err, errno = errno)

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
        raise TRexTypeError(arg_name, type(arg), valid_types)
    else:
        raise TRexError('validate_type: valid_types should be type or list or tuple of types')


def validate_choice (arg_name, arg, choices):
    if arg is not None and not arg in choices:
        raise TRexError("validate_choice: argument '{0}' can only be one of '{1}'".format(arg_name, choices))


# throws TRexError if not exactly one argument is present
def verify_exclusive_arg (args_list):
    if not (len(list(filter(lambda x: x is not None, args_list))) == 1):
        raise TRexError('exactly one parameter from {0} should be provided'.format(args_list))

def listify (x):
    if isinstance(x, list):
        return x
    elif isinstance(x, tuple):
        return list(x)
    else:
        return [x]


def listify_if_int (x):
    return [x] if isinstance(x, int) else x


# shows as 'N/A', but does not let any compares for user to not mistake in automation
class StatNotAvailable(str):
    def __new__(cls, value, *args, **kwargs):
        cls.stat_name = value
        return super(StatNotAvailable, cls).__new__(cls, 'N/A')

    def __cmp__(self, *args, **kwargs):
        raise Exception("Stat '%s' not available at this setup" % self.stat_name)


class LRU_cache(OrderedDict):
    def __init__(self, maxlen = 20, *args, **kwargs):
        OrderedDict.__init__(self, *args, **kwargs)
        self.maxlen = maxlen

    def __setitem__(self, *args, **kwargs):
        OrderedDict.__setitem__(self, *args, **kwargs)
        if len(self) > self.maxlen:
            self.popitem(last = False)
