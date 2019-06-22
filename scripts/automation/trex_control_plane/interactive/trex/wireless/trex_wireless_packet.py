from .logger import *
from .services.trex_stl_ap import *

class Packet:
    """A Packet is a wrapper on bytes that provides functionalities on network packets. (TODO)"""

    def __init__(self, raw_bytes):
        self.pkt_bytes = bytes(raw_bytes)

    def __getitem__(self, key):
        return self.pkt_bytes[key]

    def __len__(self):
        return len(self.pkt_bytes)

    def endswith(self, s):
        return self.pkt_bytes.endswith(s)

    def __copy__(self):
        raise NotImplementedError

    def __deepcopy__(self, memo):
        raise NotImplementedError

    def __getstate__(self):
        raise NotImplementedError
