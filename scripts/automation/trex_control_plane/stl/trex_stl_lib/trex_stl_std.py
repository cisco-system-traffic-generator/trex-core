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

    # send something initial to calm down switches with arps etc.
    stream = STLStream(packet = base_pkt,
                       mode = STLTXSingleBurst(pps = 100000, total_pkts = 1))
    client.add_streams(stream, ports)

    client.start(ports, mult = "50%")
    client.wait_on_traffic(ports)
    client.reset(ports)

    tx_pkts = {}
    pkts = 1
    for port in ports:
        tx_pkts[pkts] = port
        stream = STLStream(packet = base_pkt,
                           mode = STLTXSingleBurst(pps = 100000, total_pkts = pkts * 3))

        client.add_streams(stream, [port])
        
        pkts *= 2

    # inject
    client.clear_stats()
    client.start(ports, mult = "50%")
    client.wait_on_traffic(ports)
    
    stats = client.get_stats()

    # cleanup
    client.reset(ports = ports)

    table = {'map': {}, 'bi' : [], 'unknown': []}

    # actual mapping
    for port in ports:

        ipackets = int(round(stats[port]["ipackets"] / 3.0)) # majority out of 3 to clean random noises
        table['map'][port] = None

        for pkts in tx_pkts.keys():
            if ( (pkts & ipackets) == pkts ):
                tx_port = tx_pkts[pkts]
                table['map'][port] = tx_port

    unmapped = list(ports)
    while len(unmapped) > 0:
        port_a = unmapped.pop(0)
        port_b = table['map'][port_a]

        # if unknown - add to the unknown list
        if port_b == None:
            table['unknown'].append(port_a)
        # self-loop, due to bug?
        elif port_a == port_b:
            continue
        # bi-directional ports
        elif (table['map'][port_b] == port_a):
            unmapped.remove(port_b)
            table['bi'].append( (port_a, port_b) )

    return table

