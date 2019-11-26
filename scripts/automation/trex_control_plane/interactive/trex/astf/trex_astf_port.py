
from .topo import ASTFTopology
from ..common.trex_port import Port

class ASTFPort(Port):
    def __init__(self, *a, **k):
        Port.__init__(self, *a, **k)
        self.topo = ASTFTopology()
        self.service_mode          = False
        self.service_mode_filtered = False
        self.service_mask          = 0

    def _check_astf_req(self, enabled, filtered, mask):
        assert enabled or filtered, "Cannot turn off service mode in ASTF!"
        assert mask & 0xFE, "Cannot turn off NO_TCP_UDP flag in ASTF!"

    def set_service_mode(self, enabled, filtered, mask):
        self.service_mode          = enabled
        self.service_mode_filtered = filtered
        self.service_mask          = mask
        return self.ok()

    def is_service_mode_on(self):
        return self.service_mode
    
    def is_service_filtered_mode_on(self):
        return self.service_mode_filtered


    def is_server(self):
        return bool(self.port_id % 2)

    def is_client(self):
        return not self.is_server()

    def _is_service_req(self):
        return False