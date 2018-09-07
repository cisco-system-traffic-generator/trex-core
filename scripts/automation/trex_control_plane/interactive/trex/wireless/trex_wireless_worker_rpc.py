from .trex_wireless_rpc_message import *
from .utils.utils import load_service
from .trex_wireless_manager_private import DeviceInfo

class WorkerCall(RPCMessage):
    """Represents a Remote Call from WirelessManager to a WirelessWorker."""

    TYPE = "cmd"

    NUM_STATES = 3  # 3 specific fields to be pickled

    NAME = None  # should be defined for subclasses

    def __init__(self, worker, *args):
        """Create a WorkerCall.

        Should check for correctness of args.

        Args:
            name: name of the method to call
            args: arguments to pass to the method
            worker: worker to call
        """
        super().__init__(WorkerCall.TYPE)
        if not type(self).NAME:
            raise AttributeError(
                "A WorkerCall must have a NAME static field set.")

        # Each new RPC has a different ID
        self.id = self.create_unique_id()
        self.args = args
        self.worker = worker

    def __getstate__(self):
        """Return state values to be pickled."""
        return (self.id, type(self).NAME, self.args) + super().__getstate__()

    def __setstate__(self, state):
        """Restore state from the unpickled state values."""
        super().__setstate__(state[WorkerCall.NUM_STATES:])
        self.id, type(self).NAME, self.args = state[:WorkerCall.NUM_STATES]

    def __str__(self):
        return self.NAME

# Commands


class WorkerCall_stop(WorkerCall):
    NAME = "stop"

    def __init__(self, worker):
        super().__init__(worker)


class WorkerCall_is_on(WorkerCall):
    NAME = "is_on"

    def __init__(self, worker):
        super().__init__(worker)


# Actions

class WorkerCall_create_aps(WorkerCall):
    NAME = "create_aps"

    def __init__(self, worker, port_layer_cfg, aps):
        super().__init__(worker, port_layer_cfg, aps)


class WorkerCall_create_clients(WorkerCall):
    NAME = "create_clients"

    def __init__(self, worker, clients):
        super().__init__(worker, clients)


class WorkerCall_join_aps(WorkerCall):
    NAME = "join_aps"

    def __init__(self, worker, ids=None, max_concurrent=float('inf')):
        super().__init__(worker, ids, max_concurrent)

class WorkerCall_stop_aps(WorkerCall):
    NAME = "stop_aps"

    def __init__(self, worker, ids=None, max_concurrent=float('inf')):
        super().__init__(worker, ids, max_concurrent)
        
class WorkerCall_stop_clients(WorkerCall):
    NAME = "stop_clients"

    def __init__(self, worker, ids=None, max_concurrent=float('inf')):
        super().__init__(worker, ids, max_concurrent)


class WorkerCall_add_services(WorkerCall):
    NAME = "add_services"

    def __init__(self, worker, device_ids, service_class, wait_for_completion=False, max_concurrent=float('inf')):
        if not device_ids:
            raise ValueError("device_ids must be given")
        if not service_class:
            raise ValueError("service_class must be given")

        # check that the loading is successful
        load_service(service_class)

        super().__init__(worker, device_ids, service_class, wait_for_completion, max_concurrent)

# Packets

class WorkerCall_wrap_client_pkts(WorkerCall):
    NAME = "wrap_client_pkts"
    def __init__(self, worker, client_pkts):
        """WorkerCall for wrapping clients' packets into capwap data.
        
        Args:
            worker (WirelessWorker): worker to call
            client_pkts (dict): dict client_mac -> list of packets (Dot11)
        Returns:
            client_pkts (dict): same as input with wrapped packets (Ether / IP / UDP / CAPW DATA / Dot11 ...)
        """
        super().__init__(worker, client_pkts)
        
# Getters

class WorkerCall_get_aps_retries(WorkerCall):
    NAME = "get_aps_retries"

    def __init__(self, worker, ap_ids):
        super().__init__(worker, ap_ids)


class WorkerCall_get_ap_join_times(WorkerCall):
    NAME = "get_ap_join_times"

    def __init__(self, worker):
        super().__init__(worker)

class WorkerCall_get_ap_join_durations(WorkerCall):
    NAME = "get_ap_join_durations"

    def __init__(self, worker):
        super().__init__(worker)

class WorkerCall_get_client_join_times(WorkerCall):
    NAME = "get_client_join_times"

    def __init__(self, worker):
        super().__init__(worker)


class WorkerCall_get_client_states(WorkerCall):
    NAME = "get_client_states"

    def __init__(self, worker):
        super().__init__(worker)


class WorkerCall_get_ap_states(WorkerCall):
    NAME = "get_ap_states"

    def __init__(self, worker):
        super().__init__(worker)


class WorkerCall_get_clients_services_info(WorkerCall):
    NAME = "get_clients_services_info"

    def __init__(self, worker):
        super().__init__(worker)


class WorkerCall_get_clients_service_specific_info(WorkerCall):
    NAME = "get_clients_service_specific_info"

    def __init__(self, worker, service_name, key):
        super().__init__(worker, service_name, key)


class WorkerCall_set_clients_service_specific_info(WorkerCall):
    NAME = "set_clients_service_specific_info"

    def __init__(self, worker, service_name, key, value):
        super().__init__(worker, service_name, key, value)


class WorkerCall_get_aps_service_specific_info(WorkerCall):
    NAME = "get_aps_service_specific_info"

    def __init__(self, worker, service_name, key):
        super().__init__(worker, service_name, key)


class WorkerCall_set_aps_service_specific_info(WorkerCall):
    NAME = "set_aps_service_specific_info"

    def __init__(self, worker, service_name, key, value):
        super().__init__(worker, service_name, key, value)


class WorkerCall_get_clients_done_count(WorkerCall):
    NAME = "get_clients_done_count"

    def __init__(self, worker, service_name):
        super().__init__(worker, service_name)


class WorkerCall_get_aps_services_info(WorkerCall):
    NAME = "get_aps_services_info"

    def __init__(self, worker):
        super().__init__(worker)
