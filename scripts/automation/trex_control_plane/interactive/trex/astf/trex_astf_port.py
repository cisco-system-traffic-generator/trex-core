
from ..common.trex_port import Port

class ASTFPort(Port):

    def is_service_mode_on(self):
        return True

    def set_service_mode(self, *a, **k):
        return self.ok()

    def is_server(self):
        return bool(self.port_id % 2)

    def is_client(self):
        return not self.is_server()
