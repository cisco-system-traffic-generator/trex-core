'''
Based on pyzmq-ctypes and pyzmq
Updated to work with latest ZMQ shared object

https://github.com/zeromq/pyzmq
https://github.com/svpcom/pyzmq-ctypes
'''


from zmq import error

from zmq.bindings import *
from zmq.error import *
from zmq.context import *
from zmq.socket import *

major = c_int()
minor = c_int()
patch = c_int()

zmq_version(byref(major), byref(minor), byref(patch))

__zmq_version__ = tuple((x.value for x in (major, minor, patch)))

def zmq_version():
    return '.'.join(map(str, __zmq_version__))

