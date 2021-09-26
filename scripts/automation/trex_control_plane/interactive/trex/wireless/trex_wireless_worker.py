import cProfile
import io
import queue
import time
import traceback
import threading
import socket
import logging
import multiprocessing
import multiprocessing.connection
from concurrent.futures import ThreadPoolExecutor
import builtins
import simpy

from trex.common import *
from .logger import *
from .trex_wireless_ap import *
from .trex_wireless_client import *
from .trex_wireless_worker_rpc import *
from .trex_wireless_ap_state import *
from .trex_wireless_client_state import *
from .services.trex_stl_ap import *
from .utils.utils import SynchronizedStore, SynchronizedServicePipe
from .utils.utils import remote_call, RemoteCallable, SyncronizedConnection, thread_safe_remote_call, mac2str, load_service
from scapy.contrib.capwap import CAPWAP_PKTS
from .trex_wireless_packet import Packet
from .services.trex_stl_ap import ServiceAP
from .services.trex_wireless_device_service import WirelessDeviceService
from .services.general.general_service import GeneralService
import select

wait_for_pkt = 0.1
event_polling_interval = 0.01


def get_ap_per_port(aps):
    """Build a dictionnary port_id -> aps belonging on this port_id."""
    ap_per_port = {}
    for ap in aps:
        if ap.port_id in ap_per_port:
            ap_per_port[ap.port_id].append(ap)
        else:
            ap_per_port[ap.port_id] = [ap]
    return ap_per_port

@RemoteCallable
class WirelessWorker(multiprocessing.Process):
    """
    An WirelessWorker is a process simulating a set of APs and all Clients attached to theses APs.
    It is responsible for the simulation of these APs and Clients for the join processes and other tasks.
    It should be instanciated and controlled by an WirelessManager.
    """

    def __init__(self, worker_id, cmd_pipe, pkt_pipe, port_id, config, ssl_ctx, log_queue, log_level, log_filter, pubsub, parent_publisher=None):
        '''Construct a WirelessWorker.

        Args:
            worker_id: identifier of the worker
            cmd_pipe: pipe connection for commands (and responses) from the manager
            pkt_pipe: pipe connection for packets from/to manager
            port_id: trex port id
            config: trex wireless parsed config
            ssl_ctx: context ssl
            log_queue: queue for log messages
            log_level: level of logs
            log_filter: log filter
            pubsub: PubSub reference
            parent_publisher: if present, the pubsub parent publisher
                all pubsub messages will be published in a subtopic of the parent's
        '''
        super().__init__()
        self.worker_id = worker_id
        self.name = "WirelessWorker_#{}".format(worker_id)
        self.cmd_pipe = cmd_pipe
        self._pkt_pipe = pkt_pipe
        self.stop_queues = {}  # singalling queues to thread for stop
        self.threads = {}
        self.port_id = port_id
        self.config = config
        self.ssl_ctx = ssl_ctx
        self.stopped = True

        # ap lookup 'tables'
        self.aps_lock = threading.RLock()
        self.ap_by_name = {}
        self.ap_by_mac = {}
        self.ap_by_ip = {}
        self.aps_by_udp_port = {}
        self.ap_by_radio_mac = {}
        self.aps = []

        # client lookup 'tables'
        self.clients_lock = threading.RLock()
        self.client_by_id = {}
        self.clients = []

        # logging
        self._log_queue = log_queue
        self._log_level = log_level
        self._log_filter = log_filter
        self.logger = None

        # pubsub
        self.pubsub = pubsub
        if parent_publisher:
            self.publisher = parent_publisher.SubPublisher("Worker")
        else:
            self.publisher = pubsub.Publisher("Worker")
        self.publish = self.publisher.publish

        self.filters = {}
        self.services = {}  # for now, service_name -> GeneralService instance
        self.__service_processes = {}  # service name => simpyprocess for GeneralService
        self.stl_services = {}  #
        self.services_lock = threading.Lock()
        self.filter = None


        self.__stop_rpc_message_id = None  # id of the stop message received from manager


    def init(self):
        """Initializes the worker with process dependent attributes.
        To be launched in a new process (in run() method).
        """
        # clearing handlers
        handlers = list(logging.getLogger().handlers)
        for h in handlers:
            logging.getLogger().removeHandler(h)

        # sim
        self.env = simpy.rt.RealtimeEnvironment(factor=1, strict=False)
        # logging
        self.logger = get_queue_logger(self._log_queue, type(
            self).__name__, self.name, self._log_level, self._log_filter)
        # command
        self.cmd_pipe_lock = threading.Lock()  # write lock
        # pkt
        self.pkt_pipe = SyncronizedConnection(self._pkt_pipe)

        # for event passing between threads and simpy run loop
        self.event_store = SynchronizedStore(self.env)

        # dictionnary topic -> list of subscriptions constructed using function 'register_sub'
        # used for local events bypassing pubsub's subscriptions for WirelessService
        # for performance
        self.topics_to_subs = {}


    def run(self):
        """Start the WirelessWorker."""
        # Threads :
        #   sim (simpy processes run loop)
        #   management (waiting for commands from manager)
        #   worker_traffic_handler (managing traffic from and to the traffic_handler process)

        try:
            self.init()

            self.logger.info("started, PID %s" % os.getpid())
            self.publish("started")

            # initiating threads, with a stop queue that serves as signaling for stopping
            q = multiprocessing.Queue(maxsize=1)
            self.threads["sim"] = threading.Thread(
                target=self.__sim, args=(q,))
            self.stop_queues['sim'] = q

            q = multiprocessing.Queue(maxsize=1)
            self.threads["management"] = threading.Thread(
                target=self.__management, args=(q,))
            self.stop_queues['management'] = q

            q = multiprocessing.Queue(maxsize=1)
            self.threads["traffic_handler"] = threading.Thread(
                target=self.__worker_traffic_handler, args=(q,))
            self.stop_queues['traffic_handler'] = q

            # launch threads as daemons
            for _, thread in self.threads.items():
                thread.daemon = True
                thread.start()

            # wait for threads
            for _, thread in self.threads.items():
                thread.join()

            self.logger.info("{} stopped".format(self.name))

        except KeyboardInterrupt:
            # wait for threads
            for _, thread in self.threads.items():
                thread.join()

            self.logger.info(
                "{} stopped".format(self.name))
            pass
        except Exception as e:
            if not self.stopped:
                report = RPCExceptionReport(e)
                with self.cmd_pipe_lock:
                    self.cmd_pipe.send(report)
        return

    def _get_ap_by_id(self, ap_id):
        """Return the attached ap with given id, which can be the ip, mac, name, or radio mac."""
        if isinstance(ap_id, AP):
            return ap_id

        with self.aps_lock:
            # check in all lookup tables
            if ap_id in self.ap_by_name:
                return self.ap_by_name[ap_id]
            elif ap_id in self.ap_by_mac:
                return self.ap_by_mac[ap_id]
            elif ap_id in self.ap_by_ip:
                return self.ap_by_ip[ap_id]
            elif ap_id in self.ap_by_radio_mac:
                return self.ap_by_radio_mac[ap_id]
            else:
                # ap ips may change or be set by dhcp
                for ap in self.aps:
                    if ap.ip == ap_id:
                        self.ap_by_ip[ap.ip] = ap
                        return ap
        raise ValueError('AP with id %s does not exist! %s' % (ap_id, str(self.ap_by_mac)))

    def device_by_mac(self, device_mac):
        """Return the attached device with given id (mac)."""
        if device_mac in self.ap_by_mac:
            return self.ap_by_mac[device_mac]
        else:
            return self.client_by_id[device_mac]

    def is_ap_ip(self, ip):
        """Return True iff 'ip' is the ip address of an attached ip"""
        with self.aps_lock:
            if ip in self.ap_by_ip:
                return True
            else:
                return ip in [ap.ip for ap in self.aps]

    def _get_clients_by_ids(self, ids):
        """Return the attached clients with given ids (mac or ip)."""
        with self.clients_lock:
            clients = []
            for id in ids:
                if id in self.client_by_id:
                    clients.append(self.client_by_id[id])
        return clients

    def _get_client_by_id(self, id):
        with self.clients_lock:
            if id in self.client_by_id:
                return self.client_by_id[id]
        return None

    @remote_call
    def create_clients(self, client_infos):
        """Create Clients on worker.

        Args:
            client_infos: list of ClientInfo to create on workers
        """

        def add_client_infos(client):
            with self.clients_lock:
                self.client_by_id[client_info.mac] = client
                if client_info.ip:
                    self.client_by_id[client_info.ip] = client
                self.clients.append(client)

        def create_client():
            """"Create one client on worker."""
            try:
                self.logger.debug("creating client %s)" %
                                  (client_info.mac))
                with self.aps_lock:
                    ap = self.ap_by_mac[client_info.ap_info.mac]
                    client = APClient(self, client_info.mac,
                                      client_info.ip, ap, client_info.gateway_ip, client_info)
                    ap.clients.append(client)
                add_client_infos(client)
            except KeyError:
                pass

        for client_info in client_infos:
            create_client()

    @remote_call
    def create_aps(self, port_layer_cfg, aps):
        """Create APs on worker.

        Args:
            port_layer_cfg: trex port configuration (from handler)
            aps: list of _AP
        """

        def add_ap_infos(ap):
            # store ap in lookup tables
            with self.aps_lock:
                self.ap_by_name[ap.name] = ap
                self.ap_by_mac[ap.mac] = ap
                if _ap.ip:
                    self.ap_by_ip[ap.ip] = ap

                if ap.udp_port not in self.aps_by_udp_port:
                    self.aps_by_udp_port[ap.udp_port] = []
                self.aps_by_udp_port[ap.udp_port].append(ap)
                self.ap_by_radio_mac[ap.radio_mac] = ap
                self.aps.append(ap)

        def create_ap():
            """Create AP on worker."""
            if _ap.wlc_ip:
                self.logger.debug("creating ap (%s %s) on wlc %s" %
                                  (_ap.ip, _ap.mac, _ap.wlc_ip))
            else:
                self.logger.debug("creating ap (%s %s)" % (_ap.ip, _ap.mac))

            ap = AP(self, self.ssl_ctx, port_layer_cfg, _ap.port_id, _ap.mac, _ap.ip, _ap.udp_port,
                    _ap.radio_mac, _ap.wlc_ip, _ap.gateway_ip, _ap.ap_mode, _ap.rsa_ca_priv_file, _ap.rsa_priv_file, _ap.rsa_cert_file, _ap)

            # store ap info
            add_ap_infos(ap)

            ap._create_ssl
            ap.logger.debug('created')

        for _ap in aps:
            create_ap()

    # @remote_call
    def remove_ap(self, ap_id):
        raise NotImplementedError

    # @remote_call
    def remove_client(self):
        raise NotImplementedError

    @remote_call
    def add_services(self, device_ids, service_class, wait_for_completion=False, max_concurrent=float('inf')):
        """Add a service on a list of wireless devices and launch them.
        Two same services cannot run on the same device at the same time.

        Args:
            device_ids: mac addresses of the wireless devices (AP or APClient) to launch the service on
            service_class: name of the service class, including its full module path, subclass of WirelessService, defined in the module file, e.g. wireless.services.client.ClientServiceAssociation
            wait_for_completion: flag that indicates if the method should wait until services completion to return.
            max_concurrent: maximum number of concurrent service on this worker for the given service
                if the number of services is greater than max_concurrent,
                some services will wait for the other to finish before starting
        """

        service = load_service(service_class)

        wait_events = []  # events to wait on before returning

        if issubclass(service, GeneralService):
            # this service is to be run once with multiple devices attached to it
            devices = []
            for id in device_ids:
                devices.append(self.device_by_mac(id))

            done_event = None
            if wait_for_completion:
                # only one event to wait for
                done_event = threading.Event()
                wait_events = [done_event]

            new_service = service(devices=devices, env=self.env, tx_conn=service.Connection(
                self.pkt_pipe), topics_to_subs=self.topics_to_subs, done_event=done_event)

            self.logger.debug("registering service %s" % new_service.name)
            self.__register_general_service(new_service, self.env.process(new_service._run_until_interrupt()))

        else:
            # this service is to be launched once on each device

            for device_id in device_ids:
                try:
                    device = self.device_by_mac(device_id)
                except KeyError:
                    raise ValueError("device does not exist with id {}".format(device_id))
                service_name = service_class.split(".")[-1]
                if service_name in device.services:
                    raise ValueError("device {} already owns this service: {}".format(
                        device.name, service_name))

                done_event = None
                if wait_for_completion:
                    done_event = threading.Event()
                    wait_events.append(done_event)

                new_service = service(device=device, env=self.env, tx_conn=service.Connection(
                    self.pkt_pipe, device), topics_to_subs=self.topics_to_subs, done_event=done_event, max_concurrent=max_concurrent)

                # register service, adding to
                device.register_service(new_service, self.env.process(new_service._run_until_interrupt()))

        # wait for events
        for event in wait_events:
            event.wait()

        return

    @remote_call
    @thread_safe_remote_call
    def is_on(self):
        return True

    @remote_call
    @thread_safe_remote_call
    def get_aps_retries(self, ap_ids):
        """Get the count of join retries of given APs.

        Args:
            ap_id: id of an ap (ip, mac, name...)
        """
        retries = []
        for ap_id in ap_ids:
            try:
                retries.append(self._get_ap_by_id(ap_id).retries)
            except ValueError:
                # ap does not exists
                pass
        return retries

    @remote_call
    @thread_safe_remote_call
    def get_client_states(self):
        """Retrieve the client states."""
        with self.clients_lock:
            return [c.state for c in self.clients]

    @remote_call
    @thread_safe_remote_call
    def get_ap_states(self):
        """Retrieve the ap states."""
        with self.aps_lock:
            return zip([ap.mac for ap in self.aps], [ap.state for ap in self.aps])

    @remote_call
    @thread_safe_remote_call
    def get_client_join_times(self):
        """Retrieve the join times of each clients."""
        with self.clients_lock:
            return [c.join_time for c in self.clients if hasattr(c, "join_time")]

    @remote_call
    @thread_safe_remote_call
    def get_clients_services_info(self):
        """Retrieve the info given by the services attached to each client."""
        with self.clients_lock:
            return [c.get_services_info() for c in self.clients]

    @remote_call
    @thread_safe_remote_call
    def get_clients_service_specific_info(self, service_name, key):
        """Retrieve the information labeled 'key' from the service named 'service_name'"""
        with self.clients_lock:
            return [c.get_service_specific_info(service_name, key) for c in self.clients]

    @remote_call
    @thread_safe_remote_call
    def set_clients_service_specific_info(self, service_name, key, value):
        with self.clients_lock:
            for c in self.clients:
                c.set_service_specific_info(service_name, key, value)

    @remote_call
    @thread_safe_remote_call
    def get_clients_done_count(self, service_name):
        # import objgraph
        # objgraph.show_most_common_types()
        with self.clients_lock:
            infos = [c.get_done_status(service_name) for c in self.clients]
            return sum([i for i in infos if i == True])

    @remote_call
    @thread_safe_remote_call
    def get_aps_services_info(self):
        with self.aps_lock:
            return [ap.get_services_info() for ap in self.aps]

    @remote_call
    @thread_safe_remote_call
    def get_aps_service_specific_info(self, service_name, key):
        """Retrieve the information labeled 'key' from the service named 'service_name'"""
        with self.aps_lock:
            return [ap.get_service_specific_info(service_name, key) for ap in self.aps]

    @remote_call
    @thread_safe_remote_call
    def set_aps_service_specific_info(self, service_name, key, value):
        with self.aps_lock:
            for ap in self.aps:
                ap.set_service_specific_info(service_name, key, value)

    @remote_call
    @thread_safe_remote_call
    def get_ap_join_times(self):
        with self.aps_lock:
            return [ap.join_time for ap in self.aps if hasattr(ap, "join_time") and ap.join_time]

    @remote_call
    @thread_safe_remote_call
    def get_ap_join_durations(self):
        with self.aps_lock:
            return [ap.join_duration for ap in self.aps if hasattr(ap, "join_duration") and ap.join_duration]

    @remote_call
    def wrap_client_pkts(self, client_pkts):
        """Wrap clients packets into capwap data.

        Args:
            worker (WirelessWorker): worker to call
            client_pkts (dict): dict client_mac -> list of packets (Dot11)
        Returns:
            client_pkts (dict): same as input with wrapped packets (Ether / IP / UDP / CAPW DATA / Dot11 ...)
        """
        out = {}
        for client_mac, pkts in client_pkts.items():
            client = self.client_by_id[client_mac]
            wrapped = []
            for pkt in pkts:
                # FIXME
                wrapped.append(client.ap.wrap_client_ether_pkt(client, pkt))
            out[client_mac] = wrapped
        return out

    @remote_call
    def join_aps(self, ids=None, max_concurrent=float('inf')):
        """Start Join process for given aps.

        Args:
            ids (list): ids of the aps to join (if None, joins all attached aps)
            max_concurrent (int): maximum number of concurrent join processes
        """
        with self.aps_lock:
            if not ids:
                aps = list(self.aps)
            else:
                aps = [self._get_ap_by_id(id) for id in ids]

        if not aps:
            self.logger.warn("join_aps : no AP to join")
            return

        self.logger.debug("join_aps %s" % ids)

        # Will be set when all aps have joined
        self.ap_joined = threading.Event()

        ServiceAP.ap_concurrent = simpy.resources.resource.Resource(
            self.env, capacity=max_concurrent)

        for port_id, aps_of_port in get_ap_per_port(aps).items():
            tasks = []

            # discover
            tasks.extend([ServiceAPDiscoverWLC(self, ap, self.env, topics_to_subs=self.topics_to_subs)
                          for ap in aps_of_port])
            tasks.append(ServiceInfoEvent(self))

            self.__add_stl_services(tasks)

        self.ap_joined.wait()

        self.logger.info("join_aps aps have all joined")
        return

    @remote_call
    def stop_aps(self, ids=None, max_concurrent=float('inf')):
        """Stop all processes for given aps.

        Args:
            ids (list): ids of the aps to stop (if None, stops all attached aps)
            max_concurrent (int): maximum number of concurrent stop processes
        """
        with self.aps_lock:
            if not ids:
                aps = list(self.aps)
            else:
                aps = [self._get_ap_by_id(id) for id in ids]

        self.logger.info("shutting down %d aps" % len(aps))

        # get attached clients
        clients = []
        for ap in aps:
            clients.extend(ap.clients)

        # hard stop clients' services
        for client in clients:
            client.stop_services(hard=True)

        for ap in aps:
            # stop sevices
            ap.stop_services()
            ap.state = APState.CLOSING

        # clear
        self.filters.clear()
        self.stl_services.clear()

        # disconnect
        self.logger.debug("launching ApShutdown for %d APs" % len(aps))
        self.__add_stl_services([ServiceAPShutdown(self, ap, self.env, topics_to_subs=self.topics_to_subs)
                        for ap in aps])

    @remote_call
    def stop_clients(self, ids=None, max_concurrent=float('inf')):
        """Stop all processes for given clients.

        Args:
            ids (list): ids of the clients to join (if None, stops all attached clients)
            max_concurrent (int): maximum number of concurrent stop processes
        """
        with self.clients_lock:
            if not ids:
                clients = list(self.clients)
            else:
                clients = self._get_clients_by_ids(ids)
        self.logger.info("shutting down %d clients" % len(clients))
        for client in clients:
            client.stop_services()
            client.state = ClientState.CLOSE

    @remote_call
    def stop(self):
        """Stop the Worker and all attached simulated devices."""
        self.stopped = True
        self.logger.debug("received stop command")
        self.topics_to_subs = None
        for name, queue in self.stop_queues.items():
            self.logger.info("stopping %s" % name)
            queue.put(None)  # signal stop
        return

    def __add_stl_services(self, services):
        """Add Services to the simulation.

        Services can be added before or while the simulation is running.

        Args:
            services (list): list of ServiceAP that needs to be run
        """
        # register filter
        for service in services:
            filter_type = service.get_filter_type()

            # if the service does not have a filter installed - create it
            if filter_type and not filter_type in self.filters:
                self.filters[filter_type] = {
                    'inst': filter_type(), 'capture_id': None}

            # add to the filter
            if filter_type:
                self.filters[filter_type]['inst'].add(service)

            # data per service
            with self.services_lock:
                self.stl_services[service] = {'pipe': None}

        # create simpy processes
        with self.services_lock:
            for service in services:
                pipe = SynchronizedServicePipe(self.env, None)
                self.stl_services[service]['pipe'] = pipe

                if hasattr(service, "ap"):
                    # AP service
                    device = service.ap
                    device.register_service(service, self.env.process(service.run(pipe)))
                else:
                    self.env.process(service.run(pipe))


    def __register_general_subservices(self, service):
        """Register the service and all its subservices recursively.

        Args:
            service (GeneralService): service to register with its subservices
        """
        self.logger.debug("registering (sub)service %s" % service.name)
        with self.services_lock:
            self.services[service.name] = service
        for subservice in service.subservices:
            self.__register_general_subservices(subservice)

    def __register_general_service(self, service, sim_process):
        """Registers a service for multiple devices on the worker.

        Args:
            service (GeneralService): to register
            sim_process (simpy.events.Process): running simpy process, obtained via 'env.process(service)'
        Returns:
            sim_process
        """
        self.logger.debug("registering service %s" % service.name)
        with self.services_lock:
            self.__service_processes[service.name] = sim_process
        if isinstance(service, GeneralService):
            self.__register_general_subservices(service)
        return sim_process

    def __stop_general_service(self, service_name):
        """Stop the GeneralService with given name.

        Args:
            service_name (string): name of the GeneralService to stop.
        """
        if service_name not in self.__service_processes:
            # no service to stop
            return
        try:
            self.__service_processes.pop(service_name).interrupt()
        except RuntimeError:
            # process already succeeded, race condition
            pass

        service = self.services.pop(service_name)
        for subservice in service.subservices:
            # deregister subservices
            self.__stop_general_service(subservice.name)

    def __stop_devices_services(self):
        """Stop all services running in all attached wireless devices.
        To be called in a simpy process (coroutine).
        """
        with self.clients_lock:
            devices = list(self.clients)
        with self.aps_lock:
            devices += list(self.aps)
        for device in devices:
            device.stop_services()


    def __control_process(self, stop_queue):
        """Hangs until stop command is received.

        Checks CPU usage and report if too high.

        Args:
            stop_queue: queue used for stop signaling, when ready, means that the thread should stop
        """
        try:
            while stop_queue.empty():
                # measure latency:
                # take time now, instruct scheduler to wait for 1sec and measure the time taken
                start = time.time()
                yield self.env.timeout(1)
                duration = time.time() - start
                if duration > 1.2:
                    print("Worker Overloaded, {:.2f} seconds of latency".format(
                        duration - 1))
        except EOFError:
            # queue is closed : stopping
            pass

        # print("disabling profiler")
        # self.pr.disable()
        # self.pr.dump_stats("PROFILING_" + self.name)

        self.logger.debug("control process stopping wireless device services")

        # stop received, closing devices
        with self.clients_lock:
            clients = list(self.clients)
        for client in clients:
            client.state = ClientState.CLOSE

        with self.aps_lock:
            aps = list(self.aps)
        for ap in aps:
            if ap.state >= APState.DISCOVER and ap.state < APState.CLOSING:
                ap.state = APState.CLOSING

        # stop all services in all devices
        self.__stop_devices_services()

        if aps:
            # disconnect DTLS
            self.logger.debug("launching ApShutdown for all aps")
            self.__add_stl_services([ServiceAPShutdown(self, ap, self.env, topics_to_subs=self.topics_to_subs)
                            for ap in aps])
            self.logger.debug("waiting for shutdown completion")

            timer = PassiveTimer(5)  # max 5 seconds to close aps
            while sum(ap.state > APState.INIT for ap in aps) > 0 and not timer.has_expired():
                yield self.env.timeout(1)
            self.logger.debug("all APs successfully shut")

            # stop all services in all devices
            self.__stop_devices_services()
        return

    def __sim(self, stop_queue, services=[]):
        """Simulation Thread.

        Start the simulation with given services.
        This thread also starts another thread used for simpy events to wake up when pubsub events happen.

        Args:
            services: services to be run at start
        """

        try:
            self.logger.debug("simulation started")

            if not hasattr(self, "env"):
                # sim
                self.env = simpy.rt.RealtimeEnvironment(factor=1, strict=False)
                # for event passing between threads and simpy run loop
                self.event_store = SynchronizedStore(self.env)

            if len(self.filters) > 1:
                raise Exception(
                    'Services here should have one common filter per AP')
            if len(self.filters) == 1:
                self.filter = list(self.filters.values())[0]['inst']
                if not hasattr(self.filter, 'services_per_ap'):
                    raise Exception('Services here should have filter with attribute services_per_ap, got %s, type: %s' % (
                        self.filter, type(self.filter)))

            try:
                control_process = self.env.process(
                    self.__control_process(stop_queue))

                # event_thread = threading.Thread(
                #     target=wait_event_thread, args=(self, ), daemon=True)
                # event_request_thread = threading.Thread(
                #     target=wait_event_request_thread, args=(self, ), daemon=True)
                # event_request_thread.start()
                # event_thread.start()

                # print("enabling profiler")
                # self.pr = cProfile.Profile()
                # self.pr.enable()

                # add service needed for message passing between threads and simpy loop
                service = ServiceEventManager(self.event_store)
                self.env.process(service.run())

                for service in services:
                    self.env.process(service.run())

                # starts simpy simulation, will block until control_process returns
                self.env.run(until=control_process)

                if not stop_queue.empty():
                    # stop order has been sent
                    resp = RPCResponse(
                        self.__stop_rpc_message_id, RPCResponse.SUCCESS, None)
                    with self.cmd_pipe_lock:
                        self.cmd_pipe.send(resp)

            except:
                self.logger.exception("Exception in " + self.name)
            finally:
                # profiling worker simulation
                if not stop_queue.empty():
                    stop_queue.get()

            self.logger.debug("sim stopped")
            return
        except EOFError:
            # queue is closed : stop
            pass
        except Exception as e:
            if not self.stopped:
                report = RPCExceptionReport(e)
                with self.cmd_pipe_lock:
                    self.cmd_pipe.send(report)
            self.stop()

    def __management(self, stop_queue):
        """Management Thread, communication between WirelessManager and WirelessWorker.

        Read on cmd_pipe and execute received remote calls, and finally respond to the remote.
        For any call there will be one response.
        Remote messages for calls should be of this type:
        ['cmd', name, id, args],
        name: the name of the call, i.e. the name of the worker_call to be called
        id: the id of the call, should uniquely identify this call, this is used to send a response back
        args: a tuple made of the arguments for the function to call
        """
        try:
            self.logger.debug("management started")

            # launch two threading pools, one for 'thread-safe' methods (thread safe with respect to other remote calls that is)
            # and one for 'critical' methods that may take long to finish (e.g. join_aps), that must be executed sequentially, therefore only one worker
            # the two threading pools are created to avoid 'thread safe' remote calls to be blocked by blocking calls
            critical_executor = ThreadPoolExecutor(
                max_workers=1)  # the number of workers must be 1
            # the number of workers can be arbitrary
            safe_executor = ThreadPoolExecutor(max_workers=2)

            while True:
                readable, _, _ = select.select(
                    [stop_queue._reader, self.cmd_pipe], [], [])

                if stop_queue._reader in readable:
                    # stop
                    self.logger.debug("management stopped")
                    return
                elif self.cmd_pipe in readable:
                    msg = self.cmd_pipe.recv()

                    if isinstance(msg, WorkerCall):
                        # Worker Call
                        worker_call = msg
                        self.logger.debug(
                            "remote call: {}".format(worker_call.NAME))

                        if worker_call.NAME == 'stop':
                            # special case, the call is to stop the worker
                            self.logger.debug("stopping")

                            def stop_management():
                                self.__stop_rpc_message_id = worker_call.id
                                # shut executors
                                critical_executor.shutdown(wait=False)
                                safe_executor.shutdown(wait=False)
                                self.logger.debug("management stopped")
                            stop_management()
                            self.stop()
                            return

                        elif worker_call.NAME in self.remote_calls:

                            if worker_call.NAME in self.thread_safe_remote_calls:
                                executor = safe_executor
                            else:
                                executor = critical_executor

                            # retrieve function to call
                            command = self.remote_calls[worker_call.NAME]

                            self.logger.debug(
                                "remote call: {}".format(worker_call.NAME))

                            def execute(self, command, worker_call):
                                # this is run in another thread
                                try:
                                    if worker_call.args:
                                        ret = command(self, *worker_call.args)
                                    else:
                                        ret = command(self)
                                    code = RPCResponse.SUCCESS
                                except Exception as e:
                                    print("Exception on %s:\n%s" %
                                          (self.name, traceback.format_exc()))
                                    ret = None
                                    code = RPCResponse.ERROR

                                # Send Response
                                resp = RPCResponse(worker_call.id, code, ret)
                                self.logger.debug(
                                    "sending response: {}".format(resp))
                                with self.cmd_pipe_lock:
                                    self.cmd_pipe.send(resp)
                                return

                            executor.submit(
                                execute, *(self, command, worker_call))

                        else:
                            # command does not exist
                            self.logger.warning(
                                "bad remote call: ' {}' command does not exist: {}".format(worker_call.NAME, msg))
                            continue

                    else:
                        self.logger.warn(
                            self.name + " received unknown message: " + str(msg))
        except EOFError as e:
            # queue is closed : stop
            return
        except Exception as e:
            if not self.stopped:
                report = RPCExceptionReport(e)
                with self.cmd_pipe_lock:
                    self.cmd_pipe.send(report)
            self.stop()


    def handle_packet(self, rx_bytes, logger):
        """Process the packet, assumed to be ethernet."""
        ARP_ETHTYPE = b'\x08\x06'
        IPv4_ETHTYPE = b'\x08\x00'
        IPv6_ETHTYPE = b'\x86\xdd'
        ICMP_PROTO = b'\x01'
        UDP_PROTO = b'\x11'
        CAPWAP_CTRL_PORT = b'\x14\x7e'
        CAPWAP_DATA_PORT = b'\x14\x7f'
        WLAN_ASSOC_RESP = b'\x00\x10'
        WLAN_DEAUTH = b'\x00\xc0'
        WLAN_DEASSOC = b'\x00\xa0'
        ARP_REQ = b'\x00\x01'
        ARP_REP = b'\x00\x02'
        ICMP_REQ = b'\x08'

        def handle_arp():
            def AP_ARP_RESP_TEMPLATE(src_mac, dst_mac, src_ip, dst_ip):
                return (
                    dst_mac + src_mac + ARP_ETHTYPE +  # Ethernet
                    b'\x00\x01\x08\x00\x06\x04\x00\x02' + src_mac + src_ip + dst_mac + dst_ip  # ARP
                )
            src_ip = rx_bytes[28:32]
            src_ip_str = socket.inet_ntoa(bytes(src_ip))
            dst_ip = rx_bytes[38:42]
            dst_ip_str = socket.inet_ntoa(bytes(dst_ip))
            if src_ip == dst_ip:  # GARP
                return
            elif not self.is_ap_ip(dst_ip_str):  # check IP
                return
            ap = self._get_ap_by_id(dst_ip_str)
            src_mac = rx_bytes[6:12]
            dst_mac = rx_bytes[:6]

            if dst_mac not in (b'\xff\xff\xff\xff\xff\xff', ap.mac_bytes):  # check MAC
                logger.warning('Bad MAC (%s) of AP %s' %
                                (dst_mac, ap.name))
                return

            if rx_bytes[20:22] == ARP_REQ:  # 'who-has'
                logger.debug('received ARP who-has')
                tx_pkt = AP_ARP_RESP_TEMPLATE(
                    src_mac=ap.mac_bytes,
                    dst_mac=src_mac,
                    src_ip=dst_ip,
                    dst_ip=src_ip,
                )
                self.pkt_pipe.send(tx_pkt)

            elif rx_bytes[20:22] == ARP_REP:  # 'is-at'
                if src_ip == ap.wlc_ip_bytes:
                    # assume response from wlc
                    ap.wlc_mac_bytes = src_mac
                    ap.wlc_mac = str2mac(src_mac)
                    ap.logger.debug("received ARP 'is-at")
                    ap._wake_up()

        def handle_icmp():
            rx_pkt = Ether(rx_bytes)
            icmp_pkt = rx_pkt[ICMP]
            if icmp_pkt.type == 8:  # echo-request
                logger.debug("received ping for {}".format(rx_pkt[IP].dst))
                ap = self._get_ap_by_id(rx_pkt[IP].dst)
                if rx_pkt[IP].dst == ap.ip:  # ping to AP
                    tx_pkt = rx_pkt.copy()
                    tx_pkt.src, tx_pkt.dst = tx_pkt.dst, tx_pkt.src
                    tx_pkt[IP].src, tx_pkt[IP].dst = tx_pkt[IP].dst, tx_pkt[IP].src
                    tx_pkt[ICMP].type = 'echo-reply'
                    del tx_pkt[ICMP].chksum
                    self.pkt_pipe.send(bytes(tx_pkt))

        def handle_ipv4():

            def handle_udp():

                def process_capwap_ctrl():
                    # do not forward capwap control if not reconstructed
                    forward = False

                    def capwap_reassemble(ap, rx_pkt_buf):
                        """Return the reassembled packet if 'rx_pkt_buf' is the last fragmented,
                        or None if more fragmented packets are expected, or the packet itself if not fragmented.
                        The returned packet is a CAPWAP CTRL / PAYLOAD"""
                        capwap_assemble = ap.capwap_assemble

                        # is_fragment
                        if struct.unpack('!B', rx_pkt_buf[3:4])[0] & 0x80:
                            rx_pkt = CAPWAP_CTRL(rx_pkt_buf)
                            if capwap_assemble:
                                assert capwap_assemble[
                                    'header'].fragment_id == rx_pkt.header.fragment_id, 'Got CAPWAP fragments with out of order (different fragment ids)'
                                control_str = bytes(
                                    rx_pkt[CAPWAP_Control_Header_Fragment])
                                if rx_pkt.header.fragment_offset * 8 != len(capwap_assemble['buf']):
                                    ap.logger.error(
                                        'Fragment offset and data length mismatch')
                                    capwap_assemble.clear()
                                    return

                                capwap_assemble['buf'] += control_str

                                if rx_pkt.is_last_fragment():
                                    capwap_assemble['assembled'] = bytes(CAPWAP_CTRL(
                                        header=capwap_assemble['header'],
                                        control_header=CAPWAP_Control_Header(
                                            capwap_assemble['buf'])
                                    ))
                            else:
                                if rx_pkt.is_last_fragment():
                                    ap.logger.error(
                                        'Got CAPWAP first fragment that is also last fragment!')
                                    return
                                if rx_pkt.header.fragment_offset != 0:
                                    ap.logger.error(
                                        'Got out of order CAPWAP fragment, does not start with zero offset')
                                    return
                                capwap_assemble['header'] = rx_pkt.header
                                capwap_assemble['header'].flags &= ~0b11000
                                capwap_assemble['buf'] = bytes(
                                    rx_pkt[CAPWAP_Control_Header_Fragment])
                                capwap_assemble['ap'] = ap
                        elif capwap_assemble:
                            logger.error(
                                'Got not fragment in middle of assemble of fragments (OOO).')
                            capwap_assemble.clear()
                        else:
                            capwap_assemble['assembled'] = rx_pkt_buf
                        return rx_pkt_buf

                    # forward = False

                    if (not ap.is_dtls_established or ap.state < APState.DTLS or not ap.wlc_mac_bytes):
                        if rx_bytes[42:43] == b'\0':  # capwap header,  discovery response
                            capwap_bytes = rx_bytes[42:]
                            capwap_hlen = (struct.unpack('!B', capwap_bytes[1:2])[
                                0] & 0b11111000) >> 1
                            ctrl_header_type = struct.unpack(
                                '!B', capwap_bytes[capwap_hlen + 3:capwap_hlen + 4])[0]
                            if ctrl_header_type != 2:
                                return
                            if not ap.wlc_ip:
                                ap.wlc_ip_bytes = rx_bytes[26:30]
                                ap.wlc_ip = str2ip(ap.wlc_ip_bytes)
                            if rx_bytes[26:30] == ap.wlc_ip_bytes:
                                ap.wlc_mac_bytes = rx_bytes[6:12]
                                ap.wlc_mac = str2mac(ap.wlc_mac_bytes)
                            result_code = CAPWAP_PKTS.parse_message_elements(
                                capwap_bytes, capwap_hlen, ap, self)
                            ap.logger.debug(
                                "received discovery response")
                            ap._wake_up()
                            ap.rx_responses[ctrl_header_type] = result_code

                        elif rx_bytes[42:43] == b'\1':  # capwap dtls header
                            # forward message to ap
                            logger.debug(
                                "received dtls handshake message destination: %s" % mac2str(dst_mac))
                            try:
                                ap.logger.debug("packet to service: %s",
                                                ap.active_service)
                                with self.services_lock:
                                    self.stl_services[ap.active_service]['pipe']._on_rx_pkt(
                                        rx_bytes, None)
                            except KeyError:
                                # no service registered, drop
                                pass
                        else:
                            ap.logger.debug(
                                "dropping non expected packet")
                            if (rx_bytes[46:47] == b'\x15'):  # DTLS alert
                                ap.logger.error(
                                    "Server sent DTLS alert to AP")
                                ap.got_disconnect = True

                        return

                    is_dtls = struct.unpack('?', rx_bytes[42:43])[0]
                    if not is_dtls:  # dtls is established, ctrl should be encrypted
                        ap.logger.error(
                            "received not encrypted capwap control packet, dropping")
                        return

                    if (rx_bytes[46:47] == b'\x15'):  # DTLS alert
                        ap.logger.error(
                            "Server sent DTLS alert to AP")
                        ap.got_disconnect = True

                    rx_pkt_buf = ap.decrypt(rx_bytes[46:])
                    if not rx_pkt_buf:
                        return
                    # definitely not CAPWAP... should we debug it?
                    if rx_pkt_buf[0:1] not in (b'\0', b'\1'):
                        ap.logger.debug('Not CAPWAP, skipping')
                        return

                    ap.last_recv_ts = time.time()
                    # get reassembled if needed
                    # capwap_assemble = ap.capwap_assemble
                    rx_pkt_buf = capwap_reassemble(ap, rx_pkt_buf)
                    if not rx_pkt_buf or rx_pkt_buf[0:1] != b'\0':
                        return
                    ap.capwap_assemble.clear()

                    # send to AP services rx_bytes[:46] + rx_pkt_buf
                    reconstructed = rx_bytes[:42] + rx_pkt_buf
                    # send the last fragmented packet reconstructed, with the last packet's header
                    for service in ap.services.values():
                        if service.active:
                            ap.logger.debug(
                                "forwarding capwap packet to service: {}".format(service.name))
                            service._on_rx_pkt(reconstructed)

                    capwap_hlen = (struct.unpack('!B', rx_pkt_buf[1:2])[
                        0] & 0b11111000) >> 1
                    ctrl_header_type = struct.unpack(
                        '!B', rx_pkt_buf[capwap_hlen + 3:capwap_hlen + 4])[0]

                    if ctrl_header_type == 7:  # Configuration Update Request

                        CAPWAP_PKTS.parse_message_elements(
                            rx_pkt_buf, capwap_hlen, ap, self)  # get info from incoming packet
                        seq = struct.unpack(
                            '!B', rx_pkt_buf[capwap_hlen + 4:capwap_hlen + 5])[0]
                        tx_pkt = ap.get_config_update_capwap(seq)
                        encrypted = ap.encrypt(tx_pkt)
                        if encrypted:
                            self.pkt_pipe.send(ap.wrap_capwap_pkt(
                                b'\1\0\0\0' + encrypted))

                    elif ctrl_header_type == 14:  # Echo Response
                        ap.logger.debug("received echo reply")
                        ap.echo_resp_timer = None

                    elif ctrl_header_type == 17:  # Reset Request
                        logger.error(
                            'AP %s got Reset request, shutting down' % ap.name)
                        ap.got_disconnect = True

                    elif ctrl_header_type in (4, 6, 12):
                        result_code = CAPWAP_PKTS.parse_message_elements(
                            rx_pkt_buf, capwap_hlen, ap, self)
                        ap.rx_responses[ctrl_header_type] = result_code

                    else:
                        logger.error(
                            'Got unhandled capwap header type: %s' % ctrl_header_type)

                def process_capwap_data():

                    def handle_client_arp():
                        ip = dot11_bytes[58:62]
                        mac_bytes = dot11_bytes[4:10]
                        mac = mac2str(mac_bytes)
                        from_mac_bytes = dot11_bytes[10:16]
                        client = self._get_client_by_id(mac)
                        if not client:
                            return
                        self.logger.info(
                            "client {} received an arp".format(mac))
                        if not client:
                            return
                        if client.ap is not ap:
                            self.logger.warn('Got ARP to client %s via wrong AP (%s)' %
                                                (client.mac, ap.name))
                            return

                        if dot11_bytes[40:42] == ARP_REQ:  # 'who-has'
                            if dot11_bytes[48:52] == dot11_bytes[58:62]:  # GARP
                                return
                            if not hasattr(client, "ip_bytes") or not client.ip_bytes:
                                return
                            tx_pkt = ap.wrap_client_ether_pkt(client, ap.get_arp_pkt(
                                'is-at', src_mac_bytes=client.mac_bytes, src_ip_bytes=client.ip_bytes, dst_ip_bytes=from_mac_bytes))
                            self.pkt_pipe.send(tx_pkt)

                        elif dot11_bytes[40:42] == ARP_REP:  # 'is-at'
                            client.seen_arp_reply = True
                            client.logger.debug("received arp reply")
                            ap._wake_up()

                    def handle_client_icmp():
                        mac_bytes = dot11_bytes[4:10]
                        mac = mac2str(mac_bytes)
                        client = self._get_client_by_id(mac)
                        if not client:
                            self.logger.error("Received ICMP packet for non-existing MAC {}".format(mac))
                            return
                        self.logger.info(
                            "client {} received an ICMP".format(client.mac))
                        if client.ap is not ap:
                            self.logger.warn('Got ICMP to client %s via wrong AP (%s)' %
                                                (client.mac, ap.name))
                            return

                        if dot11_bytes[54:55] == ICMP_REQ:
                            rx_pkt = Dot11_swapped(dot11_bytes)
                            tx_pkt = Ether(src=client.mac, dst=rx_pkt.addr3) / \
                                rx_pkt[IP].copy()
                            tx_pkt[IP].src, tx_pkt[IP].dst = tx_pkt[IP].dst, tx_pkt[IP].src
                            tx_pkt[ICMP].type = 'echo-reply'
                            del tx_pkt[ICMP].chksum
                            tx_pkt = ap.wrap_client_ether_pkt(client, bytes(tx_pkt))
                            self.pkt_pipe.send(tx_pkt)

                    logger.debug("received capwap data")
                    if ord(rx_bytes[45:46]) & 0b1000:  # CAPWAP Data Keep-alive
                        ap.got_keep_alive = True
                        ap.logger.debug(
                            "received CAPWAP Data Keep-alive")
                        ap._wake_up()
                        if ap.state >= APState.JOIN:
                            assert ap.session_id is not None
                            if ap.got_keep_alive:
                                if not ap.expect_keep_alive_response:
                                    # have to respond
                                    self.pkt_pipe.send(ap.wrap_capwap_pkt(
                                        CAPWAP_PKTS.keep_alive(ap), dst_port=5247))
                                    ap.expect_keep_alive_response = True
                                else:
                                    # response to ap's keep alive
                                    ap.expect_keep_alive_response = False
                            else:
                                ap.logger.debug(
                                    "Received CAPWAP Data Keep-alive for non joined AP")
                        return

                    dot11_offset = 42 + \
                        ((ord(rx_bytes[43:44]) & 0b11111000) >> 1)
                    dot11_bytes = rx_bytes[dot11_offset:]

                    # assume 802.11 frame for client
                    mac_bytes = dot11_bytes[4:10]
                    mac = mac2str(mac_bytes)

                    # send packet to client services that are active
                    packet_l3_type = dot11_bytes[32:34]
                    try:
                        dest_client = self.client_by_id[mac]
                        for service in dest_client.services.values():
                            if service.active:
                                dest_client.logger.debug(
                                    "forwarding packet of type {} to service: {}".format(packet_l3_type, service.name))
                                service._on_rx_pkt(dot11_bytes)
                    except KeyError:
                        # non local client
                        pass

                    if packet_l3_type == ARP_ETHTYPE:
                        handle_client_arp()

                    elif packet_l3_type == IPv4_ETHTYPE and dot11_bytes[43:44] == ICMP_PROTO:
                        handle_client_icmp()

                udp_port_str = rx_bytes[36:38]
                udp_src = rx_bytes[34:36]

                if udp_src == CAPWAP_CTRL_PORT:
                    process_capwap_ctrl()
                elif udp_src == CAPWAP_DATA_PORT:
                    process_capwap_data()
                return

            ip = rx_bytes[30:34]  # destination ip (ap)
            ip_str = socket.inet_ntoa(bytes(ip))
            if not self.is_ap_ip(ip_str):  # check IP
                return
            ap = self._get_ap_by_id(ip_str)
            dst_mac = rx_bytes[:6]
            if dst_mac not in ('\xff\xff\xff\xff\xff\xff', ap.mac_bytes):  # check MAC
                logger.warning('dropped packet: bad MAC (%s), although IP of AP (%s)' % (
                    str2mac(dst_mac), str2ip(ip)))
                return

            ip_proto = rx_bytes[23:24]

            # demultiplex layer-4 protocol
            if ip_proto == ICMP_PROTO:
                handle_icmp()
            elif ip_proto == UDP_PROTO:
                handle_udp()
            else:
                # drop
                logger.debug(
                    'dropped packet: layer-4 protocol not supported: {}'.format(ip_proto))
                return

        # by default, forward to AP services, disabled for fragmented capwap control
        # (forwarding the reconstructed packet)
        forward = True

        ether_type = rx_bytes[12:14]

        # demultiplex layer-3 protocol
        if ether_type == ARP_ETHTYPE:
            handle_arp()
        elif ether_type == IPv4_ETHTYPE:
            handle_ipv4()
        else:
            logger.debug(
                'dropped packet: layer-3 protocol not supported: {}'.format(ether_type))

        # forwarding to ap services
        if forward:
            try:
                mac = mac2str(rx_bytes[:6])
                ap = self.ap_by_mac[mac]

                for service in ap.services.values():
                    if service.active:
                        ap.logger.debug(
                            "forwarding packet to service: {}".format(service.name))
                        service._on_rx_pkt(rx_bytes)
            except KeyError:
                # non local ap
                pass


    def __worker_traffic_handler(self, stop_queue):
        """Handles traffic from Handler and demultiplex to APs' rx_buffer."""

        try:
            self.logger.debug("worker traffic_handler started")

            logger = get_child_logger(self.logger, "TrafficHandler")

            while True:
                readable, _, _ = select.select(
                    [stop_queue._reader, self.pkt_pipe], [], [])

                if stop_queue._reader in readable:
                    # stop
                    self.logger.debug("worker_traffic_handler stopped")
                    stop_queue.get()
                    break

                elif self.pkt_pipe in readable:
                    try:
                        rx_bytes = Packet(self.pkt_pipe.recv())
                        # assume ethernet
                        self.handle_packet(rx_bytes, logger)

                    except EOFError:
                        break
            return

        except EOFError:
            # queue is closed : stop
            pass
        except Exception as e:
            self.logger.exception("an error occured")
            if not self.stopped:
                report = RPCExceptionReport(e)
                with self.cmd_pipe_lock:
                    self.cmd_pipe.send(report)
            self.stop()
