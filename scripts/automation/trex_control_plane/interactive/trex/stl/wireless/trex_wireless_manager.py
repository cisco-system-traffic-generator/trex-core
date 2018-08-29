import multiprocessing as mp
import threading
import importlib
import signal
import copy
import os
from collections import OrderedDict

from trex_openssl import *
from scapy.all import Ether

from trex.common import *
from .logger import *
from .trex_wireless_config import *
from .trex_wireless_traffic_handler import *
from .trex_wireless_worker import WirelessWorker
from .trex_wireless_traffic_handler_rpc import *
from .trex_wireless_worker_rpc import *
from .trex_wireless_ap import *
from .trex_wireless_client import *
from .trex_wireless_client_state import *
from .trex_wireless_manager_private import *
from .pubsub.pubsub import PubSub
from .services.trex_stl_ap import *
from .utils.utils import round_robin_list, timeout, load_service, get_capture_port
from trex.utils.parsing_opts import check_mac_addr, check_ipv4_addr
from trex.stl.trex_stl_client import STLClient
from trex.stl.trex_stl_streams import STLStream, STLProfile
from trex.stl.trex_stl_packet_builder_scapy import increase_mac, increase_ip
import select

config = None


class WirelessManager:
    """A WirelessManager offers the API for simulating APs and Clients on a TREX Client."""

    # API

    def __init__(self, config_filename=None, log_filter=None, log_level=logging.INFO, trex_client=None):
        """Create a WirelessManager.

        Args:
            config_filename (str): configuration file or None for defaults
            log_filter (logging.Filter, optional): filter of the logs
            log_level (int): Level of the log (default INFO)
            trex_client (STLTrexClient): TRex client to use, if None a new one will be created
                                         from the config
        """
        global config
        config = load_config(config_filename)
        self.config = config

        self.is_running = False

        self.name = "WirelessManager"
        self.ssl_ctx = None
        self.ap_info_by_name = {}
        self.ap_info_by_mac = {}
        self.ap_info_by_ip = {}
        self.ap_info_by_udp_port = {}
        self.ap_info_by_radio_mac = {}
        self.client_info_by_id = {}
        self._init_base_vals()

        self.trex_client = trex_client

        self.server_ip = config.server_ip

        self.num_workers = config.num_workers
        if self.num_workers == "auto" or int(self.num_workers) <= 0:
            # 3 accounts for CPU used by TRex servers with 1 interface
            self.num_workers = max(multiprocessing.cpu_count() - 3, 1)

        self.next_worker = 0  # round robin load balancing for creating aps

        # workers are attached to a trex port
        self.workers_by_port = {}  # map trex_port_id -> workers list

        # map trex_port_id -> List of (Connection (Pipe end), threading.Lock) for worker commands, for the worker attached to trex_port_id port
        # Connections between Worker and Manager, used for sending commands to Worker
        self.workers_cmd_connection_by_port = {}

        # map trex_port_id -> List of Connection (Pipe end) for worker packets , for the worker attached to trex_port_id port
        # Connections between Worker and Traffic Handler, used for sending/receiving packets
        self.workers_pkt_connection_by_port = {}
        self.worker_by_ap_mac = {}

        self.worker_by_device_mac = {}  # map device_mac -> worker

        # map trex_port_id -> List of (Connection (Pipe end), threading.Lock) for Traffic Handler commands, for the TrafficHandler attached to trex_port_id port
        # Connections between Manager and Traffic Handler, used for sending commands to Traffic Handler
        self.traffic_handlers_cmd_connection_by_port = {}
        # map trex_port_id -> TrafficHandler
        self.traffic_handlers_by_port = {}

        # AP lookup tables
        self.aps_infos = []
        self.aps_by_worker = {}
        self.ap_info_by_name = {}
        self.ap_info_by_mac = {}
        self.ap_by_ip = {}
        self.ap_by_udp_port = {}
        self.ap_by_radio_mac = {}

        # Client lookup tables
        self.clients = []

        # for ap parameters generation
        self._gen_macs = []
        self._gen_ips = []
        self._gen_upd_ports = []
        self._gen_radio_macs = []

        # for client parameters generation
        self._gen_client_macs = []
        self._gen_client_ips = []

        # log_queue that gets logs of every process
        self.log_queue = mp.Queue()
        self.log_level = log_level
        self.log_filter = log_filter
        self.log_filename = config.log_filename

        # publish/subscribe for events
        self.pubsub = PubSub(log_level=self.log_level, log_filter=self.log_filter, log_queue=self.log_queue)
        self.publisher = self.pubsub.Publisher("WirelessManager")

        # dict id -> response for remote calls
        # receiver of response should free the map of the response
        self.rc_responses = {}

        # event for waiting on worker responses
        self.new_rc_response = threading.Condition()

        # response from traffic handler
        self.new_handler_rc_response = threading.Condition()
        # dict id -> response for remote calls
        self.traffic_handlers_rc_responses = {}

        # user defined client filters
        self.client_filters = ""
        # user defined filters
        self.filters = ""

        self.client_asso_service = "wireless.services.client.client_service_association.ClientServiceAssociation"

        # base values
        self.set_base_values(ap_mac=config.base_values.ap_mac, ap_ip=config.base_values.ap_ip, ap_udp=config.base_values.ap_udp,
                              ap_radio=config.base_values.ap_radio, client_mac=config.base_values.client_mac, client_ip=config.base_values.client_ip)

    def start(self, trex_port_ids=None):
        """Start the manager for use. Must be stopped by calling 'stop' method on manager.
        Launches the other processes (workers, traffic handler, and log listener).


        Args:
            trex_port_ids: trex ports number to use, either one or a list of ids, default from config


        Implementation Details:

        The Manager offers the API, it splits the load onto the workers, the Traffic Handler is responsible for sent and received packets.

        Each Worker is responsible for a set of APs and the attached Clients on the APs.
        The Manager is connected to the Workers via pipe ends for command and responses.

        The Traffic Handler is responsible for getting the packets from the Trex Server and demultiplex them to the workers,
        as well as getting the packets from the Workers and sending them to the Trex Server.
        The Manager is connected to the Traffic Handler via a pipe end for commands and responses.

        Each Worker is connected to the Traffic Handler via a pipe end for packets, this way a Worker can send packets as if using a socket, same for the Traffic Handler.
        """
        self.is_running = True

        if not trex_port_ids:
            self.trex_port_ids = config.ports
        else:
            if type(trex_port_ids) is int:
                trex_port_ids = [trex_port_ids]
            self.trex_port_ids = trex_port_ids

        self.worker_cmd_connections = []
        self.traffic_handlers_cmd_connections = []

        # create log listener : process that listens on self.log_queue and writes the received messages as logs
        self.listener = LogListener(
            self.log_queue, self.log_filename, self.log_level)
        self.listener.daemon = True
        self.listener.start()

        # set the logger
        self.logger = get_queue_logger(self.log_queue, type(
            self).__name__, self.name, self.log_level, self.log_filter)

        # pubsub
        self.pubsub.start()

        self.logger.info("started, PID %s" % os.getpid())

        self.publisher.publish("started")

        # trex client
        self.__connect_client(port_ids=config.ports)

        for port_id in self.trex_port_ids:
            if not self.ssl_ctx:
                self.ssl_ctx = SSL_Context()

            # create workers
            self.workers_by_port[port_id] = []
            self.workers_cmd_connection_by_port[port_id] = []
            self.workers_pkt_connection_by_port[port_id] = []
            # launch workers as processes
            for i in range(self.num_workers):
                # ends of the Command Pipe, one for the manager, one for the worker
                manager_to_worker_cmd_connection, worker_to_manager_cmd_connection = mp.Pipe()
                # ends of the Packet Pipe, one for the manager, one for the TrafficHandler
                traffic_handler_to_worker_pkt_connection, worker_to_traffic_handler_pkt_connection = mp.Pipe()
                # instanciate the worker using its two Connection ends (one for control/commands, one for the Packets to TrafficHandler)
                p = WirelessWorker(i, worker_to_manager_cmd_connection, worker_to_traffic_handler_pkt_connection,
                                   port_id, self.config, self.ssl_ctx, self.log_queue, self.log_level, self.log_filter, self.pubsub, self.publisher)

                self.workers_by_port[port_id].append(p)
                self.aps_by_worker[p] = []

                lock = threading.Lock()
                self.workers_cmd_connection_by_port[port_id].append((
                    manager_to_worker_cmd_connection, lock))
                self.worker_cmd_connections.append(
                    manager_to_worker_cmd_connection)

                # order is important
                self.workers_pkt_connection_by_port[port_id].append(
                    traffic_handler_to_worker_pkt_connection)

            # launch management thread (with worker pipes)
            # Management Thread is responsible for Communication between WirelessManager and WirelessWorkers (for control commands)
            self.management_worker_thread = threading.Thread(
                target=self.__management_workers, args=(), daemon=True)
            self.management_worker_thread.start()

            # start workers
            for worker in self.workers_by_port[port_id]:
                worker.daemon = True
                worker.start()
                self.logger.debug(
                    "launched worker # {} for port # {}".format(worker.worker_id, port_id))

            # traffic_handler process

            # ends of the Command Pipe, one for the manager, one for the Traffic Handler
            manager_to_traffic_handler_cmd_connection, traffic_handler_to_manager_cmd_connection = mp.Pipe()
            self.traffic_handlers_cmd_connection_by_port[port_id] = (
                manager_to_traffic_handler_cmd_connection, threading.Lock())
            self.traffic_handlers_cmd_connections.append(
                manager_to_traffic_handler_cmd_connection)

            self.traffic_handlers_by_port[port_id] = TrafficHandler(traffic_handler_to_manager_cmd_connection, self.server_ip, port_id, self.workers_pkt_connection_by_port[port_id],
                                                                    self.log_queue, self.log_level, self.log_filter, self.pubsub, self.publisher, filters=self.filters, client_filters=self.client_filters)

            # launch management thread (with handlers pipes)
            # Management Thread is responsible for Communication between WirelessManager and TrafficHandlers (for control commands)
            self.__management_traffic_handlers_thread = threading.Thread(
                target=self.__management_traffic_handlers, args=(), daemon=True)
            self.__management_traffic_handlers_thread.start()

            self.traffic_handlers_by_port[port_id].daemon = True
            self.traffic_handlers_by_port[port_id].start()
            self.logger.debug(
                "launched traffic handler for port {}".format(port_id))

        self.device_updater = threading.Thread(
            target=self.__device_state_updater, args=(), daemon=True)
        self.device_updater.start()

        self.logger.info("finished start process")
        return

    def stop(self):
        """Stop the WirelessManager and its worker and traffic handler."""

        @timeout(timeout_sec=5, timeout_callback=self.__force_stop_workers)
        def stop_workers():
            """Try to stop the workers."""
            # Workers
            self.logger.debug("stopping workers")

            calls = []
            for port_id in self.trex_port_ids:
                for worker in self.workers_by_port[port_id]:
                    calls.append(WorkerCall_stop(worker))

            self.__worker_calls(calls)

        @timeout(timeout_sec=2, timeout_callback=self.__force_stop_traffic_handlers)
        def stop_traffic_handlers():
            self.logger.debug("stopping traffic handlers")
            if self.trex_port_ids:
                for port in self.trex_port_ids:
                    if port in self.traffic_handlers_by_port:
                        self.__handler_call(TrafficHandlerCall_stop(), port)

        def cleanup():
            self.logger.debug("closing multiprocess connections")
            for port in self.trex_port_ids:
                for cmd_conn in self.workers_cmd_connection_by_port[port]:
                    cmd_conn[0].close()
                for conn in self.workers_pkt_connection_by_port[port]:
                    conn.close()
                self.traffic_handlers_cmd_connection_by_port[port][0].close()
            self.log_queue.close()

        if not self.is_running:
            return

        self.is_running = False

        with self.trex_client_lock:
            self.trex_client.stop()
        self.remove_all_streams()

        # workers
        stop_workers()
        self.logger.debug("workers stopped")
        # traffic handlers
        stop_traffic_handlers()
        self.logger.debug("traffic handlers stopped")
        self.logger.debug("stopping log listener")
        self.pubsub.stop()
        self.logger.debug("pubsub stopped")
        self.__disconnect_client()
        self.logger.debug("TRex client disconnected")

        
        cleanup()

        # log listener
        # send quit signal to log listener
        self.log_queue.close()
        return

    def create_aps(self, macs, ips, udp_ports, radio_macs, trex_port_id=None, wlc_ips=None, gateway_ips=None, ap_modes=None, rsa_ca_priv_file=None, rsa_priv_file=None, rsa_cert_file=None):
        """Create APs.

        Args:
            macs: array of macs of APs to create, in order
            ips: array of ips of APs to create, in order
            udp_ports: array of udp ports of APs to create, in order
            radio_macs: array of macs of radios of APs to create, in order
            trex_port_id: trex port to attach APs on, in case of multiple ports
            wlc_ips: array of ip of controllers to connect the APs to, in order of the APs
            gateway_ips: array of ip of gateways, in order of the APs
            ap_modes: modes of each AP, APMode.LOCAL for local mode AP, APMode.REMOTE for remote AP (also known as FlexConnect). Default is Local.
            rsa_ca_priv_file: filename of rsa private key of WLC CA, default: None
            rsa_priv_file: filename of rsa private key of AP, default from configuration
            rsa_cert_file: filename of rsa certificate of AP, default from configuration
        """
        self.logger.debug("create_aps")
        if not wlc_ips:
            wlc_ips = [None] * len(macs)
        if not gateway_ips:
            gateway_ips = [None] * len(macs)
        if not ap_modes:
            ap_modes = [APMode.LOCAL] * len(macs)
        new_aps_by_worker = {}

        if not trex_port_id:
            if not len(config.ports) == 1:
                raise ValueError(
                    "if trex_port_id is not explicitely given, there must one and only one port in config file")
            trex_port_id = config.ports[0]

        # check that lengths of input list correspond
        args = [macs, ips, udp_ports, radio_macs, wlc_ips, gateway_ips, ap_modes]
        num_aps = len(macs)
        assert(all(num_aps == len(a) for a in args))

        if rsa_ca_priv_file:
            rsa_ca_priv_file = bytes(rsa_priv_file, encoding='ascii')
            rsa_priv_file = None
            rsa_cert_file = None
        else:
            # use AP keys
            if not rsa_priv_file and not rsa_cert_file:
                rsa_priv_file = config.ap_key
                rsa_cert_file = config.ap_cert
            rsa_priv_file = bytes(rsa_priv_file, encoding='ascii')
            rsa_cert_file = bytes(rsa_cert_file, encoding='ascii')

        port_layer_cfg = self.__get_port_layer_cfg(trex_port_id)

        worker_connection_id_by_mac = {}  # map mac -> connection
        for (mac, ip, udp_port, radio_mac, wlc_ip, gateway_ip, ap_mode) in zip(macs, ips, udp_ports, radio_macs, wlc_ips, gateway_ips, ap_modes):
            if trex_port_id not in self.workers_by_port:
                raise Exception('TRex port %s does not exist!' % trex_port_id)
            if ':' not in mac:
                mac = str2mac(mac)
            if ':' not in radio_mac:
                radio_mac = str2mac(radio_mac)
            if mac in self.ap_info_by_mac:
                raise Exception('AP with such MAC (%s) already exists!' % mac)
            if ip in self.ap_by_ip:
                raise Exception('AP with such IP (%s) already exists!' % ip)
            if udp_port in self.ap_by_udp_port:
                raise Exception(
                    'AP with such UDP port (%s) already exists!' % udp_port)
            if radio_mac in self.ap_by_radio_mac:
                raise Exception(
                    'AP with such radio MAC port (%s) already exists!' % radio_mac)

            # assign worker to this ap
            # round robin
            worker = self.workers_by_port[trex_port_id][self.next_worker]
            self.next_worker = (self.next_worker + 1) % (self.num_workers)
            ap_info = APInfo(trex_port_id, ip, mac, radio_mac, udp_port,
                             wlc_ip, gateway_ip, ap_mode if ap_mode is not None else APMode.LOCAL,
                             rsa_ca_priv_file, rsa_priv_file, rsa_cert_file)
            self.__add_ap_info(ap_info, worker)

            if worker not in new_aps_by_worker:
                new_aps_by_worker[worker] = []
            new_aps_by_worker[worker].append(ap_info)

            # self.workers_pkt_connection_by_port[trex_port_id][worker.id]
            worker_connection_id_by_mac[mac] = worker.worker_id

        # set the route to correct worker
        self.__handler_call(TrafficHandlerCall_route_macs(
            worker_connection_id_by_mac), trex_port_id)

        calls = []
        for worker, aps in new_aps_by_worker.items():
            calls.append(WorkerCall_create_aps(worker, port_layer_cfg, aps))
        self.__worker_calls(calls)
        self.logger.debug("done create_aps")

    def create_clients(self, macs, ips, ap_ids=None):
        """Create clients.

        Args:
            macs (list): array, macs of Clients to create, in order
            ips (list): array, ips of Clients to create, in order
            ap_ids (list): ids of the AP to attach the Clients to, in order
                that is, ap_ids[i] should be the MAC of the AP attached to the Client with MAC macs[i]
                Can also be None, in that case, it is attached in a round-robin manner.
        """
        num_aps = len(macs)
        assert(len(macs) == len(ips))
        assert( (ap_ids is None and len(self.ap_info_by_name.keys()) > 0) or len(macs) == len(ap_ids))

        if not ap_ids:
            ap_ids = round_robin_list(len(macs), list(self.ap_info_by_name.keys()))
            assert(len(macs) == len(ap_ids))

        clients_per_worker = {}
        for (mac, ip, ap_id) in zip(macs, ips, ap_ids):
            ap_info = self.get_ap_info_by_id(ap_id)
            if ':' not in mac:
                mac = str2mac(mac)
            if mac in self.client_info_by_id:
                raise Exception(
                    'Client with such MAC (%s) already exists!' % mac)
            if ip is not None and ip in self.client_info_by_id:
                raise Exception(
                    'Client with such IP (%s) already exists!' % ip)
            client = ClientInfo(mac=mac, ip=ip, ap_info=ap_info)
            client.ap_info.clients.append(client)
            # for manager
            self.client_info_by_id[mac] = client
            if ip is not None:  # if IP is specified it is an identifier
                self.client_info_by_id[ip] = client
            self.clients.append(client)
            worker = self.worker_by_ap_mac[ap_info.mac]
            self.worker_by_device_mac[client.mac] = worker
            if worker not in clients_per_worker:
                clients_per_worker[worker] = []

            clients_per_worker[worker].append(client)

        # now send the calls
        calls = []
        for worker, cs in clients_per_worker.items():
            calls.append(WorkerCall_create_clients(worker, cs))
        self.__worker_calls(calls, wait=True)

    def stop_aps(self, ap_ids, max_concurrent=float('inf'), wait=True):
        """Stop running services on given APs.
        
        Args:
            ap_ids (list): ids of ap to to stop (mac addresses)
            max_concurrent (int): number of maximum concurrent shutdown processes at any time, default no limit
            wait (bool): if set, the function will return when the calls terminated, otherwise, returns immediately
        """
        max_concurrent_per_worker = max_concurrent
        if isinstance(max_concurrent, int):
            max_concurrent_per_worker = max(max_concurrent/self.num_workers, 1)
        self.logger.debug("stop_aps")
        aps = [self.get_ap_info_by_id(ap_id) for ap_id in ap_ids]
        if not aps:
            raise Exception('No APs to stop')

        # dispatch to workers
        calls = []
        for worker, worker_aps in self.aps_by_worker.items():
            aps_worker = [
                ap.mac for ap in worker_aps if ap.mac in ap_ids]
            calls.append(WorkerCall_stop_aps(
                worker, aps_worker, max_concurrent_per_worker))
        self.__worker_calls(calls, wait)
        self.logger.debug("done stop_aps")

    def stop_clients(self, client_ids, max_concurrent=float('inf'), wait=True):
        """Stop running services on given APs.
        
        Args:
            clients_ids (list): ids of clients to to stop (mac addresses)
            max_concurrent (int): number of maximum concurrent shutdown processes at any time, default no limit
            wait (bool): if set, the function will return when the calls terminated, otherwise, returns immediately
        """
        max_concurrent_per_worker = max_concurrent
        if isinstance(max_concurrent, int):
            max_concurrent_per_worker = max(max_concurrent/self.num_workers, 1)
        self.logger.debug("stop_clients")
        clients = [self.get_client_info_by_id(id) for id in client_ids]
        if not clients:
            raise Exception('No APs to stop')

        # dispatch to workers
        clients_per_worker = {}
        calls = []
        for client in clients:
            worker = self.worker_by_ap_mac[client.ap_info.mac]

            if worker not in clients_per_worker:
                clients_per_worker[worker] = []
            clients_per_worker[worker].append(client.mac)

        for worker, clients_for_worker in clients_per_worker.items():
            calls.append(WorkerCall_stop_clients(
                worker, clients_for_worker, max_concurrent_per_worker))

        self.__worker_calls(calls, wait)
        self.logger.debug("done stop_clients")


    def join_aps(self, ap_ids, max_concurrent=float('inf'), wait=True):
        """Launch join process for each ap given by ids.

        Args:
            ap_ids (list): ids of ap to join, can be mac addresses, mac, udp port or name of each ap
            max_concurrent (int): number of maximum concurrent join processes at any time, default no limit
            wait (bool): if set, the function will return when the calls terminated, otherwise, returns immediately

        If max_concurrent is too high and machine performances are too low, it is possible that no AP would join.
        In this case performance warning reports will be printed on console.
        You have responsability to adapt this to fit the performance of the machine.
        """
        max_concurrent_per_worker = max_concurrent
        if isinstance(max_concurrent, int):
            max_concurrent_per_worker = max(max_concurrent/self.num_workers, 1)

        self.logger.debug("join_aps")
        aps = [self.get_ap_info_by_id(ap_id) for ap_id in ap_ids]
        if not aps:
            raise Exception('No APs to join')

        # dispatch to workers
        calls = []
        for worker, worker_aps in self.aps_by_worker.items():
            aps_to_join = [
                ap.name for ap in worker_aps if ap in self.aps_infos]
            calls.append(WorkerCall_join_aps(
                worker, aps_to_join, max_concurrent_per_worker))
        self.__worker_calls(calls, wait)
        self.logger.debug("done join_aps")

    def join_clients(self, client_ids, max_concurrent=float('inf'), wait=True):
        """Launch join process for each client given by ids.

        Args:
            client_ids: ids of clients to join, can be mac addresses, ip...
            max_concurrent: number of maximum concurrent join processes at any time, default no limit
            wait: if set, the function will return when the calls terminated, otherwise, returns immediately

        If max_concurrent is too high and machine performances are too low, it is possible that no AP would join.
        In this case performance warning reports will be printed on console.
        You have the responsability to adapt this to fit the performance of the machine.
        """
        self.add_clients_service(client_ids=client_ids, service_class=self.client_asso_service,
                                 wait_for_completion=wait, max_concurrent=max_concurrent)

    def add_aps_service(self, ap_ids, service_class, wait_for_completion=False, max_concurrent=float('inf')):
        """Add a APService to a list of AP.

        Args:
            ap_ids: ids of the aps, mac addresses, if None, it runs on all APs
            service_class (string): Class (including its package) to run as a service
            max_concurrent: maximum number of concurrent service
                of the number of services is greater than max_concurrent some services will wait for the other to finish before starting.
        """
        assert(self.num_workers > 0)
        self.__load_service(service_class, client=False)
        max_concurrent_per_worker = max(max_concurrent/self.num_workers, 1)

        calls = []

        aps_per_worker = {}  # worker -> list of attached aps that needs the service to be run
        if ap_ids:
            for ap_id in ap_ids:
                try:
                    ap = self.get_ap_info_by_id(ap_id)
                    worker = self.worker_by_ap_mac[ap.mac]
                except KeyError:
                    raise ValueError(
                        "AP does not exist with given id : {}.".format(ap_id))

                if worker not in aps_per_worker:
                    aps_per_worker[worker] = []
                aps_per_worker[worker].append(ap_id)
        else:
            for ap in self.ap_info_by_mac.values():
                worker = self.worker_by_ap_mac[ap.mac]
                if worker not in aps_per_worker:
                    aps_per_worker[worker] = []
                aps_per_worker[worker].append(ap.mac)

        for worker, worker_ap_ids in aps_per_worker.items():
            try:
                calls.append(WorkerCall_add_services(
                    worker, worker_ap_ids, service_class, wait_for_completion, max_concurrent_per_worker))
            except ValueError as e:
                raise e

        self.__worker_calls(calls)

    def add_general_service(self, device_ids, service_class, wait_for_completion=False):
        """Add a ClientService to a list of clients.

        Args:
            device_ids (list): ids of the clients or aps (either aps or clients, not both), mac addresses
            service_class (string): Class (including its package) to run as a service
        """
        assert(device_ids)
        assert(self.num_workers > 0)

        self.__load_service(service_class, client=False)

        calls = []

        # worker -> list of attached devices that needs the service to be run
        devices_per_worker = {}
        for mac in device_ids:
            try:
                worker = self.worker_by_device_mac[mac]
            except KeyError:
                raise ValueError(
                    "device not registered with given mac: {}.".format(mac))

            if worker not in devices_per_worker:
                devices_per_worker[worker] = []
            devices_per_worker[worker].append(mac)

        for worker, worker_devices_ids in devices_per_worker.items():
            try:
                calls.append(WorkerCall_add_services(
                    worker, worker_devices_ids, service_class, wait_for_completion))
            except ValueError as e:
                raise e

        self.__worker_calls(calls)




    def add_clients_service(self, client_ids, service_class, wait_for_completion=False, max_concurrent=float('inf')):
        """Add a ClientService to a list of clients.

        Args:
            client_ids (list): ids of the clients, mac addresses, if None, applies to all clients
            service_class (string): Class (including its package) to run as a service
            max_concurrent: maximum number of concurrent service
                if the number of services is greater than max_concurrent,
                some services will wait for the other to finish before starting.
        """
        assert(self.num_workers > 0)
        self.__load_service(service_class, client=True)
        max_concurrent_per_worker = int(
            max(max_concurrent/self.num_workers, 1))

        calls = []

        # worker -> list of attached clients that needs the service to be run
        clients_per_worker = {}

        if client_ids:
            for client_id in client_ids:
                try:
                    worker = self.worker_by_ap_mac[self.client_info_by_id[client_id].ap_info.mac]
                except KeyError:
                    raise ValueError(
                        "client does not exist with given id : {}.".format(client_id))

                if worker not in clients_per_worker:
                    clients_per_worker[worker] = []
                clients_per_worker[worker].append(client_id)
        else:
            for client in self.clients:
                worker = self.worker_by_ap_mac[client.ap_info.mac]

                if worker not in clients_per_worker:
                    clients_per_worker[worker] = []
                clients_per_worker[worker].append(client.mac)

        for worker, worker_client_ids in clients_per_worker.items():
            try:
                calls.append(WorkerCall_add_services(
                    worker, worker_client_ids, service_class, wait_for_completion, max_concurrent_per_worker))
            except ValueError as e:
                raise e

        self.__worker_calls(calls)

    def set_base_values(self, ap_mac=None, ap_ip=None, ap_udp=None, ap_radio=None, client_mac=None, client_ip=None):
        """Set base values, used for generating ap and client incrementing configurations (ip, mac, ...).
        
        Args:
            ap_mac: start ap mac, defaults to None (no change)
            ap_ip: start ap ip, defaults to None (no change)
            ap_udp: start upd_port, defaults to None (no change)
            ap_radio: start udp radio mac, defaults to None (no change)
            client_mac: start client mac, defaults to None (no change)
            client_ip: start client ip, defaults to None (no change)
        """
        # first pass, check arguments
        if ap_mac:
            check_mac_addr(ap_mac)
        if ap_ip:
            check_ipv4_addr(ap_ip)
        if ap_udp:
            if ap_udp < 1023 and ap_udp > 65000:
                raise Exception(
                    'Base UDP port should be within range 1024-65000')
        if ap_radio:
            check_mac_addr(ap_radio)
            if ap_radio.split(':')[-1] != '00':
                raise Exception(
                    'Radio MACs should end with zero, got: %s' % ap_radio)
        if client_mac:
            check_mac_addr(client_mac)
        if client_ip:
            check_ipv4_addr(client_ip)

        # second pass, assign arguments
        if ap_mac:
            self.next_ap_mac = ap_mac
        if ap_ip:
            self.next_ap_ip = ap_ip
        if ap_udp:
            self.next_ap_udp = ap_udp
        if ap_radio:
            self.next_ap_radio = ap_radio
        if client_mac:
            self.next_client_mac = client_mac
        if client_ip:
            self.next_client_ip = client_ip

    def create_aps_params(self, num_aps, dhcp=False):
        """Helper function to generate ap parameters : a tuple of macs, ips, udp ports and radio macs.

        Args:
            num_aps (int): number of aps to create
            dhcp (bool): if set, no ip will be set to APs, Defaults to False
        Returns:
            tuple: (macs, ips, udp_ports, radio_macs), each element being of list of parameters
        """
        num_aps = int(num_aps)
        assert(num_aps >= 0)

        macs, ips, udp_ports, radio_macs = [], [], [], []
        for _ in range(num_aps):
            # mac
            while self.next_ap_mac in macs:
                self.next_ap_mac = increase_mac(self.next_ap_mac)
                # Make sure it doesn't end with 0
                mac = mac2str(self.next_ap_mac)
                mac = mac_str_to_num(mac)
                if mac & 0xff == 0:
                    self.next_ap_mac = increase_mac(self.next_ap_mac)
                assert is_valid_mac(self.next_ap_mac)
            macs.append(self.next_ap_mac)
            self.next_ap_mac = increase_mac(self.next_ap_mac)

            if not dhcp:
                # ip
                while self.next_ap_ip in ips:
                    self.next_ap_ip = increase_ip(self.next_ap_ip)
                    # Make sure it doens't end with 0 or 255
                    ip_val = ipv4_str_to_num(
                        is_valid_ipv4_ret(self.next_ap_ip))
                    if ip_val & 0xff == 0:
                        self.next_ap_ip = increase_ip(self.next_ap_ip)
                    elif ip_val & 0xff == 0xff:
                        self.next_ap_ip = increase_ip(self.next_ap_ip, val=2)
                    assert is_valid_ipv4(self.next_ap_ip)
                ips.append(self.next_ap_ip)
                self.next_ap_ip = increase_ip(self.next_ap_ip)

            # udp
            while self.next_ap_udp in udp_ports:
                if self.next_ap_udp >= 65500:
                    raise Exception(
                        'Can not increase base UDP any further: %s' % self.next_ap_udp)
                self.next_ap_udp += 1
            udp_ports.append(self.next_ap_udp)
            self.next_ap_udp += 1

            # radio
            while self.next_ap_radio in radio_macs:
                self.next_ap_radio = increase_mac(self.next_ap_radio, 256)
                assert is_valid_mac(self.next_ap_radio)
            radio_macs.append(self.next_ap_radio)
            self.next_ap_radio = increase_mac(self.next_ap_radio, 256)

        if dhcp:
            ips = [None] * num_aps

        return macs, ips, udp_ports, radio_macs

    def create_clients_params(self, num_clients, dhcp=False):
        """Helper function to generate client parameters : a tuple of macs, ips.
        
        Args:
            num_clients (int): number of clients to generate parameters for
        Returns:
            tuple: (macs, ips), each element being of list of parameters
        """
        macs = [None] * num_clients  # preallocation
        ips = [None] * num_clients
        for i in range(num_clients):
            # mac
            while self.next_client_mac in self._gen_client_macs:
                self.next_client_mac = increase_mac(self.next_client_mac)
                # Make sure it doesn't end with 0
                mac = mac2str(self.next_client_mac)
                mac = mac_str_to_num(mac)
                if mac & 0xff == 0:
                    self.next_client_mac = increase_mac(self.next_client_mac)

            macs[i] = self.next_client_mac
            self._gen_client_macs.append(self.next_client_mac)
            self.next_client_mac = increase_mac(self.next_client_mac)
            # ip
            if not dhcp:
                while self.next_client_ip in self._gen_client_ips:
                    self.next_client_ip = increase_ip(self.next_client_ip)

                    # Make sure it doens't end with 0 or 255
                    ip_val = ipv4_str_to_num(
                        is_valid_ipv4_ret(self.next_client_ip))
                    if ip_val & 0xff == 0xff:
                        self.next_client_ip = increase_ip(
                            self.next_client_ip, val=2)
                    elif ip_val & 0xff == 0:
                        self.next_client_ip = increase_ip(self.next_client_ip)

                ips[i] = self.next_client_ip
            self._gen_client_ips.append(self.next_client_ip)
            self.next_client_ip = increase_ip(self.next_client_ip)

        return macs, ips

    # traffic

    def remove_all_streams(self, ports=None):
        """Remove all added streams from ports.

        Args:
            ports (list): ports on which to remove the streams, default None (all ports)
        """
        with self.trex_client_lock:
            self.trex_client.remove_all_streams(ports)

    def remove_streams(self, stream_id_list, ports=None):
        """Remove certain streams from ports.

        Args:
            stream_id_list: list of streams id (int) to remove
            ports (list): ports on which to remove the streams, default None (all ports)
        """
        with self.trex_client_lock:
            self.trex_client.remove_all_streams(stream_id_list, ports)

    def add_profile(self, client_ids, filename, **k):
        """Add a profile (list of streams) on clients.
        
        Args:
            client_id (list): mac (string) of the clients
            filename (string): name of the Profile file
        """
        profile = STLProfile.load(filename, **k)
        streams = profile.get_streams()
        client_streams = {}
        for mac in client_ids:
            client_streams[mac] = streams
        return self.add_streams(client_streams)

    def add_streams(self, client_streams):
        """Add streams on clients.
        
        Args:
            client_streams (dict): dictionnay client_mac -> list of STLStream for this client
        Returns:
            ids (list): dict client_mac -> stream ids for this client
        """
        # split load onto workers
        # construct dict worker -> {client_mac -> STLStreams}
        by_worker = {}
        for client_mac, streams in client_streams.items():
            # get worker associated with the client
            worker = self.worker_by_device_mac[client_mac]
            if worker not in by_worker:
                by_worker[worker] = {}
            by_worker[worker][client_mac] = streams
        
        # patch streams
        by_worker = self.__patch_streams(by_worker)
        
        # add streams to trex client
        # split for port_ids
        streams_by_port_by_client = {}

        for worker, streams_by_client in by_worker.items():
            for client_mac, streams in streams_by_client.items():
                client = self.client_info_by_id[client_mac]
                port_id = client.ap_info.port_id

                if port_id not in streams_by_port_by_client:
                    streams_by_port_by_client[port_id] = {}
                if client_mac not in streams_by_port_by_client[port_id]:
                    streams_by_port_by_client[port_id][client_mac] = []
                streams_by_port_by_client[port_id][client_mac].extend(streams)

        stream_ids = {} # client_mac -> list of streams id
        for port_id, streams_by_client in streams_by_port_by_client.items():
            for client_mac, streams in streams_by_client.items():
                with self.trex_client_lock:
                    ids = self.trex_client.add_streams(streams, [port_id])
                if type(ids) is not list:
                    ids = [ids]

                if client_mac not in stream_ids:
                    stream_ids[client_mac] = []
                stream_ids[client_mac].extend(ids)

        return stream_ids
            
    def __patch_streams(self, client_streams_by_worker):
        """Patch client stream to be wrapped in capwap data by the AP.

        Args:
            client_streams (dict): worker -> {client_mac -> STLStreams}
        Returns:
            client_streams (dict): patched input worker -> {client_mac -> STLStreams}
        """
        patched = {}

        for worker, client_streams in client_streams_by_worker.items():
            patched[worker] = {}
            client_pkts = {}

            for client_mac, streams in client_streams.items():
                client = self.client_info_by_id[client_mac]
                with self.trex_client_lock:
                    port_layer = self.trex_client.ports[client.ap_info.port_id].get_layer_cfg()
                patched_streams = []
                client_pkts[client_mac] = []

                for stream in streams:
                    assert(isinstance(stream, STLStream))

                    stream = copy.deepcopy(stream)
                    patched_pkt = Ether(stream.pkt)
                    if stream.fields['packet']['meta']:
                        pkt_meta = '%s\nPatched stream: Added WLAN' % stream.fields['packet']['meta']
                    else:
                        pkt_meta = 'Patched stream: Added WLAN'
                    
                    if stream.fields['flags'] & 1 == 0:
                        pkt_meta += ', Changed source'
                        patched_pkt.src = port_layer['ether']['src']
                    if stream.fields['flags'] & 0x110 == 0:
                        pkt_meta += ', Changed destination'
                    
                    stream.fields['packet']['meta'] = pkt_meta
                
                    # set the destination MAC address to be the WLC's
                    patched_pkt.dst = port_layer['ether']['dst']

                    stream.pkt = patched_pkt
                    client_pkts[client_mac].append(patched_pkt)
                    patched_streams.append(stream)

                client_streams[client_mac] = patched_streams

            # make packets pickable
            for client in client_pkts.keys():
                client_pkts[client] = [ bytes(p) for p in client_pkts[client] ]

            client_packets = self.__worker_call(
                WorkerCall_wrap_client_pkts(worker, client_pkts)
            )

            for client_mac, packets in client_packets.items():
                assert(len(client_streams_by_worker[worker][client_mac]) == len(packets))
                for stream, patched_pkt in zip(client_streams_by_worker[worker][client_mac], packets):
                    stream.pkt = patched_pkt
                    stream.fields['packet']['binary'] = base64encode(stream.pkt)


                # instruct to offset the default 0 offset when parsing
                # (avoid capwap data encapsulation for rules applying to capwap data payload (client packet))
                for inst in stream.fields['vm']['instructions']:
                    if 'pkt_offset' in inst:
                        inst['pkt_offset'] += 78 # Size of wrapping layers minus removed Ethernet
                    elif 'offset' in inst:
                        inst['offset'] += 78

        return client_streams_by_worker

    def start_streams(self,
               ports = None,
               mult = "1",
               force = True,
               duration = -1,
               total = False,
               core_mask = None,
               synchronized = False):
        """Start the streams attached to given ports.

        Args:
            ports (list): ports to start the traffic on, defaults to None (all attached ports)
            mult (string): Multiplier in a form of pps, bps, or line util in %
                Examples: "5kpps", "10gbps", "85%", "32mbps"
            force (bool): If the ports are not in stopped mode or do not have sufficient bandwidth for the traffic, determines whether to stop the current traffic and force start.
                True: Force start, False: Do not force start
            duration (int): Limit the run time (seconds), defaults to -1 (unlimited)
            total (bool): Determines whether to divide the configured bandwidth among the ports, or to duplicate the bandwidth for each port.
                True: Divide bandwidth among the ports, False: Duplicate
            core_mask: CORE_MASK_SPLIT, CORE_MASK_PIN, CORE_MASK_SINGLE or a list of masks (one per port)
                Determine the allocation of cores per port
                In CORE_MASK_SPLIT all the traffic will be divided equally between all the cores
                associated with each port
                In CORE_MASK_PIN, for each dual ports (a group that shares the same cores)
                the cores will be divided half pinned for each port
            synchronized (bool): In case of several ports, ensure their transmitting time is syncronized.
                Must use adjacent ports (belong to same set of cores).
                Will set default core_mask to 0x1.
                Recommended ipg 1ms and more.
        """
        with self.trex_client_lock:

            if not ports:
                    ports = self.trex_client.get_acquired_ports()
            
            if ports:
                active_ports = set(self.trex_client.get_active_ports())
                ports_to_stop = set(ports).intersection(active_ports)
                if ports_to_stop:
                    self.trex_client.stop(ports_to_stop)            

            ret = self.trex_client.start(ports, mult, force, duration, total, core_mask, synchronized)
        return ret
        
    
    # getters

    def get_base_values(self):
        """Return next base values:
        OrderedDict  {"AP MAC": ap_mac, "AP IP": ap_ip, "AP UDP port": ap_udp, "AP RADIO": ap_radio, "Client MAC": client_mac, "Client IP": client_ip).
        """
        d = OrderedDict()
        d["AP MAC"] = self.next_ap_mac
        d["AP IP"] = self.next_ap_ip
        d["AP RADIO"] = self.next_ap_radio
        d["AP UDP port"] = self.next_ap_udp
        d["Client MAC"] = self.next_client_mac
        d["Client IP"] = self.next_client_ip
        return d

    def get_ap_count(self):
        """Return the number of APs created."""
        return len(self.aps_infos)

    def get_client_count(self):
        """Return the number of Clients created."""
        return len(self.clients)

    def get_clients_services_info(self):
        """Retrieve the information of each service of each client."""
        records = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                res = self.__worker_call(
                    WorkerCall_get_clients_services_info(worker))
                if res:
                    records.extend(res)
        return records

    def get_clients_service_info_for_service(self, service_name, key=None):
        """Retrieve the information of a service of each client.

        Args:
        service_name: name of the service to retrieve info about
        key: The key to retrieve the info for. If key is None everything is returned
        """

        if key:
            records = []
            for workers in self.workers_by_port.values():
                for worker in workers:
                    res = self.__worker_call(
                        WorkerCall_get_clients_service_specific_info(worker, service_name, key))
                    if res:
                        records.extend(res)
            return records
        else:
            infos = self.get_clients_services_info()
            return [client_info[service_name] for client_info in infos if service_name in client_info]

    def set_clients_service_info_for_service(self, service_name, key, value):
        """Set the information of a service of each client.

        Args:
        service_name: name of the service to set the info about
        key: The key to set the info for.
        value: The value to set (needs to be pickled)
        """

        for workers in self.workers_by_port.values():
            for worker in workers:
                self.__worker_call(
                    WorkerCall_set_clients_service_specific_info(worker, service_name, key, value))

    def get_clients_done_count(self, service_name):
        """Retrieve the specific information for a service.

        Args:
            service_name: name of the service to retreive info from
            key: key, id of the info to retreive
        """
        calls = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                calls.append(WorkerCall_get_clients_done_count(
                    worker, service_name))
        resps = self.__worker_calls(calls)
        cnt = sum(resps)
        return cnt

    def get_aps_services_info(self):
        """Retrieve the information of each service of each ap."""
        records = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                records.extend(self.__worker_call(
                    WorkerCall_get_aps_services_info(worker)))
        return records

    def get_aps_service_info_for_service(self, service_name, key=None):
        """Retrieve the information of a service of each ap.

        Args:
        service_name: name of the service to retrieve info about
        key: The key to retrieve the info for. If key is None everything is returned
        """

        if key:
            records = []
            for workers in self.workers_by_port.values():
                for worker in workers:
                    res = self.__worker_call(
                        WorkerCall_get_aps_service_specific_info(worker, service_name, key))
                    if res:
                        records.extend(res)
            return records
        else:
            infos = self.get_aps_services_info()
            return [ap_info[service_name] for ap_info in infos if service_name in ap_info]

    def set_aps_service_info_for_service(self, service_name, key, value):
        """Set the information of a service of each AP.

        Args:
        service_name: name of the service to set the info about
        key: The key to set the info for.
        value: The value to set (needs to be pickled)
        """

        for workers in self.workers_by_port.values():
            for worker in workers:
                self.__worker_call(
                    WorkerCall_set_aps_service_specific_info(worker, service_name, key, value))

    def get_client_states(self):
        """Retrieve the states of all attached Clients, unordered."""
        states = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                worker_states = self.__worker_call(
                    WorkerCall_get_client_states(worker))
                states.extend(worker_states)
        return states
    
    def get_client_joined_count(self):
        """Return the number of associated Clients."""
        states = self.get_client_states()
        return len([s for s in states if s == ClientState.RUN])

    def get_ap_joined_count(self):
        """Return the number of joined APs."""
        states = self.get_ap_states()
        return len([s[0] for s in states if s[1] == APState.RUN])

    def get_ap_states(self):
        """Retrieve the states of all attached APs, unordered."""
        calls = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                calls.append(WorkerCall_get_ap_states(worker))
        results = self.__worker_calls(calls)
        return [state for result in results for state in result]

    def get_ap_join_times(self):
        calls = []
        for workers in self.workers_by_port.values():
            calls.extend([WorkerCall_get_ap_join_times(worker)
                          for worker in workers])

        resps = self.__worker_calls(calls)
        if resps:
            return [time for resp in resps for time in resp if resp]

    def get_ap_join_durations(self):
        calls = []
        for workers in self.workers_by_port.values():
            calls.extend([WorkerCall_get_ap_join_durations(worker)
                          for worker in workers])

        resps = self.__worker_calls(calls)
        if resps:
            return [time for resp in resps for time in resp if resp]

    def get_aps_retries(self, ap_ids):
        calls = []
        for workers in self.workers_by_port.values():
            for worker in workers:
                calls.append(WorkerCall_get_aps_retries(worker, ap_ids))
        resps = self.__worker_calls(calls)
        return resps

    def get_ap_info_by_id(self, ap_id):
        """Return the attached ap with given id, which can be the ip, mac, name, udp port or radio mac.

        Args:
        ap_id: id of an AP, can be mac address, ip address, radio_mac ...
        """
        if isinstance(ap_id, AP):
            return ap_id
        if ap_id in self.ap_info_by_name:
            return self.ap_info_by_name[ap_id]
        elif ap_id in self.ap_info_by_mac:
            return self.ap_info_by_mac[ap_id]
        elif ap_id in self.ap_info_by_ip:
            return self.ap_info_by_ip[ap_id]
        elif ap_id in self.ap_info_by_udp_port:
            return self.ap_info_by_udp_port[ap_id]
        elif ap_id in self.ap_info_by_radio_mac:
            return self.ap_info_by_radio_mac[ap_id]
        else:
            raise Exception('AP with id %s does not exist!' % ap_id)

    def get_client_info_by_id(self, client_id):
        """Return the attached client with given id (mac or ip).

        Args:
        client_id: id of a wireless Client, can be mac address or ip address
        """
        if client_id in self.client_info_by_id:
            return self.client_info_by_id[client_id]
        else:
            raise Exception('Client with id %s does not exist!' % client_id)

    # Implementation

    def __connect_client(self, port_ids):
        """Connects the TrafficHandler to the TRexClient."""
        self.trex_client_lock = threading.Lock()
        with self.trex_client_lock:
            if not self.trex_client:
                self.trex_client = STLClient(self.server_ip)
            if not self.trex_client.is_connected():
                self.trex_client.connect()
            self.trex_client.acquire(port_ids, force=True)
            for port in port_ids:
                self.trex_client.set_service_mode(port, True)
                capture_port = get_capture_port(port)
                bpf_filter = ('arp or (ip and (icmp or udp src port 5246 or udp src port 5247 or '  # arp, ping, capwap ctrl/data
                    # capwap data keep-alive
                    + '(udp src port 5247 and (udp[11] & 8 == 8 or '
                    # client assoc. resp
                    + 'udp[16:2] == 16 or '
                    # client deauthentication
                    + 'udp[16:2] == 192 or '
                    # client arp
                    + 'udp[48:2] == 2054 or '
                    # client ping
                    + '(udp[48:2] == 2048 and udp[59] == 1)))))')

                # Make sure the port is stopped
                self.trex_client.stop_capture_port(port)

                # Start it
                self.trex_client.start_capture_port(port, capture_port, bpf_filter)
                if not self.trex_client.get_port_attr(port=port)['prom'] == 'on':
                    self.trex_client.set_port_attr(
                        ports=port, promiscuous=True)


    def __disconnect_client(self):
        with self.trex_client_lock:
            self.trex_client.stop()
            self.trex_client.disconnect()

    def __get_port_layer_cfg(self, trex_port_id):
        """Return the configuration of the Trex Port."""
        with self.trex_client_lock:
            ret = self.trex_client.ports[trex_port_id].get_layer_cfg()
        return ret

    def __force_stop_workers(self):
        """Terminate the workers."""
        self.logger.debug("force stopping workers")

        for port_id in self.trex_port_ids:
            for p in self.workers_by_port[port_id]:
                p.terminate()
                p.join()

    def __force_stop_traffic_handlers(self):
        """Terminate the traffic handlers."""
        self.logger.debug("force stopping traffic handlers")
        for traffic_handler in self.traffic_handlers_by_port.values():
            traffic_handler.terminate()
            traffic_handler.join()

    def __force_stop(self):
        """Force stop the WirelessManager and sub processes."""
        self.__force_stop_workers()
        self.__force_stop_traffic_handlers()

        self.logger.debug("stopping log listener")
        # send quit signal to log listener
        self.log_queue.put(None)
        self.is_running = False
        return

    def __handle_remote_exception(self, report):
        """Handler for a received remote exception from sub processes."""
        # an exception occured in subprocess
        print("An Exception occured in a Worker:")
        print(report)
        print("Stopping...")
        self.__force_stop()
        print("Stopped")

    def __apply_device_update(self, update):
        """Update Manager's devices state from an update.

        Args:
            update (WirelessDeviceStateUpdate): update from Worker
        """
        try:
            identifier = update.identifier
            if identifier in self.client_info_by_id:
                device = self.client_info_by_id[identifier]
            elif identifier in self.ap_info_by_mac:
                device = self.ap_info_by_mac[identifier]
            else:
                raise ValueError("unknown device: {}".format(identifier))
            
            # update AP
            if isinstance(device, APInfo):
                ap = device
            # update Client
            elif isinstance(device, ClientInfo):    
                client = device


                new_ip_client, deleted_ip_client, modified_ip_client = False, False, False
                if 'ip' in update.update:
                    new_ip = update.update['ip']

                    # can instruct trex-server that there is a new client

                    new_ip_client = client.ip is None and new_ip is not None
                    deleted_ip_client = client.ip is not None and new_ip is None
                    modified_ip_client = client.ip is not None and new_ip is not None

                elif client.ip and 'state' in update.update and update.update['state'] == ClientState.RUN:
                    new_ip = client.ip
                    new_ip_client, deleted_ip_client, modified_ip_client = True, False, False



                if deleted_ip_client or modified_ip_client:
                    # need to delete from server outdated info
                    self.logger.debug("removing capwap client from server")
                    # FIXME: Remove CAPWAP Info from TRex Server
                    # self.trex_client.remove_capwap_client(client.mac, ports=[client.ap_info.port_id])

                if new_ip_client or modified_ip_client:
                    # need to add to server new info
                    self.logger.debug("adding capwap client to server")
                    
                    # FIXME: Add CAPWAP Info to TRex Server
                    # self.trex_client.add_capwap_client(
                    #     client_mac=client.mac,
                    #     ap_ip=client.ap_info.ip,
                    #     wlc_ip=client.ap_info.wlc_ip,
                    #     ap_mac=client.ap_info.mac,
                    #     wlc_mac=client.ap_info.wlc_mac,
                    #     radio_mac=client.ap_info.radio_mac,
                    #     ports=[client.ap_info.port_id])

            # fix all updated attributes
            for name, value in update.update.items():
                setattr(device, name, value)
                
        except Exception as e:
            self.logger.exception('Error in updating devices')
            print(e)

    def __device_state_updater(self):
        """Thread responsible for keeping the devices' state updated in the Manager.
        Eventual Consistency.
        """

        # subscribe on device updates
        sub = self.pubsub.subscribe("WirelessDevice")
        self.logger.debug("device state updater started")
        try:
            while True:
                message = sub.get()
                update = message.value
                if not isinstance(update, WirelessDeviceStateUpdate):
                    continue
                self.__apply_device_update(update)
        except EOFError:
            return
        except Exception as e:
            self.logger.warn("device updater stopped: {}".format(e))

    def __management_traffic_handlers(self):
        """Management Thread, communication between WirelessManager and TrafficHandlers.

        Read on cmd_pipe and store responses from rpc calls into rc_responses for later use by e.g. API.
        """
        self.logger.info("Management (Traffic Handlers) Thread started")
        while True:
            ready_pipes, _, _ = select.select(
                self.traffic_handlers_cmd_connections, [], [])
            for pipe in ready_pipes:
                resp = pipe.recv()
                if isinstance(resp, RPCResponse):
                    self.logger.debug(
                        "received response from traffic handler: {}".format(resp))

                    if resp.code != RPCResponse.SUCCESS:
                        self.logger.debug(
                            "handler_call id {} received non successful response: {}".format(resp.id, resp))
                        continue

                    with self.new_handler_rc_response:
                        self.traffic_handlers_rc_responses[resp.id] = resp.ret
                        self.new_handler_rc_response.notify_all()

                elif isinstance(resp, RPCExceptionReport):
                    self.__handle_remote_exception(resp)
                else:
                    self.logger.warning(
                        "bad response from traffic handler: {}".format(resp))

    def __management_workers(self):
        """Management Thread, communication between WirelessManager and WirelessWorkers.

        Read on cmd_pipe and store responses from rpc calls into rc_responses for later use by e.g. API.
        """
        self.logger.info("Management (Workers) Thread started")
        while True:
            ready_pipes, _, _ = select.select(
                self.worker_cmd_connections, [], [])
            for pipe in ready_pipes:
                resp = pipe.recv()

                if isinstance(resp, RPCResponse):
                    self.logger.debug(
                        "received response from worker: {}".format(resp))

                    if resp.code != RPCResponse.SUCCESS:
                        self.logger.debug(
                            "worker_call id {} received non successful response: {}".format(resp.id, resp))
                        continue
                    # store response's return value(s) as is
                    self.rc_responses[resp.id] = resp.ret
                    # new response event, receiver thread should clear it when received
                    with self.new_rc_response:
                        self.new_rc_response.notify_all()
                elif isinstance(resp, RPCExceptionReport):
                    self.__handle_remote_exception(resp)
                    return
                else:
                    self.logger.warning(
                        "bad response from worker: {}".format(resp))

        self.logger.info("Management Thread stopped")

    def __handler_call(self, call, port_id):
        """Perform the remote call on a worker and wait for the response and return the response.

        Args:
            port_id: trex port id, on which is attached the TrafficHandler
            call: TrafficHandlerCall instance that describes the call (traffic handler, method, arguments and protocol specific fields)
        """
        try:
            conn, lock = self.traffic_handlers_cmd_connection_by_port[port_id]
        except KeyError:
            raise ValueError(
                "No TrafficHandler exists on port {}".format(port_id))

        self.logger.debug("sending {} call to traffic handler".format(call))
        with lock:
            conn.send(call)

        def predicate():
            resps = self.traffic_handlers_rc_responses.keys()
            return call.id in resps

        with self.new_handler_rc_response:
            self.new_handler_rc_response.wait_for(predicate)
            resp = self.traffic_handlers_rc_responses[call.id]

        return resp

    def __worker_call(self, call, wait=True):
        """Perform the remote call on a worker and wait for the response and return the response.

        Args:
            call: WorkerCall instance that describes the call (worker, method, arguments and protocol specific fields)
            wait: if set, the function will return when the calls terminated, otherwise, returns as soon as the calls are sent
        """
        return self.__worker_calls([call])[0]

    def __worker_calls(self, calls, wait=True):
        """Perform the remote calls on workers and wait for all the response, and return those responses as a list.

        Args:
            calls: WorkerCall list, each describing the call (worker, method, arguments and protocol specific fields)
            wait: if set, the function will return when the calls terminated, otherwise, returns as soon as the calls are sent
        """

        def wait_for_responses(calls):
            """Block until all responses from given WorkerCalls have been received from WirelessWorker(s).
            Args:
            calls: WorkerCall list of the responses to wait for
            """

            def predicate():
                call_ids = set([call.id for call in calls])

                resps = self.rc_responses.keys()
                return call_ids.issubset(set(resps))

            with self.new_rc_response:
                self.new_rc_response.wait_for(predicate)
                return [self.rc_responses[id] for id in [call.id for call in calls]]

        self.logger.debug("worker_calls : {}".format(
            [str(call) for call in calls]))
        for call in calls:
            worker = call.worker
            # then send command request
            conn, lock = self.workers_cmd_connection_by_port[worker.port_id][worker.worker_id]
            with lock:
                conn.send(call)

        responses = None
        if wait:
            # now wait for the responses
            responses = wait_for_responses(calls)

        self.logger.debug("done worker_calls : {}".format(
            [str(call) for call in calls]))
        return responses

    def _init_base_vals(self):
        '''Initilize values for ap and client ip and mac info.'''
        try:
            self.set_base_values(load=True)
        except:
            # Default values
            self.next_ap_mac = '94:12:12:12:12:01'
            self.next_ap_ip = '9.9.12.1'
            self.next_ap_udp = 10001
            self.next_ap_radio = '94:14:14:14:01:00'
            self.next_client_mac = '94:13:13:13:13:01'
            self.next_client_ip = '9.9.13.1'
            self.wlc_ip = '255.255.255.255'

    def __add_ap_info(self, ap_info, worker):
        """Add ap info to manager."""
        if worker not in self.aps_by_worker:
            self.aps_by_worker[worker] = []
        self.aps_by_worker[worker].append(ap_info)
        self.ap_info_by_name[ap_info.name] = ap_info
        self.ap_info_by_mac[ap_info.mac] = ap_info
        self.ap_info_by_ip[ap_info.ip] = ap_info
        self.ap_info_by_udp_port[ap_info.udp_port] = ap_info
        self.ap_info_by_radio_mac[ap_info.radio_mac] = ap_info
        self.worker_by_ap_mac[ap_info.mac] = worker
        self.worker_by_device_mac[ap_info.mac] = worker
        self.aps_infos.append(ap_info)

    def __load_service(self, service_class, client=False):
        """Load a service from name, and return the class of the service.

        Args:
            service_class: class & module to load from (e.g. mod1.mod2.thing.Class)
            client: if set, will add client filters
        """
        try:
            service = load_service(service_class)
            if client:
                if self.client_filters:
                    self.client_filters += " or {}".format(
                        service.FILTER)
                else:
                    self.client_filters += service.FILTER
            else:
                if self.filters:
                    self.filters += " or {}".format(service.FILTER)
                else:
                    self.filters += service.FILTER
            #FIXME: Update filter of capture port
        except ImportError:
            raise ValueError("cannot import module " + service_class)
        except AttributeError:
            raise ValueError(
                "service {} does not exist".format(service_class))

class APMode:
    """Describe the mode of operation of an AP. """
    LOCAL = 'Local'
    REMOTE = 'Remote'
