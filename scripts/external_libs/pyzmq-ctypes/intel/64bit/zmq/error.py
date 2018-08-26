'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''

from zmq.constants import *
from zmq.bindings import *

class ZMQBaseError(Exception): pass

class ZMQError(ZMQBaseError):
    def __init__(self, errno):
        e = zmq_strerror(errno)
        if not isinstance(e, str):
            e = e.decode()
        self.strerror = e
        self.errno = errno

    def __str__(self):
        return self.strerror

class ContextTerminated(ZMQError): pass
class Again(ZMQError): pass
class InterruptedSystemCall(ZMQError): pass
class ZMQBindError(ZMQBaseError): pass

def _check_zmq_errno():
    errno = get_errno()
    if errno == EAGAIN:
        raise Again(errno)
    elif errno == EINTR:
        raise InterruptedSystemCall(errno)
    elif errno == ETERM:
        raise ContextTerminated(errno)
    elif errno != 0:
        raise ZMQError(errno)
    else:
        raise Exception('Unknown exception')

def _check_rc(rc):
    if rc == -1:
        _check_zmq_errno()

def _check_ptr(ptr):
    if ptr is None:
        _check_zmq_errno()
