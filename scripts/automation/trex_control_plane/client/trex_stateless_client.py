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


class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""
    RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

    def __init__(self, username, server="localhost", port=5050, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.user = username
        self.tx_link = CTRexStatelessClient.CTxLink(server, port, virtual)
        self._conn_handler = {}
        self._active_ports = set()
        self._stats = CTRexStatsManager("port", "stream")
        self._system_info = None

    # ----- decorator methods ----- #
    def force_status(owned=True, active_and_owned=False):
        def wrapper(func):
            def wrapper_f(self, *args, **kwargs):
                port_ids = kwargs.get("port_id")
                if isinstance(port_ids, int):
                    # make sure port_ids is a list
                    port_ids = [port_ids]
                bad_ids = set()
                for port_id in port_ids:
                    port_owned = self._conn_handler.get(kwargs.get(port_id))
                    if owned and not port_owned:
                        bad_ids.add(port_ids)
                    elif active_and_owned:    # stronger condition than just owned, hence gets precedence
                        if port_owned and port_id in self._active_ports:
                            continue
                        else:
                            bad_ids.add(port_ids)
                    else:
                        continue
                if bad_ids:
                    # Some port IDs are not according to desires status
                    raise RuntimeError("The requested method ('{0}') cannot be invoked since port IDs {1} are not"
                                       "at allowed stated".format(func.__name__))
                else:
                    func(self, *args, **kwargs)
            return wrapper_f
        return wrapper

    @property
    def system_info(self):
        if not self._system_info:
            self._system_info = self.get_system_info()
        return self._system_info

    # ----- user-access methods ----- #
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

    def acquire(self, port_id, force=False):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [self.RpcCmdData("acquire", {"port_id": p_id, "user": self.user, "force": force})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_acquire_response)
        else:
            params = {"port_id": port_id,
                      "user": self.user,
                      "force": force}
            command = self.RpcCmdData("acquire", params)
            self._handle_acquire_response(command, self.transmit(command.method, command.params))
            return self._conn_handler.get(port_id)

    @force_status(owned=True)
    def release(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [self.RpcCmdData("release", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_release_response)
        else:
            self._conn_handler.pop(port_id)
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = self.RpcCmdData("release", params)
            self._handle_release_response(command, self.transmit(command.method, command.params))
            return

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
    def remove_stream(self, stream_id, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id}
        return self.transmit("remove_stream", params)

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
            commands = [self.RpcCmdData("start_traffic", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_start_traffic_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = self.RpcCmdData("start_traffic", params)
            self._handle_start_traffic_response(command, self.transmit(command.method, command.params))
            return

    @force_status(owned=False, active_and_owned=True)
    def stop_traffic(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [self.RpcCmdData("stop_traffic", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_stop_traffic_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = self.RpcCmdData("stop_traffic", params)
            self._handle_start_traffic_response(command, self.transmit(command.method, command.params))
            return

    def get_global_stats(self):
        command = self.RpcCmdData("get_global_stats", {})
        return self._handle_get_global_stats_response(command, self.transmit(command.method, command.params))
        # return self.transmit("get_global_stats")

    @force_status(owned=True, active_and_owned=True)
    def get_port_stats(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [self.RpcCmdData("get_port_stats", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_get_port_stats_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = self.RpcCmdData("get_port_stats", params)
            return self._handle_get_port_stats_response(command, self.transmit(command.method, command.params))

    @force_status(owned=True, active_and_owned=True)
    def get_stream_stats(self, port_id=None):
        if not self._is_ports_valid(port_id):
            raise ValueError("Provided illegal port id input")
        if isinstance(port_id, list) or isinstance(port_id, set):
            # handle as batch mode
            port_ids = set(port_id)  # convert to set to avoid duplications
            commands = [self.RpcCmdData("get_stream_stats", {"handler": self._conn_handler.get(p_id), "port_id": p_id})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)
            if rc:
                self._process_batch_result(commands, resp_list, self._handle_get_stream_stats_response)
        else:
            params = {"handler": self._conn_handler.get(port_id),
                      "port_id": port_id}
            command = self.RpcCmdData("get_stream_stats", params)
            return self._handle_get_stream_stats_response(command, self.transmit(command.method, command.params))

    # ----- internal methods ----- #
    def transmit(self, method_name, params={}):
        return self.tx_link.transmit(method_name, params)

    def transmit_batch(self, batch_list):
        return self.tx_link.transmit_batch(batch_list)

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

    # ----- handler internal methods ----- #
    def _handle_acquire_response(self, request, response):
        if response.success:
            self._conn_handler[request.get("port_id")] = response.data

    def _handle_release_response(self, request, response):
        if response.success:
            del self._conn_handler[request.get("port_id")]

    def _handle_start_traffic_response(self, request, response):
        if response.success:
            self._active_ports.add(request.get("port_id"))

    def _handle_stop_traffic_response(self, request, response):
        if response.success:
            self._active_ports.remove(request.get("port_id"))

    def _handle_get_global_stats_response(self, request, response):
        if response.success:
            return CGlobalStats(**response.success)
        else:
            return False

    def _handle_get_port_stats_response(self, request, response):
        if response.success:
            return CPortStats(**response.success)
        else:
            return False

    def _handle_get_stream_stats_response(self, request, response):
        if response.success:
            return CStreamStats(**response.success)
        else:
            return False

    def _is_ports_valid(self, port_id):
        if isinstance(port_id, list) or isinstance(port_id, set):
            # check each item of the sequence
            return all([self._is_ports_valid(port)
                        for port in port_id])
        elif (isinstance(port_id, int)) and (port_id > 0) and (port_id <= self.get_port_count()):
            return True
        else:
            return False

    def _process_batch_result(self, req_list, resp_list, handler_func=None, success_test=default_success_test):
        for i, response in enumerate(resp_list):
            # testing each result with success test so that a conclusion report could be deployed in future.
            if success_test(response):
                # run handler method with its params
                handler_func(req_list[i], response)
            else:
                continue  # TODO: mark in this case somehow the bad result


    # ------ private classes ------ #
    class CTxLink(object):
        """describes the connectivity of the stateless client method"""
        def __init__(self, server="localhost", port=5050, virtual=False):
            super(CTRexStatelessClient.CTxLink, self).__init__()
            self.virtual = virtual
            self.server = server
            self.port = port
            self.rpc_link = JsonRpcClient(self.server, self.port)
            if not self.virtual:
                self.rpc_link.connect()

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
