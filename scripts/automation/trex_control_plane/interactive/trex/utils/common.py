import os
import sys
import string
import random
import time
import socket
import struct
import re
import cProfile, pstats

from scapy.utils import *
from scapy.utils6 import *
from ..common.trex_types import listify, validate_type

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

def parse_ports_from_profiles(ports):
    """
    Parse distinct port ids from profiles

    :parameters:
        ports : list
            list of profiles(PortProfileID)

    :return:
        list of port ids(int)
    """
    ports = listify(ports)
    port_id_list = list(set([int(port) for port in ports]))
    return port_id_list

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
    def __init__ (self, timeout_sec = None):
        self.time_begin = time.time()

        if timeout_sec != None:
            self.expr_sec = self.time_begin + timeout_sec
        else:
            self.expr_sec = None

    def has_expired (self):
        # if no timeout was set - return always false
        if self.expr_sec == None:
            return False

        return (time.time() > self.expr_sec)

    def has_elapsed (self, timegap):
        if (time.time() - self.time_begin) > timegap:
            self.time_begin = time.time()
            return True
        else:
            return False

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


def safe_ord (c):
    if type(c) is str:
        return ord(c)
    elif type(c) is int:
        return c
    else:
        raise TypeError("Cannot convert: {0} of type: {1}".format(c, type(c)))

def _buffer_to_num(str_buffer):
    validate_type('str_buffer', str_buffer, bytes)
    res=0
    for i in str_buffer:
        res = res << 8
        res += safe_ord(i)
    return res


def ipv4_str_to_num (ipv4_buffer):
    validate_type('ipv4_buffer', ipv4_buffer, bytes)
    assert len(ipv4_buffer)==4, 'Size of ipv4_buffer is not 4'
    return _buffer_to_num(ipv4_buffer)

def mac_str_to_num (mac_buffer):
    validate_type('mac_buffer', mac_buffer, bytes)
    assert len(mac_buffer)==6, 'Size of mac_buffer is not 6'
    return _buffer_to_num(mac_buffer)

def int2mac(val):
    mac_arr = []
    for _ in range(6):
        val, char = divmod(val, 256)
        mac_arr.insert(0, '%02x' % char)
    return ':'.join(mac_arr)

def int2ip(val):
    ip_arr = []
    for _ in range(4):
        val, char = divmod(val, 256)
        ip_arr.insert(0, '%s' % char)
    return '.'.join(ip_arr)

def ip2int(ip):
    return ipv4_str_to_num(is_valid_ipv4_ret(ip))

def ipv62int(ip):
    ''' Convert ipv6 to 2 numbers, 8 bytes each'''
    ip = inet_pton(socket.AF_INET6, ip)
    a, b =  struct.unpack("!QQ", ip)
    return a, b

def int2ipv6(a, b):
    ''' Convert the two 8bytes numbers to IPv6 human readable string '''
    return socket.inet_ntop(socket.AF_INET6, struct.pack('!QQ', a, b))

def increase_mac(mac_str, val = 1):
    if ':' in mac_str:
        mac_str = mac2str(mac_str)
    mac_val = mac_str_to_num(mac_str)
    return int2mac((mac_val + val) % (1 << 48))

def increase_ip(ip_str, val = 1):
    ip_val = ipv4_str_to_num(is_valid_ipv4_ret(ip_str))
    return int2ip((ip_val + val) % (1 << 32))

def increase_ipv6(ipv6, val = 1):
    ''' Increase `ipv6` address by `val`, notice this will increase only the lower 8 bytes. '''
    assert is_valid_ipv6(ipv6)
    a, b = ipv62int(ipv6)
    return int2ipv6(a, b + val)

# RFC 3513
def generate_ipv6(mac_str, prefix = 'fe80'):
    mac_arr = mac_str.split(':')
    assert len(mac_arr) == 6, 'mac should be in format of 11:22:33:44:55:66, got: %s' % mac_str
    return '%s::%s' % (prefix, in6_mactoifaceid(mac_str).lower())

# RFC 4291
def generate_ipv6_solicited_node(ipv6):
    ipv6_buf = is_valid_ipv6_ret(ipv6)
    solic_buf = in6_getnsma(ipv6_buf)
    return inet_ntop(socket.AF_INET6, solic_buf)

# RFC 1972
# return multicast mac based on ipv6 ff02::1 -> 33:33:00:00:00:01
def multicast_mac_from_ipv6(ipv6):
    ipv6_buf = is_valid_ipv6_ret(ipv6)
    return in6_getnsmac(ipv6_buf)


def is_valid_ipv4_ret(ip_addr):
    """
    Return buffer in network order
    """
    if  type(ip_addr) == bytes and len(ip_addr) == 4:
        return ip_addr

    if  type(ip_addr)== int:
        ip_addr = socket.inet_ntoa(struct.pack("!I", ip_addr))

    try:
        return socket.inet_pton(socket.AF_INET, ip_addr)
    except AttributeError:  # no inet_pton here, sorry
        return socket.inet_aton(ip_addr)
    except socket.error:  # not a valid address
        raise TypeError('Not valid IPv4 format: %s' % ip_addr);


def is_valid_ipv6_ret(ipv6_addr):
    """
    Return buffer in network order
    """
    if type(ipv6_addr) == bytes and len(ipv6_addr) == 16:
        return ipv6_addr
    try:
        return socket.inet_pton(socket.AF_INET6, ipv6_addr)
    except AttributeError:  # no inet_pton here, sorry
        raise TypeError('No inet_pton function available')
    except:
        raise TypeError('Not valid IPv6 format: %s' % ipv6_addr)

def compress_ipv6(ipv6_addr):
    """
    Compress ipv6 address, i.e: compress_ipv6(0001:0001:0000:0000:0000:1111:0000:0000') -> '1:1::1111:0:0'
    """
    ipv6_addr = is_valid_ipv6_ret(ipv6_addr)
    return socket.inet_ntop(socket.AF_INET6, ipv6_addr)

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

def filter_none(seq):
    return filter(lambda x: x is not None, seq)

def all_none(seq):
    return not filter_none(seq)

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


