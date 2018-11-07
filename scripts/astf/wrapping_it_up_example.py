# Example for creating your program by specifying buffers to send, without relaying on pcap file

from trex.astf.api import *
import base64
import struct

class Prof1():

    def __init__(self):
        self.cq_depth = 256
        self.base_pkt_length = 42
        self.payload_length = 14 + 8 + 16
        self.packet_length = self.base_pkt_length + self.payload_length
        self.cmpl_ofst = 14 + 8
        self.cq_ofst = 14
        self.cmpl_base = (1 << 14) | (1 << 15)

    
    def create_first_payload(self, color):
        cqe = "%04X%04X%08X%02X%02X%04X%04X%02X%02X" % (
                   0,     # placeholder for completed index
                   0,     # q_number_rss_type_flags
                   0,     # RSS hash
                   self.packet_length,  # bytes_written_flags
                   0,
                   0,     # vlan 
                   0,     # cksum
                   ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 5)), # flags
                   7 | color
               )
        return ('z' * 14 + 'x' * 8 + base64.b16decode(cqe))

    def update_payload(self, payload, cmpl_ofst, cmpl_idx, cq_ofst, cq_addr):
        payload = payload[0:cmpl_ofst] + struct.pack('<H', cmpl_idx) + payload[cmpl_ofst+2:]
        payload = payload[0:cq_ofst] + struct.pack('!Q', cq_addr) + payload[cq_ofst+8:]
        return payload


    def create_template(self, sip, dip, cq_addr1, cq_addr2, color1, color2, pps):

        prog_c = ASTFProgram(stream=False)  # UDP

        # This loop will send the first 256 packets.
        cmpl_idx = self.cmpl_base
        my_cq_addr = cq_addr1
        payload = self.create_first_payload(color1)

        for _ in range(self.cq_depth):
            payload = self.update_payload(payload, self.cmpl_ofst, cmpl_idx, self.cq_ofst, my_cq_addr)
            prog_c.send_msg(payload)
            prog_c.delay(1000000/pps) # pps = packets per second - therefore delay is 1 sec/ pps 
            cmpl_idx += 1
            my_cq_addr += 16

        # This loop will send the second 256 packets.
        cmpl_idx = self.cmpl_base
        my_cq_addr = cq_addr2
        payload = self.create_first_payload(color2)

        for _ in range(self.cq_depth):
            payload = self.update_payload(payload, self.cmpl_ofst, cmpl_idx, self.cq_ofst, my_cq_addr)
            prog_c.send_msg(payload)
            prog_c.delay(1000000/pps) # pps = packets per second - therefore delay is 1 sec/ pps 
            cmpl_idx += 1
            my_cq_addr += 16

        ip_gen_c = ASTFIPGenDist(ip_range=[sip, sip], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=[dip, dip], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        prog_s = ASTFProgram(stream=False) # UDP
        prog_s.recv_msg(2*self.cq_depth) # server expects to receive 2 times 256 packets.

        temp_c = ASTFTCPClientTemplate(program=prog_c, ip_gen=ip_gen, limit=1)
        temp_s = ASTFTCPServerTemplate(program=prog_s)  # using default association
        return ASTFTemplate(client_template=temp_c, server_template=temp_s)


    def create_profile(self, pps = 1):

        # ip generator
        source_ips = ["10.0.0.1", "33.33.33.37", "11.11.22.99", "11.11.22.99", "10.0.0.1"]
        dest_ips = ["10.0.0.3", "199.111.33.44", "10.33.0.4", "1.2.3.4", "10.0.0.1"]
        cq_addrs1 = [0x84241d000, 0x1111111111111111, 0x2222222222222222, 0x3333333333333333, 0x4444444444444444]
        cq_addrs2 = [0x84241d000, 0x1818181818181818, 0x2828282828282828, 0x3838383838383838, 0x4848484848484848]
        colors1 = [0x80, 0, 0, 0, 0]
        colors2 = [0x00, 0x80, 0x80, 0x80, 0x80]
        templates = []

        for i in range(5):
            templates.append(self.create_template(sip=source_ips[i], dip=dest_ips[i], cq_addr1=cq_addrs1[i],
                             cq_addr2=cq_addrs2[i],color1=colors1[i], color2=colors2[i], pps=pps))

        # profile
        ip_gen_c = ASTFIPGenDist(ip_range=[source_ips[0], source_ips[0]], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=[dest_ips[0], dest_ips[0]], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c,
                            dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen, templates=templates)

    def get_profile(self, **kwargs):
        pps = kwargs.get('pps', 1)
        return self.create_profile(pps)


def register():
    return Prof1()
