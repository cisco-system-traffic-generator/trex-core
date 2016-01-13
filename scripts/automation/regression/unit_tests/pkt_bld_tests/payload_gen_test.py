#!/router/bin/python

import pkt_bld_general_test
from client_utils.packet_builder import *
from dpkt.ethernet import Ethernet
from dpkt.ip import IP
from dpkt.icmp import ICMP
from nose.tools import assert_equal
from nose.tools import assert_not_equal
from nose.tools import assert_raises
from nose.tools import raises
import re
import binascii

class CTRexPayloadGen_Test(pkt_bld_general_test.CGeneralPktBld_Test):

    def setUp(self):
#       echo = dpkt.icmp.ICMP.Echo()
#       echo.id = random.randint(0, 0xffff)
#       echo.seq = random.randint(0, 0xffff)
#       echo.data = 'hello world'
#
#       icmp = dpkt.icmp.ICMP()
#       icmp.type = dpkt.icmp.ICMP_ECHO
#       icmp.data = echo

        # packet generation is done directly using dpkt package and 
        self.packet = Ethernet()
        ip = IP(src='\x01\x02\x03\x04', dst='\x05\x06\x07\x08', p=1)
        icmp = ICMP(type=8, data=ICMP.Echo(id=123, seq=1, data='foobar'))
        ip.data = icmp
        self.packet.src = "\x00\x00\x55\x55\x00\x00"
        self.packet.dst = "\x00\x00\x11\x11\x00\x00"
        self.packet.data = ip
        self.print_packet(self.packet)
        self.max_pkt_size = 1400
        self.pld_gen = CTRexPktBuilder.CTRexPayloadGen(self.packet, self.max_pkt_size)

    @staticmethod
    def special_match(strg, search=re.compile(r'[^a-zA-Z0-9]').search):
        return not bool(search(strg))

    @staticmethod
    def principal_period(s):
        # finds the string the repeats itself in the string
        i = (s+s).find(s, 1, -1)
        return None if i == -1 else s[:i]

    def test_gen_random_str(self):
        generated_str = self.pld_gen.gen_random_str()
        # print "\nGenerated string: {}".format(generated_str)
        # chech that the generated string is accorsing to rules.
        assert CTRexPayloadGen_Test.special_match(generated_str)
        assert_equal(len(generated_str), (self.max_pkt_size - len(self.packet)))
        
    def test_gen_repeat_ptrn(self):
        gen_len = self.max_pkt_size - len(self.packet)
        # case 1 - repeated string
        repeat_str = "HelloWorld"
        generated_str = self.pld_gen.gen_repeat_ptrn(repeat_str)
        for i in xrange(len(repeat_str)):
            if generated_str.endswith(repeat_str[:i+1]):
                # remove the string residue, if found
                generated_str = generated_str[:-(i+1)]
                # print generated_str
                break
        assert_equal(CTRexPayloadGen_Test.principal_period(generated_str), "HelloWorld")

        # case 2.1 - repeated single number - long number
        repeat_num = 0x645564646465
        generated_str = self.pld_gen.gen_repeat_ptrn(repeat_num)
        ptrn = binascii.unhexlify(hex(repeat_num)[2:])
        for i in xrange(len(ptrn)):
            if generated_str.endswith(ptrn[:i+1]):
                # remove the string residue, if found
                generated_str = generated_str[:-(i+1)]
                break
        assert_equal(CTRexPayloadGen_Test.principal_period(generated_str), ptrn)
        # case 2.2 - repeated single number - 1 byte
        repeat_num = 0x64
        generated_str = self.pld_gen.gen_repeat_ptrn(repeat_num)
        ptrn = binascii.unhexlify(hex(repeat_num)[2:])
        assert_equal(CTRexPayloadGen_Test.principal_period(generated_str), ptrn)
        assert_equal(len(generated_str), (self.max_pkt_size - len(self.packet)))
        # case 3 - repeated sequence
        repeat_seq = (0x55, 0x60, 0x65, 0x70, 0x85)
        ptrn = binascii.unhexlify(''.join(hex(x)[2:] for x in repeat_seq))
        generated_str = self.pld_gen.gen_repeat_ptrn(repeat_seq)
        # ptrn = binascii.unhexlify(hex(repeat_num)[2:])
        for i in xrange(len(ptrn)):
            if generated_str.endswith(ptrn[:i+1]):
                # remove the string residue, if found
                generated_str = generated_str[:-(i+1)]
                break
        assert_equal(CTRexPayloadGen_Test.principal_period(generated_str), ptrn)
        
        # in tuples, check that if any of the numbers exceeds limit
        assert_raises(ValueError, self.pld_gen.gen_repeat_ptrn, (0x1, -18))
        assert_raises(ValueError, self.pld_gen.gen_repeat_ptrn, (0xFFF, 5))
        # finally, check an exception is thrown in rest of cases
        assert_raises(ValueError, self.pld_gen.gen_repeat_ptrn, 5.5)





        








        pass


    def tearDown(self):
        pass


if __name__ == "__main__":
    pass

