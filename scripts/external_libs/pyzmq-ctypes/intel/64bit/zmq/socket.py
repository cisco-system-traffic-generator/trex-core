'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''

from zmq.bindings import *
from zmq.utils import *
from zmq.utils import _retry_sys_call
from zmq.error import _check_rc

import random

# collections of sockopts, based on type:
bytes_sockopts = [
    ROUTING_ID,
    SUBSCRIBE,
    UNSUBSCRIBE,
    ]
int64_sockopts = [
    AFFINITY,
    MAXMSGSIZE,
    RCVMORE,
    ]
int_sockopts = [
    BACKLOG,
    IMMEDIATE,
    LINGER,
    MECHANISM,
    MULTICAST_HOPS,
    RATE,
    RCVBUF,
    RCVHWM,
    RCVTIMEO,
    RECONNECT_IVL,
    RECONNECT_IVL_MAX,
    RECOVERY_IVL,
    SNDBUF,
    SNDHWM,
    SNDTIMEO,
    TCP_KEEPALIVE,
    TCP_KEEPALIVE_CNT,
    TCP_KEEPALIVE_IDLE,
    TCP_KEEPALIVE_INTVL,
    TYPE,
    ]


class Socket(object):
    def __init__(self, context, socket_type):
        self.context = context
        self.handle = zmq_socket(context.handle, socket_type)
        self._closed = False
        self._ref = context._add_socket(self)

    def _check_closed(self):
        if self._closed:
            return True
        if self._closed:
            raise ZMQError(ENOTSUP)

    def _check_closed_deep(self):
        """thorough check of whether the socket has been closed,
        even if by another entity (e.g. ctx.destroy).
        Only used by the `closed` property.
        returns True if closed, False otherwise
        """
        if self._closed:
            return True
        try:
            self.getsockopt(TYPE)
        except ZMQError as e:
            if e.errno == ENOTSOCK:
                self._closed = True
                return True
            raise
        return False

    @property
    def closed(self):
        return self._check_closed_deep()

    def close(self, linger = 0):
        if self._closed or self.handle is None:
            return

        if linger is not None:
            self.setsockopt(LINGER, linger)

        rc = zmq_close(self.handle)
        self._closed = True
        self.handle = None
        self.context._rm_socket(self._ref)
        _check_rc(rc)

    def bind(self, addr):
        if isinstance(addr, unicode):
            addr = addr.encode('utf8')
        rc = zmq_bind(self.handle, addr)
        _check_rc(rc)

    def unbind(self, addr):
        if isinstance(addr, unicode):
            addr = addr.encode('utf8')
        rc = zmq_unbind(self.handle, addr)
        _check_rc(rc)

    def connect(self, addr):
        if isinstance(addr, unicode):
            addr = addr.encode('utf8')
        rc = zmq_connect(self.handle, addr)
        _check_rc(rc)

    def disconnect(self, addr):
        if isinstance(addr, unicode):
            addr = addr.encode('utf8')
        rc = zmq_disconnect(self.handle, addr)
        _check_rc(rc)

    def getsockopt(self, option):
        if option in int64_sockopts:
            optval = c_int64()
        elif option in int_sockopts:
            optval = c_int32()
        else:
            raise ZMQError(EINVAL)

        optlen = c_size_t(sizeof(optval))
        _retry_sys_call(zmq_getsockopt, self.handle, option, byref(optval), byref(optlen))
        return optval.value

    def setsockopt(self, option, optval):
        if isinstance(optval, unicode):
            raise TypeError("unicode not allowed, use bytes")

        if option in bytes_sockopts:
            if not isinstance(optval, bytes):
                raise TypeError('expected bytes, got: %r' % optval)
            zmq_setsockopt(self.handle, option, optval, len(optval))
        elif option in int64_sockopts:
            if not isinstance(optval, int):
                raise TypeError('expected int, got: %r' % optval)
            optval_int64_c = c_int64(optval)
            zmq_setsockopt(self.handle, option, byref(optval_int64_c), sizeof(optval_int64_c))
        elif option in int_sockopts:
            if not isinstance(optval, int):
                raise TypeError('expected int, got: %r' % optval)
            optval_int32_c = c_int32(optval)
            zmq_setsockopt(self.handle, option, byref(optval_int32_c), sizeof(optval_int32_c))

        else:
            raise ZMQError(EINVAL)


    def send(self, data, flags=0, copy=True, track=False):
        if isinstance(data, unicode):
            raise TypeError("Message must be in bytes, not an unicode Object")

        rc = zmq_send(self.handle, c_char_p(data), len(data), c_int(flags))
        _check_rc(rc)

    def recv(self, flags=0, copy=True, track=False):

        zmq_msg = byref(zmq_msg_t())
        zmq_msg_init(zmq_msg)

        try:
            _retry_sys_call(zmq_msg_recv, zmq_msg, self.handle, flags)
        except Exception:
            zmq_msg_close(zmq_msg)
            raise

        data = zmq_msg_data(zmq_msg)
        size = zmq_msg_size(zmq_msg)
        buf = string_at(data, size)
        rc = zmq_msg_close(zmq_msg)
        _check_rc(rc)
        return buf

    recv_string = recv

