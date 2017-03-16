#!/usr/bin/python
import sys, os
from multiprocessing import Process
import tempfile

def check_offsets(build, stdout, scapy_str = None, pcap = None):
    import sys
    sys.stdout = stdout
    sys.stderr = stdout

    # clean this env
    for key in sys.modules.copy().keys():
        if key.startswith('scapy.'):
            del sys.modules[key]
    globals().clear()

    import os
    import outer_packages
    from scapy.all import Ether, IP, UDP

    if scapy_str:
        pkt = eval(scapy_str)
    elif pcap:
        from scapy.utils import rdpcap
        pkt = rdpcap(pcap)[0]
    else:
        raise Exception('Please specify scapy_str or pcap.')

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


def isolate_env(f, *a, **k):
    with tempfile.TemporaryFile(mode = 'w+') as tmpfile:
        k.update({'stdout': tmpfile})
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

    def test_offsets_udp_build(self):
        isolate_env(check_offsets, scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = True)

    def test_offsets_udp_nobuild(self):
        isolate_env(check_offsets, scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = False)

    def test_offsets_pcap_build(self):
        isolate_env(check_offsets, pcap = self.dir + 'golden/bp_sim_dns_vlans.pcap', build = True)

    def test_offsets_pcap_nobuild(self):
        isolate_env(check_offsets, pcap = self.dir + 'golden/bp_sim_dns_vlans.pcap', build = False)

