
from ..common.trex_port import Port, owned, writeable, up

class ASTFPort(Port):

    def __init__ (self, ctx, port_id, rpc, info):
        Port.__init__(self, ctx, port_id, rpc, info)

    def is_service_mode_on (self):
        return True

    def is_server(self): # TODO: this is wrong, query the server for real value
        return bool(port_id % 2)

    def is_client(self): # TODO: this is wrong, query the server for real value
        return not self.is_server()
