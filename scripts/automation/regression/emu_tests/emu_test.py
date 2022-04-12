
import time

from trex.emu.api import *
from trex.common.trex_exceptions import *

from .emu_general_test import CEmuGeneral_Test, CTRexScenario


class Emu_Test(CEmuGeneral_Test):
    """Tests for TRex-EMU"""
    def setUp(self):
        """ Set up the Emu tests. Create an EMU client and connect to it. Set port attributes and service mode as needed."""
        CEmuGeneral_Test.setUp(self)

        self.emu_trex.reset()
        self.emu_trex.set_port_attr(promiscuous = True, multicast = True)
        self.emu_client = EMUClient(username = 'TRexRegression',
                                    server = self.configuration.trex['trex_name'],
                                    verbose_level = "debug" if CTRexScenario.json_verbose else "none")
        self.emu_client.connect()
        self.emu_client.acquire(force = True)
        self.emu_trex.set_service_mode(enabled = True)

    def tearDown(self):
        """Disable service mode when finishing"""
        CEmuGeneral_Test.tearDown(self)
        self.emu_trex.set_service_mode(enabled = False)

    @staticmethod
    def convert_mac_unix_format(mac_address):
        if "." in mac_address:
            # cisco formatted, need to convert to unix
            # convert from 0000.0001.0002 to 00:00:00:01:00:02
            mac = mac_address.replace(".", "") # remove dots
            mac_pairs = [a + b for a, b in zip(mac[::2], mac[1::2])] # ["00", "00", "00", "01", "00", "02"]
            return ":".join(mac_pairs)
        else:
            # other formats are not necessary for now
            return mac_address

    def _create_basic_profile(self, port, num_clients, client_plugs, ns_plugs):
        """Creates a basic profile with one namespace and multiple clients.
        Namespace is simple and consists only of port 0.

        Args:
            port: (int): Port which we'll use as namespace key.
            num_clients: (int) : Number of clients to add to the profile.
            client_plugs: (dict): Plugins to activate for clients.
            ns_plugs: (dict): Plugins to activate for namespace.

        Returns:
            (List[EMUClientKey], EMUNamespaceKey, EMUProfile): Client Keys, Namespace key and Profile.
        """
        mac = Mac('00:00:00:70:00:03')
        ipv4 = Ipv4('1.1.1.3')
        ipv4_dg = Ipv4('1.1.1.1')
        emu_clients = []
        client_keys = []
        ns_key = EMUNamespaceKey(vport = port)
        for i in range(num_clients):
            emu_client = EMUClientObj(mac = mac[i].V(),
                                      ipv4 = ipv4[i].V(),
                                      ipv4_dg = ipv4_dg.V(),
                                      plugs = client_plugs)
            emu_clients.append(emu_client)
            c_key = EMUClientKey(ns_key, mac[i].V())
            client_keys.append(c_key)
        emu_ns = EMUNamespaceObj(ns_key  = ns_key,
                                 clients = emu_clients)
        profile = EMUProfile(ns = emu_ns, def_ns_plugs = ns_plugs)
        return client_keys, ns_key, profile

    def test_arp_1_client(self):
        """One client only, make sure that it resolved the default gateway and MAC and check the ARP counters"""

        port = 0
        print(" -- Creating profile with 1 client and ARP enabled...")
        _, ns_key, profile = self._create_basic_profile(port = port, num_clients = 1, client_plugs = {"arp": {"enable": True}}, ns_plugs = {"arp": {"enable": True}})
        print(" -- Clearing router ARP entry...")
        self.router.clear_ip_arp(["1.1.1.3"])
        res = self.router.get_arp_entry("1.1.1.3")
        assert not res, "Failed clearing router ARP entry"
        print(" -- Router ARP entry cleared successfully.")
        print(" -- Loading profile...")
        self.emu_client.load_profile(profile)
        time.sleep(1) #wait for resolve

        dual_if = self.router.if_mngr.get_dual_if_list()[port] # same port as we used to create the profile.
        resolved_mac = Emu_Test.convert_mac_unix_format(dual_if.client_if.get_src_mac_addr())

        print(" -- Verifying loaded profile...")
        expected = [
            {'clients': [
                {'dg_ipv6': '::',
                'dgw': {'resolve': True, 'rmac': resolved_mac},
                'dhcp_ipv6': '::',
                'ipv4': '1.1.1.3',
                'ipv4_dg': '1.1.1.1',
                'ipv4_force_dg': False,
                'ipv4_force_mac': '00:00:00:00:00:00',
                'ipv4_mtu': 1500,
                'ipv6': '::',
                'ipv6_dgw': None,
                'ipv6_force_dg': False,
                'ipv6_force_mac': '00:00:00:00:00:00',
                'ipv6_local': 'fe80::200:ff:fe70:3',
                'ipv6_router': None,
                'ipv6_slaac': '::',
                'mac': '00:00:00:70:00:03',
                'plug_names': 'arp'}],
            'tci': [0, 0],
            'tpid': [0, 0],
            'vport': 0}
        ]
        res = self.emu_client.get_all_ns_and_clients()
        assert expected == res, "Loaded profile differs from expected!"
        print(" -- Loaded profile verified correctly.")

        print(" -- Verifying ARP cache...")
        expected = [
            {'ipv4': [1, 1, 1, 1],
             'mac':  Mac(resolved_mac).V(),
             'refc': 1,
             'resolve': True,
             'state': 'Complete'}]
        res = self.emu_client.arp.show_cache(ns_key)
        assert expected == res, "ARP cache differs from expected!"
        print(" -- ARP cache verified correctly.")

        # ARP counters
        print(' -- Verifying ARP counters...')
        expected = {
        'arp': {
            'addIncomplete': 1,
            'addLearn': 0,
            'associateWithClient': 1,
            'disasociateWithClient': 0,
            'eventsChangeDgIPv4': 0,
            'eventsChangeSrc': 0,
            'moveComplete': 1,
            'moveIncompleteAfterRefresh': 0,
            'moveLearned': 0,
            'pktRxArpQuery': 0,
            'pktRxArpQueryNotForUs': 0,
            'pktRxArpReply': 1,
            'pktRxErrTooShort': 0,
            'pktRxErrWrongOp': 0,
            'pktTxArpQuery': 1,
            'pktTxGArp': 1,
            'pktTxReply': 0,
            'tblActive': 1,
            'tblAdd': 1,
            'tblRemove': 0,
            'timerEventComplete': 0,
            'timerEventIncomplete': 0,
            'timerEventLearn': 0,
            'timerEventRefresh': 0}
        }
        res = self.emu_client.arp.get_counters(ns_key, zero = True, verbose = False)
        if 'arp' in res:
            res['arp'].pop('pktRxErrNoBroadcast', None)
        assert expected == res, "ARP counters differ from expected!"
        print(" -- ARP counters verified correctly.")

        print(" -- Verifying router ARP table")
        res = self.router.get_arp_entry("1.1.1.3")
        ip = res.get("Address", "")
        mac = res.get("Hardware Addr", "")
        assert ip == "1.1.1.3", "Router ARP table verification failed."
        assert mac == "0000.0070.0003", "Router ARP table verification failed."
        print(" -- Router ARP table verified correctly.")

        self.emu_client.remove_profile()

    def test_arp_multiple_clients(self, num_clients = 250):
        """Multiple clients in the same subnet try to resolve the MAC address of the default gateway"""

        port = 0
        print(" -- Creating profile with {num_clients} clients and ARP enabled...".format(num_clients = num_clients))
        _, ns_key, profile = self._create_basic_profile(port = port, num_clients = num_clients, client_plugs = {"arp": {"enable": True}}, ns_plugs = {"arp": {"enable": True}})
        print(" -- Loading profile...")
        self.emu_client.load_profile(profile, max_rate = 100) # The router might get stressed from gARP if we don't police the rate.

        time.sleep(1) # Wait for resolve...

        dual_if = self.router.if_mngr.get_dual_if_list()[port] # same port as we used to create the profile.
        resolved_mac = Emu_Test.convert_mac_unix_format(dual_if.client_if.get_src_mac_addr())

        print(" -- Verifying ARP cache...")
        expected = [
            {'ipv4': [1, 1, 1, 1],
             'mac': Mac(resolved_mac).V(),
             'refc': num_clients,
             'resolve': True,
             'state': 'Complete'}]
        res = self.emu_client.arp.show_cache(ns_key)
        assert expected == res, "ARP cache differs from expected!"
        print(" -- ARP cache verified correctly.")

        # ARP counters
        print(' -- Verifying ARP counters...')
        res = self.emu_client.arp.get_counters(ns_key, zero = True, verbose = False)
        pktTxArpQuery = res["arp"]["pktTxArpQuery"]
        assert pktTxArpQuery == num_clients, "pktTxArpQuery expected = {}, received = {}".format(num_clients, pktTxArpQuery)
        pktTxGArp = res["arp"]["pktTxGArp"]
        assert pktTxGArp == num_clients, "pktTxGArp expected = {}, received = {}".format(num_clients, pktTxGArp)
        pktRxArpReply = res["arp"]["pktRxArpReply"]
        assert pktRxArpReply == num_clients, "pktRxArpReply expected = {}, received = {}".format(num_clients, pktRxArpReply)
        associateWithClient = res["arp"]["associateWithClient"]
        assert associateWithClient == num_clients, "associateWithClient expected = {}, received = {}".format(num_clients, associateWithClient)
        tblActive = res["arp"]["tblActive"]
        assert tblActive == 1, "tblActive expected = {}, received = {}".format(1, tblActive)
        print(" -- ARP counters verified correctly.")

        # We don't clear the ARP table before checking the ARP clients because it would take too long.
        # The number of clients being huge gives the indication that the entries are just added.
        print(" -- Verifying router ARP table")
        arp_table = self.router.get_arp_table()
        arp_table_ip_addresses = set([entry["Address"] for entry in arp_table])
        client_ip_addresses = set(["1.1.1.{}".format(x) for x in range(3, num_clients + 3)])
        assert client_ip_addresses.issubset(arp_table_ip_addresses), "Not all clients appear in the router's ARP table."
        print(" -- Router's ARP table verified correctly.")

        self.emu_client.remove_profile()

    def test_icmp(self):
        """ Ping the default gateway (router) and verify we get responses. That is only in case the default gateway MAC address is resolved.
            Ping from one client to another client and verify we get response."""

        def _verify_ping_stats(c_key):
            ping_stats, error = self.emu_client.icmp.get_ping_stats(c_key = c_key)
            if error:
                # We had an error
                raise TRexError("Failed getting ping stats.")
            icmp_ping_stats = ping_stats["icmp_ping_stats"]
            assert icmp_ping_stats["repliesInOrder"] == icmp_ping_stats["requestsSent"], "Number of replies in order differs from number of requests sent."

        port = 0
        print(" -- Creating profile with 1 client and ARP and ICMP enabled...")
        plugs = {"icmp": {}, "arp": {"enable": True}}
        client_keys, ns_key, profile = self._create_basic_profile(port = port, num_clients = 2, client_plugs = plugs, ns_plugs = plugs)

        print(" -- Loading profile...")
        self.emu_client.load_profile(profile)

        time.sleep(1) # Wait for resolve...

        print(" -- Verifying DG MAC address is resolved...")
        res = self.emu_client.get_info_clients(c_keys = client_keys)
        for client in res:
            dg = client["dgw"]
            assert dg.get("resolve", False) is True, "Default Gateway MAC is not resolved."
        print(" -- DG MAC address is resolved for all clients")

        print(" -- Starting pinging the default gateway ...")
        self.emu_client.icmp.start_ping(c_key = client_keys[0], amount = 5, pace = 5.0) # Send 5 packets at a rate of 5 PPS to Default gateway. Send from client 0.

        time.sleep(0.5) # Wait for the ping to finish. Careful, too much might expire.

        print(" -- Finished pinging, verifying results...")
        _verify_ping_stats(client_keys[0])
        print(" -- Successfully pinged and got replies from default gateway (router).")

        print(" -- Starting pinging in between clients...")
        self.emu_client.icmp.start_ping(c_key = client_keys[0], amount = 5, pace = 5.0, dst = Ipv4("1.1.1.4").V()) # Send 5 packets at a rate of 5 PPS to Default gateway. Send from client 0 to client 1.

        time.sleep(0.5) # Wait for the ping to finish.

        print(" -- Finished pinging, verifying results...")
        _verify_ping_stats(client_keys[0])
        print(" -- Succesfully pinged and got replies from other client.")

        print(" -- Starting ping from router to client...")
        ping_stats = self.router.ping(ip="1.1.1.4", timeout=1)
        router_tx = ping_stats["txPkts"]
        router_rx = ping_stats["rxPkts"]
        assert router_tx == router_rx, "Router echo requests {} differs from echo replies {}".format(router_tx, router_rx)
        print(" -- Router pinged successfully.")

        self.emu_client.remove_profile()




