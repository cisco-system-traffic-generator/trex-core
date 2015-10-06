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
from collections import namedtuple


class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""
    RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

    def __init__(self, server="localhost", port=5050, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.tx_link = CTRexStatelessClient.CTxLink(server, port, virtual)
        self._conn_handler = {}
        self._active_ports = set()
        self._port_stats = CTRexStatsManager()
        self._stream_stats = CTRexStatsManager()


    # ----- decorator methods ----- #
    def force_status(owned=True, active=False):
        def wrapper(func):
            def wrapper_f(self, *args, **kwargs):
                port_ids = kwargs.get("port_id")
                if isinstance(port_ids, int):
                    # make sure port_ids is a list
                    port_ids = [port_ids]
                bad_ids = set()
                for port_id in port_ids:
                    if not self._conn_handler.get(kwargs.get(port_id)):
                        bad_ids.add(port_ids)
                if bad_ids:
                    # Some port IDs are not according to desires status
                    own_str = "owned" if owned else "not-owned"
                    act_str = "active" if active else "non-active"
                    raise RuntimeError("The requested method ('{0}') cannot be invoked since port IDs {1} are not both" \
                                       "{2} and {3}".format(func.__name__,
                                                            bad_ids,
                                                            own_str,
                                                            act_str))
                else:
                    func(self, *args, **kwargs)
            return wrapper_f
        return wrapper

    # def owned(func):
    #     def wrapper(self, *args, **kwargs):
    #         if self._conn_handler.get(kwargs.get("port_id")):
    #             return func(self, *args, **kwargs)
    #         else:
    #             raise RuntimeError("The requested method ('{0}') cannot be invoked unless the desired port is owned".
    #                                format(func.__name__))
    #     return wrapper

    # ----- user-access methods ----- #
    def acquire(self, port_id, username, force=False):
        if isinstance(port_id, list) or isinstance(port_id, set):
            port_ids = set(port_id) # convert to set to avoid duplications
            commands = [self.RpcCmdData("acquire", {"port_id":p_id, "user":username, "force":force})
                        for p_id in port_ids]
            rc, resp_list = self.transmit_batch(commands)

        else:
            params = {"port_id": port_id,
                      "user": username,
                      "force": force}
            self._conn_handler[port_id] = self.transmit("acquire", params)
            return self._conn_handler[port_id]

    @force_status(owned=True)
    def release(self, port_id=None):
        self._conn_handler.pop(port_id)
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id}
        return self.transmit("release", params)

    @force_status(owned=True)
    def add_stream(self, stream_id, stream_obj, port_id=None):
        assert isinstance(stream_obj, CStream)
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id,
                  "stream": stream_obj.dump()}
        return self.transmit("add_stream", params)

    @force_status(owned=True)
    def remove_stream(self, stream_id, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id}
        return self.transmit("remove_stream", params)

    @force_status(owned=True,active=)
    def get_stream_id_list(self, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id}
        return self.transmit("get_stream_list", params)

    @owned
    def get_stream(self, stream_id, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id,
                  "stream_id": stream_id}
        return self.transmit("get_stream_list", params)

    @owned
    def start_traffic(self, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id}
        return self.transmit("start_traffic", params)

    @owned
    def stop_traffic(self, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),
                  "port_id": port_id}
        return self.transmit("stop_traffic", params)

    def get_global_stats(self):
        return self.transmit("get_global_stats")

    @owned
    def get_port_stats(self, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),   # TODO: verify if needed
                  "port_id": port_id}
        return self.transmit("get_port_stats", params)

    @owned
    def get_stream_stats(self, port_id=None):
        params = {"handler": self._conn_handler.get(port_id),   # TODO: verify if needed
                  "port_id": port_id}
        return self.transmit("get_stream_stats", params)

    # ----- internal methods ----- #
    def transmit(self, method_name, params={}):
        return self.tx_link.transmit(method_name, params)

    def transmit_batch(self, batch_list):
        return self.tx_link.transmit_batch(batch_list)

    @staticmethod
    def _object_decoder(obj_type, obj_data):
        if obj_type=="global":
            return CGlobalStats(**obj_data)
        elif obj_type=="port":
            return CPortStats(**obj_data)
        elif obj_type=="stream":
            return CStreamStats(**obj_data)
        else:
            # Do not serialize the data into class
            return obj_data





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
                id, msg = self.rpc_link.create_jsonrpc_v2(method_name, params)
                print msg
                return
            else:
                return self.rpc_link.invoke_rpc_method(method_name, params)

        def transmit_batch(self, batch_list):
            if self.virtual:
                self._prompt_virtual_tx_msg()
                print [msg
                       for id, msg in [self.rpc_link.create_jsonrpc_v2(command.method, command.params)
                                       for command in batch_list]]
            else:
                batch = self.rpc_link.create_batch()
                for command in batch_list:
                    batch.add(command.method, command.params)
                # invoke the batch
                return batch.invoke()


        def _prompt_virtual_tx_msg(self):
            print "Transmitting virtually over tcp://{server}:{port}".format(
                    server=self.server,
                    port=self.port)

class CStream(object):
    """docstring for CStream"""
    DEFAULTS = {"rx_stats": CRxStats,
                "mode": CTxMode,
                "isg": 5.0,
                "next_stream": -1,
                "self_start": True,
                "enabled": True}

    def __init__(self, **kwargs):
        super(CStream, self).__init__()
        for k, v in kwargs.items():
            setattr(self, k, v)
        # set default values to unset attributes, according to DEFAULTS dict
        set_keys = set(kwargs.keys())
        keys_to_set = [x for x in self.DEFAULTS if x not in set_keys]
        for key in keys_to_set:
            default = self.DEFAULTS.get(key)
            if type(default)==type:
                setattr(self, key, default())
            else:
                setattr(self, key, default)

    @property
    def packet(self):
        return self._packet

    @packet.setter
    def packet(self, packet_obj):
        assert isinstance(packet_obj, CTRexPktBuilder)
        self._packet = packet_obj

    @property
    def enabled(self):
        return self._enabled

    @enabled.setter
    def enabled(self, bool_value):
        self._enabled = bool(bool_value)

    @property
    def self_start(self):
        return self._self_start

    @self_start.setter
    def self_start(self, bool_value):
        self._self_start = bool(bool_value)

    @property
    def next_stream(self):
        return self._next_stream

    @next_stream.setter
    def next_stream(self, value):
        self._next_stream = int(value)

    def dump(self):
        pass
        return {"enabled":self.enabled,
                "self_start":self.self_start,
                "isg":self.isg,
                "next_stream":self.next_stream,
                "packet":self.packet.dump_pkt(),
                "mode":self.mode.dump(),
                "vm":self.packet.get_vm_data(),
                "rx_stats":self.rx_stats.dump()}

class CRxStats(object):

    def __init__(self, enabled=False, seq_enabled=False, latency_enabled=False):
        self._rx_dict = {"enabled" : enabled,
                         "seq_enabled" : seq_enabled,
                         "latency_enabled" : latency_enabled}

    @property
    def enabled(self):
        return self._rx_dict.get("enabled")

    @enabled.setter
    def enabled(self, bool_value):
        self._rx_dict['enabled'] = bool(bool_value)

    @property
    def seq_enabled(self):
        return self._rx_dict.get("seq_enabled")

    @seq_enabled.setter
    def seq_enabled(self, bool_value):
        self._rx_dict['seq_enabled'] = bool(bool_value)

    @property
    def latency_enabled(self):
        return self._rx_dict.get("latency_enabled")

    @latency_enabled.setter
    def latency_enabled(self, bool_value):
        self._rx_dict['latency_enabled'] = bool(bool_value)

    def dump(self):
        return {k:v
                for k,v in self._rx_dict.items()
                if v
                }

class CTxMode(object):
    """docstring for CTxMode"""
    def __init__(self, tx_mode, pps):
        super(CTxMode, self).__init__()
        if tx_mode not in ["continuous", "single_burst", "multi_burst"]:
            raise ValueError("Unknown TX mode ('{0}')has been initialized.".format(tx_mode))
        self._tx_mode = tx_mode
        self._fields = {'pps':float(pps)}
        if tx_mode == "single_burst":
            self._fields['total_pkts'] = 0
        elif tx_mode == "multi_burst":
            self._fields['pkts_per_burst'] = 0
            self._fields['ibg'] = 0.0
            self._fields['count'] = 0
        else:
            pass

    def set_tx_mode_attr(self, attr, val):
        if attr in self._fields:
            self._fields[attr] = type(self._fields.get(attr))(val)
        else:
            raise ValueError("The provided attribute ('{0}') is not a legal attribute in selected TX mode ('{1}')".
                format(attr, self._tx_mode))

    def dump(self):
        dump = {"type":self._tx_mode}
        dump.update({k:v
                     for k, v in self._fields.items()
                     })
        return dump


if __name__ == "__main__":
    pass
