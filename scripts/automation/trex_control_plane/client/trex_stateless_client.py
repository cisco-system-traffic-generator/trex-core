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

    def add_stream(self):
        pass

    def transmit(self, method_name, params={}):
        return self.tx_link.transmit(method_name, params)


    # ------ private classes ------ #
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
            self._rx_dict['enabled'] = bool_value

        @property
        def seq_enabled(self):
            return self._rx_dict.get("seq_enabled")

        @seq_enabled.setter
        def seq_enabled(self, bool_value):
            self._rx_dict['seq_enabled'] = bool_value

        @property
        def latency_enabled(self):
            return self._rx_dict.get("latency_enabled")

        @latency_enabled.setter
        def latency_enabled(self, bool_value):
            self._rx_dict['latency_enabled'] = bool_value

        def dump(self):
            return json.dumps({i:self._rx_dict.get(i)
                               for i in self._rx_dict.keys()
                               if self._rx_dict.get(i)
                               })

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
