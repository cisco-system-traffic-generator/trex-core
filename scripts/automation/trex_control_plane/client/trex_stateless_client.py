#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages
from client_utils.jsonrpc_client import JsonRpcClient
from client_utils.packet_builder import CTRexPktBuilder
import json


class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""
    def __init__(self, server="localhost", port=5050, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.tx_link = CTRexStatelessClient.CTxLink(server, port, virtual)

    def add_stream(self, handler, port_id, stream_id, stream_obj):
        assert isinstance(stream_obj, CTRexStatelessClient.CStream)
        params = {"handler":handler,
                  "port_id":port_id,
                  "stream_id":stream_id,
                  "stream":stream_obj.dump()}
        return self.transmit("add_stream", params)

    def transmit(self, method_name, params={}):
        return self.tx_link.transmit(method_name, params)


    # ------ private classes ------ #
    class CStream(object):
        """docstring for CStream"""
        def __init__(self):
            super(CStream, self).__init__()
            self.packet = CTRexPktBuilder()
            self.rx_stats = CRxStats()
            self.mode = CTxMode()
            self.isg
            self._next_stream = -1
            self._self_start
            self._enabled

        @property
        def enabled(self):
            return self._enabled

        @enabled.setter
        def enabled(self, bool_value):
            self._enabled = bool(bool_value)

        @property
        def self_start(self):
            return self._self_start

        @enabled.setter
        def self_start(self, bool_value):
            self._self_start = bool(bool_value)

        @property
        def next_stream(self):
            return self._next_stream

        @enabled.setter
        def next_stream(self, value):
            self._next_stream = int(bool_value)

        def dump(self):
            pass
            return {"enabled":self.enabled,
                    "self_start":self.self_start,
                    "isg":self.isg,
                    "next_stream":self.next_stream,
                    "packet":self.packet.dump_pkt(),
                    "mode":self.mode.dump(),
                    "vm":self.packet.dump_vm_instructions(),  # TODO - add this method to packet builder module
                    "rx_stats":self.rx_stats.dump()})

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
                    })

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
                print "Transmitting virtually over tcp://{server}:{port}".format(
                    server=self.server,
                    port=self.port)
                id, msg = self.rpc_link.create_jsonrpc_v2(method_name, params)
                print msg
                return
            else:
                return self.rpc_link.invoke_rpc_method(method_name, params)




if __name__ == "__main__":
    pass
