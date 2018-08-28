import os
import signal
import errno
from functools import wraps
import binascii
import threading
import multiprocessing
import importlib

def get_capture_port(port):
    """Return the TRex ipc file location for given port."""
    return "ipc://{}".format("/tmp/trex_capture_port_{}".format(port))

def mac_str(mac):
    hex = binascii.hexlify(mac)
    return hex.decode('ascii')


def mac_split(mac_str):
    return ':'.join(a+b for a, b in zip(mac_str[::2], mac_str[1::2]))


def mac2str(mac):
    hex = binascii.hexlify(mac)
    s = hex.decode('ascii')
    return ':'.join(s[i:i + 2] for i in range(0, len(s), 2))

class SyncronizedConnection:
    """A thread safe (for writing) wrapper on Connection (pipe end).

    Allows one consumer and multiple procducer on the same end pipe.
    """

    def __init__(self, connection):
        """Constructs a SynchronizedConnection from a multiprocessing.connection.Connection

        Args:
            connection: multiprocessing.connection.Connection that needs synchronization
        """
        self.connection = connection
        self.fileno = connection.fileno
        self.lock = threading.Lock()

    def send(self, obj):
        with self.lock:
            self.connection.send(obj)

    def recv(self):
        return self.connection.recv()


def round_robin_list(num, to_distribute):
    """Return a list of 'num' elements from 'to_distribute' that are evenly distributed

    Args:
        num: number of elements in requested list
        to_distribute: list of element to be put in the requested list

    >>> round_robin_list(5, ['first', 'second'])
    ['first', 'second', 'first', 'second', 'first']
    >>> round_robin_list(3, ['first', 'second'])
    ['first', 'second', 'first']
    >>> round_robin_list(4, ['first', 'second', 'third'])
    ['first', 'second', 'third', 'first']
    >>> round_robin_list(1, ['first', 'second'])
    ['first']
    """
    if not to_distribute:
        return []

    quotient = num // len(to_distribute)
    remainder = num - quotient * len(to_distribute)

    assignment = to_distribute * quotient
    assignment.extend(to_distribute[:remainder])
    return assignment


class TimeoutError(Exception):
    """An Exception representing a Timeout Event."""
    pass


def timeout(timeout_sec, timeout_callback=None):
    """Decorator for timing out a function after 'timeout_sec' seconds.
    To be used like, for a 7 seconds timeout:
    @timeout(7, callback):
    def foo():
        ...

    Args:
        timeout_sec: duration to wait for the function to return before timing out
        timeout_callback: function to call in case of a timeout
    """
    def decorator(f):
        def timeout_handler(signum, frame):
            raise TimeoutError(os.strerror(errno.ETIME))

        def wrapper(*args, **kwargs):
            signal.signal(signal.SIGALRM, timeout_handler)
            signal.alarm(timeout_sec)
            result = None
            try:
                result = f(*args, **kwargs)
            except TimeoutError:
                if timeout_callback:
                    timeout_callback()
                pass
            finally:
                signal.alarm(0)
            return result
        return wraps(f)(wrapper)

    return decorator


def remote_call(method):
    """Decorator to set a method as callable remotely (remote call)."""
    method.remote_call = True
    return method


def thread_safe_remote_call(method):
    """Decorator to inform that a method for a remote call is thread safe with respect of other remote calls."""
    method.thread_safe = True
    return method


def RemoteCallable(cls):
    """Class Decorator for enabling @remote_call methods to be callable.
    Set the methods in the instance dictionary, and can be accessed like :
        function = self.remote_calls["function name"]
    Then when receiving a call command, the function can be looked up like this, and then called.
    """
    class Wrapper(cls):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.remote_calls = {}
            self.thread_safe_remote_calls = {}
            cmds = [cmd for cmd in cls.__dict__.values() if hasattr(
                cmd, "remote_call") and cmd.remote_call]
            thread_safe_cmds = [cmd for cmd in cls.__dict__.values() if hasattr(
                cmd, "thread_safe_remote_call") and cmd.remote_call]
            for cmd in cmds:
                self.remote_calls[cmd.__name__] = cmd
            for cmd in cmds:
                self.thread_safe_remote_calls[cmd.__name__] = cmd

    return Wrapper


def load_service(service_class):
    """Loads a WirelessService from a module name and the name of the service all
    concatenated with dot (.), e.g. wireless.services.client.ClientService
    Returns the service class (the WirelessService).

    Args:
        service_class: name of the module and class, e.g. 'wireless.services.client.client_service_association.ClientServiceAssociation'
    """

    parts = service_class.split('.')
    assert len(parts) >= 2
    service_name = parts[-1]
    module_name = '.'.join(parts[:-1])
    try:
        module = importlib.import_module(module_name)
        service = getattr(module, service_name)
    except ImportError as e:
        # error at import_module
        raise ValueError(
            "cannnot import module {}: {} ".format(module_name, e))
    except AttributeError as e:
        # error at getattr
        raise ValueError(
            "wireless service {} does not exist: {}".format(service_name, e))
    return service
