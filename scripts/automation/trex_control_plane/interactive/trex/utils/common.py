import os
import sys
import string
import random
import time
import socket
import re
import cProfile, pstats

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

# actually first list minus second
def list_difference (l1, l2):
    return list(filter(lambda x: x not in l2, l1))

# symmetric diff
def list_xor(l1, l2):
    return list(set(l1) ^ set(l2))

def is_sub_list (l1, l2):
    return set(l1) <= set(l2)

# splits a timestamp in seconds to sec/usec
def sec_split_usec (ts):
    return int(ts), int( (ts - int(ts)) * 1e6 )
    
    
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
    except (socket.error, TypeError):
        return False

def is_valid_ipv6(addr):
    try:
        socket.inet_pton(socket.AF_INET6, addr)
        return True
    except (socket.error, TypeError):
        return False

def is_valid_mac (mac):
    return bool(re.match("[0-9a-f]{2}([-:])[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$", mac.lower()))

def list_remove_dup (l):
    tmp = list()
    
    for x in l:
        if x not in tmp:
            tmp.append(x)
            
    return tmp
 

def has_dup (l):
    return len(l) > len(set(l))


def bitfield_to_list (bf):
    rc = []
    bitpos = 0

    while bf > 0:
        if bf & 0x1:
            rc.append(bitpos)
        bitpos += 1
        bf = bf >> 1

    return rc

def set_window_always_on_top (title):
    # we need the GDK module, if not available - ignroe this command
    try:
        if sys.version_info < (3,0):
            from gtk import gdk
        else:
            #from gi.repository import Gdk as gdk
            return

    except ImportError:
        return

    # search the window and set it as above
    root = gdk.get_default_root_window()

    for id in root.property_get('_NET_CLIENT_LIST')[2]:
        w = gdk.window_foreign_new(id)
        if w:
            name = w.property_get('WM_NAME')[2]
            if title in name:
                w.set_keep_above(True)
                gdk.window_process_all_updates()
                break

                
def bitfield_to_str (bf):
    lst = bitfield_to_list(bf)
    return "-" if not lst else ', '.join([str(x) for x in lst])
    

# BPS L1 from pps and BPS L2
def calc_bps_L1 (bps, pps):
    if (pps == 0) or (bps == 0):
        return 0

    factor = bps / (pps * 8.0)
    return bps * ( 1 + (20 / factor) )


def round_float (f):
    return float("%.2f" % f) if type(f) is float else f

def try_int(i):
    try:
        return int(i)
    except:
        return i

# a2 before a10 in sorting
# https://blog.codinghorror.com/sorting-for-humans-natural-sort-order/
def natural_sorted_key(val):
    return [int(c) if c.isdigit() else c for c in re.split('(\d+)', val)]


# with Profiler_Context():
#     <profiled code>
class Profiler_Context:
    def __init__(self, lines = None, sortby = None):
        self.lines = lines
        #default_sort = 'cumulative' # func time includes inner funcs
        default_sort = 'time'     # func time excludes inner funcs
        self.sortby = sortby or default_sort

    def __enter__(self):
        self.pr = cProfile.Profile()
        self.pr.enable()

    def __exit__(self, *a, **k):
        self.pr.disable()
        ps = pstats.Stats(self.pr).sort_stats(self.sortby)
        if self.lines:
            ps.print_stats(self.lines)
        else:
            ps.print_stats()


