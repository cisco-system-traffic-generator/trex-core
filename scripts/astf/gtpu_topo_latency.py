# Example of Tunnel Topology profile

from trex.astf.api import *
from trex.astf.tunnels_topo import TunnelsTopo



def get_topo():
    topo = TunnelsTopo()

    topo.add_tunnel_ctx(
        src_start = '16.0.0.0',
        src_end  = '16.0.0.255',
        initial_teid = 0,
        teid_jump = 1,
        sport = 5000,
        version = 4,
        tunnel_type = 1,
        src_ip = '1.1.1.11',
        dst_ip = '12.2.2.2',
        activate = True
    )

    topo.add_tunnel_latency(
        client_port_id = 0,
        client_ip = "16.0.0.0",
        server_ip = "48.0.0.0"
    )
    return topo
