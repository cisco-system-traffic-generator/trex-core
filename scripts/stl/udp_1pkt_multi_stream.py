from trex_stl_lib.api import *
import argparse


# split the range of IP to cores 
# add tunable by fsize to change the size of the frame 
# latency frame is always 64 
# trex>start -f stl/udp_1pkt_src_ip_split_latency.py -t fsize=64 -m 30% --port 0 --force
#
#

class STLS1(object):

    def __init__ (self):
        self.fsize  =64;


    def create_stream (self, dir,port_id):
        # Create base packet and pad it to size
        size = self.fsize - 4; # HW will add 4 bytes ethernet FCS

        if dir==0:
            src_ip="16.0.0.1"
            dst_ip="48.0.0.1"
        else:
            src_ip="48.0.0.1"
            dst_ip="16.0.0.1"

        base_pkt = Ether()/IP(src=src_ip,dst=dst_ip)/UDP(dport=12,sport=1025)

        pad = max(0, size - len(base_pkt)) * 'x'
                             
        vm = STLScVmRaw( [   STLVmFlowVar ( "ip_src",  min_value="10.0.0.1",
                                            max_value="10.0.0.255", size=4, step=1,op="inc"),
                             STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src" ), # write ip to packet IP.src
                             STLVmFixIpv4(offset = "IP")                                # fix checksum
                                  ]
                              ,cache_size =255 # the cache size
                              );

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)
        streams =[]
        for stream_id in range(self.streams):
           streams.append(
                    # latency stream   
                     STLStream(packet = STLPktBuilder(pkt = base_pkt/pad),
                            mode = STLTXCont(pps=1),
                            flow_stats = STLFlowStats(pg_id = 12+port_id+stream_id))
                     )
  
        return streams


    def get_streams (self, direction, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--fsize',
                            type=int,
                            default=64,
                            help="The packets length.")
        parser.add_argument('--streams',
                            type=int,
                            default=1,
                            help="The number of streams.")
        args = parser.parse_args(tunables)
        self.fsize  = args.fsize
        self.streams = args.streams
        return self.create_stream(direction, kwargs['port_id'])


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



