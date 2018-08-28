import os
import time
import logging
from threading import Event

from trex_path import *
from wireless.trex_wireless_manager import WirelessManager, load_config, APMode
from wireless.utils.utils import round_robin_list
import yaml

# load configuration file
config = load_config("trex_wireless_cfg.yaml")

# number of devices to start
num_aps = config.num_aps
num_client_per_ap = config.num_client_per_ap
num_clients_total = num_aps * num_client_per_ap  # total number of clients

# assign wlc by round robin, e.g. [wlc1, wlc2, wlc1, wlc2, ...] for 2 wlcs for num_aps times
# wlc_assignment[i] should be the attached wlc of the i'th AP
wlc_assignment = round_robin_list(num_aps, config.wlc_ips)
# assign gateway by round robin for aps
gateway_assignment = round_robin_list(num_aps, config.gateway_ips)

# instanciate the manager with configuration
manager = WirelessManager(
    config_filename="trex_wireless_cfg.yaml", log_level=logging.DEBUG)

# loading service that will run on the clients in the future
# to be done before excuting the 'start' method of the manager 
client_dhcp_service = "wireless.services.client.client_service_dhcp.ClientServiceDHCP"

# initialize manager, starting subprocesses
manager.start()

try:
    print("Creating APs...")
    # generate macs, ips, udp_ports, radio_macs sequentially
    aps_params = manager.create_aps_params(num_aps)
    manager.create_aps(*aps_params, wlc_ips=wlc_assignment, gateway_ips=gateway_assignment, ap_modes=[config.ap_mode]*num_aps)
    ap_macs = list(aps_params[0])

    print("Creating Clients...")
    clients_params = manager.create_clients_params(
        num_clients_total, dhcp=True)
    # assign the aps on which the clients will join
    # ap_ids[i] is the id of the AP of the i'th client
    ap_ids = []
    for ap_name in manager.ap_info_by_name.keys():
        ap_ids.extend([ap_name] * num_client_per_ap)

    manager.create_clients(*clients_params, ap_ids=ap_ids)

    print("Joining APs...")
    start_ap_join = time.time()
    manager.join_aps(
        ap_macs, max_concurrent=config.ap_concurrent, wait=True)
    print("Joined")

    start_client_join = time.time()
    print("Start Association of Clients...")
    manager.join_clients(client_ids=[c.mac for c in manager.clients], max_concurrent=config.client_concurrent, wait=True)
    manager.add_clients_service(client_ids=[c.mac for c in manager.clients],
                                service_class=client_dhcp_service, wait_for_completion=True, max_concurrent=config.client_concurrent)
    print("All clients have joined in {}".format(
        time.time() - start_client_join))

    # Keeps the APs and Clients online until Ctrl-C
    Event().wait()

except KeyboardInterrupt:
    pass
finally:

    # Retrieve information from manager

    def save_csv_infos(infos, filename):
        """Save a list of information on a file 'filename' as csv"""
        import csv
        with open(filename, 'w', newline='') as f:
            wr = csv.writer(f)
            wr.writerows(infos)
        return

    def get_client_dhcp_infos(infos, start):
        """Return the list of client dhcp informations.
        Args:
            infos: retrieved infos from all clients, all services
            start: start time of the dhcp services
        """
        dhcp_infos = []
        for client_info in infos:
            if set(["stop_time", "total_retries"]) <= set(client_info.keys()):
                dhcp_infos.append([client_info['stop_time'] -
                                start, client_info['total_retries']])
        return dhcp_infos

    # retrieve ap join durations
    times = manager.get_ap_join_times()
    times = [[t - start_ap_join] for t in times]
    save_csv_infos(times, "ap_join_times.csv")

    # retrieve client association durations
    infos = manager.get_clients_services_info()
    rows = []
    for client_info in infos:
        if "ClientServiceAssociation" in client_info:
            service_info = client_info['ClientServiceAssociation']
            if set(["stop_time"]) <= set(service_info.keys()):
                rows.append([service_info['stop_time'] - start_client_join])
    save_csv_infos(rows, "client_join_times.csv")

    if manager.is_running:
        print("Stopping...")
        manager.stop()
        print("Stopped.")


