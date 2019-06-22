from .astf_general_test import CASTFGeneral_Test, CTRexScenario
import os, sys
from trex.astf.api import *
import time
import pprint
from nose.tools import assert_raises, nottest
from scapy.all import *
from trex.astf.topo import *
import tempfile
import copy


class ASTFTopology_Test(CASTFGeneral_Test):
    ''' Checks for Topology '''
    resolved = False

    @classmethod
    def setUpClass(cls):
        CASTFGeneral_Test.setUpClass()
        if cls.is_dummy_ports or cls.is_vdev or cls.is_vf_nics or cls.is_virt_nics or not cls.is_loopback:
            cls.skip('Topo tests are designed to work with non-dummy baremetal loopback NICs')
        cls.c_ports = []
        cls.s_ports = []
        for p1, p2 in CTRexScenario.ports_map['bi']:
            if p1 % 2:
                p1, p2 = p2, p1
            cls.c_ports.append(p1)
            cls.s_ports.append(p2)


    def setUp(self):
        CASTFGeneral_Test.setUp(self)
        c = CTRexScenario.astf_trex
        c.topo_clear()
        c.set_port_attr(promiscuous = True)


    @classmethod
    def tearDownClass(cls):
        CASTFGeneral_Test.tearDownClass()
        c = CTRexScenario.astf_trex
        c.remove_all_captures()
        c.set_port_attr(promiscuous = False)


    def tearDown(self):
        CASTFGeneral_Test.tearDown(self)
        self.astf_trex.topo_clear()


    def test_topo_traffic(self):
        print('')

        c = self.astf_trex
        traffic_dist = c.get_traffic_distribution('16.0.0.0', '16.0.0.255', '1.0.0.0', False)
        topo = ASTFTopology()
        caps = {}
        for c_p, s_p in zip(self.c_ports, self.s_ports):
            c_port = c.get_port(c_p)
            ip_minimal = 0xffffffff
            for range in traffic_dist[c_p].values():
                ip_minimal = min(ip_minimal, ip2int(range['start']))
            src_mac1 = '11:00:00:00:01:%02d' % c_p
            src_mac2 = '11:00:00:00:02:%02d' % c_p
            src_mac3 = c_port.get_layer_cfg()['ether']['src']
            dst_mac1 = '11:00:00:00:03:%02d' % c_p
            dst_mac2 = '11:00:00:00:04:%02d' % c_p
            dst_mac3 = '11:00:00:00:05:%02d' % c_p
            ip_t = '{}.0.0.%s'.format(16 + int(c_p / 2))

            def get_ip(offset):
                return int2ip(ip_minimal + offset)

            port_id = '%s.1' % c_p
            topo.add_vif(port_id = port_id, src_mac = src_mac1)
            topo.add_gw(port_id = port_id, dst = dst_mac1, src_start = get_ip(0), src_end = get_ip(0))
            if len(caps) < 10:
                cap_info = c.start_capture(tx_ports = c_p, limit = 1, bpf_filter = 'ether src %s and ether dst %s' % (src_mac1, dst_mac1))
                caps[cap_info['id']] = port_id

            port_id = '%s.2' % c_p
            topo.add_vif(port_id = port_id, src_mac = src_mac2)
            topo.add_gw(port_id = port_id, dst = dst_mac2, src_start = get_ip(1), src_end = get_ip(2))
            if len(caps) < 10:
                cap_info = c.start_capture(tx_ports = c_p, limit = 1, bpf_filter = 'ether src %s and ether dst %s' % (src_mac2, dst_mac2))
                caps[cap_info['id']] = port_id

            port_id = '%s' % c_p
            topo.add_gw(port_id = port_id, dst = dst_mac3, src_start = get_ip(3), src_end = get_ip(255))
            if len(caps) < 10:
                cap_info = c.start_capture(tx_ports = c_p, limit = 1, bpf_filter = 'ether src %s and ether dst %s' % (src_mac3, dst_mac3))
                caps[cap_info['id']] = port_id

        c.topo_load(topo)
        try:
            c.topo_resolve()
            c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'))
            c.start(mult = 10000, duration = 1)
            c.wait_on_traffic()
        finally:
            c.stop()
            c.topo_clear()

        matched = [c.get_capture_status()[k]['matched'] for k in caps.keys()]
        if not all(matched):
            for cap_id in sorted(caps.keys()):
                print('%-6s   %-8s   %-8s' % ('ID:', 'Matched:', 'GW port:'))
                for cap_id, cap_info in c.get_capture_status().items():
                    print('%-6s   %8s   %8s' % (cap_id, cap_info['matched'], caps[cap_id]))
            raise TRexError('Some captures are empty => topo failed...')
        print('All the captures got filled')


    def test_topo_negative(self):
        print('')
        c = self.astf_trex
        c_p = '%s' % self.c_ports[0]
        vif_p = '%s.2' % c_p

        with assert_raises(TypeError):
            TopoVIF()
        with assert_raises(TRexError): # port_id should be string
            TopoVIF(1.3, '12:12:12:12:12:12')
        with assert_raises(TRexError):
            TopoVIF('1', '12:12:12:12:12:12')
        with assert_raises(TypeError):
            TopoVIF('1.3')
        with assert_raises(TRexError):
            TopoVIF('1.3.3', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            TopoVIF('1.3', ':12')
        with assert_raises(TRexError):
            TopoVIF('1.-3', '12:12:12:12:12:12')
        with assert_raises(TRexError): # bad ipv4/6
            TopoVIF('1.3', '12:12:12:12:12:12', src_ipv4 = 123)
        with assert_raises(TRexError):
            TopoVIF('1.3', '12:12:12:12:12:12', src_ipv6 = 123)
        with assert_raises(TRexError): # bad vlans
            TopoVIF('1.3', '12:12:12:12:12:12', vlan = -1)
        with assert_raises(TRexError):
            TopoVIF('1.3', '12:12:12:12:12:12', vlan = 99999)
        with assert_raises(TRexError):
            TopoVIF('1.3', '12:12:12:12:12:12', vlan = '3')
        with assert_raises(TRexError):
            TopoVIF('1.3', '12:12:12:12:12:12', vlan = [])

        topo = ASTFTopology()
        topo.add_vif('-1.3', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            c.topo_load(topo)

        topo = ASTFTopology()
        vif1 = TopoVIF('1.3', '12:12:12:12:12:12')
        topo.add_vif_obj(vif1)
        topo.add_vif_obj(vif1)
        with assert_raises(TRexError): # duplicate VIF
            c.topo_load(topo)

        topo = ASTFTopology()
        vif2 = TopoVIF('1.4', '12:12:12:12:12:12')
        topo.add_vif_obj(vif1)
        topo.add_vif_obj(vif2)
        with assert_raises(TRexError):
            c.topo_load(topo)

        with assert_raises(TypeError):
            TopoGW()
        with assert_raises(TRexTypeError):
            TopoGW(c_p, '16.0.0.0', '16.0.0.1', 123)
        with assert_raises(TRexError): # server port
            TopoGW('1', '16.0.0.0', '16.0.0.1', '1.1.1.1')
        with assert_raises(TRexError):
            TopoGW(c_p, '16.0.0', '16.0.0.1', '1.1.1.1')
        with assert_raises(TRexError):
            TopoGW(c_p, '16.0.0.0', '16.0.0', '1.1.1.1')
        with assert_raises(TRexError):
            TopoGW(c_p, '16.0.0.0', '16.0.0.1', '')
        with assert_raises(TRexError):
            TopoGW(c_p, '16.0.0.0', '16.0.0.1', '11:11:11:11:11:11', dst_mac = '22:22:22:22:22:22')

        topo = ASTFTopology()
        topo.add_gw('-2', '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            c.topo_load(topo)

        topo = ASTFTopology()
        topo.add_gw(c_p, '16.0.0.1', '16.0.0.0', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            c.topo_load(topo)

        topo = ASTFTopology()
        gw = TopoGW(c_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        topo.add_gw_obj(gw)
        topo.add_gw_obj(gw)
        with assert_raises(TRexError):
            c.topo_load(topo)

        topo = ASTFTopology()
        topo.add_gw(c_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        topo.add_gw(c_p, '16.0.0.1', '16.0.0.2', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            c.topo_load(topo)

        topo = ASTFTopology()
        topo.add_gw(vif_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        with assert_raises(TRexError):
            c.topo_load(topo)

        vif = TopoVIF(vif_p, '12:12:12:12:12:12')
        topo = ASTFTopology()
        topo.add_gw(vif_p, '16.0.0.0', '16.0.0.1', '1.1.1.1')
        topo.add_vif_obj(vif)
        with assert_raises(TRexError):
            c.topo_load(topo)
        topo = ASTFTopology()
        topo.add_gw(vif_p, '16.0.0.0', '16.0.0.1', '::1')
        topo.add_vif_obj(vif)
        with assert_raises(TRexError):
            c.topo_load(topo)

        # backup client port, simulate L2
        port = c.get_port(self.c_ports[0])
        port_attr = port.get_ts_attr()
        port_attr_bu = copy.deepcopy(port_attr)
        port_attr['layer_cfg']['ipv4']['state'] = 'none'
        port_attr['layer_cfg']['ipv6']['enabled'] = False
        try:
            port.update_ts_attr(port_attr)
            topo = ASTFTopology()
            topo.add_gw(c_p, '16.0.0.0', '16.0.0.1', '1.1.1.1')
            with assert_raises(TRexError):
                c.topo_load(topo)
            topo = ASTFTopology()
            topo.add_gw(c_p, '16.0.0.0', '16.0.0.1', '::1')
            with assert_raises(TRexError):
                c.topo_load(topo)
            port_attr['layer_cfg']['ipv6']['enabled'] = True
            c.topo_load(topo)
            with assert_raises(TRexError):
                c.topo_resolve()
        finally:
            port.update_ts_attr(port_attr_bu)

        topo = ASTFTopology()
        topo.add_gw(c_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        c.topo_clear()
        c.topo_load(topo)
        with assert_raises(TRexError):
            c.topo_load(topo)

        c.topo_clear()
        with assert_raises(TRexError):
            c.topo_load('asdfasdfasfd')

        topo = ASTFTopology()
        with assert_raises(TRexError):
            c.topo_load(topo)
        c.topo_clear()
        with assert_raises(TRexError):
            c.topo_load('esgsdfg.py')
        with assert_raises(TRexError):
            c.topo_load('sdfsdfg.json')
        with assert_raises(TRexError):
            c.topo_load('sdfgsdfg.yaml')

        with tempfile.NamedTemporaryFile(mode = 'w', suffix = '.py') as f:
            f.write('def get_topo(): pass')
            f.flush()
            with assert_raises(TRexError):
                c.topo_load(f.name)

        with tempfile.NamedTemporaryFile(mode = 'w', suffix = '.py') as f:
            f.write('def get_topo(): pass')
            f.flush()
            with assert_raises(TRexError):
                c.topo_load(f.name)

        with assert_raises(TRexError):
            c.topo_mngr.from_data({'gws': 1})

        with assert_raises(TRexError):
            c.topo_mngr.from_data({'vifs': 1})

        handler_bu = c.handler
        try:
            c.handler = None
            with assert_raises(TRexError):
                c.topo_clear()
        finally:
            c.handler = handler_bu
        c.topo_mngr.to_json(True)


    def test_topo_save_load_and_coverage(self):
        c = self.astf_trex
        c_p = '%s' % self.c_ports[0]
        vif_p = '%s.2' % c_p

        TopoGW(c_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12')
        TopoGW(c_p, '16.0.0.0', '16.0.0.1', '1.1.1.1')
        TopoGW(c_p, '16.0.0.0', '16.0.0.1', '::1')
        TopoVIF(vif_p, '12:12:12:12:12:12', vlan = None)
        vif = TopoVIF(vif_p, '12:12:12:12:12:12', vlan = 1)
        vif.get_data(True)
        gw = TopoGW(vif_p, '16.0.0.0', '16.0.0.1', '12:12:12:12:12:12', dst_mac = '12:12:12:12:12:12')
        gw.get_data(True)

        topo = ASTFTopology()
        topo.add_gw_obj(gw)
        topo.add_vif_obj(vif)
        topo_data = topo.get_data()
        c.topo_load(topo)

        yaml_file = tempfile.mktemp(suffix = '.yaml')
        c.topo_save(yaml_file)
        try:
            c.topo_clear()
            c.topo_load(yaml_file)
        finally:
            os.unlink(yaml_file)
        if topo_data != c.topo_mngr.get_merged_data():
            raise TRexError('Saved to YAML and loaded back topology is not equal to original')
        json_file = tempfile.mktemp(suffix = '.json')
        c.topo_save(json_file)
        try:
            c.topo_clear()
            c.topo_load(json_file)
        finally:
            os.unlink(json_file)
        if topo_data != c.topo_mngr.get_merged_data():
            raise TRexError('Saved to JSON and loaded back topology is not equal to original')
        python_file = tempfile.mktemp(suffix = '.py')
        c.topo_save(python_file)
        try:
            c.topo_clear()
            c.topo_load(python_file)
        finally:
            os.unlink(python_file)
        if topo_data != c.topo_mngr.get_merged_data():
            raise TRexError('Saved to Python code and loaded back topology is not equal to original')

        devnull = open(os.devnull, 'w')
        stdout, sys.stdout = sys.stdout, devnull
        try:
            c.topo_mngr.info('info msg')
            c.topo_mngr.warn('warning msg')
            c.topo_show(self.c_ports[0])
            c.topo_show()
            c.topo_clear()
            c.topo_resolve()
            c.topo_show()
        finally:
            sys.stdout = stdout
            devnull.close()


    def test_topo_resolve(self):
        print('')
        c = CTRexScenario.astf_trex
        c_p = '%s' % self.c_ports[0]
        vif_p = '%s.2' % c_p
        port = c.get_port(self.c_ports[0])
        port_attr = port.get_ts_attr()
        if port_attr['layer_cfg']['ipv4']['state'] == 'none':
            self.skip('No IPv4 on port')
        if port_attr['layer_cfg']['ipv6']['enabled']:
            self.skip('IPv6 on port')
        topo = ASTFTopology()
        topo.add_vif(vif_p, '12:12:12:12:12:12', src_ipv4 = port_attr['layer_cfg']['ipv4']['src'])
        topo.add_gw(vif_p, '16.0.0.0', '16.0.0.1', port_attr['layer_cfg']['ipv4']['dst'])
        topo.add_gw(vif_p, '17.0.0.0', '17.0.0.1', port_attr['layer_cfg']['ipv4']['dst'])
        topo.add_gw(vif_p, '18.0.0.0', '18.0.0.1', '12:12:12:12:12:12')
        c.topo_clear()
        c.topo_load(topo)
        c.set_port_attr(promiscuous = False)
        with assert_raises(TRexError):
            c.topo_resolve(self.c_ports[0])
        c.set_port_attr(promiscuous = True)
        topo.add_gw(c_p, '19.0.0.0', '19.0.0.1', port_attr['layer_cfg']['ipv4']['dst'])
        topo.add_gw(c_p, '20.0.0.0', '20.0.0.1', port_attr['layer_cfg']['ipv4']['dst'])
        c.topo_clear()
        c.topo_load(topo)
        c.topo_resolve(self.c_ports[0])
        topo.add_gw(c_p, '21.0.0.0', '21.0.0.1', '200.200.200.200')
        topo.add_gw(c_p, '22.0.0.0', '22.0.0.1', '201.201.201.201')
        c.topo_clear()
        c.topo_load(topo)
        with assert_raises(TRexError):
            c.topo_resolve(self.c_ports[0])

        ASTFTopology_Test.resolved = True


    # should run last in class
    def test_topo_zzz_coverage(self):
        print('')
        if not ASTFTopology_Test.resolved:
            self.skip('Resolve was not done, coverage not full')
        cov = CTRexScenario.coverage
        if not cov:
            self.skip('Coverage not imported')
        cov.exclude("Could not clear topology: ") # error from server
        cov.exclude("Could not get topology from server") # error from server
        cov.exclude('Server response is expected to have "topo_data" key') # error from server
        cov.exclude('Could not upload topology') # error from server
        path = os.path.join('trex', 'astf', 'topo.py')
        fullpath, all_lines, miss_lines, miss_friendly = cov.analysis(path)
        if miss_lines:
            raise TRexError('Not covered %s/%s line(s) in trex.astf.topo: %s' % (len(miss_lines), len(all_lines), miss_friendly))



