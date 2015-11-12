#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages
from client_utils.jsonrpc_client import JsonRpcClient, BatchMessage
from client_utils.packet_builder import CTRexPktBuilder
import json
from common.trex_stats import *
from common.trex_streams import *
from collections import namedtuple

from trex_async_client import CTRexAsyncClient

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# simple class to represent complex return value
class RC:

    def __init__ (self, rc, data):
        self.rc = rc
        self.data = data

    def good (self):
        return self.rc

    def bad (self):
        return not self.rc

    def data (self):
        if self.good():
            return self.data
        else:
            return ""

    def err (self):
        if self.bad():
            return self.data
        else:
            return ""

RC_OK = RC(True, "")
def RC_ERR (err):
    return RC(False, err)

class RC_LIST:
    def __init__ (self):
        self.rc_list = []

    def add (self, rc):
        self.rc_list.append(rc)

    def good(self):
        return all([x.good() for x in self.rc_list])

    def bad (self):
        not self.good()

    def data (self):
        return [x.data() for x in self.rc_list]

    def err (self):
        return [x.err() for x in self.rc_list]


# describes a single port
class Port:

    STATE_DOWN       = 0
    STATE_IDLE       = 1
    STATE_STREAMS    = 2
    STATE_TX         = 3
    STATE_PAUSE      = 4

    def __init__ (self, port_id, user, transmit):
        self.port_id = port_id
        self.state = self.STATE_IDLE
        self.handler = None
        self.transmit = transmit
        self.user = user

        self.streams = {}

    def err (self, msg):
        return RC_ERR("port {0} : {1}".format(self.port_id, msg))

    # take the port
    def acquire (self, force = False):
        params = {"port_id": self.port_id,
                  "user":    self.user,
                  "force":   force}

        command = RpcCmdData("acquire", params)
        rc = self.transmit(command.method, command.params)
        if rc.success:
            self.handler = rc.data
            return RC_OK
        else:
            return RC_ERR(rc.data)


    # release the port
    def release (self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        command = RpcCmdData("release", params)
        rc = self.transmit(command.method, command.params)
        if rc.success:
            self.handler = rc.data
            return RC_OK
        else:
            return RC_ERR(rc.data)

    def is_acquired (self):
        return (self.handler != None)

    def is_active (self):
        return (self.state == self.STATE_TX ) or (self.state == self.STATE_PAUSE)

    def sync (self, sync_data):

        self.handler = sync_data['handler']

        if sync_data['state'] == "DOWN":
            self.state = self.STATE_DOWN
        elif sync_data['state'] == "IDLE":
            self.state = self.STATE_IDLE
        elif sync_data['state'] == "STREAMS":
            self.state = self.STATE_STREAMS
        elif sync_data['state'] == "TX":
            self.state = self.STATE_TX
        elif sync_data['state'] == "PAUSE":
            self.state = self.STATE_PAUSE
        else:
            raise Exception("port {0}: bad state received from server '{1}'".format(self.port_id, sync_data['state']))

        return RC_OK
        

    # return TRUE if write commands
    def is_port_writeable (self):
        # operations on port can be done on state idle or state sreams
        return ((self.state == STATE_IDLE) or (self.state == STATE_STREAMS))

    # add stream to the port
    def add_stream (self, stream_id, stream_obj):

        if not self.is_port_writeable():
            return self.err("Please stop port before attempting to add streams")


        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "stream_id": stream_id,
                  "stream": stream_obj.dump()}

        rc, data = self.transmit("add_stream", params)
        if not rc:
            return self.err(data)

        # add the stream
        self.streams[stream_id] = stream_obj

        # the only valid state now
        self.state = self.STATE_STREAMS

        return RC_OK

    # remove stream from port
    def remove_stream (self, stream_id):

        if not stream_id in self.streams:
            return self.err("stream {0} does not exists".format(stream_id))

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "stream_id": stream_id}
                  

        rc, data = self.transmit("remove_stream", params)
        if not rc:
            return self.err(data)

        self.streams[stream_id] = None

        return RC_OK

    # remove all the streams
    def remove_all_streams (self):
        for stream_id in self.streams.keys():
            rc = self.remove_stream(stream_id)
            if rc.bad():
                return rc

        return RC_OK

    # start traffic
    def start (self, mul):
        if self.state == self.STATE_DOWN:
            return self.err("Unable to start traffic - port is down")

        if self.state == self.STATE_IDLE:
            return self.err("Unable to start traffic - no streams attached to port")

        if self.state == self.STATE_TX:
            return self.err("Unable to start traffic - port is already transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "mul": mul}

        rc, data = self.transmit("remove_stream", params)
        if not rc:
            return self.err(data)

        self.state = self.STATE_TX

        return RC_OK

    def stop (self):
        if (self.state != self.STATE_TX) and (self.state != self.STATE_PAUSE):
            return self.err("Unable to stop traffic - port is down")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("stop_traffic", params)
        if not rc:
            return self.err(data)

        # only valid state after stop
        self.state = self.STREAMS

        return RC_OK


class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""

    def __init__(self, username, server="localhost", sync_port = 5050, async_port = 4500, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.user = username
        self.comm_link = CTRexStatelessClient.CCommLink(server, sync_port, virtual)
        self.verbose = False
        self._conn_handler = {}
        self._active_ports = set()
        self._system_info = None
        self._server_version = None
        self.__err_log = None

        self._async_client = CTRexAsyncClient(async_port)

        self.connected = False

    ############# helper functions section ##############

    def __validate_port_list(self, port_id):
        if isinstance(port_id, list) or isinstance(port_id, set):
            # check each item of the sequence
            return all([self._is_ports_valid(port)
                        for port in port_id])
        elif (isinstance(port_id, int)) and (port_id >= 0) and (port_id <= self.get_port_count()):
            return True
        else:
            return False

    def __ports (self, port_id_list):
        if port_id_list == None:
            return range(0, self.get_port_count())

        for port_id in port_id_list:
            if not isinstance(port_id, int) or (port_id < 0) or (port_id > self.get_port_count()):
                raise ValueError("bad port id {0}".format(port_id))

        return [port_id_list] if isinstance(port_id_list, int) else port_id_list

    ############ boot up section ################

    # connection sequence
    def connect (self):

        self.connected = False

        # connect
        rc, data = self.comm_link.connect()
        if not rc:
            return RC_ERR(data)


        # cache system info
        rc, data = self.transmit("get_system_info")
        if not rc:
            return RC_ERR(data)

        self.system_info = data

        # cache supported cmds
        rc, data = self.transmit("get_supported_cmds")
        if not rc:
            return RC_ERR(data)

        self.supported_cmds = data

        # create ports
        self.ports = []
        for port_id in xrange(0, self.get_port_count()):
            self.ports.append(Port(port_id, self.user, self.transmit))

        # acquire all ports
        rc = self.acquire()
        if rc.bad():
            return rc

        rc = self.sync_with_server()
        if rc.bad():
            return rc

        self.connected = True

        return RC_OK

    def is_connected (self):
        return self.connected


    def disconnect(self):
        self.connected = False
        self.comm_link.disconnect()


    ########### cached queries (no server traffic) ###########

    def get_supported_cmds(self):
        return self.supported_cmds

    def get_version(self):
        return self.server_version

    def get_system_info(self):
        return self.system_info

    def get_port_count(self):
        return self.system_info.get("port_count")

    def get_port_ids(self, as_str=False):
        port_ids = range(self.get_port_count())
        if as_str:
            return " ".join(str(p) for p in port_ids)
        else:
            return port_ids

    def get_stats_async (self):
        return self._async_client.get_stats()

    def get_connection_port (self):
        return self.comm_link.port

    def get_acquired_ports(self):
        return [port for port in self.ports if port.is_acquired()]


    def get_active_ports(self):
        return [port for port in self.ports if port.is_active()]

    def set_verbose(self, mode):
        self.comm_link.set_verbose(mode)
        self.verbose = mode

    ############# server actions ################

    # ping server
    def ping(self):
        rc, info = self.transmit("ping")
        return RC(rc, info)


    def sync_with_server(self, sync_streams=False):
        rc, data = self.transmit("sync_user", {"user": self.user, "sync_streams": sync_streams})
        if not rc:
            return RC_ERR(data)

        for port_info in data:

            rc = self.ports[port_info['port_id']].sync(port_info)
            if rc.bad():
                return rc

        return RC_OK



    ########## port commands ##############
    # acquire ports, if port_list is none - get all
    def acquire (self, port_id_list = None, force = False):
        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].acquire(force)
            rc_list.add(rc)
        
        return rc_list
    
    # release ports
    def release (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc, msg = self.ports[port_id].release(force)
            rc_list.add(rc)
        
        return rc_list

 
    def add_stream(self, stream_id, stream_obj, port_id_list = None):
        assert isinstance(stream_obj, CStream)

        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].add_stream(stream_id, stream_obj)
            rc_list.add(rc)
        
        return rc_list

      
    def add_stream_pack(self, stream_pack_list, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for stream_pack in stream_pack_list:
            rc = self.add_stream(stream_pack.stream_id, stream_pack.stream, port_id_list)
            rc_list.add(rc)
        
        return rc_list



    def remove_stream(self, stream_id, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].remove_stream(stream_id)
            rc_list.add(rc)
        
        return rc_list



    def remove_all_streams(self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].remove_all_streams()
            rc_list.add(rc)
        
        return rc_list

    
    def get_stream(self, stream_id, port_id, get_pkt = False):

        return self.ports[port_id].get_stream(stream_id)


    def get_all_streams(self, port_id, get_pkt = False):

        return self.ports[port_id].get_all_streams()


    def get_stream_id_list(self, port_id):

        return self.ports[port_id].get_stream_id_list()


    def start_traffic (self, multiplier, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].start(multiplier)
            rc_list.add(rc)
        
        return rc_list



    def stop_traffic (self, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc_list = RC_LIST()

        for port_id in port_id_list:
            rc = self.ports[port_id].stop()
            rc_list.add(rc)
        
        return rc_list


    def get_port_stats(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("get_port_stats", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_get_port_stats_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = RpcCmdData("get_port_stats", params)
            return self._handle_get_port_stats_response(command, self.transmit(command.method, command.params))

    def get_stream_stats(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("get_stream_stats", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_get_stream_stats_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = RpcCmdData("get_stream_stats", params)
            return self._handle_get_stream_stats_response(command, self.transmit(command.method, command.params))



    def transmit(self, method_name, params={}):
        return self.comm_link.transmit(method_name, params)

    def transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)


    @staticmethod
    def default_success_test(result_obj):
        if result_obj.success:
            return True
        else:
            return False

    @staticmethod
    def ack_success_test(result_obj):
        if result_obj.success and result_obj.data == "ACK":
            return True
        else:
            return False


    ######################### Console (high level) API #########################

    # reset
    # acquire, stop, remove streams and clear stats
    #
    #
    def cmd_reset (self, annotate_func):


        # sync with the server
        rc = self.sync_with_server()
        annotate_func("Syncing with the server:", rc.good(), rc.err())
        if rc.bad():
            return rc

        # force acquire all ports
        rc = self.acquire(force = True)
        annotate_func("Force acquiring all ports:", rc.good(), rc.err())
        if rc.bad():
            return rc


        # force stop all ports
        port_id_list = self.get_active_ports()
        rc = self.stop_traffic(port_id_list)
        annotate_func("Stop traffic on all ports:", rc.good(), rc.err())
        if rc.bad():
            return rc

        return

        # remove all streams
        rc = self.remove_all_streams(ports)
        annotate_func("Removing all streams from all ports:", rc.good(), rc.err())
        if rc.bad():
            return rc

        # TODO: clear stats
        return RC_OK
        

    # stop cmd
    def cmd_stop (self, ports, annotate_func):

        # find the relveant ports
        active_ports = set(self.get_active_ports()).intersection(ports)
        if not active_ports:
            annotate_func("No active traffic on porvided ports")
            return True

        rc, log = self.stop_traffic(active_ports)
        annotate_func("Stopping traffic on ports {0}:".format([port for port in active_ports]), rc, log)
        if not rc:
            return False

        return True

    # start cmd
    def cmd_start (self, ports, stream_list, mult, force, annotate_func):

        if (force and set(self.get_active_ports()).intersection(ports)):
            rc = self.cmd_stop(ports, annotate_func)
            if not rc:
                return False

        rc, log = self.remove_all_streams(ports)
        annotate_func("Removing all streams from ports {0}:".format([port for port in ports]), rc, log,
                      "Please either retry with --force or stop traffic")
        if not rc:
            return False

        rc, log = self.add_stream_pack(stream_list.compiled, port_id= ports)
        annotate_func("Attaching streams to port {0}:".format([port for port in ports]), rc, log)
        if not rc:
            return False

        # finally, start the traffic
        rc, log = self.start_traffic(mult, ports)
        annotate_func("Starting traffic on ports {0}:".format([port for port in ports]), rc, log)
        if not rc:
            return False

        return True

    # ----- handler internal methods ----- #
    def _handle_general_response(self, request, response, msg, success_test=None):
        port_id = request.params.get("port_id")
        if not success_test:
            success_test = self.default_success_test
        if success_test(response):
            self._conn_handler[port_id] = response.data
            return RpcResponseStatus(True, port_id, msg)
        else:
            return RpcResponseStatus(False, port_id, response.data)


    def _handle_acquire_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        if success_test(response):
            self._conn_handler[port_id] = response.data
            return RpcResponseStatus(True, port_id, "Acquired")
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_add_stream_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        stream_id = request.params.get("stream_id")
        if success_test(response):
            return RpcResponseStatus(True, port_id, "Stream {0} added".format(stream_id))
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_remove_streams_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        if success_test(response):
            return RpcResponseStatus(True, port_id, "Removed")
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_release_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        if success_test(response):
            del self._conn_handler[port_id]
            return RpcResponseStatus(True, port_id, "Released")
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_start_traffic_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        if success_test(response):
            self._active_ports.add(port_id)
            return RpcResponseStatus(True, port_id, "Traffic started")
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_stop_traffic_response(self, request, response, success_test):
        port_id = request.params.get("port_id")
        if success_test(response):
            if port_id in self._active_ports:
                self._active_ports.remove(port_id)
            return RpcResponseStatus(True, port_id, "Traffic stopped")
        else:
            return RpcResponseStatus(False, port_id, response.data)

    def _handle_get_global_stats_response(self, request, response, success_test):
        if response.success:
            return CGlobalStats(**response.success)
        else:
            return False

    def _handle_get_port_stats_response(self, request, response, success_test):
        if response.success:
            return CPortStats(**response.success)
        else:
            return False

    def _handle_get_stream_stats_response(self, request, response, success_test):
        if response.success:
            return CStreamStats(**response.success)
        else:
            return False


    def _process_batch_result(self, req_list, resp_list, handler_func=None, success_test=default_success_test):
        res_ok = True
        responses = []
        if isinstance(success_test, staticmethod):
            success_test = success_test.__func__
        for i, response in enumerate(resp_list):
            # run handler method with its params
            processed_response = handler_func(req_list[i], response, success_test)
            responses.append(processed_response)
            if not processed_response.success:
                res_ok = False
            # else:
            #     res_ok = False # TODO: mark in this case somehow the bad result
        # print res_ok
        # print responses
        return res_ok, responses


    # ------ private classes ------ #
    class CCommLink(object):
        """describes the connectivity of the stateless client method"""
        def __init__(self, server="localhost", port=5050, virtual=False):
            super(CTRexStatelessClient.CCommLink, self).__init__()
            self.virtual = virtual
            self.server = server
            self.port = port
            self.verbose = False
            self.rpc_link = JsonRpcClient(self.server, self.port)

        @property
        def is_connected(self):
            if not self.virtual:
                return self.rpc_link.connected
            else:
                return True

        def set_verbose(self, mode):
            self.verbose = mode
            return self.rpc_link.set_verbose(mode)

        def connect(self):
            if not self.virtual:
                return self.rpc_link.connect()

        def disconnect(self):
            if not self.virtual:
                return self.rpc_link.disconnect()

        def transmit(self, method_name, params={}):
            if self.virtual:
                self._prompt_virtual_tx_msg()
                _, msg = self.rpc_link.create_jsonrpc_v2(method_name, params)
                print msg
                return
            else:
                return self.rpc_link.invoke_rpc_method(method_name, params)

        def transmit_batch(self, batch_list):
            if self.virtual:
                self._prompt_virtual_tx_msg()
                print [msg
                       for _, msg in [self.rpc_link.create_jsonrpc_v2(command.method, command.params)
                                      for command in batch_list]]
            else:
                batch = self.rpc_link.create_batch()
                for command in batch_list:
                    batch.add(command.method, command.params)
                # invoke the batch
                return batch.invoke()

        def _prompt_virtual_tx_msg(self):
            print "Transmitting virtually over tcp://{server}:{port}".format(server=self.server,
                                                                             port=self.port)


if __name__ == "__main__":
    pass
