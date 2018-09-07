import zmq
from zmq.error import ZMQError
import threading
import time
import os
from collections import deque
import ctypes
from .logger import *
from trex.common import *
from trex.common.trex_exceptions import TRexError
from .trex_wireless_traffic_handler_rpc import *
from .utils.utils import remote_call, RemoteCallable, get_capture_port

polling_interval = 0.001


@RemoteCallable
class TrafficHandler(multiprocessing.Process):
    """Process that forwards packets from workers to socket, and demultiplex packets from socket to workers."""

    bpf_filter = ('arp or (ip and (icmp or udp src port 5246 or '  # arp, ping, capwap control
                  # capwap data keep-alive
                  + '(udp src port 5247 and (udp[11] & 8 == 8 or '
                  # client assoc. resp
                  + 'udp[16:2] == 16 or '
                  # client deauthentication
                  + 'udp[16:2] == 192 or '
                  # client arp
                  + 'udp[48:2] == 2054 or '
                  + '(udp[48:2] == 2048 and udp[59] == 1)))))')  # client ping

    def __init__(self, cmd_pipe, server_ip, port_id, worker_connections, log_queue, log_level, log_filter, pubsub, parent_publisher=None, filters="", client_filters=""):
        """Construct a TrafficHandler.

        Args:
            cmd_pipe: pipe end (Connection) of a command pipe between TrafficHandler and AP_C_Mananger
            server_ip: server IP
            port_id: trex port id
            worker_connections: a list of pipe ends (Connections) to all workers this TrafficHandler should handle.
            log_queue: queue for log messages
            log_level: level of logs
            log_filter: log filter
            filters: additional bpf filters for services, e.g. "udp port 1234"
            client_filters: additional bpf client filters for services, e.g. udp port 1234"
                            these rules should be rules for clients, i.e. not below capwap data
            pubsub: PubSub reference
            parent_publisher: if present, the pubsub parent publisher
                        all pubsub messages will be published in a subtopic of the parent's
        """
        super().__init__()
        self.name = "TrafficHandler_Port:{}".format(port_id)
        self.server_ip = server_ip
        self.port_id = port_id
        self.filter = "({})".format(TrafficHandler.bpf_filter)
        if client_filters:
            # standard filters or (CAPWAP DATA and user filters)
            self.filter += " or (udp src port 5247 and ({}))".format(client_filters)
        if filters:
            self.filter += " or ({})".format(filters)

        self.threads = {}

        # logging
        self._log_queue = log_queue
        self._log_level = log_level
        self._log_filter = log_filter

        # pubsub
        self.pubsub = pubsub
        if parent_publisher:
            self.publisher = parent_publisher.SubPublisher("TrafficHandler")
        else:
            self.publisher = pubsub.Publisher("TrafficHandler")

        # packet Connections between Workers and the Traffic Handler
        self.workers_connections = worker_connections

        self.manager_cmd_connection = cmd_pipe

        self.aps_info = []
        self.mac_routes_lock = threading.Lock()
        self.pkt_connection_id_by_mac = {}  # map: mac address -> connection index

    def connect_zmq(self):
        """Connects the TrafficHandler to the server ZMQ packet socket."""
        # Bind our ZeroMQ
        # FIXME: Add a setting for this
        self.capture_port = get_capture_port(self.port_id)
        self.zmq_context = zmq.Context()
        self.zmq_socket = self.zmq_context.socket(zmq.PAIR)
        self.zmq_socket.bind(self.capture_port)
        self.logger.debug("ZeroMQ Socket connected to Capture Port")
    
    def disconnect_zmq(self):
        """Dicconnects the TrafficHandler from the server ZMQ packet socket."""
        if self.zmq_context:
            self.zmq_context.term()

    def init(self):
        """Finish initializing a TrafficHandler, should be called in process' run method."""
        # clearing handlers
        handlers = list(logging.getLogger().handlers)
        for h in handlers:
            logging.getLogger().removeHandler(h)

        self.manager_cmd_connection_lock = threading.Lock()

        try:
            self.logger = get_queue_logger(self._log_queue, type(
                self).__name__, self.name, self._log_level, self._log_filter)
        except Exception as e:
            import traceback
            traceback.print_exc()

        self.is_stopped = threading.Event()

        self.connect_zmq()
        
    def run(self):
        """Run the Traffic Handler, once launched it has to be stopped by sending it a stop message.

        Setup and launch threads.
        A Traffic Handler has 2 main threads :
        traffic: receive packets from workers and push them to the Trex Server as well as receiving them from TRex server to push them to the workers
        management: thread for receiving management commands from a WirelessManager, e.g. start or stop commands (see @remote_call methods).
        """
        try:
            # finish initialization
            self.init()

            self.logger.info("started, PID %s" % os.getpid())
            self.publisher.publish("started")

            # create threads
            self.threads["traffic"] = threading.Thread(
                target=self.__traffic, args=(), daemon=True)
            self.threads["management"] = threading.Thread(
                target=self.__management, args=(), daemon=True)

            # launch threads
            for k, t in self.threads.items():
                t.start()
                self.logger.debug("thread %s started" % k)

            for k, t in self.threads.items():
                t.join()
                self.logger.debug("thread %s joined" % k)

            self.is_stopped.wait()

            self.logger.debug("stopped")
        except KeyboardInterrupt:
            try:
                for k, t in self.threads.items():
                    t.join()
                    self.logger.debug("thread %s joined" % k)

                self.logger.debug(
                    "{} stopped after KeyboardInterrupt".format(self.name))
            except AttributeError:
                # no logger
                pass
        except Exception as e:
            report = RPCExceptionReport(e)
            with self.manager_cmd_connection_lock:
                self.manager_cmd_connection.send(report)

    @remote_call
    def route_macs(self, mac_to_connection_id_map):
        """Set the connection for packets with given mac.
        Upon receiving a packet with mac address in 'mac_to_connection_map' keys,
        the Traffic Handler will forward it to (and only to) given 'connection' that is the value associated with the mac address.
        Will override previous association if collision.

        Args:
            mac_to_connection_map: dictionnary of mac keys and connection index (worker) values, each value being the associated connection id to the key mac.
        """
        with self.mac_routes_lock:
            # check that all connections actually exists in traffic handlers
            ids = mac_to_connection_id_map.values()
            if any([True for _id in ids if _id >= len(self.workers_connections) or _id < 0]):
                raise ValueError("connections ids must exist in TrafficHandler")

            for mac_str, conn in mac_to_connection_id_map.items():
                mac = bytes.fromhex(mac_str.replace(
                    ":", ""))  # mac string to bytes
                self.pkt_connection_id_by_mac[mac] = conn
        return

    @remote_call
    def stop(self):
        """Stop the Traffic Handler."""
        self.logger.info("stopping")
        self.is_stopped.set()
        try:
            self.disconnect_zmq()
        except TRexError as e:
            self.logger.exception("encountered error when stopping")
        self.logger.info("stopped")
        return

    # Threads functions

    def __management(self):
        """Thread responsible for handling commands from WirelessManager (received via manager_cmd_connection)."""
        try:
            while not self.is_stopped.is_set():
                # Manager <-> Traffic Handler
                msg = self.manager_cmd_connection.recv()

                if isinstance(msg, TrafficHandlerCall):
                    # Worker Call
                    traffic_handler_call = msg
                    if traffic_handler_call.NAME not in self.remote_calls:
                        # command does not exist
                        self.logger.warning(
                            "bad remote call: ' {}' command does not exist: {}".format(traffic_handler_call.NAME, msg))
                        raise ValueError("bad remote call, {} does not exist".format(traffic_handler_call))
                    command = self.remote_calls[traffic_handler_call.NAME]

                    self.logger.debug(
                        "remote call: {} args: {}".format(traffic_handler_call.NAME, traffic_handler_call.args))

                    args = traffic_handler_call.args
                    try:
                        if args:
                            ret = command(self, *args)
                        else:
                            ret = command(self)
                        code = RPCResponse.SUCCESS
                    except Exception as e:
                        # this is an exception happened during the call
                        print("Exception on %s:\n%s" %
                              (self.name, traceback.format_exc()))
                        ret = None
                        code = RPCResponse.ERROR

                    resp = RPCResponse(traffic_handler_call.id, code, ret)
                    with self.manager_cmd_connection_lock:
                        self.manager_cmd_connection.send(resp)
                else:
                    e = ValueError("bad message, management thread should only receive TrafficHandlerCalls")
                    report = RPCExceptionReport(e)
                    with self.manager_cmd_connection_lock:
                        self.manager_cmd_connection.send(report)
                    return
        except Exception as e:
            self.logger.exception('Management thread exception')
            report = RPCExceptionReport(e)
            with self.manager_cmd_connection_lock:
                self.manager_cmd_connection.send(report)
            return


    def __traffic(self):
        """Thread responsible for retrieving packets from trex server and send them to the workers and the other way around."""
        try:
            self.logger.info("traffic started")
            sockets = {}
            for c in self.workers_connections:
                sockets[c.fileno()] = c


            # Create list of socket to listen to
            nb_fds = len(self.workers_connections) + 1
            polling_items = (zmq.zmq_pollitem_t * nb_fds)()

            for i in range(len(self.workers_connections)):
                polling_items[i].socket = 0
                polling_items[i].fd = self.workers_connections[i].fileno()
                polling_items[i].events = zmq.POLLIN
                polling_items[i].revents = 0

            polling_items[len(self.workers_connections)].socket = self.zmq_socket.handle
            polling_items[len(self.workers_connections)].fd = 0
            polling_items[len(self.workers_connections)].events = zmq.POLLIN
            polling_items[len(self.workers_connections)].revents = 0

            timeout = -1
            while not self.is_stopped.is_set():
                ret = zmq.zmq_poll(polling_items, ctypes.c_int(nb_fds), ctypes.c_long(timeout))
                if ret == -1:
                    # Error
                    self.logger.error("Error in ZMQ select, returned %d", ret)
                    break
                elif ret > 0:
                    # Some events occured
                    for l in polling_items:
                        if (l.revents & zmq.POLLIN) == 0:
                            continue
                        if l.socket == self.zmq_socket.handle:
                            rx_bytes = self.zmq_socket.recv()
                            dst_mac = rx_bytes[:6]

                            # packet switching
                            with self.mac_routes_lock:
                                pkt_connection_id = self.pkt_connection_id_by_mac.get(
                                    dst_mac, None)

                                if pkt_connection_id is not None:
                                    self.logger.debug(
                                        "forwarded packet to worker: mac: {}".format(dst_mac))
                                    self.workers_connections[pkt_connection_id].send(
                                        rx_bytes)

                                elif dst_mac == b'\xff\xff\xff\xff\xff\xff':
                                    # send to all workers
                                    self.logger.debug(
                                        "received broadcast frame, forwarding to all workers")
                                    for pipe_id in self.pkt_connection_id_by_mac.values():

                                        self.workers_connections[pipe_id].send(
                                            rx_bytes)

                                else:
                                    self.logger.debug(
                                        "dropped packet: unknown mac: {}".format(dst_mac))
                        elif l.fd:
                            sock = sockets[l.fd]
                            while sock.poll():
                                pkt = sock.recv()
                                self.zmq_socket.send(pkt)

            self.logger.info("traffic stopped")
        except EOFError:
            # stop
            return
        except zmq.error.ContextTerminated as e:
            # FIXME happens at the end
            self.logger.warn("traffic stopped due to ZMQ error: {}".format(e))
            self.stop()
        except zmq.error.ZMQError as e:
            if self.is_stopped.is_set():
                # Stop
                return
            self.logger.warn("traffic stopped due to ZMQ error: {}".format(e))
            self.stop()
        except TRexError as e:
            self.logger.warn("down_up stopped due to TRexError")
            self.stop()
        except Exception as e:
            self.logger.exception("Unexpected exception")
            try:
                report = RPCExceptionReport(e)
                with self.manager_cmd_connection_lock:
                    self.manager_cmd_connection.send(report)
            except ConnectionError:
                return
