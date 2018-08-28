#!/usr/bin/python

import os
import sys

from trex.console.plugins import *
from trex.stl.api import *
from trex.stl.wireless.trex_wireless_manager import WirelessManager, load_config
from trex.stl.wireless.utils.utils import round_robin_list



class Wireless_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'Wireless AP & Client simulation.'

    def plugin_load(self):
        self.manager = None
        self.ap_macs = []
        self.new_aps = [] # aps that have not been set to join yet
        self.client_macs = []
        self.new_clients = [] # clients that have not been set to authenticate yet

        self.add_argument("--cfg", action="store", nargs='?', default='trex_wireless_cfg.yaml',
                type=str, required=True, dest="cfg_file",
                help="set up the configuration from file, default: 'trex_wireless_cfg.yaml'")

        self.add_argument("--logging_level", default=3, type=int,
                dest = 'logging_level',
                help = 'Logging level, 0 = quiet, 1 = errors , 2 = warnings, 3 = info (default), 4 = debug')

        self.add_argument("-f", "--filename", action="store", nargs=1, type=str, required=True, dest="filename",
                help="file path to load")

        self.add_argument("-s", "--service", action="store", nargs=1, type=str, required=True, dest="service",
                help="module name, e.g. 'ClientServiceAssociation'")

        self.add_argument("-m", "--module", action="store", nargs=1, type=str, required=True, dest="module",
                help="module name, e.g. 'wireless.services.client.client_service_association'")

        self.add_argument("--no-wait", action="store_false", dest="wait",
                help="if set, does not wait until services finish")

        self.add_argument('-p', '--port', action='store', nargs=1,  type=int, required=True, dest='trex_port',
                help = 'port to apply the command on')

        self.add_argument('-c', '--count', action='store', nargs='?', default=1, type=int, dest = 'count',
                help = 'amount of actions to apply')


        self.add_argument('--client-macs', nargs = '+', action = 'merge', type = str,
                dest = 'client_macs',
                help = 'A list of client MACs')


        # base values
        self.add_argument('--ap-mac', type = check_mac_addr,
                dest = 'ap_mac',
                help = 'Base AP MAC')
        self.add_argument('--ap-ip', type = check_ipv4_addr,
                dest = 'ap_ip',
                help = 'Base AP IP')
        self.add_argument('--ap-udp', type = int,
                dest = 'ap_udp',
                help = 'Base AP UDP port')
        self.add_argument('--ap-radio', metavar = 'MAC', type = check_mac_addr,
                dest = 'ap_radio',
                help = 'Base AP Radio MAC')
        self.add_argument('--client-mac', metavar = 'MAC', type = check_mac_addr,
                dest = 'client_mac',
                help = 'Base client MAC')
        self.add_argument('--client-ip', metavar = 'IP', type = check_ipv4_addr,
                dest = 'client_ip',
                help = 'Base client IP')

        # traffic

        self.add_argument('-m', '--mult', default = '1', type = parsing_opts.match_multiplier_strict,
                dest = 'multiplier',
                help = parsing_opts.match_multiplier_help)
        self.add_argument('-t', metavar = 'T1=VAL,T2=VAL ...', action = 'merge', default = None, type = parsing_opts.decode_tunables,
                dest = 'tunables',
                help = 'Sets tunables for a profile. Example = -t fsize=100,pg_id=7')
        self.add_argument('--total', action = 'store_true',
                dest = 'total_mult',
                help = 'Traffic will be divided between all clients specified')


    def __require_manager(self):
        """Check if manager is initialized, if not raises an Exception."""
        if not self.manager:
            raise Exception("Wireless must be started")

    def do_show(self):
        """Prints information on simulated APs and Clients."""
        self.__require_manager()
        table = text_tables.Texttable(max_width = 200)
        table.set_cols_align(['c', 'c', 'c', 'c'])
        categories = ["AP count", "AP joined", "Client count", "Client joined"]
        ap_count = self.manager.get_ap_count()
        ap_joined = self.manager.get_ap_joined_count()
        client_count = self.manager.get_client_count()
        client_joined = self.manager.get_client_joined_count()

        table.add_rows([
            categories,
            [ap_count, ap_joined, client_count, client_joined]
            ])
        print(table.draw())
        
    def do_show_base(self):
        """Prints base values for creation of APs and Clients."""
        self.__require_manager()
        table = text_tables.Texttable(max_width = 200)
        table.set_cols_align(['l', 'l'])
        base_values = self.manager.get_base_values()
        for key, val in base_values.items():
            table.add_row([key, val])
        print(table.draw())
    
    def do_base(self, ap_mac, ap_ip, ap_udp, ap_radio, client_mac, client_ip):
        '''Set base values of MAC, IP etc. for created AP/Client.
        Will be increased for each new device.
        '''
        self.__require_manager()
        self.manager.set_base_values(ap_mac, ap_ip, ap_udp, ap_radio, client_mac, client_ip)
        self.do_show_base()

    def do_start(self, cfg_file, logging_level):
        """Starts wireless."""
        if self.manager:
            raise Exception("Wireless already started")
        # load configuration file
        self.config = load_config(cfg_file)
        # add config file's directory to path
        self.abs_cfg_dir_path = os.path.dirname(os.path.abspath(cfg_file))
        self.manager = WirelessManager(
            config_filename=cfg_file, log_level=logging_level, trex_client=self.trex_client)
        self.manager.start()

    def do_stop(self):
        """Closes all wireless ressources."""
        if self.manager:
            self.manager.stop()

    # def do_load_ap_service(self, module, service):
    #     """Load a plugin for AP."""
    #     self.__require_manager()
    #     self.manager.load_ap_service(module, service)

    # def do_load_client_service(self, module, service):
    #     """Load a plugin for Client."""
    #     self.__require_manager()
    #     self.manager.load_client_service(module, service)

    def do_create_ap(self, trex_port, count):
        """Create AP(s) on port."""
        self.__require_manager()
        wlc_assignment = round_robin_list(count, self.config.wlc_ips)
        gateway_assignment = round_robin_list(count, self.config.gateway_ips)

        aps_params = self.manager.create_aps_params(count)
        self.manager.create_aps(*aps_params, wlc_ips=wlc_assignment,
                        gateway_ips=gateway_assignment)
        self.ap_macs.extend(list(aps_params[0]))
        self.new_aps.extend(list(aps_params[0]))

    def do_join_aps(self, wait):
        """Join new Aps."""
        self.__require_manager()
        self.manager.join_aps(
            self.new_aps, max_concurrent=self.config.ap_concurrent, wait=wait)
        self.new_aps = []

    def do_stop_aps(self):
        """Stop all APs services, disjoin.
        Disjoin on controller may take some time.
        """
        self.__require_manager()
        self.manager.stop_aps(
            self.ap_macs, max_concurrent=self.config.ap_concurrent, wait=False)
        self.new_aps = list(self.ap_macs)
        self.new_clients = list(self.client_macs)

    def do_stop_clients(self):
        """Stop all Clients services, disassociate."""
        self.__require_manager()
        self.manager.stop_clients(
            self.client_macs, max_concurrent=self.config.ap_concurrent, wait=False)
        self.new_clients = list(self.client_macs)

    def do_create_client(self, count):
        """Create Client(s) on each AP."""
        self.__require_manager()

        clients_params = self.manager.create_clients_params(
            len(self.ap_macs) * count)
        # assign the aps on which the clients will join
        # ap_ids[i] is the id of the AP of the i'th client
        ap_ids = []
        for ap_name in self.manager.ap_info_by_name.keys():
            ap_ids.extend([ap_name] * count)

        self.manager.create_clients(*clients_params, ap_ids=ap_ids)
        self.client_macs.extend(clients_params[0])
        self.new_clients.extend(clients_params[0])

    def do_join_clients(self, wait):
        """Join new Clients."""
        if len(self.new_aps) > 0:
            raise Exception("cannot join clients when APs are not yet joined, run join_aps command")
        self.__require_manager()
        self.manager.join_clients(self.new_clients, max_concurrent=self.config.client_concurrent, wait=wait)
        self.new_clients = []

    def do_start_client_traffic(self, client_macs, filename, multiplier, tunables, total_mult):
        """Start traffic on behalf on client(s)."""
        self.__require_manager()
        filename = filename[0]

        if not client_macs:
            client_macs = self.client_macs

        clients = set([self.manager.client_info_by_id[id] for id in client_macs])
        if len(client_macs) != len(clients):
            raise Exception('Client IDs should be unique')
        if not clients:
            raise Exception('No clients to start traffic on behalf of them!')

        ports = set([client.ap_info.port_id for client in clients])

        # stop ports if needed
        active_ports = list(ports.intersection(self.manager.trex_client.get_active_ports()))
        if active_ports:
            self.trex_client.stop(active_ports)

        self.trex_client.acquire(ports, force=True)

        # remove all streams
        self.trex_client.remove_all_streams(ports)

        # pack the profile
        client_streams = {}
        try:
            tunables = tunables or {}
            for client in clients:
                profile = STLProfile.load(filename,
                                          direction = tunables.get('direction', client.ap_info.port_id % 2),
                                          port_id = client.ap_info.port_id,
                                          **tunables)


                client_streams[client.mac] = profile.get_streams()

            self.manager.add_streams(client_streams)

        except STLError as e:
            msg = bold("\nError loading profile '%s'" % filename)
            self.manager.logger.error(msg + '\n')
            self.manager.logger.error(e.brief() + "\n")

        # self.manager.trex_client.start(ports = ports, mult = multiplier, force = True, total = total_mult)
        self.manager.start_streams()

        return RC_OK()

    def plugin_unload(self):
        self.do_stop()


