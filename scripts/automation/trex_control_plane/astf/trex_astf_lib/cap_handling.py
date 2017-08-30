import sys
from scapy.all import RawPcapReader, Ether


class CPacketData():
    def __init__(self, direction, payload):
        self.direction = direction
        self.payload = payload

    def dump(self):
        print("{0} - len {1}".format(self.direction, len(self.payload)))

    def is_empty(self):
        if len(self.payload) == 0:
            return True
        else:
            return False

    def __add__(self, other):
        assert(self.direction == other.direction)
        return CPacketData(self.direction, self.payload + other.payload)


class _CPcapReader_help(object):
    states = {"init": 0, "syn": 1, "syn+ack": 2}

    def __init__(self, file_name):
        self.file_name = file_name
        self._pkts = []
        self.c_ip = None
        self.s_ip = None
        self.s_port = -1
        self.d_port = -1
        self.c_tcp_win = -1
        self.s_tcp_win = -1
        self.total_payload_len = 0
        self.state = _CPcapReader_help.states["init"]

    def fail(self, msg):
        print('\nError for file %s: %s\n' % (self.file_name, msg))
        sys.exit(1)

    @property
    def pkts(self):
        return self._pkts

    def condense_pkt_data(self):
        prev_pkt = None
        new_list = []
        for i in range(1, len(self._pkts)):
            pkt = self._pkts[i]
            if pkt.is_empty():
                continue

            if prev_pkt:
                if prev_pkt.direction == pkt.direction:
                    prev_pkt += pkt
                else:
                    new_list.append(prev_pkt)
                    prev_pkt = pkt
            else:
                prev_pkt = pkt
        new_list.append(prev_pkt)
        self._pkts = new_list

    # return -1 if packets in given cap_reader equals to ones in self.
    # If not equal, return number of packets which differs
    def is_same_pkts(self, cap_reader):
        if len(self._pkts) != len(cap_reader._pkts):
            return -2

        for i in range(0, len(self._pkts)):
            if self._pkts[i] == cap_reader._pkts[i]:
                return i

        return -1

    def dump(self):
        print("{0}:{1} --> {2}:{3}".format(self.c_ip, self.s_port, self.s_ip, self.d_port))
        if self.c_tcp_win != -1:
            print("Client TCP window:{0}".format(self.c_tcp_win))
        if self.s_tcp_win != -1:
            print("Server TCP window:{0}".format(self.s_tcp_win))
        for i in range(0, len(self._pkts)):
            self._pkts[i].dump()

    def analyze(self):
        pcap = RawPcapReader(self.file_name).read_all()
        if not pcap:
            self.fail('Empty pcap {0}'.format(self.file_name))

        l4_type = None
        for index, (raw_pkt, _) in enumerate(pcap):
            scapy_pkt = Ether(raw_pkt)

            # l3
            ipv4 = scapy_pkt.getlayer('IP')
            ipv6 = scapy_pkt.getlayer('IPv6')
            if ipv4 and ipv6:
                scapy_pkt.show2()
                self.fail('Packet #%s in pcap has both IPv4 and IPv6!' % index)

            if ipv4:
                l3 = ipv4
            elif ipv6:
                l3 = ipv6
            else:
                scapy_pkt.show2()
                self.fail('Packet #%s in pcap is not IPv4/6.' % index)

            # first packet
            if self.c_ip is None:
                self.c_ip = l3.src
                self.s_ip = l3.dst
                direction = "c"
            else:
                if self.c_ip == l3.src and self.s_ip == l3.dst:
                    direction = "c"
                elif self.s_ip == l3.src and self.c_ip == l3.dst:
                    direction = "s"
                else:
                    self.fail('Only one session is allowed in a file. Packet {0} is from different session'
                              .format(index))

            # l4
            tcp = scapy_pkt.getlayer('TCP')
            udp = scapy_pkt.getlayer('UDP')
            if tcp and udp:
                scapy_pkt.show2()
                self.fail('Packet #%s in pcap has both TCP and UDP' % index)

            elif tcp:
                l4 = tcp
                # SYN
                if l4.flags & 0x02:
                    # SYN + ACK
                    if l4.flags & 0x10:
                        self.s_tcp_win = tcp.window
                        if self.state == _CPcapReader_help.states["init"]:
                            self.fail('Packet #%s is SYN+ACK, but there was no SYN yet, or ' % index)
                        else:
                            if self.state != _CPcapReader_help.states["syn"]:
                                self.fail('Packet #%s is SYN+ACK, but there was already SYN+ACK in cap file' % index)
                        self.state = _CPcapReader_help.states["syn+ack"]
                    # SYN - no ACK. Should be first packet client->server
                    else:
                        self.c_tcp_win = tcp.window
                        # allowing syn retransmission because cap2/https.pcap contains this
                        if self.state > _CPcapReader_help.states["syn"]:
                            self.fail('Packet #%s is TCP SYN, but there was already TCP SYN in cap file' % index)
                        else:
                            self.state = _CPcapReader_help.states["syn"]
                else:
                    if self.state != _CPcapReader_help.states["syn+ack"]:
                        self.fail('Cap file must start with syn, syn+ack sequence')
                if l4_type not in (None, 'TCP'):
                    self.fail('PCAP contains both TCP and %s. This is not supported currently.' % l4_type)
                l4_type = 'TCP'
            elif udp:
                l4 = udp
                if l4_type not in (None, 'UDP'):
                    self.fail('PCAP contains both UDP and %s. This is not supported currently.' % l4_type)
                l4_type = 'UDP'
            else:
                scapy_pkt.show2()
                self.fail('Packet #%s in pcap is not TCP or UDP.' % index)

            if self.s_port == -1:
                self.s_port = l4.sport
                self.d_port = l4.dport

            padding = scapy_pkt.getlayer('Padding')
            if padding is not None:
                pad_len = len(padding)
            else:
                pad_len = 0
            l4_payload_len = len(l4.payload) - pad_len
            self.total_payload_len += l4_payload_len
            self._pkts.append(CPacketData(direction, bytes(l4.payload)[0:l4_payload_len]))

    def gen_prog_file_header(self, out):
        out.write("import astf_path\n")
        out.write("from trex_astf_lib.api import *\n")
        out.write("import json\n")
        out.write("\n")

#     ??? think if we need this
#    def gen_prog_file(self, out, origin="c"):
#        self.gen_prog_file_header(out)
#        prog_var_name = "prog"
#        c_tcp_var_name = "c_tcp"
#        s_tcp_var_name = "s_tcp"
#
#        c_tcp_info = ASTFTCPInfo(window=self.c_tcp_win)
#        c_tcp_info.dump(out, c_tcp_var_name)
#        s_tcp_info = ASTFTCPInfo(window=self.s_tcp_win)
#        s_tcp_info.dump(out, s_tcp_var_name)
#
#        prog = ASTFProgram()
#        prog.create_cmds(self._pkts, origin)
#        print(prog.to_json())
#        prog.dump(out, prog_var_name)
#
#        out.write("prof = ASTFProfile(program={0}, tcp_info={1})\n"
#                  .format(prog_var_name, c_tcp_var_name))


class CPcapReader(object):
    last_file_name = None
    obj = None
    condensed = False
    analyzed = False

    def __init__(self, file_name):
        if file_name != CPcapReader.last_file_name:
            CPcapReader.last_file_name = file_name
            CPcapReader.obj = _CPcapReader_help(file_name)
            CPcapReader.condensed = False
            CPcapReader.analyzed = False

        self.obj = CPcapReader.obj

    def condense_pkt_data(self):
        if not self.condensed:
            CPcapReader.condensed = True
            return self.obj.condense_pkt_data()

    def dump(self):
        return self.obj.dump()

    def is_same_pkts(self, cap_reader):
        return self.obj.is_same_pkts(cap_reader.obj)

    def analyze(self):
        if not self.analyzed:
            CPcapReader.analyzed = True
            self.obj.analyze()

    def gen_prog_file_header(self, out):
        return self.obj.gen_prog_file_header(out)

    def gen_prog_file(self, out, origin="c"):
        return self.obj.gen_prog_file(out, origin)

    @property
    def pkts(self):
        return self.obj._pkts

    @property
    def c_ip(self):
        return self.obj.c_ip

    @property
    def s_ip(self):
        return self.obj.s_ip

    @property
    def s_port(self):
        return self.obj.s_port

    @property
    def d_port(self):
        return self.obj.d_port

    @property
    def c_tcp_win(self):
        return self.obj.c_tcp_win

    @property
    def s_tcp_win(self):
        return self.obj.s_tcp_win

    @property
    def payload_len(self):
        return self.obj.total_payload_len
