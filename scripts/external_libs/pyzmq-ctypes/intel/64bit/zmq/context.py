'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''

from zmq.bindings import *
from zmq.socket import *
from zmq.error import _check_rc, _check_ptr

import weakref

class Context(object):
    def __init__(self, io_threads=1):
        if not io_threads > 0:
            raise ZMQError(EINVAL)
        self.handle = zmq_ctx_new()
        _check_ptr(self.handle)
        zmq_ctx_set(self.handle, IO_THREADS, io_threads)
        self._closed = False
        self._sockets = set()

    @property
    def closed(self):
        return self._closed

    def _add_socket(self, socket):
        ref = weakref.ref(socket)
        self._sockets.add(ref)
        return ref

    def _rm_socket(self, ref):
        if ref in self._sockets:
            self._sockets.remove(ref)

    def term(self):
        rc = zmq_ctx_destroy(self.handle)
        try:
            _check_rc(rc)
        except InterruptedSystemCall:
            # ignore interrupted term
            # see PEP 475 notes about close & EINTR for why
            pass

        self.handle = None
        self._closed = True

    def destroy(self, linger = None):
        if self.closed:
            return

        sockets = self._sockets
        self._sockets = set()
        for s in sockets:
            s = s()
            if s and not s.closed:
                if linger is not None:
                    s.setsockopt(LINGER, linger)
                s.close()

        self.term()

    def socket(self, kind):
        if self._closed:
            raise ZMQError(ENOTSUP)
        return Socket(self, kind)

