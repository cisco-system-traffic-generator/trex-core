# Example of Topology profile

from trex.astf.api import *
from trex.astf.tunnels_topo import TunnelsTopo



def get_topo():
    topo = TunnelsTopo()

    topo.add_tunnel_ctx(
        tunnel_type = 1,
        label=10,
        tc=6,
        s=1,
        ttl=135
    )

    return topo