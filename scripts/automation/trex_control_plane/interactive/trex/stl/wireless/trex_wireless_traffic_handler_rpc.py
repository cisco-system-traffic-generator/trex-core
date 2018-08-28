from .trex_wireless_rpc_message import *


class TrafficHandlerCall(RPCMessage):
    """Represents a Remote Call from WirelessManager to a WirelessWorker."""

    TYPE = "cmd"

    NUM_STATES = 3

    NAME = None  # should be defined for subclasses

    def __init__(self, *args):
        """Create a TrafficHandlerCall.

        Args:
            name: name of the method to call
            args: arguments to pass to the method
        """
        super().__init__(TrafficHandlerCall.TYPE)
        self.id = self.create_unique_id()
        self.args = args

    def __getstate__(self):
        """Return state values to be pickled."""
        return (self.id, type(self).NAME, self.args) + super().__getstate__()

    def __setstate__(self, state):
        """Restore state from the unpickled state values."""
        super().__setstate__(state[TrafficHandlerCall.NUM_STATES:])
        self.id, type(
            self).NAME, self.args = state[:TrafficHandlerCall.NUM_STATES]


class TrafficHandlerCall_stop(TrafficHandlerCall):
    """RPC Call to TrafficHandler for method 'stop'.
        See TrafficHandler for documentation.
    """
    NAME = "stop"

    def __init__(self):
        super().__init__()

class TrafficHandlerCall_route_macs(TrafficHandlerCall):
    """RPC Call to TrafficHandler for method 'route_macs'.
    See TrafficHandler for documentation.
    """
    NAME = "route_macs"

    def __init__(self, mac_to_connection_map):
        assert(isinstance(mac_to_connection_map, dict))
        super().__init__(mac_to_connection_map)
