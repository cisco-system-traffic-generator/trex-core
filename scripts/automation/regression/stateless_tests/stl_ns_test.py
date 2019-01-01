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

    def tearDown(self):
        CStlGeneral_Test.tearDown(self)
        self.stl_trex.set_service_mode(enabled = False)

    def test_ns_add_remove(self):
        c= self.stl_trex

        port= 0

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

        print(macs)
        if len(macs) != 1:
           self.fail(' macs should include one mac')
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
        pprint.pprint(cnt)

        assert cnt['tx_multicast_pkts']>0, 'multicast rx counter is zero'

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
        pprint.pprint(cnt)

        assert len(cnt)==0, 'multicast tx shoule be zero '

        # remove one
        cmds=NSCmds()
        cmds.remove_node(MAC)
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);


        cmds=NSCmds()
        cmds.get_nodes()
        c.set_namespace_start(port, cmds)
        r=c.wait_for_async_results(port);
        macs=r[0]['result']['nodes']

        print(macs)
        if len(macs) != 0:
           self.fail(' macs should include zero')

    def test_ping_to_ns(self):

        if not CTRexScenario.setup_name in ('trex17'):
            return 
        # this test works on specific setup with specific configuration 
        c= self.stl_trex
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
           c.namespace_remove_all()

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






        


        


        




