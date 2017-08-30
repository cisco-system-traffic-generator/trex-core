from stl_path import *
from trex_stl_lib.api import *
from trex_stl_lib.trex_stl_wlc import AP_Manager
from pprint import pprint
import threading
import sys
import yaml

with open(sys.argv[1]) as f:
    config_yaml = yaml.load(f.read())

base_data = config_yaml['base']
ports_data = config_yaml['ports']
ap_ports = list(ports_data.keys())


c = STLClient(server = config_yaml['server'])
c.connect()
c.acquire(force = True)
c.stop()
c.remove_all_streams()
#c.remove_all_captures()

m = AP_Manager(c)

try:
    def establish_setup():
        m.set_base_values(
            name = base_data['ap_name'],
            mac = base_data['ap_mac'],
            ip = base_data['ap_ip'],
            udp = base_data['ap_udp'],
            radio = base_data['ap_radio'],
            client_mac = base_data['client_mac'],
            client_ip = base_data['client_ip'],
            )

        for port_id, port_data in ports_data.items():
            m.init(port_id)
            ap_params = m._gen_ap_params()
            m.create_ap(port_id, *ap_params)
            m.aps[-1].profile_dst_ip = port_data['dst_ip'] # dirty hack
            for _ in range(port_data['clients']):
                client_params = m._gen_client_params()
                m.create_client(*client_params, ap_id = ap_params[0])

        print('Joining APs')
        m.join_aps()
    
        print('Associating clients')
        m.join_clients()

    establish_setup()
    pprint(m.get_info())
    #time.sleep(1)

    for client in m.clients:
        m.add_profile(client, '../../../../stl/imix.py', direction = client.ap.port_id % 2, port = client.ap.port_id)
    print('Starting traffic')
    c.start(ports = ap_ports, mult = '0.5%', force = True)

    while m.get_connected_aps():
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            break

except KeyboardInterrupt:
    pass

finally:
    c.stop()
    m.close()
    time.sleep(0.1)
    c.disconnect()




