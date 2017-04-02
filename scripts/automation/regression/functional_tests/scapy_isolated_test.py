#!/usr/bin/python
import sys, os
from multiprocessing import Process
import tempfile

def check_offsets(build, stdout, scapy_str):
    import sys
    sys.stdout = stdout
    sys.stderr = stdout

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

def check_offsets_pcap(stdout, pcap):
    import sys
    sys.stdout = stdout
    sys.stderr = stdout

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
            else:
                if field.get_size_bytes() == 0:
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            not_built_offsets[lay.name][field.name] = field._offset

        lay = lay.payload

    print('')
    pkt.build()

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
            else:
                if field.get_size_bytes() == 0:
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            assert not_built_offsets[lay.name][field.name] == field._offset, 'built and not built pcap offsets differ'

        lay = lay.payload


def isolate_env(f, *a, **k):
    with tempfile.TemporaryFile(mode = 'w+') as tmpfile:
        k['stdout'] = tmpfile
        p = Process(target = f, args = a, kwargs = k)
        p.start()
        p.join()
        print('')
        tmpfile.seek(0)
        print(tmpfile.read())
    if p.exitcode:
        raise Exception('Return status not zero, check the output')

class CScapyOffsets_Test():
    def setUp(self):
        self.dir = os.path.abspath(os.path.dirname(__file__)) + '/'

    # verify that built packet gives non-zero offsets
    def test_offsets_udp_build(self):
        isolate_env(check_offsets, scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = True)

    # verify that non-built packet gives zero offsets
    def test_offsets_udp_nobuild(self):
        isolate_env(check_offsets, scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = False)

    # verify that pcap either built or not gives same non-zero offsets
    def test_offsets_pcap(self):
        isolate_env(check_offsets_pcap, pcap = self.dir + 'golden/bp_sim_dns_vlans.pcap')

