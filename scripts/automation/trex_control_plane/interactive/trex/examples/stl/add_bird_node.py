import stl_path
from trex.stl.api import *
from trex.pybird.bird_cfg_creator import *
from trex.pybird.bird_zmq_client import *

"""
Topology::
                                          +------------------------------+
                                          |                              |
                                          |   TRex         bird namespace|
                                          |        (veth) +--------------+
+------------+               (PHY PORT 0) |       1.1.1.3 |              |
|            |    1.1.1.1       1.1.1.2   |      <--------+              |
|            | <------------><----------->+               |              |
|            |                            |               | Bird Process |
|    DUT     |                            |        (veth) |              |
|            |    1.1.2.1       1.1.2.1   |       1.1.2.3 |              |
|            | <------------><----------->+      <--------+              |
|            |               (PHY PORT 1) |               |              |
+------------+                            |               +--------------+
                                          |                              |
                                          +------------------------------+
"""

c = STLClient(verbose_level = 'debug')
my_ports = [0, 1]
c.connect()
c.acquire(ports = my_ports)
c.set_service_mode(ports = my_ports, enabled = True)

bird_cfg = BirdCFGCreator()
bgp_data1 = """
    local 1.1.1.3 as 65000;
    neighbor 1.1.1.1 as 65000;
    ipv4 {
            import all;
            export all;
    };
"""
bgp_data2 = """
    local 1.1.2.3 as 65000;
    neighbor 1.1.2.1 as 65000;
    ipv4 {
            import all;
            export all;
    };
"""

bird_cfg.add_protocol(protocol = "bgp", name = "my_bgp1", data = bgp_data1)
bird_cfg.add_protocol(protocol = "bgp", name = "my_bgp2", data = bgp_data2)
bird_cfg.add_route(dst_cidr = "42.42.42.42/32", next_hop = "1.1.1.3")
bird_cfg.add_route(dst_cidr = "42.42.42.43/32", next_hop = "1.1.2.3")
cfg = bird_cfg.merge_to_string()
c.set_bird_config(config = cfg)  # sending our new config

# create 2 veth's in bird namespace
c.set_bird_node(node_port      = 0,
                mac            = "00:00:00:01:00:07",
                ipv4           = "1.1.1.3",
                ipv6_enabled   = True,
                ipv4_subnet    = 24,
                ipv6_subnet    = 124)

c.set_bird_node(node_port      = 1,
                mac            = "00:00:00:01:00:08",
                ipv4           = "1.1.2.3",
                ipv6_enabled   = True,
                ipv4_subnet    = 24,
                ipv6_subnet    = 124)

c.wait_for_protocols(['my_bgp1, my_bgp2'])
c.disconnect()
