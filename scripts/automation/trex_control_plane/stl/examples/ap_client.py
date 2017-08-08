from stl_path import *
from trex_stl_lib.api import *
from trex_stl_lib.trex_stl_wlc import AP_Manager
from pprint import pprint
import threading
import sys
import yaml

with open(sys.argv[1]) as f:
    config_yaml = yaml.load(f.read())

ap_count = 2
clients_per_ap = 1
ap_ports = list(range(ap_count))


c = STLClient(server = config_yaml['server'])
c.connect()
c.acquire(ports = ap_ports, force = True)
c.stop(ports = ap_ports)
c.remove_all_streams(ports = ap_ports)
#c.remove_all_captures()

m = AP_Manager(c)

try:
    m.init(ap_ports)

    for index in range(ap_count):
        index += 1
        m.create_ap(
            trex_port_id = index - 1,
            name = 'ap-yaro-%s' % index,
            #mac = '94:d4:69:12:34:%02x' % index,
            mac = '94:12:12:12:12:%02x' % index,
            ip = config_yaml['ap_ip'] % index,
            udp_port = 10000 + index,
            radio_mac = '94:12:12:12:%02x:00' % index,
            #rsa_priv_file = '/users/ybrustin/certs/priv_key',
            #rsa_cert_file = '/users/ybrustin/certs/cisco_signed_cert.cert',
            )

    m.join_aps()
    print 'Joined all APs'

    for client_index in range(ap_count * clients_per_ap):
        ap_index = client_index % ap_count
        client_index += 1
        m.create_client(
            mac = '94:13:13:13:13:%02x' % client_index,
            ip = config_yaml['client_ip'] % client_index,
            ap_id = m.aps[ap_index].ip_src,
            )

    m.join_clients()
    print 'Associated all clients, starting traffic'
    pprint(m.get_info())
    #time.sleep(1)

    m.add_profile(m.clients[0], '../../../../stl/imix.py')
    m.add_profile(m.clients[1], '../../../../stl/imix.py', direction = 1, port = 1)
    c.start(ports = ap_ports, mult = '0.5%', force = True)

    while m.get_connected_aps():
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            break

except KeyboardInterrupt:
    pass

finally:
    c.stop(ports = ap_ports)
    m.close()




