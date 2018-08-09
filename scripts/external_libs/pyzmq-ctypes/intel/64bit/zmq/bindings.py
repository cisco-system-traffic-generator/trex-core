'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''

import os
from zmq.constants import *
from ctypes import *


cur_dir = os.path.abspath(os.path.dirname(__file__))
libzmq = CDLL(os.path.join(cur_dir, 'libzmq.so'), use_errno=True)

assert(libzmq)


libzmq.zmq_version.restype = None
libzmq.zmq_version.argtypes = [POINTER(c_int)]*3

# Error number as known by the 0MQ library

libzmq.zmq_strerror.restype = c_char_p
libzmq.zmq_strerror.argtypes = [c_int]

# 0MQ infrastructure

libzmq.zmq_ctx_new.restype = c_void_p
libzmq.zmq_ctx_new.argtypes = None

libzmq.zmq_ctx_set.argtypes = [c_void_p, c_int, c_int]

libzmq.zmq_ctx_destroy.argtypes = [c_void_p]

# 0MQ message definition

class zmq_msg_t(Structure):
    _fields_ = [
        ('_', c_ubyte*64)
        ]

libzmq.zmq_msg_init.argtypes = [POINTER(zmq_msg_t)]

# requires a free function:
libzmq.zmq_msg_close.argtypes = [POINTER(zmq_msg_t)]
libzmq.zmq_msg_data.restype = c_void_p
libzmq.zmq_msg_data.argtypes = [POINTER(zmq_msg_t)]
libzmq.zmq_msg_size.restype = c_size_t
libzmq.zmq_msg_size.argtypes = [POINTER(zmq_msg_t)]

# 0MQ socket definition

libzmq.zmq_socket.restype = c_void_p
libzmq.zmq_socket.argtypes = [c_void_p, c_int]

libzmq.zmq_close.argtypes = [c_void_p]

libzmq.zmq_setsockopt.argtypes = [c_void_p, c_int, c_void_p, c_size_t]
libzmq.zmq_getsockopt.argtypes = [c_void_p, c_int, c_void_p, POINTER(c_size_t)]
libzmq.zmq_bind.argtypes = [c_void_p, c_char_p]
libzmq.zmq_unbind.argtypes = [c_void_p, c_char_p]
libzmq.zmq_connect.argtypes = [c_void_p, c_char_p]
libzmq.zmq_disconnect.argtypes = [c_void_p, c_char_p]
libzmq.zmq_msg_recv.argtypes = [POINTER(zmq_msg_t), c_void_p, c_int]

libzmq.zmq_send.argtypes = [c_void_p, c_void_p, c_size_t, c_int]

class zmq_pollitem_t(Structure):
    _fields_ = [
            ('socket', c_void_p),
            ('fd', c_int),
            ('events', c_short),
            ('revents', c_short)
            ]

libzmq.zmq_poll.restype = c_int
libzmq.zmq_poll.argtypes = [POINTER(zmq_pollitem_t), c_int, c_long]


def _shortcuts():
    for symbol in dir(libzmq):
        if symbol.startswith('zmq_') and not symbol in globals():
            fn = getattr(libzmq, symbol)
            globals()[symbol] = fn



_shortcuts()

