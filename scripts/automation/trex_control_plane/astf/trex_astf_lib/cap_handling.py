from .trex_astf_exceptions import ASTFError
import dpkt
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
            return;
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
        pkt_num = 0
        pcap = None
        with open(self.file_name, 'rb') as f:
            pcap = dpkt.pcap.Reader(f)
            if not pcap:
               self.fail('Empty pcap {0}'.format(self.file_name))

            l4_type = None
            last_time=None;
            index =0 ;
            for (time,buf) in pcap:
                dtime= time
                pkt_time=0.0; # time from last pkt
                if last_time == None:
                    pkt_time = 0.0;
                else:
                    pkt_time=dtime-last_time;
                last_time = dtime
    
                pkt_num += 1

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
                                      .format(pkt_num, tcp.seq, exp_c_seq))
                        exp_c_seq = tcp.seq + l4_payload_len
                    else:
                        if exp_s_seq != tcp.seq:
                            self.fail("""TCP seq in packet {0} is {1}. We expected {2}. Please check that there are
                            no packet loss or retransmission in cap file"""
                                      .format(pkt_num, tcp.seq, exp_s_seq))
                        exp_s_seq = tcp.seq + l4_payload_len
                index = index +1
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
def  pcap_reader(file_name):
    obj = _CPcapReader(file_name)
    return (obj)


