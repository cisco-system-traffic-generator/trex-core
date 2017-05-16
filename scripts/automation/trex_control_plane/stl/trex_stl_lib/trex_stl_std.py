from .trex_stl_streams import *
from .trex_stl_packet_builder_scapy import *
from .utils.common import *

# map ports
# will destroy all streams/data on the ports
def stl_map_ports (client, ports = None):
    # by default use all ports
    if ports is None:
        ports = client.get_all_ports()

    client.acquire(ports, force = True, sync_streams = False)

    unresolved_ports = list_difference(ports, client.get_resolved_ports())
    if unresolved_ports:
        raise STLError("Port(s) {0} have unresolved destination addresses".format(unresolved_ports))

    stl_send_3_pkts(client, ports)

    PKTS_SENT = 5
    pgid_per_port = {}
    active_pgids_tmp = client.get_active_pgids()
    active_pgids = []
    for key in active_pgids_tmp.keys():
        active_pgids += active_pgids_tmp[key]
    base_pkt = Ether()/IP()/UDP()/('x' * 18)
    test_pgid = 10000000

    # add latency packet per checked port
    for port in ports:
        for i in range(3):
            while test_pgid in active_pgids:
                test_pgid += 1
            stream = STLStream(packet = STLPktBuilder(pkt = base_pkt),
                               flow_stats = STLFlowLatencyStats(pg_id = test_pgid),
                               mode = STLTXSingleBurst(pps = 1e4, total_pkts = PKTS_SENT))
            try:
                client.add_streams(stream, [port])
            except STLError:
                continue
            pgid_per_port[port] = test_pgid
            test_pgid += 1
            break

    if len(pgid_per_port) != len(ports):
        raise STLError('Could not add flow stats streams per port.')

    # inject
    client.clear_stats(ports, clear_global = False, clear_flow_stats = True, clear_latency_stats = False, clear_xstats = False)
    client.start(ports, mult = "5%")
    client.wait_on_traffic(ports)

    stats = client.get_pgid_stats(list(pgid_per_port.values()))['flow_stats']

    # cleanup
    client.reset(ports)

    table = {'map': {}, 'bi' : [], 'unknown': []}

    # actual mapping
    for tx_port in ports:
        table['map'][tx_port] = None
        for rx_port in ports:
            if stats[pgid_per_port[tx_port]]['rx_pkts'][rx_port] * 2 > PKTS_SENT:
                table['map'][tx_port] = rx_port

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

# reset ports and send 3 packets from each acquired port
def stl_send_3_pkts(client, ports = None):

    base_pkt = STLPktBuilder(pkt = Ether()/IP())
    stream = STLStream(packet = base_pkt,
                       mode = STLTXSingleBurst(pps = 100000, total_pkts = 3))

    client.reset(ports)
    client.add_streams(stream, ports)
    client.start(ports, mult = "5%")
    client.wait_on_traffic(ports)
    client.remove_all_streams(ports)

