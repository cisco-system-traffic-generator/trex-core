"""
RPC Protocol for IPC.
"""

import importlib
import traceback
import threading
import uuid
import time



class RPCMessage:
    global_lock = threading.Lock()
    message_id = 0

    """Represents a Message between a WirelessManager and a WirelessWorker."""

    def __init__(self, type):
        """Construct a RPCMessage.

        Args:
            type: the type of the RPCMessage e.g. 'cmd', 'reps', ...
        """
        self.type = type

    def __getstate__(self):
        """Return state values to be pickled."""
        return (self.type,)

    def __setstate__(self, state):
        self.type, = state

    def create_unique_id(self):
        # Some Python implementation have a buggy uuid when used in multithreaded scenarios
        with RPCMessage.global_lock:
            RPCMessage.message_id += 1
            return str(RPCMessage.message_id)


class RPCExceptionReport(RPCMessage):
    """RPCExceptionReport represent information from a remote process to a WirelessManager reporting an Exception that occured in the process."""
    TYPE = "exception"
    NUM_STATES = 2  # 3 specific fields to be pickled

    def __init__(self, exception):
        """Construct a RPCExceptionReport.

        Args:
            exception: exception to report
        """
        super().__init__(RPCExceptionReport.TYPE)
        self.formatted = traceback.format_exception(
            type(exception), exception, exception.__traceback__)
        self.timestamp = time.time()

    def __getstate__(self):
        """Return state values to be pickled."""
        return (self.formatted, self.timestamp) + super().__getstate__()

    def __setstate__(self, state):
        """Restore state from the unpickled state values."""
        super().__setstate__(state[RPCExceptionReport.NUM_STATES:])
        self.formatted, self.timestamp = state[:RPCExceptionReport.NUM_STATES]

    def __str__(self):
        s = "Exception:\n{}\n".format(time.ctime(self.timestamp))
        for line in self.formatted:
            s += line
        return s


class RPCResponse(RPCMessage):
    """Represents a Response from a Remote Call, response from a WirelessWorker or TrafficHandler to a WirelessManager."""
    TYPE = "resp"

    SUCCESS = 1
    ERROR = 2

    NUM_STATES = 3  # 3 specific fields to be pickled

    def __init__(self, id, code, ret):
        """Construct a RPCResponse.

        Args:
            id: the id of the response, which should correspond (equal) the id of the RPC call.
            code: success code or error code
            ret: the return value(s) of the call
        """
        super().__init__(RPCResponse.TYPE)
        self.id = id
        self.code = code
        self.ret = ret

    def __getstate__(self):
        """Return state values to be pickled."""
        return (self.id, self.code, self.ret) + super().__getstate__()

    def __setstate__(self, state):
        """Restore state from the unpickled state values."""
        super().__setstate__(state[RPCResponse.NUM_STATES:])
        self.id, self.code, self.ret = state[:RPCResponse.NUM_STATES]
