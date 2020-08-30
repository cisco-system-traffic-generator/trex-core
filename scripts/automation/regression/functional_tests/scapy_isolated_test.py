#!/usr/bin/python
import sys, os
from multiprocessing import Process
import tempfile
from functools import wraps


def check_offsets(build, scapy_str):
    import sys

    # clean this env
    for key in sys.modules.copy().keys():
        if key.startswith('scapy.'):
            del sys.modules[key]
    globals().clear()

    import outer_packages
    from scapy.all import Ether, IP, UDP

    pkt = eval(scapy_str)

    if build:
        pkt.build()

    assert pkt
    assert pkt.payload

    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            if build:
                assert lay._offset != 0, 'Offset of second and further layers should not be zero if packets is built.'
            else:
                assert lay._offset == 0, 'Offset of second and further layers should be zero if packets is not built.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
            else:
                if build:
                    if field.get_size_bytes() == 0:
                        continue
                    assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
                else:
                    assert field._offset == 0, 'Offset of second and further fields should be zero if packets is not built.'

        lay = lay.payload


def check_offsets_pcap(pcap):
    import sys

    # clean this env
    for key in sys.modules.copy().keys():
        if key.startswith('scapy.'):
            del sys.modules[key]
    globals().clear()

    import outer_packages
    from scapy.all import Ether, IP, UDP
    from scapy.layers.dns import DNS
    from scapy.utils import rdpcap

    pkt = rdpcap(pcap)[0]
    assert pkt
    assert pkt.payload

    not_built_offsets = {}
    cond_var_length = False
    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        not_built_offsets[lay.name] = {}
        not_built_offsets[lay.name]['_offset'] = lay._offset
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            assert lay._offset != 0, 'Offset of second and further layers should not be zero.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
                if field.name == "length":
                    cond_var_length = True
            else:
                if field.get_size_bytes() == 0:
                    continue
                if cond_var_length:
                    cond_var_length = False
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            not_built_offsets[lay.name][field.name] = field._offset

        lay = lay.payload

    print('')
    pkt.build()
    cond_var_length = False
    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        assert not_built_offsets[lay.name]['_offset'] == lay._offset, 'built and not built pcap offsets differ'
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            assert lay._offset != 0, 'Offset of second and further layers should not be zero.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
                if field.name == "length":
                    cond_var_length = True
            else:
                if field.get_size_bytes() == 0:
                    continue
                if cond_var_length:
                    cond_var_length = False
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            assert not_built_offsets[lay.name][field.name] == field._offset, 'built and not built pcap offsets differ'

        lay = lay.payload


def isolate_env(f):
    @wraps(f)
    def wrapped(*a):
        print('')
        p = Process(target = f, args = a)
        p.start()
        p.join()
        if p.exitcode:
            raise Exception('Return status not zero, check the output')
    return wrapped


class CScapy_Test():
    def setUp(self):
        self.dir = os.path.abspath(os.path.dirname(__file__)) + '/'

    # verify that built packet gives non-zero offsets
    @isolate_env
    def test_scapy_offsets_udp_build(self):
        check_offsets(scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = True)

    # verify that non-built packet gives zero offsets
    @isolate_env
    def test_scapy_offsets_udp_nobuild(self):
        check_offsets(scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = False)

    # verify that pcap either built or not gives same non-zero offsets
    @isolate_env
    def test_scapy_offsets_pcap(self):
        check_offsets_pcap(pcap = self.dir + 'golden/bp_sim_dns_vlans.pcap')

    @isolate_env
    def test_scapy_ipfix(self):
        from scapy.contrib.ipfix import IPFIX
        p = IPFIX()
        p.show2()

    @isolate_env
    def test_scapy_utils(self):
        from scapy.all import str2int, int2str, str2ip

        # str2int
        assert str2int('0') == ord('0')
        assert str2int(b'\x07') == 7
        assert str2int(b'\xff') == 255
        assert str2int(b'\x01\xff') == 511
        assert str2int(5) == 5
        assert str2int('be') == 25189 # ord(b) * 2^8 + ord(e)
        assert str2int('') == 0

        # int2str
        assert int2str(98) == b'b' 
        assert int2str(255) == b'\xff'
        assert int2str(50000, 2) == b'\xc3\x50'
        assert int2str(25189) == "be".encode('utf-8')
        assert int2str(0) == ''.encode('utf-8')
        assert int2str(5, 3) == b'\x00\x00\x05'

        # str2ip
        assert str2ip(b'\xff\xff\xff\xff') == '255.255.255.255'
        assert str2ip(b'\xc0\xa8\x00\x01') == '192.168.0.1'
