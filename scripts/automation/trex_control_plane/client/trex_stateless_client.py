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

from trex_async_client import TrexAsyncClient

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# RpcResponseStatus = namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])

class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""

    def __init__(self, username, server="localhost", sync_port=5050, async_port = 4500, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.user = username
        self.comm_link = CTRexStatelessClient.CCommLink(server, sync_port, virtual)
        self.verbose = False
        self._conn_handler = {}
        self._active_ports = set()
        self._stats = CTRexStatsManager("port", "stream")
        self._system_info = None
        self._server_version = None
        self.__err_log = None

        self._async_client = TrexAsyncClient(async_port)


    # ----- decorator methods ----- #
    def force_status(owned=True, active_and_owned=False):
        def wrapper(func):
            def wrapper_f(self, *args, **kwargs):
                port_ids = kwargs.get("port_id")
                if not port_ids:
                    port_ids = args[0]
                if isinstance(port_ids, int):
                    # make sure port_ids is a list
                    port_ids = [port_ids]
                bad_ids = set()
                for port_id in port_ids:
                    port_owned = self._conn_handler.get(port_id)
                    if owned and not port_owned:
                        bad_ids.add(port_id)
                    elif active_and_owned:    # stronger condition than just owned, hence gets precedence
                        if port_owned and port_id in self._active_ports:
                            continue
                        else:
                            bad_ids.add(port_id)
                    else:
                        continue
                if bad_ids:
                    # Some port IDs are not according to desires status
                    raise ValueError("The requested method ('{0}') cannot be invoked since port IDs {1} are not "
                                     "at allowed states".format(func.__name__, list(bad_ids)))
                else:
                    return func(self, *args, **kwargs)
            return wrapper_f
        return wrapper

    @property
    def system_info(self):
        if not self._system_info:
            rc, info = self.get_system_info()
            if rc:
                self._system_info =  info
            else:
                self.__err_log = info
        return self._system_info if self._system_info else "Unknown"

    @property
    def server_version(self):
        if not self._server_version:
            rc, ver_info = self.get_version()
            if rc:
                self._server_version = ver_info
            else:
                self.__err_log = ver_info
        return self._server_version if self._server_version else "Unknown"

    def is_connected(self):
        return self.comm_link.is_connected

    # ----- user-access methods ----- #
    def connect(self):
        rc, err = self.comm_link.connect()
        if not rc:
            return rc, err
        return self._init_sync()

    def get_stats_async (self):
        return self._async_client.get_stats()

    def get_connection_port (self):
        return self.comm_link.port

    def disconnect(self):
        return self.comm_link.disconnect()

    def ping(self):
        return self.transmit("ping")

    def get_supported_cmds(self):
        return self.transmit("get_supported_cmds")

    def get_version(self):
        return self.transmit("get_version")

    def get_system_info(self):
        return self.transmit("get_system_info")

    def get_port_count(self):
        return self.system_info.get("port_count")

    def get_port_ids(self, as_str=False):
        port_ids = range(self.get_port_count())
        if as_str:
            return " ".join(str(p) for p in port_ids)
        else:
            return port_ids

    def get_acquired_ports(self):
        return self._conn_handler.keys()

    def get_active_ports(self):
        return list(self._active_ports)

    def set_verbose(self, mode):
        self.comm_link.set_verbose(mode)
        self.verbose = mode

    def acquire(self, port_id, force=False):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("acquire", {"port_id": p_id, "user": self.user, "force": force})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                return self._process_batch_result(commands, resp_list, self._handle_acquire_response)
        else:
            params = {"port_id": port_id,
                      "user": self.user,
                      "force": force}
            command = RpcCmdData("acquire", params)
            return self._handle_acquire_response(command,
                                                 self.transmit(command.method, command.params),
                                                 self.default_success_test)

    @force_status(owned=True)
    def release(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("release", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                return self._process_batch_result(commands, resp_list, self._handle_release_response,
                                                  success_test=self.ack_success_test)
        else:
            self._conn_handler.pop(port_id)
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = RpcCmdData("release", params)
            return self._handle_release_response(command,
                                          self.transmit(command.method, command.params),
                                          self.ack_success_test)

    @force_status(owned=True)
    def add_stream(self, stream_id, stream_obj, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        assert isinstance(stream_obj, CStream)
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id,
                  "stream": stream_obj.dump()}
        return self.transmit("add_stream", params)

    @force_status(owned=True)
    def add_stream_pack(self, port_id=None, *stream_packs):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")

        # since almost every run contains more than one transaction with server, handle all as batch mode
        port_ids = set(port_id)  # convert to set to avoid duplications
        commands = []
        for stream_pack in stream_packs:
            commands.extend([RpcCmdData("add_stream", {"port_id": p_id,
                                                       "handler": self._conn_handler.get(p_id),
                                                       "stream_id": stream_pack.stream_id,
                                                       "stream": stream_pack.stream}
                                        )
                            for p_id in port_ids]
                            )
        res_ok, resp_list = self.transmit_batch(commands)
        if res_ok:
            return self._process_batch_result(commands, resp_list, self._handle_add_stream_response,
                                              success_test=self.ack_success_test)

    @force_status(owned=True)
    def remove_stream(self, stream_id, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id}
        return self.transmit("remove_stream", params)

    @force_status(owned=True)
    def remove_all_streams(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("remove_all_streams", {"port_id": p_id, "handler": self._conn_handler.get(p_id)})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                return self._process_batch_result(commands, resp_list, self._handle_remove_streams_response,
                                                  success_test=self.ack_success_test)
        else:
            params = {"port_id": port_id,
                      "handler": self._conn_handler.get(port_id)}
            command = RpcCmdData("remove_all_streams", params)
            return self._handle_remove_streams_response(command,
                                                        self.transmit(command.method, command.params),
                                                        self.ack_success_test)
        pass

    @force_status(owned=True, active_and_owned=True)
    def get_stream_id_list(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id}
        return self.transmit("get_stream_list", params)

    @force_status(owned=True, active_and_owned=True)
    def get_stream(self, stream_id, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id}
        return self.transmit("get_stream_list", params)

    @force_status(owned=True)
    def start_traffic(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("start_traffic", {"handler": self._conn_handler.get(p_id), "port_id": p_id, "mul": 1.0})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                return self._process_batch_result(commands, resp_list, self._handle_start_traffic_response,
                                                  success_test=self.ack_success_test)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id,
                      "mul": 1.0}
            command = RpcCmdData("start_traffic", params)
            return self._handle_start_traffic_response(command,
                                                       self.transmit(command.method, command.params),
                                                       self.ack_success_test)

    @force_status(owned=False, active_and_owned=True)
    def stop_traffic(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [RpcCmdData("stop_traffic", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                return self._process_batch_result(commands, resp_list, self._handle_stop_traffic_response,
                                                  success_test=self.ack_success_test)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = RpcCmdData("stop_traffic", params)
            return self._handle_start_traffic_response(command,
                                                       self.transmit(command.method, command.params),
                                                       self.ack_success_test)

#    def get_global_stats(self):
#        command = RpcCmdData("get_global_stats", {})
#        return self._handle_get_global_stats_response(command, self.transmit(command.method, command.params))
#        # return self.transmit("get_global_stats")

    @force_status(owned=True, active_and_owned=True)
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

    @force_status(owned=True, active_and_owned=True)
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

    # ----- internal methods ----- #
    def _init_sync(self):
        # get server version and system info
        err = False
        if self.server_version == "Unknown" or self.system_info == "Unknown":
            self.disconnect()
            return False, self.__err_log
        return True, ""

    def transmit(self, method_name, params={}):
        return self.comm_link.transmit(method_name, params)

    def transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)

    @staticmethod
    def _object_decoder(obj_type, obj_data):
        if obj_type == "global":
            return CGlobalStats(**obj_data)
        elif obj_type == "port":
            return CPortStats(**obj_data)
        elif obj_type == "stream":
            return CStreamStats(**obj_data)
        else:
            # Do not serialize the data into class
            return obj_data

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

    def _is_ports_valid(self, port_id):
        if isinstance(port_id, list) or isinstance(port_id, set):
            # check each item of the sequence
            return all([self._is_ports_valid(port)
                        for port in port_id])
        elif (isinstance(port_id, int)) and (port_id >= 0) and (port_id <= self.get_port_count()):
            return True
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
