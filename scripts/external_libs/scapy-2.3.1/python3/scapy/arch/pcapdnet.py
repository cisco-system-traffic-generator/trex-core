## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
Packet sending and receiving with libdnet and libpcap/WinPcap.
"""

import time,struct,sys,socket
if not sys.platform.startswith("win"):
    from fcntl import ioctl
from scapy.data import *
from scapy.config import conf
from scapy.utils import warning
from scapy.supersocket import SuperSocket
from scapy.error import Scapy_Exception
import scapy.arch

if conf.use_dnet:
  try:
    from .cdnet import *
  except OSError as e:
    if conf.interactive:
      log_loading.error("Unable to import libdnet library: %s" % e)
      conf.use_dnet = False
    else:
      raise

if conf.use_winpcapy:
  try:
    from .winpcapy import *
    def winpcapy_get_if_list():
      err = create_string_buffer(PCAP_ERRBUF_SIZE)
      devs = POINTER(pcap_if_t)()
      ret = []
      if pcap_findalldevs(byref(devs), err) < 0:
        return ret
      try:
        p = devs
        while p:
          ret.append(p.contents.name.decode('ascii'))
          p = p.contents.next
        return ret
      finally:
        pcap_freealldevs(devs)

  except OSError as e:
    if conf.interactive:
      log_loading.error("Unable to import libpcap library: %s" % e)
      conf.use_winpcapy = False
    else:
      raise

  # From BSD net/bpf.h
  #BIOCIMMEDIATE=0x80044270
  BIOCIMMEDIATE=-2147204496

  class PcapTimeoutElapsed(Scapy_Exception):
      pass
    
if conf.use_netifaces:
  try:
    import netifaces
  except ImportError as e:
    log_loading.warning("Could not load module netifaces: %s" % e)
    conf.use_netifaces = False

if conf.use_netifaces:
  def get_if_raw_hwaddr(iff):
      if iff == scapy.arch.LOOPBACK_NAME:
          return (772, '\x00'*6)
      try:
          s = netifaces.ifaddresses(iff)[netifaces.AF_LINK][0]['addr']
          return struct.pack('BBBBBB', *[ int(i, 16) for i in s.split(':') ])
      except:
          raise Scapy_Exception("Error in attempting to get hw address for interface [%s]" % iff)
      return l
  def get_if_raw_addr(ifname):
      try:
        s = netifaces.ifaddresses(ifname)[netifaces.AF_INET][0]['addr']
        return socket.inet_aton(s)
      except Exception as e:
        return None
  def get_if_list():
      #return [ i[1] for i in socket.if_nameindex() ]
      return netifaces.interfaces()
  def in6_getifaddr():
      """
      Returns a list of 3-tuples of the form (addr, scope, iface) where
      'addr' is the address of scope 'scope' associated to the interface
      'ifcace'.

      This is the list of all addresses of all interfaces available on
      the system.
      """

      ret = []
      interfaces = get_if_list()
      for i in interfaces:
        addrs = netifaces.ifaddresses(i)
        if netifaces.AF_INET6 not in addrs:
          continue
        for a in addrs[netifaces.AF_INET6]:
          addr = a['addr'].split('%')[0]
          scope = scapy.utils6.in6_getscope(addr)
          ret.append((addr, scope, i))
      return ret
elif conf.use_winpcapy:
  def get_if_raw_hwaddr(iff):
    err = create_string_buffer(PCAP_ERRBUF_SIZE)
    devs = POINTER(pcap_if_t)()
    ret = b"\0\0\0\0\0\0"
    if pcap_findalldevs(byref(devs), err) < 0:
      return ret
    try:
      p = devs
      while p:
        if p.contents.name.endswith(iff.encode('ascii')):
          a = p.contents.addresses
          while a:
            if hasattr(socket, 'AF_LINK') and a.contents.addr.contents.sa_family == socket.AF_LINK:
              ap = a.contents.addr
              val = cast(ap, POINTER(sockaddr_dl))
              ret = bytes(val.contents.sdl_data[ val.contents.sdl_nlen : val.contents.sdl_nlen + val.contents.sdl_alen ])
            a = a.contents.next
          break
        p = p.contents.next
      return ret
    finally:
      pcap_freealldevs(devs)
  def get_if_raw_addr(iff):
    err = create_string_buffer(PCAP_ERRBUF_SIZE)
    devs = POINTER(pcap_if_t)()
    ret = b"\0\0\0\0"
    if pcap_findalldevs(byref(devs), err) < 0:
      return ret
    try:
      p = devs
      while p:
        if p.contents.name.endswith(iff.encode('ascii')):
          a = p.contents.addresses
          while a:
            if a.contents.addr.contents.sa_family == socket.AF_INET:
              ap = a.contents.addr
              val = cast(ap, POINTER(sockaddr_in))
              ret = bytes(val.contents.sin_addr[:4])
            a = a.contents.next
          break
        p = p.contents.next
      return ret
    finally:
      pcap_freealldevs(devs)
  get_if_list = winpcapy_get_if_list
  def in6_getifaddr():
    err = create_string_buffer(PCAP_ERRBUF_SIZE)
    devs = POINTER(pcap_if_t)()
    ret = []
    if pcap_findalldevs(byref(devs), err) < 0:
      return ret
    try:
      p = devs
      ret = []
      while p:
        a = p.contents.addresses
        while a:
          if a.contents.addr.contents.sa_family == socket.AF_INET6:
            ap = a.contents.addr
            val = cast(ap, POINTER(sockaddr_in6))
            addr = socket.inet_ntop(socket.AF_INET6, bytes(val.contents.sin6_addr[:]))
            scope = scapy.utils6.in6_getscope(addr)
            ret.append((addr, scope, p.contents.name.decode('ascii')))
          a = a.contents.next
        p = p.contents.next
      return ret
    finally:
      pcap_freealldevs(devs)
    
elif conf.use_dnet:
  intf = dnet_intf()
  def get_if_raw_hwaddr(iff):
      return intf.get(iff)['link_addr']
  def get_if_raw_addr(iff):
      return intf.get(iff)['addr']
  def get_if_list():
      return intf.names
  def in6_getifaddr():
      ret = []
      for i in get_if_list():
        for a in intf.get(i)['addr6']:
          addr = socket.inet_ntop(socket.AF_INET6, a)
          scope = scapy.utils6.in6_getscope(addr)
          ret.append((addr, scope, i))
      return ret

else:
  log_loading.warning("No known method to get ip and hw address for interfaces")
  def get_if_raw_hwaddr(iff):
      "dummy"
      return b"\0\0\0\0\0\0"
  def get_if_raw_addr(iff):
      "dummy"
      return b"\0\0\0\0"
  def get_if_list():
      "dummy"
      return []
  def in6_getifaddr():
      return []

if conf.use_winpcapy:
  from ctypes import POINTER, byref, create_string_buffer
  class _PcapWrapper_pypcap:
      def __init__(self, device, snaplen, promisc, to_ms):
          self.errbuf = create_string_buffer(PCAP_ERRBUF_SIZE)
          self.iface = create_string_buffer(device.encode('ascii'))
          self.pcap = pcap_open_live(self.iface, snaplen, promisc, to_ms, self.errbuf)
          self.header = POINTER(pcap_pkthdr)()
          self.pkt_data = POINTER(c_ubyte)()
          self.bpf_program = bpf_program()
      def next(self):
          c = pcap_next_ex(self.pcap, byref(self.header), byref(self.pkt_data))
          if not c > 0:
              return
          ts = self.header.contents.ts.tv_sec
          #pkt = "".join([ chr(i) for i in self.pkt_data[:self.header.contents.len] ])
          pkt = bytes(self.pkt_data[:self.header.contents.len])
          return ts, pkt
      def datalink(self):
          return pcap_datalink(self.pcap)
      def fileno(self):
          if sys.platform.startswith("win"):
            error("Cannot get selectable PCAP fd on Windows")
            return 0
          return pcap_get_selectable_fd(self.pcap) 
      def setfilter(self, f):
          filter_exp = create_string_buffer(f.encode('ascii'))
          if pcap_compile(self.pcap, byref(self.bpf_program), filter_exp, 0, -1) == -1:
            error("Could not compile filter expression %s" % f)
            return False
          else:
            if pcap_setfilter(self.pcap, byref(self.bpf_program)) == -1:
              error("Could not install filter %s" % f)
              return False
          return True
      def setnonblock(self, i):
          pcap_setnonblock(self.pcap, i, self.errbuf)
      def send(self, x):
          pcap_sendpacket(self.pcap, x, len(x))
      def close(self):
          pcap_close(self.pcap)
  open_pcap = lambda *args,**kargs: _PcapWrapper_pypcap(*args,**kargs)
  class PcapTimeoutElapsed(Scapy_Exception):
      pass

  class L2pcapListenSocket(SuperSocket):
      desc = "read packets at layer 2 using libpcap"
      def __init__(self, iface = None, type = ETH_P_ALL, promisc=None, filter=None):
          self.type = type
          self.outs = None
          self.iface = iface
          if iface is None:
              iface = conf.iface
          if promisc is None:
              promisc = conf.sniff_promisc
          self.promisc = promisc
          self.ins = open_pcap(iface, 1600, self.promisc, 100)
          try:
              ioctl(self.ins.fileno(),BIOCIMMEDIATE,struct.pack("I",1))
          except:
              pass
          if type == ETH_P_ALL: # Do not apply any filter if Ethernet type is given
              if conf.except_filter:
                  if filter:
                      filter = "(%s) and not (%s)" % (filter, conf.except_filter)
                  else:
                      filter = "not (%s)" % conf.except_filter
              if filter:
                  self.ins.setfilter(filter)
  
      def close(self):
          self.ins.close()
          
      def recv(self, x=MTU):
          ll = self.ins.datalink()
          if ll in conf.l2types:
              cls = conf.l2types[ll]
          else:
              cls = conf.default_l2
              warning("Unable to guess datalink type (interface=%s linktype=%i). Using %s" % (self.iface, ll, cls.name))
  
          pkt = None
          while pkt is None:
              pkt = self.ins.next()
              if pkt is not None:
                  ts,pkt = pkt
              if scapy.arch.WINDOWS and pkt is None:
                  raise PcapTimeoutElapsed
          
          try:
              pkt = cls(pkt)
          except KeyboardInterrupt:
              raise
          except:
              if conf.debug_dissector:
                  raise
              pkt = conf.raw_layer(pkt)
          pkt.time = ts
          return pkt
  
      def send(self, x):
          raise Scapy_Exception("Can't send anything with L2pcapListenSocket")
  

  conf.L2listen = L2pcapListenSocket
  class L2pcapSocket(SuperSocket):
      desc = "read/write packets at layer 2 using only libpcap"
      def __init__(self, iface = None, type = ETH_P_ALL, filter=None, nofilter=0):
          if iface is None:
              iface = conf.iface
          self.iface = iface
          self.ins = open_pcap(iface, 1600, 0, 100)
          try:
              ioctl(self.ins.fileno(),BIOCIMMEDIATE,struct.pack("I",1))
          except:
              pass
          if nofilter:
              if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                  filter = "ether proto %i" % type
              else:
                  filter = None
          else:
              if conf.except_filter:
                  if filter:
                      filter = "(%s) and not (%s)" % (filter, conf.except_filter)
                  else:
                      filter = "not (%s)" % conf.except_filter
              if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                  if filter:
                      filter = "(ether proto %i) and (%s)" % (type,filter)
                  else:
                      filter = "ether proto %i" % type
          if filter:
              self.ins.setfilter(filter)
      def send(self, x):
          sx = bytes(x)
          if hasattr(x, "sent_time"):
              x.sent_time = time.time()
          return self.ins.send(sx)

      def recv(self,x=MTU):
          ll = self.ins.datalink()
          if ll in conf.l2types:
              cls = conf.l2types[ll]
          else:
              cls = conf.default_l2
              warning("Unable to guess datalink type (interface=%s linktype=%i). Using %s" % (self.iface, ll, cls.name))
  
          pkt = self.ins.next()
          if pkt is not None:
              ts,pkt = pkt
          if pkt is None:
              return
          
          try:
              pkt = cls(pkt)
          except KeyboardInterrupt:
              raise
          except:
              if conf.debug_dissector:
                  raise
              pkt = conf.raw_layer(pkt)
          pkt.time = ts
          return pkt
  
      def nonblock_recv(self):
          self.ins.setnonblock(1)
          p = self.recv(MTU)
          self.ins.setnonblock(0)
          return p
  
      def close(self):
          if hasattr(self, "ins"):
              self.ins.close()
          if hasattr(self, "outs"):
              self.outs.close()

  class L3pcapSocket(L2pcapSocket):
      #def __init__(self, iface = None, type = ETH_P_ALL, filter=None, nofilter=0):
      #    L2pcapSocket.__init__(self, iface, type, filter, nofilter)
      def recv(self, x = MTU):
          r = L2pcapSocket.recv(self, x) 
          if r:
            return r.payload
          else:
            return
      def send(self, x):
          cls = conf.l2types[1]
          sx = bytes(cls()/x)
          if hasattr(x, "sent_time"):
              x.sent_time = time.time()
          return self.ins.send(sx)
  conf.L2socket=L2pcapSocket
  conf.L3socket=L3pcapSocket
    
if conf.use_winpcapy and conf.use_dnet:
    class L3dnetSocket(SuperSocket):
        desc = "read/write packets at layer 3 using libdnet and libpcap"
        def __init__(self, type = ETH_P_ALL, filter=None, promisc=None, iface=None, nofilter=0):
            self.iflist = {}
            self.intf = dnet_intf()
            if iface is None:
                iface = conf.iface
            self.iface = iface
            self.ins = open_pcap(iface, 1600, 0, 100)
            try:
                ioctl(self.ins.fileno(),BIOCIMMEDIATE,struct.pack("I",1))
            except:
                pass
            if nofilter:
                if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                    filter = "ether proto %i" % type
                else:
                    filter = None
            else:
                if conf.except_filter:
                    if filter:
                        filter = "(%s) and not (%s)" % (filter, conf.except_filter)
                    else:
                        filter = "not (%s)" % conf.except_filter
                if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                    if filter:
                        filter = "(ether proto %i) and (%s)" % (type,filter)
                    else:
                        filter = "ether proto %i" % type
            if filter:
                self.ins.setfilter(filter)
        def send(self, x):
            iff,a,gw  = x.route()
            if iff is None:
                iff = conf.iface
            ifs,cls = self.iflist.get(iff,(None,None))
            if ifs is None:
                iftype = self.intf.get(iff)["type"]
                if iftype == INTF_TYPE_ETH:
                    try:
                        cls = conf.l2types[1]
                    except KeyError:
                        warning("Unable to find Ethernet class. Using nothing")
                    ifs = dnet_eth(iff)
                else:
                    ifs = dnet_ip()
                self.iflist[iff] = ifs,cls
            if cls is None:
                #sx = str(x)
                sx = bytes(x)
            else:
                sx = bytes(cls()/x)
            x.sent_time = time.time()
            ifs.send(sx)
        def recv(self,x=MTU):
            ll = self.ins.datalink()
            if ll in conf.l2types:
                cls = conf.l2types[ll]
            else:
                cls = conf.default_l2
                warning("Unable to guess datalink type (interface=%s linktype=%i). Using %s" % (self.iface, ll, cls.name))
    
            pkt = self.ins.next()
            if pkt is not None:
                ts,pkt = pkt
            if pkt is None:
                return
    
            try:
                pkt = cls(pkt)
            except KeyboardInterrupt:
                raise
            except:
                if conf.debug_dissector:
                    raise
                pkt = conf.raw_layer(pkt)
            pkt.time = ts
            return pkt.payload
    
        def nonblock_recv(self):
            self.ins.setnonblock(1)
            p = self.recv()
            self.ins.setnonblock(0)
            return p
    
        def close(self):
            if hasattr(self, "ins"):
                self.ins.close()
            if hasattr(self, "outs"):
                self.outs.close()
    
    class L2dnetSocket(SuperSocket):
        desc = "read/write packets at layer 2 using libdnet and libpcap"
        def __init__(self, iface = None, type = ETH_P_ALL, filter=None, nofilter=0):
            if iface is None:
                iface = conf.iface
            self.iface = iface
            self.ins = open_pcap(iface, 1600, 0, 100)
            try:
                ioctl(self.ins.fileno(),BIOCIMMEDIATE,struct.pack("I",1))
            except:
                pass
            if nofilter:
                if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                    filter = "ether proto %i" % type
                else:
                    filter = None
            else:
                if conf.except_filter:
                    if filter:
                        filter = "(%s) and not (%s)" % (filter, conf.except_filter)
                    else:
                        filter = "not (%s)" % conf.except_filter
                if type != ETH_P_ALL:  # PF_PACKET stuff. Need to emulate this for pcap
                    if filter:
                        filter = "(ether proto %i) and (%s)" % (type,filter)
                    else:
                        filter = "ether proto %i" % type
            if filter:
                self.ins.setfilter(filter)
            self.outs = dnet_eth(iface)
        def recv(self,x=MTU):
            ll = self.ins.datalink()
            if ll in conf.l2types:
                cls = conf.l2types[ll]
            else:
                cls = conf.default_l2
                warning("Unable to guess datalink type (interface=%s linktype=%i). Using %s" % (self.iface, ll, cls.name))
    
            pkt = self.ins.next()
            if pkt is not None:
                ts,pkt = pkt
            if pkt is None:
                return
            
            try:
                pkt = cls(pkt)
            except KeyboardInterrupt:
                raise
            except:
                if conf.debug_dissector:
                    raise
                pkt = conf.raw_layer(pkt)
            pkt.time = ts
            return pkt
    
        def nonblock_recv(self):
            self.ins.setnonblock(1)
            p = self.recv(MTU)
            self.ins.setnonblock(0)
            return p
    
        def close(self):
            if hasattr(self, "ins"):
                self.ins.close()
            if hasattr(self, "outs"):
                self.outs.close()

    conf.L3socket=L3dnetSocket
    conf.L2socket=L2dnetSocket
