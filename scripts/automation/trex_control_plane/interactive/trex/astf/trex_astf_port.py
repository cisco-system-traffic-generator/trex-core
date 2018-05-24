
from ..common.trex_port import Port, owned, writeable, up

class ASTFPort(Port):

    def __init__ (self, ctx, port_id, rpc, info):
        Port.__init__(self, ctx, port_id, rpc, info)

    def is_service_mode_on (self):
        return True
