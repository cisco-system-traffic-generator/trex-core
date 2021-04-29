from trex_stl_lib.api import *
import argparse


class STLS1(object):
    """
    Create flow stat latency stream of UDP packet.
    Can specify using tunables the packet length (fsize) and packet group id (pg_id)
    Since we can't have two latency streams with same pg_id, in order to be able to start this profile
    on more than one port, we add port_id to the pg_id
    Notice that for perfomance reasons, latency streams are not affected by -m flag, so
    you can only change the pps value by editing the code.
    """

    def __init__ (self):
        self.fsize = 64
        self.pg_id = 0
        self.multi_tag = False

    def _create_stream (self):
        size = self.fsize - 4; # HW will add 4 bytes ethernet FCS
        base_pkt = Ether() / IP(src = "16.0.0.1", dst = "16.0.0.2") / UDP(dport = 12, sport = 1025)
        pad = max(0, size - len(base_pkt)) * 'x'
        pkt = STLPktBuilder(pkt = base_pkt/pad)

        return [STLStream(packet = pkt,
                          mode = STLTXCont(pps=1000),
                          flow_stats = STLFlowLatencyStats(pg_id = self.pg_id, multi_tag = True))
               ]

    def get_streams (self, port_id, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--fsize',
                            type = int,
                            default = 64,
                            help="define the packet's length in the stream.")
        parser.add_argument('--pg_id',
                            type = int,
                            default = 7,
                            help="define packet group id (pg_id).")
        parser.add_argument('--multi_tag',
                            type = bool,
                            default = False,
                            help="define packet is multi tag .")
        args = parser.parse_args(tunables)

        self.fsize = args.fsize
        self.pg_id = args.pg_id + port_id
        self.multi_tag = args.multi_tag
        stobj = self._create_stream()
        print (stobj[0].get_flow_stats_type())
        print (stobj[0].__str__())
        return stobj


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



