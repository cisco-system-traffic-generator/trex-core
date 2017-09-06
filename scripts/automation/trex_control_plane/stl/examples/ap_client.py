from stl_path import *
from trex_stl_lib.api import *
from trex_stl_lib.trex_stl_wlc import AP_Manager
from pprint import pprint
import sys
import yaml
import time


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

def get_pkts():
    return c.get_stats()[0]['opackets'] + c.get_stats()[0]['ipackets']

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
            for i in range(int(sys.argv[2])):

                ap_params = m._gen_ap_params()
                m.create_ap(port_id, *ap_params)
                for _ in range(int(sys.argv[3])):
                    client_params = m._gen_client_params()
                    m.create_client(*client_params, ap_id = ap_params[0])

        with Profiler_Context(20):
            print('Joining APs')
            m.join_aps()

            start_pkts = get_pkts()
            start_time = time.time()
            print('Associating clients')
            m.join_clients()
            print('Took: %s' % (time.time() - start_time))
            print('Pkts: %s' % (get_pkts() - start_pkts))

    establish_setup()


except KeyboardInterrupt:
    pass

finally:
    c.stop()
    m.close()
    time.sleep(0.1)
    c.disconnect()




