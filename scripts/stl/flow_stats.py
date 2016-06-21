from trex_stl_lib.api import *

class STLS1(object):
    """
    Create flow stat stream of UDP packet.
    Can specify using tunables the packet length (fsize) and packet group id (pg_id)
    """
    def __init__ (self):
        self.fsize = 64
        self.pg_id = 0

    def _create_stream (self):
        size = self.fsize - 4; # HW will add 4 bytes ethernet CRC
        base_pkt = Ether() / IP(src = "16.0.0.1", dst = "48.0.0.1") / UDP(dport = 12, sport = 1025)
        pad = max(0, size - len(base_pkt)) * 'x'
        pkt = STLPktBuilder(pkt = base_pkt/pad)

        return [STLStream(packet = pkt,
                          mode = STLTXCont(pps=1),
                          flow_stats = STLFlowStats(pg_id = self.pg_id))
               ]

    def get_streams (self, fsize = 64, pg_id = 7, **kwargs):
        self.fsize = fsize
        self.pg_id = pg_id
        return self._create_stream()

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



