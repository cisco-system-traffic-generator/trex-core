'''ATA over Ethernet ATA command'''

import dpkt, aoe

ATA_DEVICE_IDENTIFY = 0xec

class AOEATA(dpkt.Packet):
    __hdr__ = (
        ('aflags', 'B', 0),
        ('errfeat', 'B', 0),
	('scnt', 'B', 0),
	('cmdstat', 'B', ATA_DEVICE_IDENTIFY),
	('lba0', 'B', 0),
	('lba1', 'B', 0),
	('lba2', 'B', 0),
	('lba3', 'B', 0),
	('lba4', 'B', 0),
	('lba5', 'B', 0),
	('res', 'H', 0),
    )

    # XXX: in unpack, switch on ATA command like icmp does on type


if __name__ == '__main__':
    import unittest

    class AOEATATestCase(unittest.TestCase):
        def test_AOEATA(self):
            s = '\x03\x0a\x6b\x19\x00\x00\x00\x00\x45\x00\x00\x28\x94\x1f\x00\x00\xe3\x06\x99\xb4\x23\x2b\x24\x00\xde\x8e\x84\x42\xab\xd1\x00\x50\x00\x35\xe1\x29\x20\xd9\x00\x00\x00\x22\x9b\xf0\xe2\x04\x65\x6b'
            aoeata = AOEATA(s)
            self.failUnless(str(aoeata) == s)

    unittest.main()
