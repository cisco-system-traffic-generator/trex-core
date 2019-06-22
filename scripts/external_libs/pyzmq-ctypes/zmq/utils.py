'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''

from zmq.error import *
from zmq.error import _check_rc

import sys

if sys.version_info[0] >= 3:
    bytes = bytes
    unicode = str
    basestring = (bytes, unicode)
else:
    unicode = unicode
    bytes = str
    basestring = basestring

def cast_bytes(s, encoding='utf8', errors='strict'):
    """cast unicode or bytes to bytes"""
    if isinstance(s, bytes):
        return s
    elif isinstance(s, unicode):
        return s.encode(encoding, errors)
    else:
        raise TypeError("Expected unicode or bytes, got %r" % s)

def cast_unicode(s, encoding='utf8', errors='strict'):
    """cast bytes or unicode to unicode"""
    if isinstance(s, bytes):
        return s.decode(encoding, errors)
    elif isinstance(s, unicode):
        return s
    else:
        raise TypeError("Expected unicode or bytes, got %r" % s)

def _retry_sys_call(f, *args):
    """make a call, retrying if interrupted with EINTR"""
    while True:
        rc = f(*args)
        try:
            _check_rc(rc)
        except InterruptedSystemCall:
            continue
        else:
            break
