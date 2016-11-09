import os
import sys
import string
import random
import time
import socket

try:
    import pwd
except ImportError:
    import getpass
    pwd = None

using_python_3 = True if sys.version_info.major == 3 else False

def get_current_user():
  if pwd:
      return pwd.getpwuid(os.geteuid()).pw_name
  else:
      return getpass.getuser()


def user_input():
    if using_python_3:
        return input()
    else:
        # using python version 2
        return raw_input()


class random_id_gen:
    """
    Emulated generator for creating a random chars id of specific length

    :parameters:
        length : int
            the desired length of the generated id

            default: 8

    :return:
        a random id with each next() request.
    """
    def __init__(self, length=8):
        self.id_chars = string.ascii_lowercase + string.digits
        self.length = length

    def next(self):
        return ''.join(random.choice(self.id_chars) for _ in range(self.length))

    __next__ = next


# try to get number from input, return None in case of fail
def get_number(input):
    try:
        return long(input)
    except:
        try:
            return int(input)
        except:
            return None

def list_intersect(l1, l2):
    return list(filter(lambda x: x in l2, l1))

def list_difference (l1, l2):
    return list(filter(lambda x: x not in l2, l1))

def is_sub_list (l1, l2):
    return set(l1) <= set(l2)

# a simple passive timer
class PassiveTimer(object):

    # timeout_sec = None means forever
    def __init__ (self, timeout_sec):
        if timeout_sec != None:
            self.expr_sec = time.time() + timeout_sec
        else:
            self.expr_sec = None

    def has_expired (self):
        # if no timeout was set - return always false
        if self.expr_sec == None:
            return False

        return (time.time() > self.expr_sec)

def is_valid_ipv4 (addr):
    try:
        socket.inet_pton(socket.AF_INET, addr)
        return True
    except socket.error:
        return False
