# -*- coding: utf-8 -*-
"""Test miscellaneous objects from the standard library"""

import uuid
import unittest

import jsonpickle


class UUIDTestCase(unittest.TestCase):
    def test_random_uuid(self):
        u = uuid.uuid4()
        encoded = jsonpickle.encode(u)
        decoded = jsonpickle.decode(encoded)

        expect = u.hex
        actual = decoded.hex
        self.assertEqual(expect, actual)

    def test_known_uuid(self):
        expect = '28b56adbd18f44e2a5556bba2f23e6f6'
        exemplar = uuid.UUID(expect)
        encoded = jsonpickle.encode(exemplar)
        decoded = jsonpickle.decode(encoded)

        actual = decoded.hex
        self.assertEqual(expect, actual)


class BytesTestCase(unittest.TestCase):
    def test_bytestream(self):
        expect = (
            b'\x89HDF\r\n\x1a\n\x00\x00\x00\x00\x00\x08\x08\x00'
            b'\x04\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00'
            b'\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xffh'
            b'\x848\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff'
            b'\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00`\x00\x00'
            b'\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00'
            b'\x00\x88\x00\x00\x00\x00\x00\x00\x00\xa8\x02\x00'
            b'\x00\x00\x00\x00\x00\x01\x00\x01\x00'
        )
        encoded = jsonpickle.encode(expect)
        actual = jsonpickle.decode(encoded)
        self.assertEqual(expect, actual)


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(UUIDTestCase))
    suite.addTest(unittest.makeSuite(BytesTestCase))
    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='suite')
