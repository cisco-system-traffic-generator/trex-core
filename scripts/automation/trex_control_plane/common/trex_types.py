
from collections import namedtuple
from common.text_opts import *

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# simple class to represent complex return value
class RC():

    def __init__ (self, rc = None, data = None):
        self.rc_list = []

        if (rc != None):
            tuple_rc = namedtuple('RC', ['rc', 'data'])
            self.rc_list.append(tuple_rc(rc, data))

    def __nonzero__ (self):
        return self.good()

    def add (self, rc):
        self.rc_list += rc.rc_list

    def good (self):
        return all([x.rc for x in self.rc_list])

    def bad (self):
        return not self.good()

    def data (self):
        d = [x.data if x.rc else "" for x in self.rc_list]
        return (d if len(d) != 1 else d[0])

    def err (self):
        e = [x.data if not x.rc else "" for x in self.rc_list]
        return (e if len(e) != 1 else e[0])

    def __str__ (self):
        return str(self.data()) if self else str(self.err())

    def prn_func (self, msg, newline = True):
        if newline:
            print msg
        else:
            print msg,

    def annotate (self, log_func = None, desc = None, show_status = True):

        if not log_func:
            log_func = self.prn_func

        if desc:
            log_func(format_text('\n{:<60}'.format(desc), 'bold'), newline = False)
        else:
            log_func("")

        if self.bad():
            # print all the errors
            print ""
            for x in self.rc_list:
                if not x.rc:
                    log_func(format_text("\n{0}".format(x.data), 'bold'))

            print ""
            if show_status:
                log_func(format_text("[FAILED]\n", 'red', 'bold'))


        else:
            if show_status:
                log_func(format_text("[SUCCESS]\n", 'green', 'bold'))


def RC_OK(data = ""):
    return RC(True, data)

def RC_ERR (err):
    return RC(False, err)

