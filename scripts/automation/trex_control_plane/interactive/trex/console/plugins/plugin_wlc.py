#!/usr/bin/python

from trex.console.plugins import *
from trex.common.trex_exceptions import TRexError
from trex.utils.common import natural_sorted_key
from trex.stl.trex_stl_packet_builder_scapy import ipv4_str_to_num, is_valid_ipv4_ret


class WLC_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'WLC testing related functionality'

    def plugin_load(self):
        try: # ensure capwap_internal is imported
            from scapy.contrib.capwap import CAPWAP_PKTS
        except:
            del sys.modules['scapy.contrib.capwap']
            raise
        if 'trex.stl.trex_stl_wlc' in sys.modules:
            del sys.modules['trex.stl.trex_stl_wlc']
        from trex.stl.trex_stl_wlc import AP_Manager
        self.ap_manager = AP_Manager(self.trex_client)
        self.add_argument('-p', '--ports', nargs = '+', action = 'merge', type = int, default = None,
                dest = 'port_list',
                help = 'A list of ports on which to apply the command. Default = all')
        self.add_argument('-v', default = 2, type = int,
                dest = 'verbose_level',
                help = 'Verbosity level, 0 = quiet, 1 = errors (default), 2 = warnings, 3 = info, 4 = debug')
        self.add_argument('-c', '--count', default = 1, type = int,
                dest = 'count',
                help = 'Amount of actions to apply')
        self.add_argument('--cert', type = is_valid_file,
                dest = 'ap_cert',
                help = 'Certificate filename used for DTLS')
        self.add_argument('--priv', type = is_valid_file,
                dest = 'ap_privkey',
                help = 'Private key filename used for DTLS')
        self.add_argument('--capriv', type = is_valid_file,
                dest = 'ap_ca_priv',
                help = 'Private key filename used to sign AP certificate')
        self.add_argument('-i', '--ids', nargs = '+', default = [], action = 'merge',
                dest = 'ap_ids',
                help = 'A list of AP ID(s) - Name or MAC or IP')
        self.add_argument('-i', '--ids', nargs = '+', action = 'merge', type = str,
                dest = 'client_ids',
                help = 'A list of client IDs - MAC or IP')
        self.add_argument('-i', '--ids', nargs = '+', action = 'merge', type = str,
                dest = 'device_ids',
                help = 'A list of AP and/or Client IDs on which to apply the command')
        self.add_argument('-f', '--file', required = True, type = is_valid_file,
                dest = 'file_path',
                help = 'File path to load')
        self.add_argument('-m', '--mult', default = '1', type = parsing_opts.match_multiplier_strict,
                dest = 'multiplier',
                help = parsing_opts.match_multiplier_help)
        self.add_argument('-t', metavar = 'T1=VAL,T2=VAL ...', action = 'merge', default = None, type = parsing_opts.decode_tunables,
                dest = 'tunables',
                help = 'Sets tunables for a profile. Example = -t fsize=100,pg_id=7')
        self.add_argument('--total', action = 'store_true',
                dest = 'total_mult',
                help = 'Traffic will be divided between all clients specified')
        self.add_argument('-m', '--mac', type = check_mac_addr,
                dest = 'ap_mac',
                help = 'Base AP MAC')
        self.add_argument('-i', '--ip', type = check_ipv4_addr,
                dest = 'ap_ip',
                help = 'Base AP IP')
        self.add_argument('--client-mac', metavar = 'MAC', type = check_mac_addr,
                dest = 'client_mac',
                help = 'Base client MAC')
        self.add_argument('--client-ip', metavar = 'IP', type = check_ipv4_addr,
                dest = 'client_ip',
                help = 'Base client IP')
        self.add_argument('--wlc-ip', metavar = 'IP', type = check_ipv4_addr,
                dest = 'wlc_ip',
                help = 'Wireless controller IP (use 255.255.255.255 for broadcast discovery)')
        self.add_argument('--save', action = 'store_true',
                dest = 'base_save',
                help = 'Save "next" AP and Client base values. Will be loaded at start of console.')
        self.add_argument('--load', action = 'store_true',
                dest = 'base_load',
                help = 'Load saved AP and Client base values.')
        self.add_argument('--lan', '--wired', type = int,
                dest = 'proxy_wired_port',
                help = 'Wired side of proxy (connected to WLC).')
        self.add_argument('--wlan', '--wireless', type = int,
                dest = 'proxy_wireless_port',
                help = 'Wireless side of proxy (connected to Stateful TRex).')
        self.add_argument('--dst-mac', metavar = 'MAC', type = check_mac_addr,
                dest = 'proxy_dest_mac',
                help = 'Destination MAC of packets (by default will take WLC MAC)')
        self.add_argument('--filter-wlc-packets', action = 'store_true',
                dest = 'proxy_filter_wlc_packets',
                help = 'Do not proxify packets initiated by WLC')
        self.add_argument('--disable', action = 'store_true',
                dest = 'proxy_disable',
                help = 'Disable the proxy on specific port.')
        self.add_argument('--clear', action = 'store_true',
                dest = 'proxy_clear',
                help = 'Remove all proxy configuration on all ports. Ignores errors.')


    def plugin_unload(self):
        try:
            self.do_close(None)
        except:
            import traceback
            traceback.print_exc()
            raise


    def do_close(self, port_list):
        '''Closes all wlc-related stuff'''
        self.ap_manager.close(port_list)


    def show_base(self):
        general_table = text_tables.Texttable(max_width = 200)
        general_table.set_cols_align(['l', 'l'])
        general_table.set_deco(15)
        aps = self.ap_manager.get_connected_aps()
        if aps:
            info_arr = [('IP', aps[0].ip_dst), ('Hostname', aps[0].wlc_name.decode('ascii')), ('Image ver', '.'.join(['%s' % c for c in aps[0].wlc_sw_ver]))]
            general_table.add_row([bold('WLC'), ' / '.join(['%s: %s' % (k, v or '?') for k, v in info_arr])])
        general_table.add_row([bold('Next AP:'), 'MAC: %s / IP: %s / WLC IP: %s' % self.ap_manager._gen_ap_params()])
        general_table.add_row([bold('Next Client:'), 'MAC: %s / IP: %s' % self.ap_manager._gen_client_params()])
        self.ap_manager.log(general_table.draw())

    def do_show(self):
        '''Show status of APs'''
        self.show_base()
        info = self.ap_manager.get_info()
        if not info:
            return
        ap_client_info_table = text_tables.Texttable(max_width = 200)
        ap_client_info_table.set_cols_align(['c', 'l', 'l'])
        ap_client_info_table.set_deco(15) # full
        categories = ['Port', 'AP(s) info', 'Client(s) info']
        ap_client_info_table.header([bold(c) for c in categories])
        for port_id in sorted(info.keys()):
            proxy_status = self.ap_manager.get_proxy_stats(ports = [port_id], decode_map = False)[port_id]
            port_info = '%s\nBG thread: %s' % (port_id, 'alive' if info[port_id]['bg_thread_alive'] else bold('dead'))
            if proxy_status and proxy_status['is_active']:
                port_info += '\nProxy port %s' % proxy_status['pair_port_id']
            ap_arr = []
            client_arr = []
            name_per_num = {}
            for ap_name in sorted(info[port_id]['aps'].keys(), key = natural_sorted_key):
                ap = info[port_id]['aps'][ap_name]
                ap_info = 'Name: %s' % ap_name
                ap_info += '\nIP: %s / MAC: %s' % (ap['ip'], ap['mac'])
                is_connected = ap['dtls_established'] and ap['is_connected']
                ap_info += '\nConnected: %s / SSID: %s' % ('Yes' if is_connected else bold('No'), ap['ssid'] or bold('-'))
                ap_lines = ap_info.count('\n') + 1
                ap_info += '\n' * max(0, len(ap['clients']) - ap_lines) # pad to be same size as clients
                ap_arr.append(ap_info)
                clients_arr = []
                for client in ap['clients']:
                    clients_arr.append('IP: %s / MAC: %s / Assoc: %s' % (client['ip'], client['mac'], 'Yes' if client['is_associated'] else bold('No')))
                if clients_arr:
                    client_info = '\n'.join(clients_arr)
                    client_info += '\n' * max(0, ap_lines - len(clients_arr)) # pad to be same size as ap
                else:
                    client_info = 'None'
                    client_info += '\n' * max(0, ap_lines - 1) # pad to be same size as ap
                client_arr.append(client_info)
            ap_client_info_table.add_row([
                    port_info,
                    ('\n' + ('- ' * 22) + '\n').join(ap_arr),
                    ('\n' + ('- ' * 25) + '\n').join(client_arr)])
        self.ap_manager.log(ap_client_info_table.draw())
        self.ap_manager.log('')


    def do_create_ap(self, port_list, count, verbose_level, ap_cert, ap_privkey, ap_ca_priv):
        '''Create AP(s) on port'''
        if count < 1:
            raise TRexError('Count should be greated than zero')
        if not port_list:
            raise TRexError('Please specify TRex ports where to add AP(s)')

        bu_mac, bu_ip, _ = self.ap_manager._gen_ap_params()
        init_ports = [port for port in port_list if port not in self.ap_manager.service_ctx]
        ap_names = []
        success = False
        try:
            self.ap_manager.init(init_ports) # implicitly for console
            for port in port_list:
                for _ in range(count):
                    ap_params = self.ap_manager._gen_ap_params()
                    self.ap_manager.create_ap(port, *ap_params, verbose_level = verbose_level, rsa_ca_priv_file = ap_ca_priv, rsa_priv_file = ap_privkey, rsa_cert_file = ap_cert)
                    ap_names.append(ap_params[0])
            assert ap_names
            self.ap_manager.join_aps(ap_names)
            success = True
        finally:
            if not success:
                for name in ap_names: # rollback
                    self.ap_manager.remove_ap(name)
                self.ap_manager.set_base_values(mac = bu_mac, ip = bu_ip)
                close_ports = [port for port in init_ports if port in self.ap_manager.service_ctx]
                if close_ports:
                    self.ap_manager.close(close_ports)


    def do_add_client(self, ap_ids, count):
        '''Add client(s) to AP(s)'''
        if count < 1 or count > 200:
            raise TRexError('Count of clients should be within range 1-200')
        ap_ids = ap_ids or self.ap_manager.aps

        bu_mac, bu_ip = self.ap_manager._gen_client_params()
        client_ips = []
        success = False
        try:
            for ap_id in ap_ids:
                for _ in range(count):
                    client_params = self.ap_manager._gen_client_params()
                    self.ap_manager.create_client(*client_params, ap_id = self.ap_manager._get_ap_by_id(ap_id))
                    client_ips.append(client_params[1])
            self.ap_manager.join_clients(client_ips)
            success = True
        finally:
            if not success:
                for ip in client_ips: # rollback
                    self.ap_manager.remove_client(ip)
                self.ap_manager.set_base_values(client_mac = bu_mac, client_ip = bu_ip)


    def do_reconnect(self, device_ids):
        '''Reconnect disconnected AP(s) or Client(s).'''
        device_ids = device_ids or ([a.name for a in self.ap_manager.aps] + [c.ip for c in self.ap_manager.clients])
        ports = set()
        aps = set()
        clients = set()
        err_ids = set()
        for device_id in device_ids:
            try:
                ap = self.ap_manager._get_ap_by_id(device_id)
                aps.add(ap)
                clients |= set(ap.clients)
                ports.add(ap.port_id)
            except:
                try:
                    client = self.ap_manager._get_client_by_id(device_id)
                    clients.add(client)
                    aps.add(client.ap)
                    ports.add(client.ap.port_id)
                except:
                    err_ids.add(device_id)
        if err_ids:
            raise TRexError('Invalid IDs: %s' % ', '.join(sorted(err_ids, key = natural_sorted_key)))
        if not self.ap_manager.bg_client.is_connected():
            self.ap_manager.bg_client.connect()
        for port_id in ports:
            if port_id in self.ap_manager.service_ctx:
                if not self.ap_manager.service_ctx[port_id]['bg'].is_running():
                    self.ap_manager.service_ctx[port_id]['bg'].run()
        non_init_ports = [p for p in ports if p not in self.ap_manager.service_ctx]
        not_joined_aps = [a for a in aps if not (a.is_connected and a.is_dtls_established)]
        not_assoc_clients = [c for c in clients if not (c.is_associated and c.seen_arp_reply)]
        if not (non_init_ports or not_joined_aps or not_assoc_clients):
            self.ap_manager.log(bold('Nothing to reconnect, everything works fine.'))
            return
        while non_init_ports:
            self.ap_manager.init(non_init_ports[:10])
            non_init_ports = non_init_ports[10:]
        while not_joined_aps:
            self.ap_manager.join_aps(not_joined_aps[:10])
            not_joined_aps = not_joined_aps[10:]
        while not_assoc_clients:
            self.ap_manager.join_clients(not_assoc_clients[:20])
            not_assoc_clients = not_assoc_clients[20:]


    def do_start(self, client_ids, file_path, multiplier, tunables, total_mult):
        '''Start traffic on behalf on client(s).'''
        if not client_ids:
            clients = self.ap_manager.clients
        else:
            clients = set([self.ap_manager._get_client_by_id(id) for id in client_ids])
            if len(client_ids) != len(clients):
                raise TRexError('Client IDs should be unique')
        if not clients:
            raise TRexError('No clients to start traffic on behalf of them!')
        ports = list(set([client.ap.port_id for client in clients]))

        # stop ports if needed
        active_ports = list_intersect(self.trex_client.get_active_ports(), ports)
        if active_ports:
            self.trex_client.stop(active_ports)

        # remove all streams
        self.trex_client.remove_all_streams(ports)

        # pack the profile
        try:
            tunables = tunables or {}
            for client in clients:
                profile = STLProfile.load(file_path,
                                          direction = tunables.get('direction', client.ap.port_id % 2),
                                          port_id = client.ap.port_id,
                                          **tunables)

                self.ap_manager.add_streams(client, profile.get_streams())

        except STLError as e:
            msg = bold("\nError loading profile '%s'" % file_path)
            self.ap_manager.log(msg + '\n')
            self.ap_manager.log(e.brief() + "\n")

        self.trex_client.start(ports = ports, mult = multiplier, force = True, total = total_mult)

        return RC_OK()

    def do_start_ap_traffic(self, ap_ids, file_path, multiplier, tunables, total_mult):
        '''Start traffic on behalf on AP(s) (e.g. IAPP traffic).'''
        if not ap_ids:
            aps = self.ap_manager.aps
        else:
            aps = set([self.ap_manager._get_ap_by_id(id) for id in ap_ids])
            if len(ap_ids) != len(aps):
                raise TRexError('AP IDs should be unique')
        if not aps:
            raise TRexError('No AP to start traffic on behalf of them!')
        ports = list(set([ap.port_id for ap in aps]))

        # stop ports if needed
        active_ports = list_intersect(self.trex_client.get_active_ports(), ports)
        if active_ports:
            self.trex_client.stop(active_ports)

        # remove all streams
        self.trex_client.remove_all_streams(ports)

        # pack the profile
        try:
            tunables = tunables or {}
            for ap in aps:
                profile = STLProfile.load(file_path,
                                          direction = tunables.get('direction', ap.port_id % 2),
                                          port_id = ap.port_id,
                                          **tunables)

                self.ap_manager.add_ap_streams(ap, profile.get_streams())

        except STLError as e:
            msg = bold("\nError loading profile '%s'" % file_path)
            self.ap_manager.log(msg + '\n')
            self.ap_manager.log(e.brief() + "\n")

        self.trex_client.start(ports = ports, mult = multiplier, force = True, total = total_mult)

        return RC_OK()


    def do_base(self, ap_mac, ap_ip, client_mac, client_ip, wlc_ip, base_save, base_load):
        '''Set base values of MAC, IP etc. for created AP/Client.\nWill be increased for each new device.'''
        self.ap_manager.set_base_values(ap_mac, ap_ip, client_mac, client_ip, wlc_ip, base_save, base_load)
        self.show_base()


    def do_proxy(self, proxy_wired_port, proxy_wireless_port, proxy_dest_mac, proxy_filter_wlc_packets, proxy_disable, proxy_clear):
        '''Proxify traffic between wireless side (Stateful TRex) and wired side (WLC).'''
        if proxy_clear:
            self.ap_manager.disable_proxy_mode(ignore_errors = True)
        elif any([proxy_wired_port, proxy_wireless_port]):
            if proxy_wired_port is None:
                raise TRexError('Must specify wired port')
            if proxy_wireless_port is None:
                raise TRexError('Must specify wireless port')
            if proxy_disable:
                self.ap_manager.disable_proxy_mode(ports = [proxy_wired_port, proxy_wireless_port])
            else:
                if proxy_wireless_port not in self.trex_client.ports:
                    raise TRexError('Invalid wireless port ID: %s' % proxy_wireless_port)
                port = self.trex_client.ports[proxy_wireless_port]
                if not port.is_service_mode_on():
                    port.set_service_mode(True)
                self.ap_manager.enable_proxy_mode(proxy_wired_port, proxy_wireless_port, proxy_dest_mac, proxy_filter_wlc_packets)
        else:
            counters_dict = {
                'BPF reject':                   'm_bpf_rejected',
                'IP convert ERR':               'm_ip_convert_err',
                'Map not found':                'm_map_not_found',
                'Not IP pkt':                   'm_not_ip',
                'Pkt too large':                'm_too_large_pkt',
                'Pkt too small':                'm_too_small_pkt',
                'Pkts from WLC':                'm_pkt_from_wlc',
                'TX ERR':                       'm_tx_err',
                'TX OK':                        'm_tx_ok',
                }

            proxy_table = text_tables.Texttable(max_width = 200)
            categories = ['Port', 'Counters', 'Clients']
            proxy_table.header([bold(c) for c in categories])
            proxy_table.set_cols_align(['l'] * len(categories))
            proxy_table.set_deco(15)
            for port_id in sorted(self.trex_client.get_acquired_ports()):
                data = self.ap_manager.get_proxy_stats(ports = [port_id], decode_map = False)[port_id]
                if not data or not data['is_active']:
                    continue
                row = ['ID: %s\n(%s)\nPair: %s' % (port_id, 'WLAN' if data['is_wireless_side'] else 'LAN', data['pair_port_id'])]

                counters_table = text_tables.Texttable()
                counters_table.set_deco(0)
                counters_table.set_cols_dtype(['t', 'i'])
                counters_table.set_cols_align(['l'] * 2)
                for k in sorted(counters_dict.keys()):
                    val =  data['counters'][counters_dict[k]]
                    if val:
                        counters_table.add_row(['%s:' % k, val])
                row.append(counters_table.draw())

                clients = ''
                clients_arr = sorted(data['capwap_map'].keys(), key = natural_sorted_key)
                if len(clients_arr) > 1:
                    first_ip_num = ipv4_str_to_num(is_valid_ipv4_ret(clients_arr[0]))
                    last_ip_num = ipv4_str_to_num(is_valid_ipv4_ret(clients_arr[-1]))
                    if first_ip_num == last_ip_num - len(clients_arr) + 1: # continuous range of IPs
                        clients = 'From %s to %s' % (clients_arr[0], clients_arr[-1])
                    else:
                        while True:
                            clients_row = ['%15s' % ip for ip in clients_arr[0:5]]
                            if not clients_row:
                                break
                            clients += ', '.join(clients_row) + '\n'
                            clients_arr = clients_arr[5:]
                else:
                    clients = clients_arr[0]
                row.append(clients)

                proxy_table.add_row(row)
            self.ap_manager.log(proxy_table.draw())




