import os
import time
import logging
from threading import Event

from trex_path import *
from wireless.trex_wireless_manager import WirelessManager, load_config
from wireless.utils.utils import round_robin_list
import yaml

# load configuration file
config = load_config("trex_wireless_cfg.yaml")

# instanciate the manager with configuration
manager = WirelessManager(
    config_filename="trex_wireless_cfg.yaml", log_level=logging.INFO)

# loading service that will run on the clients in the future
# to be done before excuting the 'start' method of the manager

# Choose one of the two:

# 1. Comment out the following, if DHCP is needed
client_ip_service = "wireless.services.client.client_service_association_and_dhcp.ClientServiceAssociationAndDHCP"
use_client_dhcp = True

# 2. Use the following if static IP is needed
# client_ip_service = "wireless.services.client.client_service_association_and_arp.ClientServiceAssociationAndARP"
# use_client_dhcp = False

# initialize manager, starting subprocesses
manager.start()

try:
    # assign wlc by round robin, e.g. [wlc1, wlc2, wlc1, wlc2, ...] for 2 wlcs for num_aps times
    # wlc_assignment[i] should be the attached wlc of the i'th AP
    wlc_assignment = round_robin_list(config.num_aps, config.wlc_ips)
    # assign gateway by round robin for aps
    gateway_assignment = round_robin_list(config.num_aps, config.gateway_ips)

    print("Creating APs...")
    # generate macs, ips, udp_ports, radio_macs sequentially
    aps_params = manager.create_aps_params(config.num_aps)
    manager.create_aps(*aps_params, wlc_ips=wlc_assignment, gateway_ips=gateway_assignment, ap_modes=[config.ap_mode]*config.num_aps)
    ap_macs = list(aps_params[0])

    print("Creating Clients...")
    clients_params = manager.create_clients_params(
        config.num_aps * config.num_client_per_ap, dhcp=use_client_dhcp)

    manager.create_clients(*clients_params)

    if ap_macs:
        print("Joining APs...")
        manager.join_aps(
            ap_macs, max_concurrent=config.ap_concurrent, wait=True)
        print("Joined")

    if manager.clients:
        # This is used only for associated a client - it will be stuck in IP learn at scale so not very useful
        # print("Associating Clients...")
        # manager.join_clients(client_ids=[
        #                      c.mac for c in manager.clients], max_concurrent=config.client_concurrent, wait=True)
        # print("Associated")

        # This is for DHCP clients
        if use_client_dhcp:
            print("Starting clients with DHCP...")
            manager.add_clients_service(client_ids=[c.mac for c in manager.clients],
                                        service_class=client_ip_service, wait_for_completion=True, max_concurrent=config.client_concurrent)
            print("Clients association & DHCP done")
        else:
            # This is for static IP clients, it will ARP the gatway
            print("Starting association & ARP for clients...")
            manager.add_clients_service(client_ids=[c.mac for c in manager.clients],
                                        service_class=client_ip_service, wait_for_completion=True, max_concurrent=config.client_concurrent)
            print("Clients association & ARP done")

    # Keeps the APs and Clients online until Ctrl-C
    Event().wait()

except KeyboardInterrupt:
    pass
finally:
    print("Stopping...")
    manager.stop()
    print("Stopped.")
