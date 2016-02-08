from trex_stl_streams import *
from trex_stl_packet_builder_scapy import *

# map ports
# will destroy all streams/data on the ports
def stl_map_ports (client, ports = None):

    # by default use all ports
    if ports == None:
        ports = client.get_all_ports()

    # reset the ports
    client.reset(ports)

    # generate streams
    base_pkt = CScapyTRexPktBuilder(pkt = Ether()/IP())
    
    pkts = 1
    for port in ports:
        stream = STLStream(packet = base_pkt,
                           mode = STLTXSingleBurst(pps = 100000, total_pkts = pkts))

        client.add_streams(stream, [port])
        pkts = pkts * 2

    # inject
    client.clear_stats()
    client.start(ports, mult = "1mpps")
    client.wait_on_traffic(ports)
    
    stats = client.get_stats()

    # cleanup
    client.reset(ports = ports)

    table = {}
    for port in ports:
        table[port] = None

    for port in ports:
        ipackets = stats[port]["ipackets"]

        exp = 1
        while ipackets >= exp:
            if ((ipackets & exp) == (exp)):
                source = int(math.log(exp, 2))
                table[source] = port

            exp *= 2

    if not all(x != None for x in table.values()):
        raise STLError('unable to map ports')

    dir_a = set()
    dir_b = set()
    for src, dst in table.iteritems():
        # src is not in
        if src not in (dir_a, dir_b):
            if dst in dir_a:
                dir_b.add(src)
            else:
                dir_a.add(src)

    table['dir'] = [list(dir_a), list(dir_b)]

    return table

