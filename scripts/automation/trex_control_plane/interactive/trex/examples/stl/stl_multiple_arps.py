'''
    The following examples shows how to ARP multiple
    IPs from a single port
    then construct a traffic based on the resolved IPs
'''
import stl_path

from trex.stl.api import *
from trex.common.services.trex_service_arp import ServiceARP


c = STLClient()

try:
    port = 0
    clients_cfg = [ {'ip': '1.1.1.1', 'vlan': 100},
                    {'ip': '1.1.1.2', 'vlan': 200},
                    {'ip': '1.1.1.3', 'vlan': 300},
                    {'ip': '1.1.1.4', 'vlan': 400},
                  ]
    
    # connect to a local server
    c.connect()
    
    # reset port 0
    c.reset(ports = port)
    
    # create a service context
    ctx = c.create_service_ctx(port = port)

    # generate ARP service per client
    clients = [ServiceARP(ctx = ctx, dst_ip = cfg['ip'], vlan = cfg['vlan']) for cfg in clients_cfg]
    
    try:
        # move to service mode
        c.set_service_mode(ports = port, enabled = True)
        
        # execute clients
        ctx.run(clients)
        
    finally:
        c.set_service_mode(ports = port, enabled = False)
        
    # for each sucessful client create a stream
    records = [client.get_record() for client in clients]
    streams = []
    
    for record in records:
        if record:
            # success - add stream
            scapy_pkt = Ether(dst=record.dst_mac)/IP(dst=record.dst_ip)
            pkt = STLPktBuilder(pkt = scapy_pkt)
            streams.append(STLStream(packet = pkt))
            print('created stream for {0}/{1}'.format(record.dst_ip, record.dst_mac))
            
        else:
            print('failed to ARP resolve {0}'.format(record.dst_ip))
            
    if not streams:
        exit(1)
        
    # start traffic
    print('starting traffic...')

    c.add_streams(ports = port, streams = streams)
    c.start(ports = port, duration = 1)
    c.wait_on_traffic()
    
    print('done...')

except STLError as e:
    print(e)
    exit(1)

finally:
    c.disconnect()

