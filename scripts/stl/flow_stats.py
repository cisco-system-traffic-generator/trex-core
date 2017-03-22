from trex_stl_lib.api import *

class STLS1(object):
    """
    Create flow stat stream of UDP packet.
    Can specify using tunables following params:
      Packet length (fsize)
      Packet group id (pg_id)
      Packet type(pkt_type)
      Number of streams (num_streams)
    """
    def __init__ (self):
        self.fsize = 64
        self.pg_id = 0
        self.pkt_type = "ether"
        self.num_streams = 1

    def _create_stream (self):
        size = self.fsize - 4; # HW will add 4 bytes ethernet CRC

        pkt_types = {"ether" :
                      Ether() / IP(src = "16.0.0.1", dst = "48.0.0.1") / UDP(dport = 12, sport = 1025)
                      , "vlan":
                      Ether() / Dot1Q(vlan=11) / IP(src = "16.0.0.1", dst = "48.0.0.1") / UDP(dport = 12, sport = 1025)
                      , "qinq":
                      Ether(type=0x88A8) / Dot1Q(vlan=19) / Dot1Q(vlan=11) / IP(src = "16.0.0.1", dst = "48.0.0.1") / UDP(dport = 12, sport = 1025)
        }

        if self.pkt_type not in pkt_types.keys():
            print ("Wrong pkt_type given. Allowed values are " + format(pkt_types.keys()))
            return []

        pad = max(0, size - len(self.pkt_type)) * 'x'
        pkt = STLPktBuilder(pkt = pkt_types[self.pkt_type]/pad)

        streams = []
        for pg_id_add in range(0, self.num_streams):
            streams.append(STLStream(packet = pkt, mode = STLTXCont(pps=1), flow_stats = STLFlowStats(pg_id = self.pg_id + pg_id_add)))

        return streams

    def get_streams (self, fsize = 64, pg_id = 1, pkt_type = "ether", num_streams = 1, **kwargs):
        self.fsize = fsize
        self.pg_id = pg_id
        self.pkt_type = pkt_type
        self.num_streams = num_streams
        return self._create_stream()

# dynamic load - used for trex console or simulator
def register():
    return STLS1()



