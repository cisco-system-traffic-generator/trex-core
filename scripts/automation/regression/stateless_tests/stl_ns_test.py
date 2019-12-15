#!/usr/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
from trex.common.stats.trex_ns import CNsStats
from trex.common.services.trex_service_icmp import ServiceICMP 

import pprint

class STLNS_Test(CStlGeneral_Test):
    """Tests for NS function """

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        if self.is_vdev:
            self.skip("We don't know what to expect with vdev.")
        if not (self.is_linux_stack and self.is_loopback):
            self.skip("We need linux stack and loopback for this tests to work")
        print('')
        self.stl_trex.reset()
        self.stl_trex.set_service_mode()
        self.stl_trex.namespace_remove_all()    

    def tearDown(self):
        CStlGeneral_Test.tearDown(self)
        self.stl_trex.namespace_remove_all()    
        self.stl_trex.set_service_mode(enabled = False)

    def test_ns_add_remove(self):
        c= self.stl_trex

        port = CTRexScenario.ports_map['bi'][0][0]
        print('Using port %s' % port)

        c.namespace_remove_all()

        # clear counters
        cmds=NSCmds()
        cmds.clear_counters()
        c.set_namespace_start(port, cmds)
        c.wait_for_async_results(port);

        # add 
        cmds=NSCmds()
        MAC="00:01:02:03:04:05"
        cmds.add_node(MAC)
        cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2")
        cmds.set_ipv6(MAC,True)

        c.set_namespace_start(port, cmds)
        c.wait_for_async_results(port);

        # get nodes
        cmds=NSCmds()
        cmds.get_nodes()
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);
        macs=r[0]['result']['nodes']

        print('MACs of nodes: %s' % macs)
        if len(macs) != 1:
           self.fail(' must be exactly one MAC')
        if macs[0] != "00:01:02:03:04:05":
           self.fail(' macs should include 00:01:02:03:04:05')

        cmds=NSCmds()
        cmds.counters_get_meta()
        cmds.counters_get_values()

        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);
        ns_stat = CNsStats()
        ns_stat.set_meta_values(r[0]['result']['data'], r[1]['result'][''])
        cnt = ns_stat.get_values_stats()
        print('Counters:')
        pprint.pprint(cnt)

        for k, v in cnt.items():
            assert v < 1e6, 'Value is too big in counter %s=%s' % (k, v)
        assert cnt['tx_multicast_pkts']>0, 'multicast rx counter is zero'

        # remove Node
        cmds=NSCmds()
        cmds.remove_node(MAC)
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);

        cmds=NSCmds()
        cmds.get_nodes()
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);
        macs=r[0]['result']['nodes']
        print('MACs of nodes: %s' % macs)
        if len(macs) != 0:
            self.fail(' must be no MACs, we deleted node')

        # clear counters
        cmds=NSCmds()
        cmds.clear_counters()
        cmds.counters_get_meta()
        cmds.counters_get_values()
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);

        ns_stat = CNsStats()
        ns_stat.set_meta_values(r[1]['result']['data'], r[2]['result'][''])
        cnt = ns_stat.get_values_stats()
        print('Counters:')
        pprint.pprint(cnt)

        assert len(cnt)==0, 'Counters should be zero'

    def test_ping_to_ns(self):

        # this test works on specific setup with specific configuration 
        if not CTRexScenario.setup_name in ('trex17'):
            return
        c = self.stl_trex
        try:
            c.set_port_attr(promiscuous = True, multicast = True)
            cmds=NSCmds()
            MAC="00:01:02:03:04:05"
            cmds.add_node(MAC)
            cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2")
            cmds.set_ipv6(MAC,True)

            c.set_namespace_start(0, cmds)
            c.wait_for_async_results(0)
            c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.3')
            r=c.ping_ip(1,'1.1.1.3')

            assert len(r)==5, 'should be 5 responses '
            assert r[0].state == ServiceICMP.PINGRecord.SUCCESS 


        finally: 
            c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.1')
            c.set_port_attr(promiscuous = False, multicast = False)

    def test_ping_with_vlan(self):

        c = self.stl_trex
        try:
            c.set_port_attr(promiscuous = True, multicast = True)
            cmds=NSCmds()
            MAC = "00:01:02:03:04:05"
            cmds.add_node(MAC)
            cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2")
            cmds.set_ipv6(MAC,True)
            cmds.set_vlan(MAC, [21], [0x8100])

            mac2 = "00:01:02:03:04:06"
            cmds.add_node(mac2)
            cmds.set_ipv4(mac2,"1.1.1.4","1.1.1.2")
            cmds.set_ipv6(mac2,True)
            c.set_namespace_start(0, cmds)
            c.wait_for_async_results(0)

            c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.4')
            r = c.ping_ip(1,'1.1.1.4')
            
            assert len(r) == 5, 'should be 5 responses '
            assert r[0].state == ServiceICMP.PINGRecord.SUCCESS 

        finally: 
            c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.1')
            c.set_port_attr(promiscuous = False, multicast = False)

    def test_many_ns(self):

        def get_mac (prefix,index):
            mac="{}:{:02x}:{:02x}".format(prefix,(index>>8)&0xff,(index&0xff))
            return (mac)

        def get_ipv4 (prefix,index):
            ipv4="{}.{:d}.{:d}".format(prefix,(index>>8)&0xff,(index&0xff))
            return(ipv4)

        def build_network (size):
            cmds=NSCmds()
            MAC_PREFIX="00:01:02:03"
            IPV4_PREFIX="1.1"
            IPV4_DG ='1.1.1.2'
            for i in range(size):
                mac =  get_mac (MAC_PREFIX,i+257+1)
                ipv4  = get_ipv4 (IPV4_PREFIX,259+i)
                cmds.add_node(mac)
                cmds.set_ipv4(mac,ipv4,IPV4_DG)
                cmds.set_ipv6(mac,True)
            return (cmds)

        c = self.stl_trex

        try:
            c.namespace_remove_all()
            cmds = build_network (100)
            c.set_namespace_start(0, cmds)
            c.wait_for_async_results(0)

            cmds=NSCmds()
            cmds.get_nodes()
            c.set_namespace_start(0, cmds)
            r=c.wait_for_async_results(0);
            macs=r[0]['result']['nodes']

            print(macs)
            assert len(macs) == 100, 'number of namespace is not correct '

        finally: 
            c.namespace_remove_all()

    #####################
    # Shared ns Tests   #
    #####################

    def _create_shared_ns(self, port):
        r = self.stl_trex.set_namespace(port, method = "add_shared_ns")
        return str(r['result'])

    def test_shared_ns_add_remove(self):
        c = self.stl_trex

        port = CTRexScenario.ports_map['bi'][0][0]
        print('Using port %s' % port)

        c.namespace_remove_all()

        # clear counters
        cmds = NSCmds()
        cmds.clear_counters()
        c.set_namespace_start(port, cmds)
        c.wait_for_async_results(port)

        # add shared ns
        ns_name = self._create_shared_ns(port)

        # add veth to ns
        cmds = NSCmds()
        MAC = "00:01:02:03:04:05"

        cmds.add_node(MAC, shared_ns = ns_name)
        cmds.set_ipv4(MAC, ipv4 = "1.1.1.3", subnet = 24, shared_ns = True)
        cmds.set_ipv6(MAC, enable = True, shared_ns = True)
        cmds.set_vlan(MAC, vlans = [22], tpids = [0x8011])

        c.set_namespace_start(port, cmds)
        c.wait_for_async_results(port)

        # get nodes
        cmds = NSCmds()
        cmds.get_nodes()
        c.set_namespace_start(port, cmds)
        r = c.wait_for_async_results(port)
        macs = r[0]['result']['nodes']

        print('MACs of nodes: %s' % macs)
        if len(macs) != 1:
           self.fail(' must be exactly one MAC')
        if macs[0] != "00:01:02:03:04:05":
           self.fail(' macs should include 00:01:02:03:04:05')

        cmds = NSCmds()
        cmds.counters_get_meta()
        cmds.counters_get_values()

        c.set_namespace_start(port, cmds)
        r = c.wait_for_async_results(port)
        ns_stat = CNsStats()
        ns_stat.set_meta_values(r[0]['result']['data'], r[1]['result'][''])
        cnt = ns_stat.get_values_stats()
        print('Counters:')
        pprint.pprint(cnt)

        for k, v in cnt.items():
            assert v < 1e6, 'Value is too big in counter %s=%s' % (k, v)
        assert cnt['tx_multicast_pkts']>0, 'multicast rx counter is zero'

        # remove Node
        cmds = NSCmds()
        cmds.remove_node(MAC)
        c.set_namespace_start(port, cmds)
        r = c.wait_for_async_results(port)

        cmds = NSCmds()
        cmds.get_nodes()
        c.set_namespace_start(port, cmds)
        r = c.wait_for_async_results(port)
        macs = r[0]['result']['nodes']
        print('MACs of nodes: %s' % macs)
        if len(macs) != 0:
            self.fail(' must be no MACs, we deleted node')

        # clear counters
        cmds = NSCmds()
        cmds.clear_counters()
        cmds.counters_get_meta()
        cmds.counters_get_values()
        c.set_namespace_start(port, cmds)
        r = c.wait_for_async_results(port)

        ns_stat = CNsStats()
        ns_stat.set_meta_values(r[1]['result']['data'], r[2]['result'][''])
        cnt = ns_stat.get_values_stats()
        print('Counters:')
        pprint.pprint(cnt)

        assert len(cnt) == 0, 'Counters should be zero'

    def test_many_shared_ns(self):

        def get_mac (prefix, index):
            mac = "{}:{:02x}:{:02x}".format(prefix, (index>>8) & 0xff,(index & 0xff))
            return mac

        def get_ipv4 (prefix, index):
            ipv4 = "{}.{:d}.{:d}".format(prefix, (index >> 8) & 0xff,(index & 0xff))
            return ipv4 

        def build_network (size, ns_name):
            cmds = NSCmds()
            MAC_PREFIX = "00:01:02:03"
            IPV4_PREFIX = "1.1"
            IPV4_DG = '1.1.1.2'
            ipv4_subnet = 24
            for i in range(size):
                mac  =  get_mac(MAC_PREFIX,i+257+1)
                ipv4 = get_ipv4 (IPV4_PREFIX,259+i)
                cmds.add_node(mac, shared_ns = ns_name)
                cmds.set_ipv4(mac, ipv4 = ipv4, subnet = ipv4_subnet, shared_ns = True)
                cmds.set_ipv6(mac, enable = True, shared_ns = True)
            return cmds

        try:
            c = self.stl_trex
            c.namespace_remove_all()
            ns_name = self._create_shared_ns(port = 0)

            cmds = build_network (100, ns_name = ns_name)
            c.set_namespace_start(0, cmds)
            c.wait_for_async_results(0)

            cmds = NSCmds()
            cmds.get_nodes()
            c.set_namespace_start(0, cmds)
            r = c.wait_for_async_results(0)
            macs = r[0]['result']['nodes']

            print(macs)
            assert len(macs) == 100, 'number of namespace is not correct'

        finally: 
            c.namespace_remove_all()

    def test_ping_to_shared_ns(self):

        # this test works on specific setup with specific configuration 
        if not CTRexScenario.setup_name in ('trex17'):
            return
        c = self.stl_trex
        try:
           c.set_port_attr(promiscuous = True, multicast = True)
           c.set_namespace(0, method = 'remove_all')
           ns_name = self._create_shared_ns(port = 0)
           cmds = NSCmds()
           MAC = "00:01:02:03:04:05"
           cmds.add_node(MAC, shared_ns = ns_name)
           cmds.set_ipv4(MAC, ipv4 = "1.1.1.3", subnet = 24, shared_ns = True)
           cmds.set_dg(shared_ns = ns_name, dg = "1.1.1.2")
           cmds.set_ipv6(MAC,enable = True, shared_ns = True)

           c.set_namespace_start(0, cmds)
           c.wait_for_async_results(0)
           c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.3')
           r = c.ping_ip(1, '1.1.1.3')

           assert len(r) == 5, 'should be 5 responses '
           assert r[0].state == ServiceICMP.PINGRecord.SUCCESS 

        finally: 
           c.set_l3_mode_line('-p 1 --src 1.1.1.2 --dst 1.1.1.1')
           c.set_port_attr(promiscuous = False, multicast = False)

    def test_get_shared_ns_node_info(self):
        c = self.stl_trex
        MAC = "00:01:02:03:04:05"
        try:
            c.namespace_remove_all()
            ns_name = self._create_shared_ns(port = 0)
            
            cmds = NSCmds()
            cmds.add_node(MAC, shared_ns = ns_name)
            cmds.set_ipv4(MAC, ipv4 = "1.1.1.3", subnet = 24, shared_ns = True)
            cmds.set_ipv6(MAC, enable = True, shared_ns = True)
            cmds.set_vlan(MAC, vlans = [22], tpids = [0x8100])

            c.set_namespace_start(0, cmds)
            c.wait_for_async_results(0)

            res = c.set_namespace(0, method = "get_nodes_info",  macs_list = [MAC])
            nodes = res['result']['nodes']
            assert(len(nodes) == 1)
            node_info = nodes[0]
            assert(node_info['ether']['src'] == MAC)
            assert(node_info['ipv4']['src'] == "1.1.1.3")
            assert(node_info['ipv4']['subnet'] == 24)
            assert(node_info['ipv6']['enabled'] == True)
            assert(node_info['vlan']['tags'] == [22])
            assert(node_info['vlan']['tpids'] == [0x8100])
            
        finally:
            c.namespace_remove_all()

    def test_setting_shared_ns_vlans(self):
        c = self.stl_trex
        try:
            c.namespace_remove_all()
            ns_name = self._create_shared_ns(port = 0)

            MAC = "00:01:02:03:04:05"
            c.set_namespace(0, method = "add_node" ,mac = MAC, shared_ns = ns_name)

            vlans_list = [[22], [22, 23], [22, 23]]
            tpids_list = [[0x8100], [0x8100, 0x8100], [0x8100, 0x8100]]

            for vlans, tpids in zip(vlans_list, tpids_list):

                cmds = NSCmds()
                cmds.set_vlan(MAC, vlans, tpids)
                cmds.get_nodes_info([MAC])
                c.set_namespace_start(0, cmds)
                nodes = c.wait_for_async_results(0)[1]['result']['nodes']
                
                assert(len(nodes) == 1)
                node_info = nodes[0]
                assert(node_info['vlan']['tags'] == vlans)
                assert(node_info['vlan']['tpids'] == tpids)
        finally:
            c.namespace_remove_all()
