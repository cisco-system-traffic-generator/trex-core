
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

        if (rc != None) and (data != None):
            tuple_rc = namedtuple('RC', ['rc', 'data'])
            self.rc_list.append(tuple_rc(rc, data))

    def add (self, rc):
        self.rc_list += rc.rc_list

    def good (self):
        return all([x.rc for x in self.rc_list])

    def bad (self):
        return not self.good()

    def data (self):
        return [x.data if x.rc else "" for x in self.rc_list]

    def err (self):
        return [x.data if not x.rc else "" for x in self.rc_list]

    def annotate (self, desc = None, show_status = True):
        if desc:
            print format_text('\n{:<60}'.format(desc), 'bold'),
        else:
            print ""

        if self.bad():
            # print all the errors
            print ""
            for x in self.rc_list:
                if not x.rc:
                    print format_text("\n{0}".format(x.data), 'bold')

            print ""
            if show_status:
                print format_text("[FAILED]\n", 'red', 'bold')


        else:
            if show_status:
                print format_text("[SUCCESS]\n", 'green', 'bold')


def RC_OK(data = ""):
    return RC(True, data)
def RC_ERR (err):
    return RC(False, err)

