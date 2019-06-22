from .trex_astf_exceptions import ASTFError
import dpkt
import struct
from repoze.lru import lru_cache


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
        self._times = []
        self._dir = [] 
        self.c_ip = None
        self.s_ip = None
        self.is_tcp=None;
        self.s_port = -1
        self.d_port = -1
        self.c_tcp_win = -1
        self.s_tcp_win = -1
        self.total_payload_len = 0
        self.state = _CPcapReader_help.states["init"]
        self.condensed = False
        self.analyzed = False


    def fail(self, msg):
        raise ASTFError('\nError for file %s: %s\n' % (self.file_name, msg))

    @property
    def pkts(self):
        return self._pkts

    def condense_pkt_data(self):
        if self.condensed:
            return

        combined_data = ''
        new_pkts = []
        new_dirs = []
        for pkt in self._pkts:
            if pkt.is_empty():
                continue

            if combined_data:
                if combined_data.direction == pkt.direction:
                    combined_data += pkt
                else:
                    new_pkts.append(combined_data)
                    new_dirs.append(combined_data.direction)
                    combined_data = pkt
            else:
                combined_data = pkt
        new_pkts.append(combined_data)
        new_dirs.append(combined_data.direction)
        self._pkts = new_pkts
        self._dir = new_dirs
        self.condensed =True

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


    def get_type(self,tcp,udp):
        if tcp and udp==None:
            return("tcp");
        if udp and tcp==None:
            return("udp");
        return("other");

    def analyze(self):
        if self.analyzed:
            return;

        pcap = None
        with open(self.file_name, 'rb') as f:
            pcap = dpkt.pcap.Reader(f)
            if not pcap:
               self.fail('Empty pcap {0}'.format(self.file_name))

            l4_type = None
            last_time=None;
            for index, (time,buf) in enumerate(pcap):
                dtime= time
                pkt_time=0.0; # time from last pkt
                if last_time == None:
                    pkt_time = 0.0;
                else:
                    pkt_time=dtime-last_time;
                last_time = dtime

                eth = dpkt.ethernet.Ethernet(buf)

                l3 = None;
                next = eth.data;

                if isinstance(next, dpkt.ip.IP):
                    l3 = next;

                if isinstance(next, dpkt.ip6.IP6):
                    l3 = next;

                if not l3:
                    self.fail('Packet #%s in pcap is not IPv4 or IPv6!' % index)

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
                l4 = l3.data;

                tcp = None
                udp = None

                if  isinstance(l4, dpkt.udp.UDP):
                    udp = l4;

                if isinstance(l4, dpkt.tcp.TCP):
                    tcp = l4

                typel4 = self.get_type(tcp,udp);
    
                if typel4 == "other":
                     self.fail('Packet #%s in pcap has is not TCP or UDP' % index)
    
                if self.is_tcp == None:
                    self.is_tcp = typel4;
                else:
                    if self.is_tcp != typel4:
                        self.fail('Packet #{0} in pcap is {1} and flow is {2}'.format(index,typel4,self.is_tcp))
    
                if tcp and udp:
                    self.fail('Packet #%s in pcap has both TCP and UDP' % index)
    
                elif tcp:
                    l4 = tcp
                    # SYN
                    if l4.flags & 0x02:
                        # SYN + ACK
                        if l4.flags & 0x10:
                            #self.s_tcp_opts =tcp.opts
                            self.s_tcp_win = tcp.win
                            if self.state == _CPcapReader_help.states["init"]:
                                self.fail('Packet #%s is SYN+ACK, but there was no SYN yet, or ' % index)
                            else:
                                if self.state != _CPcapReader_help.states["syn"]:
                                    self.fail('Packet #%s is SYN+ACK, but there was already SYN+ACK in cap file' % index)
                            self.state = _CPcapReader_help.states["syn+ack"]
                            exp_s_seq = tcp.seq + 1
                        # SYN - no ACK. Should be first packet client->server
                        else:
                            #self.c_tcp_opts = tcp.options
                            self.c_tcp_win = tcp.win
                            exp_c_seq = tcp.seq + 1
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
                    #self.fail('CAP file contains UDP packets. This is not supported yet')
                     l4 = udp
                     if l4_type not in (None, 'UDP'):
                        self.fail('PCAP contains both UDP and %s. This is not supported currently.' % l4_type)
                     l4_type = 'UDP'
                else:
                    self.fail('Packet #%s in pcap is not TCP or UDP.' % index)
    
                if self.s_port == -1:
                    self.s_port = l4.sport
                    self.d_port = l4.dport
    
                pad_len = 0
                l4_payload_len = len(l4.data) 
                self.total_payload_len += l4_payload_len
                self._pkts.append(CPacketData(direction, bytes(l4.data)[0:l4_payload_len]))
                self._times.append(pkt_time)
                self._dir.append(direction)
    
                # special handling for TCP FIN
                if tcp:
                  if l4.flags & 0x01:
                      l4_payload_len = 1
                # verify there is no packet loss or retransmission in cap file
                # don't check for SYN
                if tcp and (l4.flags & 0x02) == 0:
                    if l4.sport == self.s_port:
                        if exp_c_seq != tcp.seq:
                            self.fail("""TCP seq in packet {0} is {1}. We expected {2}. Please check that there are no packet
                            loss or retransmission in cap file"""
                                      .format(index, tcp.seq, exp_c_seq))
                        exp_c_seq = tcp.seq + l4_payload_len
                    else:
                        if exp_s_seq != tcp.seq:
                            self.fail("""TCP seq in packet {0} is {1}. We expected {2}. Please check that there are
                            no packet loss or retransmission in cap file"""
                                      .format(index, tcp.seq, exp_s_seq))
                        exp_s_seq = tcp.seq + l4_payload_len

        self.analyzed =True

    def gen_prog_file_header(self, out):
        out.write("import astf_path\n")
        out.write("from trex_astf_lib.api import *\n")
        out.write("import json\n")
        out.write("\n")


class _CPcapReader(object):
    def __init__(self, file_name):
        self.obj = _CPcapReader_help(file_name)

    def condense_pkt_data(self):
          return self.obj.condense_pkt_data()

    def dump(self):
        return self.obj.dump()

    def is_same_pkts(self, cap_reader):
        return self.obj.is_same_pkts(cap_reader.obj)

    def analyze(self):
        self.obj.analyze()

    def gen_prog_file_header(self, out):
        return self.obj.gen_prog_file_header(out)

    def gen_prog_file(self, out, origin="c"):
        return self.obj.gen_prog_file(out, origin)

    def is_tcp(self):
        if self.obj.is_tcp=="tcp":
            return True
        else:
            return False

    @property
    def pkt_dirs(self):
        return self.obj._dir  #"c","s" 

    @property
    def pkt_times(self):
        return self.obj._times

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
    def c_tcp_opts(self):
        return self.obj.c_tcp_opts

    #@property
    #def s_tcp_opts(self):
    #    return self.obj.s_tcp_opts

    @property
    def payload_len(self):
        return self.obj.total_payload_len

@lru_cache(maxsize=None)
def pcap_reader(file_name):
    obj = _CPcapReader(file_name)
    return (obj)


class CPktWithTime(list):
    def __init__(self, *a):
        list.__init__(self, a)

    @property
    def ts(self):
        return self[0]

    @property
    def pkt(self):
        return self[1]

    def __repr__(self):
        return " %g: %s" % (self.ts, self.pkt)


class CPcapFixTime:
    def __init__(self, in_file_name):
        with open(in_file_name, 'rb') as f:
            self.pcap_data = list(dpkt.pcap.Reader(f))
        self.tuple = None
        self.swap = False
        self.rtt = 0
        self.rtt_syn_ack_ack = 0 # ack on the syn ack
        self.pkts = []

    def calc_rtt(self):
        cnt = 0
        first_time_set = False
        first_time = 0
        last_syn_time = 0
        rtt = 0
        rtt_syn_ack_ack = 0
        for ts, buf in self.pcap_data:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            if first_time_set == False:
                first_time = ts
                first_time_set = True;
            else:
                rtt = ts-first_time
            if ip.p != dpkt.ip.IP_PROTO_TCP:
                raise Exception("not a TCP flow ..")
            if cnt==0 or cnt==1:
                if (tcp.flags & dpkt.tcp.TH_SYN) != dpkt.tcp.TH_SYN:
                    raise Exception("first packet should be with SYN")
            if cnt == 1:
                last_syn_time = ts
            if cnt == 2:
                rtt_syn_ack_ack = ts - last_syn_time
            if cnt > 1:
                break
            cnt += 1

        self.rtt_syn_ack_ack = rtt_syn_ack_ack
        return rtt

    def is_client_side(self, swap):
        if self.swap == swap:
            return True
        else:
            return False

    def calc_timing (self):
        self.rtt = self.calc_rtt()

    @staticmethod
    def nl(buf):
        return struct.unpack('>I', buf)[0]

    def fix_timing(self, out_file_name):
        rtt = self.calc_rtt()
        print('RTT is %g msec' % (rtt*1000))
        if (rtt/2)*1000 < 5:
            raise Exception('RTT is less than 5msec, enlarge the RTT')
        time_to_center = rtt/4
        for ts, buf in self.pcap_data:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            pld = tcp.data
            pkt_swap = False
            if self.nl(ip.src) > self.nl(ip.dst):
                pkt_swap = True
                tuple = (self.nl(ip.dst), self.nl(ip.src), tcp.dport ,tcp.sport, ip.p)
            else:
                tuple = (self.nl(ip.src), self.nl(ip.dst), tcp.sport, tcp.dport, ip.p)
            if self.tuple == None:
                self.swap = pkt_swap
                self.tuple = tuple
            else:
                if self.tuple != tuple:
                    raise Exception("More than one flow - can't process")
            if self.is_client_side(pkt_swap):
                self.pkts.append(CPktWithTime(ts + time_to_center, buf))
            else:
                self.pkts.append(CPktWithTime(ts - time_to_center, buf))

        self.pkts.sort()

        with open(out_file_name, 'wb') as fo:
            pcap_out = dpkt.pcap.Writer(fo)
            for pkt in self.pkts:
                pcap_out.writepkt(pkt.pkt, pkt.ts)

def is_udp_pcap(in_file_name):
    with open(in_file_name, 'rb') as f:
        for _, buf in dpkt.pcap.Reader(f):
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            return ip.p == dpkt.ip.IP_PROTO_UDP

def pcap_cut_udp(max_data_len, in_file_name, out_file_name, verbose = False):
    assert(max_data_len)
    with open(in_file_name, 'rb') as f:
        pcap_data = list(dpkt.pcap.Reader(f))

    pkts = []
    for i, (ts, buf) in enumerate(pcap_data):
        eth = dpkt.ethernet.Ethernet(buf)
        ip = eth.data
        udp = ip.data
        assert type(udp) is dpkt.udp.UDP
        pld = udp.data
        exceed = len(pld) - max_data_len
        if exceed > 0:
            if verbose:
                print('pkt %d pld size is %d, exceeding by %d' % (i, len(pld), exceed))
            ip.sum = 0
            udp.sum = 0
            udp.ulen -= exceed
            udp.data = udp.data[:-exceed]
            pkts.append(CPktWithTime(ts, bytes(eth)))
        else:
            pkts.append(CPktWithTime(ts, buf))

    with open(out_file_name, 'wb') as fo:
        pcap_out = dpkt.pcap.Writer(fo)
        for pkt in pkts:
            pcap_out.writepkt(pkt.pkt, pkt.ts)

