import sys
import os
import unittest
import unittest.mock
from unittest.mock import patch
import time
import logging

# from trex_stl_lib.api import *
from wireless.trex_wireless_packet import *


class PacketTest(unittest.TestCase):
    

    def test_bytes_array_like(self):
        raw_bytes = b'\x00\x11\x22'
        pkt = Packet(raw_bytes)
        self.assertEqual(pkt[0], 0)
        self.assertEqual(pkt[1], 17)
        self.assertEqual(pkt[2], 34)
        self.assertRaises(IndexError, pkt.__getitem__, 3)

        self.assertEqual(pkt[:2], b'\x00\x11')
        self.assertEqual(pkt[1:], b'\x11\x22')
        self.assertEqual(pkt[:], b'\x00\x11\x22')


if __name__ == "__main__":
    unittest.main()
