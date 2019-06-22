from abc import ABC, abstractmethod, abstractproperty
import socket
import struct
import threading
from .pubsub.pubsub import PubSub, Publisher
from wireless.services.trex_wireless_service import WirelessService
from wireless.trex_wireless_manager_private import WirelessDeviceStateUpdate

class WirelessDevice(ABC):
    """A WirelessDevice is an AP or a wireless Client."""

    def __init__(self, worker, name, identifier, gateway_ip, device_info=None):
        # this is the APInfo or ClientInfo from the manager,
        # used to know which field to update to the manager
        self.device_info = device_info
        self.identifier = identifier
        self.lock = threading.RLock()
        self.name = name
        self.logger = worker.logger.getChild(self.name)
        self.pubsub = worker.pubsub
        self.config = worker.config
        # self.publisher = worker.publisher.SubPublisher(type(self).__name__)
        self.publisher = Publisher(self.pubsub, prefix_topics=self.name)
        self.services = {}  # name of service -> WirelessService
        self.__sim_processes = {} # name of service -> WirelessService
        self.services_lock = threading.RLock()
        self.services_info = {}  # name of service -> dict of information
        self.gateway_ip = gateway_ip
        if gateway_ip:
            self.gateway_ip_bytes = socket.inet_aton(gateway_ip)

        self.publish = self.publisher.publish

    def __getattribute__(self, name):
        if name in ("lock"):
            return object.__getattribute__(self, name)
        if not hasattr(self, "lock"):
            return object.__getattribute__(self, name)
        with self.lock:
            return object.__getattribute__(self, name)
            
    def __setattr__(self, name, value):
        if name in ("lock"):
            object.__setattr__(self, name, value)
            return

        if not hasattr(self, "lock"):
            object.__setattr__(self, name, value)
            return
        else:
            with self.lock:
                if name in self.__dict__ and self.device_info and name in self.device_info.__dict__:
                    # this value is being modified
                    update = WirelessDeviceStateUpdate(identifier=self.identifier, update={
                        name: value,
                    })
                    # publish as a wireless device
                    self.pubsub.publish(update, "WirelessDevice")
                object.__setattr__(self, name, value)

    @abstractproperty
    def attached_devices_macs(self):
        """Return a list of WirelessDevice macs that are directly connected to this one."""
        pass

    @abstractproperty
    def is_closed(self):
        """Return True if and only if the WirelessDevice is in a Close (Closed, Closing, ...) state."""
        pass

    @abstractproperty
    def is_running(self):
        """Return True if and only if the WirelessDevice is in a Run state."""
        pass

    def setIPAddress(self, ip_int):
        """Setter for ip address.
        Args:
            ip_int: the ipv4 address in int format
        """
        self.ip = socket.inet_ntoa(struct.pack('!I', ip_int))
        self.ip_bytes = socket.inet_aton(self.ip)

    def get_services_info(self):
        return self.services_info

    def get_service_specific_info(self, service_name, key):

        """Retrieve the information labeled 'key' from the service named 'service_name'"""
        try:
            return self.services_info[service_name][key]
        except KeyError:
            return None

    def set_service_specific_info(self, service_name, key, value):
        """Set the information labeled 'key' in the service named 'service_name' to the value 'value'"""
        if not service_name in self.services_info:
            self.services_info[service_name] = {}
        self.services_info[service_name][key] = value

    def get_done_status(self, service_name):
        try:
            return self.services_info[service_name]['done']
        except KeyError:
            return None


    def __register_subservices(self, service):
        """Register the service and all its subservices recursively.

        Args:
            service (WirelessDeviceService): service to register with its subservices
        """
        self.logger.debug("registering (sub)service %s" % service.name)
        with self.services_lock:
            self.services[service.name] = service
        for subservice in service.subservices:
            self.__register_subservices(subservice)

    def register_service(self, service, sim_process):
        """Register a service and all its subservice on a WirelessDevice.

        Args:
            service (WirelessService): to register
            sim_process (simpy.events.Process): running simpy process, obtained via 'env.process(service)'
        Returns:
            sim_process
        """
        self.logger.debug("registering service %s" % service.name)
        with self.services_lock:
            self.__sim_processes[service.name] = sim_process
        if isinstance(service, WirelessService):
            self.__register_subservices(service)
        return sim_process

    def deregister_service(self, service):
        """Deregister a service.
        
        Args:
            service (WirelessService): to deregister
        """

        self.logger.debug("deregistering service %s" % service.name)
        with self.services_lock:
            proc = self.__sim_processes[service.name]
            try:
                # stop the service
                proc.interrupt()
            except RuntimeError:
                # process already succeeded, race condition
                pass
            del self.__sim_processes[service.name]

            # stop function
            self.services[service.name].stop()
            del self.services[service.name]

    def stop_services(self, hard=False):
        """Stop all services.
        
        Args:
            hard (bool): hard stop the services (does not run 'stop' method of the services)
                default to False (gracefully stop the services)
        """
        self.logger.info("stopping all services")
        with self.services_lock:
            for proc in self.__sim_processes.values():
                try:
                    proc.interrupt()
                except RuntimeError:
                    # process already succeeded, race condition
                    pass
            self.__sim_processes.clear()

            for _, service in self.services.items():
                service.stop(hard)
            self.services.clear()