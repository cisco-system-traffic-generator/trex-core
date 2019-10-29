#!/usr/bin/python
import time
from trex.pybird.bird_cfg_creator import *
from trex.pybird.bird_zmq_client import *
from .bird_general_test import CBirdGeneral_Test, CTRexScenario


# global configurations
RIP_ROUTES_NUM   = 10
BGP_SMALL_ROUTES = 10
BGP_MANY_ROUTES  = 1e6
OSPF_SMALL_ROUTES = 10
POLL_TIME        = 2
SMALL_HANG_TIME  = 10
MANY_HANG_TIME   = 30  # time in sec

class STLBird_Test(CBirdGeneral_Test):
    """Tests for Bird Routing Daemon """
    
    def setUp(self):
        CBirdGeneral_Test.setUp(self)

        self.bird_trex.reset()
        self.bird_trex.set_service_mode(enabled = True)
        self.bird_trex.set_port_attr(promiscuous = True, multicast = True)
        self.pybird = PyBirdClient()
        self.pybird.connect()
        self.pybird.acquire(force = True)

    def tearDown(self):
        CBirdGeneral_Test.tearDown(self)
        CTRexScenario.router.clear_routes()
        self.pybird.release()
        self.pybird.disconnect()
        self.bird_trex.set_service_mode(enabled = False)

    def _conf_router_ospf(self):
        CTRexScenario.router.configure_ospf(ospf_num = 1)

    def _conf_router_bgp(self):
        CTRexScenario.router.configure_bgp()

    def _conf_router_rip(self):
        CTRexScenario.router.configure_rip()

    def _check_how_many_bgp(self):
        """ Check how many routes got to bgp table"""
        lines = CTRexScenario.router.get_bgp_routing_table()
        routes_num_line = [line for line in lines.splitlines() if 'network entries using' in line]
        if routes_num_line:
            return int(routes_num_line[0].split()[0])
        else:
            return 0

    def _check_how_many_rip(self):
        """ Check how many routes got to rip databse"""
        lines = CTRexScenario.router.get_rip_routes().splitlines()
        routes_line = [l for l in lines if l.strip().startswith("[1]")]
        return len(routes_line)

    def _get_routes_num_in_table(self, protocol):
        """ Check how many routes already in the router routing table """
        lines = CTRexScenario.router.get_routing_stats(protocol).splitlines()
        protocol_line = [l for l in lines if l.strip().startswith(protocol)]
        if protocol_line:
            return int(protocol_line[0].strip(protocol).split()[1])
        else:
            return 0

    def _clear_routes(self, protocol = None):
        CTRexScenario.router.clear_routes(protocol)

    ########
    # RIP #
    ########

    def test_bird_small_rip(self):
        self._conf_router_rip()

        c = self.bird_trex
        mac1 = "00:00:00:01:00:06"
        mac2 = "00:00:00:01:00:07"

        try:
            # add bird node
            c.set_bird_node(node_port = 0, mac = mac1, ipv4 = "1.1.1.3", ipv4_subnet = 24)
            
            # make conf file
            cfg_creator = BirdCFGCreator()
            cfg_creator.add_simple_rip()
            cfg_creator.add_many_routes("10.10.10.0", total_routes = RIP_ROUTES_NUM, next_hop = "1.1.1.3")

            # push conf file
            self.pybird.set_config(new_cfg = cfg_creator.build_config())

            # get current routing from DUT
            time.sleep(SMALL_HANG_TIME)
            rip_routes = self._check_how_many_rip()
            assert rip_routes == RIP_ROUTES_NUM, "%s routes did not reached router after %s sec" % (RIP_ROUTES_NUM, SMALL_HANG_TIME)

        finally:
            c.set_port_attr(promiscuous = False, multicast = False)
            c.namespace_remove_all()

    #######
    # BGP #
    #######
    def test_bird_small_bgp(self):
        self._conf_router_bgp()
        c = self.bird_trex
        mac1 = "00:00:00:01:00:06"
        mac2 = "00:00:00:01:00:07"

        try:
            # add bird node
            c.set_bird_node(node_port = 0, mac = mac1, ipv4 = "1.1.1.3", ipv4_subnet = 24)
            c.set_bird_node(node_port = 1, mac = mac2, ipv4 = "1.1.2.3", ipv4_subnet = 24)
            
            # make conf file
            cfg_creator = BirdCFGCreator()
            cfg_creator.add_simple_bgp()
            cfg_creator.add_many_routes("10.10.10.0", total_routes = BGP_SMALL_ROUTES, next_hop = "1.1.1.3")

            # push conf file
            self.pybird.set_config(new_cfg = cfg_creator.build_config())

            # get current routing from DUT
            time.sleep(SMALL_HANG_TIME)
            bgp_routes = self._check_how_many_bgp()
            assert bgp_routes == BGP_SMALL_ROUTES, "%s routes did not reached router after %s sec" % (BGP_SMALL_ROUTES, SMALL_HANG_TIME)

        finally:
            c.set_port_attr(promiscuous = False, multicast = False)
            c.namespace_remove_all()

    def test_bird_many_bgp(self):
        self._conf_router_bgp()
        c = self.bird_trex
        mac1 = "00:00:00:01:00:06"
        mac2 = "00:00:00:01:00:07"

        try:
            # add bird node
            c.set_bird_node(node_port = 0, mac = mac1, ipv4 = "1.1.1.3", ipv4_subnet = 24)
            c.set_bird_node(node_port = 1, mac = mac2, ipv4 = "1.1.2.3", ipv4_subnet = 24)

            # take 1M conf file
            cfg_creator = BirdCFGCreator()
            cfg_creator.add_simple_bgp()
            cfg_creator.add_many_routes("10.10.10.0", total_routes = BGP_MANY_ROUTES, next_hop = "1.1.1.3")

            # push conf file
            self.pybird.set_config(cfg_creator.build_config())

            # check 1M routes reached DUT
            for _ in range(MANY_HANG_TIME // POLL_TIME):
                time.sleep(POLL_TIME)
                routes = self._check_how_many_bgp()
                print("Dut got %s routes" % routes)
                if routes == BGP_MANY_ROUTES:
                    return
            assert routes == BGP_MANY_ROUTES, "Not all %s routes got to dut after %s min" % (BGP_MANY_ROUTES, MANY_HANG_TIME)

        finally:
            c.set_port_attr(promiscuous = False, multicast = False)
            c.namespace_remove_all()

    def test_bird_bad_filter(self):
        self._conf_router_bgp()
        c = self.bird_trex
        mac1 = "00:00:00:01:00:06"
        mac2 = "00:00:00:01:00:07"

        try:
            # add bird 2 nodes
            c.set_bird_node(node_port = 0, mac = mac1, ipv4 = "1.1.1.3", ipv4_subnet = 24)
            c.set_bird_node(node_port = 1, mac = mac2, ipv4 = "1.1.2.3", ipv4_subnet = 24)

            # make conf file with 10 routes
            cfg_creator = BirdCFGCreator()
            cfg_creator.add_simple_bgp()
            cfg_creator.add_many_routes("10.10.10.0", total_routes = BGP_SMALL_ROUTES, next_hop = "1.1.1.3")

            # push conf file
            self.pybird.set_config(new_cfg = cfg_creator.build_config())

            # check all routes got to router
            for _ in range(SMALL_HANG_TIME // POLL_TIME):
                time.sleep(POLL_TIME)
                routes = self._check_how_many_bgp()
                print("Dut got %s routes" % routes)
                if routes == BGP_SMALL_ROUTES:
                    break
            else:
                raise Exception("Not all routes got to dut after %s seconds" % SMALL_HANG_TIME)
            
            print('Got all the routes, now setting a bad filter for bird')
            c.set_namespace(0, method = 'set_filter', mac = mac1, bpf_filter = 'not udp and not tcp')
            c.set_namespace(1, method = 'set_filter', mac = mac2, bpf_filter = 'not udp and not tcp') 
            self._clear_routes(protocol = "bgp")

            # check no router do not getting routes from bird
            time.sleep(SMALL_HANG_TIME)
            routes = self._check_how_many_bgp()
            print("Dut got %s routes" % routes)
            if routes != 0:
                raise Exception("Filter did not filtered bird packets")

        finally:
            c.set_port_attr(promiscuous = False, multicast = False)
            c.namespace_remove_all()

    ########
    # OSPF #
    ########

    def test_bird_small_ospf(self):
        self._conf_router_ospf()
        c = self.bird_trex
        mac1 = "00:00:00:01:00:06"
        mac2 = "00:00:00:01:00:07"

        try:
            # add bird node
            c.set_bird_node(node_port = 0, mac = mac1, ipv4 = "1.1.1.3", ipv4_subnet = 24)
            c.set_bird_node(node_port = 1, mac = mac2, ipv4 = "1.1.2.3", ipv4_subnet = 24)
            
            # make conf file
            cfg_creator = BirdCFGCreator()
            cfg_creator.add_simple_ospf()
            cfg_creator.add_many_routes("10.10.10.0", total_routes = OSPF_SMALL_ROUTES, next_hop = "1.1.1.3")

            # push conf file
            self.pybird.set_config(new_cfg = cfg_creator.build_config())

            # get current routing from DUT
            for _ in range(MANY_HANG_TIME // POLL_TIME):
                time.sleep(POLL_TIME)
                ospf_routes = self._get_routes_num_in_table("ospf 1")
                if ospf_routes == OSPF_SMALL_ROUTES:
                    return 
            assert ospf_routes == OSPF_SMALL_ROUTES, "%s routes did not reached router after %s sec" % (OSPF_SMALL_ROUTES, MANY_HANG_TIME)

        finally:
            c.set_port_attr(promiscuous = False, multicast = False)
            c.namespace_remove_all()
