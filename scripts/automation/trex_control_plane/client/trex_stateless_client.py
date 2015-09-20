#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages
from client_utils.jsonrpc_client import JsonRpcClient



class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""
    def __init__(self, server="localhost", port=5050, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.tx_link = CTRexStatelessClient.CTxLink(server, port, virtual)


    def transmit(self, method_name, params = {}):
        return self.tx_link.transmit(method_name, params)



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

        def transmit(self, method_name, params = {}):
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
