import os
import sys
import traceback

from .text_opts import *

try:
    basestring
except NameError:
    basestring = str


# basic error for API
class ASTFError(Exception):
    def __init__(self, msg):
        self.msg = str(msg)
        self.stack = traceback.extract_stack()

    def __str__(self):
        self.tb = traceback.extract_tb(sys.exc_info()[2])
        if not self.tb:
            return self.msg

        s = format_text("\n******\n", 'bold')
        s += format_text("\nException stack (most recent call last):\n\n", 'underline')

        for i, line in enumerate(self.tb):
            fname, lineno, src = os.path.split(line[0])[1], line[1], line[3]
            s += "#{:<2}    {:<50} - '{}'\n".format(len(self.tb) - i - 1, format_text(fname, 'bold') + ':' +
                                                    format_text(lineno, 'bold'), format_text(src.strip(), 'bold'))

        s += format_text('\nSummary error message:\n\n', 'underline')
        s += format_text(self.msg + '\n', 'bold')

        return s

    def brief(self):
        return self.msg


class ASTFErrorBadParamCombination(ASTFError):
    def __init__(self, func, name1, name2):
        msg = "When creating \"{0}\", must not specify both \"{1}\" and \"{2}\"".format(func, name1, name2)
        ASTFError.__init__(self, msg)


class ASTFErrorMissingParam(ASTFError):
    def __init__(self, func, name1, name2=None):
        if name2 is not None:
            msg = "When creating \"{0}\", must specify one of \"{1}\" and \"{2}\"".format(func, name1, name2)
        else:
            msg = "When creating \"{0}\", must specify \"{1}\"".format(func, name1)
        ASTFError.__init__(self, msg)


class ASTFErrorWrongType(ASTFError):
    def __init__(self, func, param, t, allow_list):
        msg = "Parameter \"{0}\" to function \"{1}\" must be of type \"{2}\"".format(param, func, t[0])
        if len(t) > 1:
            for i in range(1, len(t)):
                msg += " or {0}".format(t[i])
        if allow_list:
            msg += " or list of the allowed types"
        ASTFError.__init__(self, msg)


class ASTFErrorBadIp(ASTFError):
    def __init__(self, func, param, addr):
        msg = "Bad IP \"{0}\" for parameter {1} to function {2}".format(addr, param, func)
        ASTFError.__init__(self, msg)


class ASTFErrorBadIpRange(ASTFError):
    def __init__(self, func, param, addr, err):
        msg = "Bad IP range \"{0}\" for parameter {1} to function {2} - {3}".format(addr, param, func, err)
        ASTFError.__init__(self, msg)


class ASTFErrorBadMac(ASTFError):
    def __init__(self, func, param, mac):
        msg = "Bad MAC \"{0}\" for parameter {1} to function {2}".format(mac, param, func)
        ASTFError.__init__(self, msg)


class ASTFErrorBadTG(ASTFError):
    def __init__(self, msg):
        ASTFError.__init__(self, msg)


class ASTFErrorOverlapIP(ASTFError):
    def __init__(self, range1, range2):
        msg = "Bad IP range! \"{0}\" overlaps with \"{1}\"".format(range1, range2)
        ASTFError.__init__(self, msg)