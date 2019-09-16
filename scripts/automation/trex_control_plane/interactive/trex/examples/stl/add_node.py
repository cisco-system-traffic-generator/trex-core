import stl_path
from trex.stl.api import *

c = STLClient(verbose_level = 'debug')
c.connect()

my_ports=[0,1]
c.acquire(ports = my_ports)

# move to service mode
c.set_service_mode (ports = my_ports, enabled = True)
c.set_port_attr(promiscuous=True)
cmds=NSCmds()
cmds.remove_all()
# start the batch
c.set_namespace_start( 1, cmds)
# wait for the results
res = c.wait_for_async_results(1);

cmds=NSCmds()
MAC="00:00:00:01:00:07"
cmds.add_node(MAC, bird=True)  # add namespace
cmds.set_ipv4(MAC, "1.1.2.3", "1.1.2.1") # configure ipv4 and default gateway
cmds.set_ipv6(MAC,True) # enable ipv6 (auto mode, get src addrees from the router


# start the batch
c.set_namespace_start( 1, cmds)
# wait for the results
res = c.wait_for_async_results(1);

c.disconnect()

# print the results
print(res);
