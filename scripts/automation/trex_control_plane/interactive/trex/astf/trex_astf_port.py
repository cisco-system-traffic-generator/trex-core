
from .topo import ASTFTopology
from ..common.trex_port import Port

class ASTFPort(Port):
    def __init__(self, *a, **k):
        Port.__init__(self, *a, **k)
        self.topo = ASTFTopology()

    def is_service_mode_on(self):
        return True

    def set_service_mode(self, *a, **k):
        return self.ok()

    def is_server(self):
        return bool(self.port_id % 2)

    def is_client(self):
        return not self.is_server()
