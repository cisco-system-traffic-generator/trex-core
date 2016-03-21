## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
General utility functions.
"""

import os,sys,socket,types
import random,time
import gzip,zlib
import re,struct,array,stat
import subprocess

import warnings
warnings.filterwarnings("ignore","tempnam",RuntimeWarning, __name__)

from .config import conf
from .data import MTU
from .error import log_runtime,log_loading,log_interactive, Scapy_Exception
from .base_classes import BasePacketList,BasePacket


WINDOWS=sys.platform.startswith("win32")

###########
## Tools ##
###########

def get_temp_file(keep=False, autoext=""):
    import tempfile
    fd, fname  = tempfile.mkstemp(suffix = ".scapy" + autoext)
    os.close(fd)
    if not keep:
        conf.temp_files.append(fname)
    return fname

def str2bytes(x):
  """Convert input argument to bytes"""
  if type(x) is bytes:
    return x
  elif type(x) is str:
    return bytes([ ord(i) for i in x ])
  else:
    return str2bytes(str(x))

def chb(x):
  if type(x) is str:
    return x
  else:
    return chr(x)

def orb(x):
  if type(x) is str:
    return ord(x)
  else:
    return x

def any2b(x):
  if type(x) is not str and type(x) is not bytes:
    try:
      x=bytes(x)
    except:
      x = str(x)
  if type(x) is str:
    x = bytes([ ord(i) for i in x ])
  return x

def sane_color(x):
    r=""
    for i in x:
        j = orb(i)
        if (j < 32) or (j >= 127):
            r=r+conf.color_theme.not_printable(".")
        else:
            r=r+chb(i)
    return r

def sane(x):
    r=""
    for i in x:
        if type(x) is str:
          j = ord(i)
        else:
          j = i
        if (j < 32) or (j >= 127):
            r=r+"."
        else:
            r=r+chb(i)
    return r

def lhex(x):
    if type(x) is int:
        return hex(x)
    elif type(x) is tuple:
        return "(%s)" % ", ".join(map(lhex, x))
    elif type(x) is list:
        return "[%s]" % ", ".join(map(lhex, x))
    else:
        return x

@conf.commands.register
def hexdump(x):
    if type(x) is not str and type(x) is not bytes:
      try:
        x=bytes(x)
      except:
        x = str(x)
    l = len(x)
    i = 0
    while i < l:
        print("%04x  " % i,end = " ")
        for j in range(16):
            if i+j < l:
                print("%02X" % orb(x[i+j]), end = " ")
            else:
                print("  ", end = " ")
            if j%16 == 7:
                print("", end = " ")
        print(" ", end = " ")
        print(sane_color(x[i:i+16]))
        i += 16

@conf.commands.register
def linehexdump(x, onlyasc=0, onlyhex=0):
    if type(x) is not str and type(x) is not bytes:
      try:
        x=bytes(x)
      except:
        x = str(x)
    l = len(x)
    if not onlyasc:
        for i in range(l):
            print("%02X" % orb(x[i]), end = " ")
        print("", end = " ")
    if not onlyhex:
        print(sane_color(x))

def chexdump(x):
    if type(x) is not str and type(x) is not bytes:
      try:
        x=bytes(x)
      except:
        x = str(x)
    print(", ".join(map(lambda x: "%#04x"%orb(x), x)))
    
def hexstr(x, onlyasc=0, onlyhex=0):
    s = []
    if not onlyasc:
        s.append(" ".join(map(lambda x:"%02x"%orb(x), x)))
    if not onlyhex:
        s.append(sane(x)) 
    return "  ".join(s)


@conf.commands.register
def hexdiff(x,y):
    """Show differences between 2 binary strings"""
    x=any2b(x)[::-1]
    y=any2b(y)[::-1]
    SUBST=1
    INSERT=1
    d={}
    d[-1,-1] = 0,(-1,-1)
    for j in range(len(y)):
        d[-1,j] = d[-1,j-1][0]+INSERT, (-1,j-1)
    for i in range(len(x)):
        d[i,-1] = d[i-1,-1][0]+INSERT, (i-1,-1)

    for j in range(len(y)):
        for i in range(len(x)):
            d[i,j] = min( ( d[i-1,j-1][0]+SUBST*(x[i] != y[j]), (i-1,j-1) ),
                          ( d[i-1,j][0]+INSERT, (i-1,j) ),
                          ( d[i,j-1][0]+INSERT, (i,j-1) ) )
                          

    backtrackx = []
    backtracky = []
    i=len(x)-1
    j=len(y)-1
    while not (i == j == -1):
        i2,j2 = d[i,j][1]
        backtrackx.append(x[i2+1:i+1])
        backtracky.append(y[j2+1:j+1])
        i,j = i2,j2

        

    x = y = i = 0
    colorize = { 0: lambda x:x,
                -1: conf.color_theme.left,
                 1: conf.color_theme.right }
    
    dox=1
    doy=0
    l = len(backtrackx)
    while i < l:
        separate=0
        linex = backtrackx[i:i+16]
        liney = backtracky[i:i+16]
        xx = sum(len(k) for k in linex)
        yy = sum(len(k) for k in liney)
        if dox and not xx:
            dox = 0
            doy = 1
        if dox and linex == liney:
            doy=1
            
        if dox:
            xd = y
            j = 0
            while not linex[j]:
                j += 1
                xd -= 1
            print(colorize[doy-dox]("%04x" % xd), end = " ")
            x += xx
            line=linex
        else:
            print("    ", end = " ")
        if doy:
            yd = y
            j = 0
            while not liney[j]:
                j += 1
                yd -= 1
            print(colorize[doy-dox]("%04x" % yd), end = " ")
            y += yy
            line=liney
        else:
            print("    ", end = " ")
            
        print(" ", end = " ")
        
        cl = ""
        for j in range(16):
            if i+j < l:
                if line[j]:
                    col = colorize[(linex[j]!=liney[j])*(doy-dox)]
                    print(col("%02X" % line[j][0]), end = " ")
                    if linex[j]==liney[j]:
                        cl += sane_color(line[j])
                    else:
                        cl += col(sane(line[j]))
                else:
                    print("  ", end = " ")
                    cl += " "
            else:
                print("  ", end = " ")
            if j == 7:
                print("", end = " ")


        print(" ",cl)

        if doy or not yy:
            doy=0
            dox=1
            i += 16
        else:
            if yy:
                dox=0
                doy=1
            else:
                i += 16

    
crc32 = zlib.crc32

if struct.pack("H",1) == b"\x00\x01": # big endian
    def checksum(pkt):
        if len(pkt) % 2 == 1:
            pkt += b"\0"
        s = sum(array.array("H", pkt))
        s = (s >> 16) + (s & 0xffff)
        s += s >> 16
        s = ~s
        return s & 0xffff
else:
    def checksum(pkt):
        if len(pkt) % 2 == 1:
            pkt += b"\0"
        s = sum(array.array("H", pkt))
        s = (s >> 16) + (s & 0xffff)
        s += s >> 16
        s = ~s
        return (((s>>8)&0xff)|s<<8) & 0xffff

def warning(x):
    log_runtime.warning(x)

def mac2str(mac):
    #return "".join(map(lambda x: chr(int(x,16)), mac.split(":")))
    if type(mac) != str:
      mac = mac.decode('ascii')
    return b''.join([ bytes([int(i, 16)]) for i in mac.split(":") ])

def str2mac(s):
    return ("%02x:"*6)[:-1] % tuple(s) 

def strxor(x,y):
    #return "".join(map(lambda i,j:chr(ord(i)^ord(j)),x,y))
    return bytes([ i[0] ^ i[1] for i in zip(x,y) ] )

# Workarround bug 643005 : https://sourceforge.net/tracker/?func=detail&atid=105470&aid=643005&group_id=5470
try:
    socket.inet_aton("255.255.255.255")
except socket.error:
    def inet_aton(x):
        if x == "255.255.255.255":
            return b"\xff"*4
        else:
            return socket.inet_aton(x)
else:
    inet_aton = socket.inet_aton

inet_ntoa = socket.inet_ntoa
try:
    inet_ntop = socket.inet_ntop
    inet_pton = socket.inet_pton
except AttributeError:
    from scapy.pton_ntop import *
    log_loading.info("inet_ntop/pton functions not found. Python IPv6 support not present")


def atol(x):
    try:
        ip = inet_aton(x)
    except socket.error:
        ip = inet_aton(socket.gethostbyname(x))
    return struct.unpack("!I", ip)[0]
def ltoa(x):
    return inet_ntoa(struct.pack("!I", x&0xffffffff))

def itom(x):
    return (0xffffffff00000000>>x)&0xffffffff

def do_graph(graph,prog=None,format='png',target=None,string=False,options=None, figsize = (12, 12), **kargs):
    """do_graph(graph, prog=conf.prog.dot, format="png",
         target=None, options=None, string=False):
    if networkx library is available and graph is instance of Graph, use networkx.draw

    string: if not False, simply return the graph string
    graph: GraphViz graph description
    format: output type (svg, ps, gif, jpg, etc.), passed to dot's "-T" option. Ignored if target==None
    target: filename. If None uses matplotlib to display
    prog: which graphviz program to use
    options: options to be passed to prog"""

    from scapy.arch import NETWORKX
    if NETWORKX:
        import networkx as nx

    if NETWORKX and isinstance(graph, nx.Graph):
        nx.draw(graph, with_labels = True, edge_color = '0.75', **kargs)
    else: # otherwise use dot as in scapy 2.x
        if string:
            return graph
        if prog is None:
            prog = conf.prog.dot

        if not target or not format:
            format = 'png'
        format = "-T %s" % format

        p = subprocess.Popen("%s %s %s" % (prog,options or "", format or ""), shell = True, stdin = subprocess.PIPE, stdout = subprocess.PIPE)
        w, r = p.stdin, p.stdout
        w.write(graph.encode('utf-8'))
        w.close()
        if target:
            with open(target, 'wb') as f:
                f.write(r.read())
        else:
            try:
                import matplotlib.image as mpimg
                import matplotlib.pyplot as plt
                plt.figure(figsize = figsize)                
                plt.axis('off')
                return plt.imshow(mpimg.imread(r, format = format), **kargs)

            except ImportError:
                warning('matplotlib.image required for interactive graph viewing. Use target option to write to a file')

_TEX_TR = {
    "{":"{\\tt\\char123}",
    "}":"{\\tt\\char125}",
    "\\":"{\\tt\\char92}",
    "^":"\\^{}",
    "$":"\\$",
    "#":"\\#",
    "~":"\\~",
    "_":"\\_",
    "&":"\\&",
    "%":"\\%",
    "|":"{\\tt\\char124}",
    "~":"{\\tt\\char126}",
    "<":"{\\tt\\char60}",
    ">":"{\\tt\\char62}",
    }
    
def tex_escape(x):
    s = ""
    for c in x:
        s += _TEX_TR.get(c,c)
    return s

def colgen(*lstcol,**kargs):
    """Returns a generator that mixes provided quantities forever
    trans: a function to convert the three arguments into a color. lambda x,y,z:(x,y,z) by default"""
    if len(lstcol) < 2:
        lstcol *= 2
    trans = kargs.get("trans", lambda x,y,z: (x,y,z))
    while 1:
        for i in range(len(lstcol)):
            for j in range(len(lstcol)):
                for k in range(len(lstcol)):
                    if i != j or j != k or k != i:
                        yield trans(lstcol[(i+j)%len(lstcol)],lstcol[(j+k)%len(lstcol)],lstcol[(k+i)%len(lstcol)])

def incremental_label(label="tag%05i", start=0):
    while True:
        yield label % start
        start += 1

#########################
#### Enum management ####
#########################

class EnumElement:
    _value=None
    def __init__(self, key, value):
        self._key = key
        self._value = value
    def __repr__(self):
        return "<%s %s[%r]>" % (self.__dict__.get("_name", self.__class__.__name__), self._key, self._value)
    def __getattr__(self, attr):
        return getattr(self._value, attr)
    def __str__(self):
        return self._key
    def __eq__(self, other):
        #return self._value == int(other)
        return self._value == hash(other)
    def __hash__(self):
        return self._value


class Enum_metaclass(type):
    element_class = EnumElement
    def __new__(cls, name, bases, dct):
        rdict={}
        for k,v in dct.items():
            if type(v) is int:
                v = cls.element_class(k,v)
                dct[k] = v
                rdict[v] = k
        dct["__rdict__"] = rdict
        return super(Enum_metaclass, cls).__new__(cls, name, bases, dct)
    def __getitem__(self, attr):
        return self.__rdict__[attr]
    def __contains__(self, val):
        return val in self.__rdict__
    def get(self, attr, val=None):
        return self._rdict__.get(attr, val)
    def __repr__(self):
        return "<%s>" % self.__dict__.get("name", self.__name__)



###################
## Object saving ##
###################


def export_object(obj):
    import dill as pickle
    import base64
    return base64.b64encode(gzip.zlib.compress(pickle.dumps(obj,4),9)).decode('utf-8')


def import_object(obj):
    import dill as pickle
    import base64
#    if obj is None:
#        obj = sys.stdin.read().strip().encode('utf-8')
    if obj is str:
        obj = obj.strip().encode('utf-8')
    return pickle.loads(gzip.zlib.decompress(base64.b64decode(obj)))


def save_object(fname, obj):
    import dill as pickle
    pickle.dump(obj,gzip.open(fname,"wb"))

def load_object(fname):
    import dill as pickle
    return pickle.load(gzip.open(fname,"rb"))

@conf.commands.register
def corrupt_bytes(s, p=0.01, n=None):
    """Corrupt a given percentage or number of bytes from bytes"""
    s = str2bytes(s)
    s = array.array("B",s)
    l = len(s)
    if n is None:
        n = max(1,int(l*p))
    for i in random.sample(range(l), n):
        s[i] = (s[i]+random.randint(1,255))%256
    return s.tobytes()

@conf.commands.register
def corrupt_bits(s, p=0.01, n=None):
    """Flip a given percentage or number of bits from bytes"""
    s = str2bytes(s)
    s = array.array("B",s)
    l = len(s)*8
    if n is None:
        n = max(1, int(l*p))
    for i in random.sample(range(l), n):
        s[i//8] ^= 1 << (i%8)
    return s.tobytes()


#############################
## pcap capture file stuff ##
#############################

@conf.commands.register
def wrpcap(filename, pkt, *args, **kargs):
    """Write a list of packets to a pcap file
gz: set to 1 to save a gzipped capture
linktype: force linktype value
endianness: "<" or ">", force endianness"""
    with PcapWriter(filename, *args, **kargs) as pcap:
        pcap.write(pkt)

@conf.commands.register
def rdpcap(filename, count=-1):
    """Read a pcap file and return a packet list
count: read only <count> packets"""
    with PcapReader(filename) as pcap:
        return pcap.read_all(count=count)

class RawPcapReader:
    """A stateful pcap reader. Each packet is returned as bytes"""
    def __init__(self, filename):
        self.filename = filename
        try:
            if not stat.S_ISREG(os.stat(filename).st_mode):
              raise IOError("GZIP detection works only for regular files")
            self.f = gzip.open(filename,"rb")
            magic = self.f.read(4)
        except IOError:
            self.f = open(filename,"rb")
            magic = self.f.read(4)
        if magic == b"\xa1\xb2\xc3\xd4": #big endian
            self.endian = ">"
            self.reader = _RawPcapOldReader(self.f, self.endian)
        elif magic == b"\xd4\xc3\xb2\xa1": #little endian
            self.endian = "<"
            self.reader = _RawPcapOldReader(self.f, self.endian)
        elif magic == b"\x0a\x0d\x0d\x0a": #PcapNG
            self.reader = _RawPcapNGReader(self.f) 
        else:
            raise Scapy_Exception("Not a pcap capture file (bad magic)")

    def __enter__(self):
        return self.reader

    def __exit__(self, exc_type, exc_value, tracback):
        self.close()

    def __iter__(self):
        return self.reader.__iter__()

    def dispatch(self, callback):
        """call the specified callback routine for each packet read
        
        This is just a convienience function for the main loop
        that allows for easy launching of packet processing in a 
        thread.
        """
        for p in self:
            callback(p)

    def read_all(self,count=-1):
        """return a list of all packets in the pcap file
        """
        res=[]
        while count != 0:
            count -= 1
            p = self.read_packet()
            if p is None:
                break
            res.append(p)
        return res

    def recv(self, size=MTU):
        """ Emulate a socket
        """
        return self.read_packet(size)[0]

    def fileno(self):
        return self.f.fileno()

    def close(self):
        return self.f.close()

    def read_packet(self, size = MTU):
        return self.reader.read_packet(size)

def align32(n):
  return n + (4 - n % 4) % 4

class _RawPcapNGReader:
    def __init__(self, filep):
        self.filep = filep
        self.filep.seek(0, 0)
        self.endian = '<'
        self.tsresol = 6
        self.linktype = None

    def __iter__(self):
        return self

    def __next__(self):
        """implement the iterator protocol on a set of packets in a pcapng file"""
        pkt = self.read_packet()
        if pkt == None:
            raise StopIteration
        return pkt

    def read_packet(self, size = MTU):
        while True:
            buf = self._read_bytes(4, check = False)
            if len(buf) == 0:
                return None
            elif len(buf) != 4:
                raise IOError("PacketNGReader: Premature end of file")
            block_type, = struct.unpack(self.endian + 'i', buf)
            if block_type == 168627466: #Section Header b'\x0a\x0d\x0d\x0a'
                self.read_section_header()
            elif block_type == 1:
                self.read_interface_description()
            elif block_type == 6:
                return self.read_enhanced_packet(size)
            else:
                warning("PacketNGReader: Unparsed block type %d/#%x" % (block_type, block_type))
                self.read_generic_block()

    def _read_bytes(self, n, check = True):
        buf = self.filep.read(n)
        if check and len(buf) < n:
            raise IOError("PacketNGReader: Premature end of file")
        return buf

    def read_generic_block(self):
        block_length, = struct.unpack(self.endian + 'I', self._read_bytes(4))
        self._read_bytes(block_length - 12)
        self._check_length(block_length)

    def read_section_header(self):
        buf = self._read_bytes(16)
        if buf[4:8] == b'\x1a\x2b\x3c\x4d':
            self.endian = '>'
        elif buf[4:8] == b'\x4d\x3c\x2b\x1a':
            self.endian = '<'
        else:
            raise Scapy_Exception('Cannot read byte order value')  
        block_length, _, major_version, minor_version, section_length = struct.unpack(self.endian + 'IIHHi', buf)
        options = self._read_bytes(block_length - 24)
        if options:
            opt = self.parse_options(options)
            for i in opt.keys():
                if not i & (0b1 << 15):
                    warning("PcapNGReader: Unparsed option %d/#%x in section header" % (i, i))
        self._check_length(block_length)

    def read_interface_description(self):
        buf = self._read_bytes(12)
        block_length, self.linktype, reserved, self.snaplen = struct.unpack(self.endian + 'IHHI', buf)
        options = self._read_bytes(block_length - 20)
        if options:
            opt = self.parse_options(options)
            for i in opt.keys():
                if 9 in opt:
                    self.tsresol = opt[9][0]
                elif not i & (0b1 << 15):
                    warning("PcapNGReader: Unparsed option %d/#%x in enhanced packet block" % (i, i))
        try:
            self.LLcls = conf.l2types[self.linktype]
        except KeyError:
            warning("RawPcapReader: unknown LL type [%i]/[%#x]. Using Raw packets" % (self.linktype,self.linktype))
            self.LLcls = conf.raw_layer

        self._check_length(block_length)

    def read_enhanced_packet(self, size = MTU):
        buf = self._read_bytes(24)
        block_length, interface, ts_high, ts_low, caplen, wirelen = struct.unpack(self.endian + 'IIIIII', buf)
        timestamp = (ts_high << 32) + ts_low

        pkt = self._read_bytes(align32(caplen))[:caplen]
        options = self._read_bytes(block_length - align32(caplen) - 32)
        if options:
            opt = self.parse_options(options)
            for i in opt.keys():
                if not i & (0b1 << 15):
                    warning("PcapNGReader: Unparsed option %d/#%x in enhanced packet block" % (i, i))
        self._check_length(block_length)
        return pkt[:MTU], (self.parse_sec(timestamp), self.parse_usec(timestamp), wirelen)
    
    def parse_sec(self, t):
        if self.tsresol & 0b10000000:
            return t >> self.tsresol
        else:
            return t // pow(10, self.tsresol)

    def parse_usec(self, t):
        if self.tsresol & 0b10000000:
            return t & (1 << self.tsresol) - 1
        else:
            return t % pow(10, self.tsresol)

    def parse_options(self, opt):
        buf = opt
        options = {}
        while buf:
            opt_type, opt_len = struct.unpack(self.endian + 'HH', buf[:4])
            if opt_type == 0:
                return options
            options[opt_type] = buf[4:4 + opt_len]
            buf = buf[ 4 + align32(opt_len):]
        return options

    def _check_length(self, block_length):
        check_length, = struct.unpack(self.endian + 'I', self._read_bytes(4))
        if check_length != block_length:
            raise Scapy_Exception('Block length values are not equal')

class _RawPcapOldReader:
    def __init__(self, filep, endianness):
        self.endian = endianness
        self.f = filep
        hdr = self.f.read(20)
        if len(hdr)<20:
            raise Scapy_Exception("Invalid pcap file (too short)")
        vermaj,vermin,tz,sig,snaplen,linktype = struct.unpack(self.endian+"HHIIII",hdr)

        self.linktype = linktype
        try:
            self.LLcls = conf.l2types[self.linktype]
        except KeyError:
            warning("RawPcapReader: unknown LL type [%i]/[%#x]. Using Raw packets" % (self.linktype,self.linktype))
            self.LLcls = conf.raw_layer

    def __iter__(self):
        return self

    def __next__(self):
        """implement the iterator protocol on a set of packets in a pcap file"""
        pkt = self.read_packet()
        if pkt == None:
            raise StopIteration
        return pkt

    def read_packet(self, size=MTU):
        """return a single packet read from the file
        bytes, (sec, #timestamp seconds
                usec, #timestamp microseconds
                wirelen) #actual length of packet 
        returns None when no more packets are available
        """
        hdr = self.f.read(16)
        if len(hdr) < 16:
            return None
        sec,usec,caplen,wirelen = struct.unpack(self.endian+"IIII", hdr)
        s = self.f.read(caplen)[:MTU]
        return s,(sec,usec,wirelen) # caplen = len(s)


class PcapReader(RawPcapReader):
    def __init__(self, filename):
        RawPcapReader.__init__(self, filename)
    def __enter__(self):
        return self
    def __iter__(self):
        return self
    def __next__(self):
        """implement the iterator protocol on a set of packets in a pcap file"""
        pkt = self.read_packet()
        if pkt == None:
            raise StopIteration
        return pkt
    def read_packet(self, size=MTU):
        rp = RawPcapReader.read_packet(self,size)
        if rp is None:
            return None
        s,(sec,usec,wirelen) = rp

        
        try:
            p = self.reader.LLcls(s)
        except KeyboardInterrupt:
            raise
        except:
            if conf.debug_dissector:
                raise
            p = conf.raw_layer(s)
        p.time = sec+0.000001*usec
        p.wirelen = wirelen
        return p

    def read_all(self,count=-1):
        res = RawPcapReader.read_all(self, count)
        import scapy.plist
        return scapy.plist.PacketList(res,name = os.path.basename(self.filename))
    def recv(self, size=MTU):
        return self.read_packet(size)


class RawPcapWriter:
    """A stream PCAP writer with more control than wrpcap()"""
    def __init__(self, filename, linktype=None, gz=False, endianness="", append=False, sync=False):
        """
        linktype: force linktype to a given value. If None, linktype is taken
                  from the first writter packet
        gz: compress the capture on the fly
        endianness: force an endianness (little:"<", big:">"). Default is native
        append: append packets to the capture file instead of truncating it
        sync: do not bufferize writes to the capture file
        """
        
        self.linktype = linktype
        self.header_present = 0
        self.append=append
        self.gz = gz
        self.endian = endianness
        self.filename=filename
        self.sync=sync
        bufsz=4096
        if sync:
            bufsz=0

        self.f = [open,gzip.open][gz](filename,append and "ab" or "wb", gz and 9 or bufsz)
        
    def fileno(self):
        return self.f.fileno()

    def _write_header(self, pkt):
        self.header_present=1

        if self.append:
            # Even if prone to race conditions, this seems to be
            # safest way to tell whether the header is already present
            # because we have to handle compressed streams that
            # are not as flexible as basic files
            g = [open,gzip.open][self.gz](self.filename,"rb")
            if g.read(16):
                return
            
        self.f.write(struct.pack(self.endian+"IHHIIII", 0xa1b2c3d4,
                                 2, 4, 0, 0, MTU, self.linktype))
        self.f.flush()
    

    def write(self, pkt):
        """accepts a either a single packet or a list of packets
        to be written to the dumpfile
        """
        if not self.header_present:
            self._write_header(pkt)
        if type(pkt) is bytes:
            self._write_packet(pkt)
        else:
            for p in pkt:
                self._write_packet(p)

    def _write_packet(self, packet, sec=None, usec=None, caplen=None, wirelen=None):
        """writes a single packet to the pcap file
        """
        if caplen is None:
            caplen = len(packet)
        if wirelen is None:
            wirelen = caplen
        if sec is None or usec is None:
            t=time.time()
            it = int(t)
            if sec is None:
                sec = it
            if usec is None:
                usec = int(round((t-it)*1000000))
        self.f.write(struct.pack(self.endian+"IIII", sec, usec, caplen, wirelen))
        self.f.write(packet)
        if self.gz and self.sync:
            self.f.flush()

    def flush(self):
        return self.f.flush()

    def close(self):
        return self.f.close()

    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_value, tracback):
        self.flush()
        self.close()


class PcapWriter(RawPcapWriter):
    def _write_header(self, pkt):
        if self.linktype == None:
            if type(pkt) is list or type(pkt) is tuple or isinstance(pkt,BasePacketList):
                pkt = pkt[0]
            try:
                self.linktype = conf.l2types[pkt.__class__]
            except KeyError:
                warning("PcapWriter: unknown LL type for %s. Using type 1 (Ethernet)" % pkt.__class__.__name__)
                self.linktype = 1
        RawPcapWriter._write_header(self, pkt)

    def _write_packet(self, packet):        
        try:
          sec = int(packet.time)
          usec = int(round((packet.time-sec)*1000000))
          s = bytes(packet)
          caplen = len(s)
          RawPcapWriter._write_packet(self, s, sec, usec, caplen, caplen)
        except Exception as e:
          log_interactive.error(e)
    def write(self, pkt):
        """accepts a either a single packet or a list of packets
        to be written to the dumpfile
        """
        if not self.header_present:
            self._write_header(pkt)
        if isinstance(pkt, BasePacket):
          self._write_packet(pkt)
        else:
            for p in pkt:
                self._write_packet(p)


re_extract_hexcap = re.compile("^((0x)?[0-9a-fA-F]{2,}[ :\t]{,3}|) *(([0-9a-fA-F]{2} {,2}){,16})")

def import_hexcap():
    p = ""
    try:
        while 1:
            l = raw_input().strip()
            try:
                p += re_extract_hexcap.match(l).groups()[2]
            except:
                warning("Parsing error during hexcap")
                continue
    except EOFError:
        pass
    
    p = p.replace(" ","")
    return p.decode("hex")
        


@conf.commands.register
def wireshark(pktlist, *args):
    """Run wireshark on a list of packets"""
    fname = get_temp_file()
    wrpcap(fname, pktlist)
    subprocess.Popen([conf.prog.wireshark, "-r", fname] + list(args))

@conf.commands.register
def tdecode(pkt, *args):
    """Run tshark to decode and display the packet. If no args defined uses -V"""
    if not args:
      args = [ "-V" ]
    fname = get_temp_file()
    wrpcap(fname,[pkt])
    subprocess.call(["tshark", "-r", fname] + list(args))

@conf.commands.register
def hexedit(x):
    """Run external hex editor on a packet or bytes. Set editor in conf.prog.hexedit"""
    x = bytes(x)
    fname = get_temp_file()
    with open(fname,"wb") as f:
      f.write(x)
    subprocess.call([conf.prog.hexedit, fname])
    with open(fname, "rb") as f:
      x = f.read()
    return x

def __make_table(yfmtfunc, fmtfunc, endline, items, fxyz, sortx=None, sorty=None, seplinefunc=None):
    vx = {} 
    vy = {} 
    vz = {}
    vxf = {}
    vyf = {}
    max_length = 0
    for record in items:
        xx,yy,zz = map(str, fxyz(record[0], record[1]))
        max_length = max(len(yy),max_length)
        vx[xx] = max(vx.get(xx,0), len(xx), len(zz))
        vy[yy] = None
        vz[(xx,yy)] = zz

    vxk = list(vx.keys())
    vyk = list(vy.keys())
    if sortx:
        vxk.sort(sortx)
    else:
        try:
            vxk.sort(key = lambda x: atol(x))
        except:
            vxk.sort()
    if sorty:
        vyk.sort(sorty)
    else:
        try:
            vyk.sort(key = lambda x: atol(x))
        except:
            vyk.sort()


    if seplinefunc:
        sepline = seplinefunc(max_length, [vx[x] for x in vxk])
        print(sepline)

    fmt = yfmtfunc(max_length)
    print(fmt % "", end = " ")
    for x in vxk:
        vxf[x] = fmtfunc(vx[x])
        print(vxf[x] % x, end = " ")
    print(endline)
    if seplinefunc:
        print(sepline)
    for y in vyk:
        print(fmt % y, end = " ")
        for x in vxk:
            print(vxf[x] % vz.get((x,y), "-"), end = " ")
        print(endline)
    if seplinefunc:
        print(sepline)

def make_table(*args, **kargs):
    __make_table(lambda l:"%%-%is" % l, lambda l:"%%-%is" % l, "", *args, **kargs)
    
def make_lined_table(*args, **kargs):
    __make_table(lambda l:"%%-%is |" % l, lambda l:"%%-%is |" % l, "",
                 seplinefunc=lambda max_length,x:"+".join([ "-"*(y+2) for y in [max_length-1]+x+[-2]]),
                 *args, **kargs)

def make_tex_table(*args, **kargs):
    __make_table(lambda l: "%s", lambda l: "& %s", "\\\\", seplinefunc=lambda a,x:"\\hline", *args, **kargs)

