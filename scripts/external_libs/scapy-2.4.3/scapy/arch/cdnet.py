from ctypes import *
from ctypes.util import find_library
import sys

WIN=False

if sys.platform.startswith('win'):
    WIN=True

if WIN:
    SOCKET = c_uint
    _lib=CDLL('dnet')
else:
    SOCKET = c_int
    _lib_name = find_library('dnet')
    if not _lib_name:
      raise OSError("Cannot find libdnet.so")
    _lib=CDLL(_lib_name)

ETH_ADDR_LEN = 6
INTF_NAME_LEN = 16
INTF_NAME_COUNT = 20
INTF_ALIAS_COUNT = 20
IP6_ADDR_LEN = 16

ADDR_TYPE_NONE =  0
ADDR_TYPE_ETH  =  1
ADDR_TYPE_IP   =  2
ADDR_TYPE_IP6  =  3

INTF_TYPE_OTHER  = 1
INTF_TYPE_ETH    = 6
INTF_TYPE_TOKENRING = 9
INTF_TYPE_FDDI   = 15
INTF_TYPE_PPP    = 23
INTF_TYPE_LOOPBACK = 24
INTF_TYPE_SLIP   = 28
INTF_TYPE_TUN    = 53


uint8_t = c_ubyte
uint16_t = c_ushort
uint32_t = c_uint
ssize_t = c_long
dnet_ip_addr_t = uint32_t

dnet_intf_name = c_char * INTF_NAME_LEN

class dnet_intf_list(Structure):
  pass

dnet_intf_list._fields_ = [ ('length', c_int),
                            ('interfaces', dnet_intf_name * 20) ]

class dnet_eth_addr(Structure):
  pass

dnet_eth_addr._fields_ = [ ('data', uint8_t * ETH_ADDR_LEN) ]
dnet_eth_addr_t = dnet_eth_addr

class dnet_ip6_addr(Structure):
  pass

dnet_ip6_addr._fields_ = [ ('data', uint8_t * IP6_ADDR_LEN) ]
dnet_ip6_addr_t = dnet_ip6_addr

class dnet_addr_u(Union):
  pass

dnet_addr_u._fields_ = [ ('eth', dnet_eth_addr_t),
                         ('ip', dnet_ip_addr_t),
                         ('ip6', dnet_ip6_addr_t),
                         ('data8', uint8_t * 16),
                         ('data16', uint16_t * 8),
                         ('data32', uint32_t * 4) ]

class dnet_addr(Structure):
  pass
dnet_addr._anonymous_ = ('__addr_u', )
dnet_addr._fields_ = [ ('addr_type', uint16_t),
                       ('addr_bits', uint16_t),
                       ('__addr_u', dnet_addr_u) ] 

class dnet_intf_entry(Structure):
  pass

dnet_intf_entry._fields_ = [ ('intf_len', c_uint),
                             ('intf_name', c_char * INTF_NAME_LEN),
                             ('intf_type', c_ushort),
                             ('intf_flags', c_ushort),
                             ('intf_mtu', c_uint),
                             ('intf_addr', dnet_addr),
                             ('intf_dst_addr', dnet_addr),
                             ('intf_link_addr', dnet_addr),
                             ('intf_alias_num', c_uint),
                             ('intf_alias_addrs', dnet_addr * INTF_ALIAS_COUNT) ]


eth_t = c_void_p
intf_t = c_void_p
ip_t = c_void_p
dnet_intf_handler = CFUNCTYPE(c_int, POINTER(dnet_intf_entry), POINTER(c_void_p))

dnet_eth_open = _lib.eth_open
dnet_eth_open.restype = POINTER(eth_t)
dnet_eth_open.argtypes = [ POINTER(c_char) ]

dnet_eth_get = _lib.eth_get
dnet_eth_get.restype = c_int
dnet_eth_get.argtypes = [ POINTER(eth_t), POINTER(dnet_eth_addr_t) ]

dnet_eth_set = _lib.eth_set
dnet_eth_set.restype = c_int
dnet_eth_set.argtypes = [ POINTER(eth_t), POINTER(dnet_eth_addr_t) ]

dnet_eth_send = _lib.eth_send
dnet_eth_send.restype = ssize_t
dnet_eth_send.argtypes = [ POINTER(eth_t), c_void_p, c_size_t ]

dnet_eth_close = _lib.eth_close
dnet_eth_close.restype = POINTER(eth_t)
dnet_eth_close.argtypes = [ POINTER(eth_t) ]

dnet_intf_open = _lib.intf_open
dnet_intf_open.restype = POINTER(intf_t)
dnet_intf_open.argtypes = [ ]

dnet_intf_get = _lib.intf_get
dnet_intf_get.restype = c_int
dnet_intf_get.argtypes = [ POINTER(intf_t), POINTER(dnet_intf_entry) ]

dnet_intf_get_src = _lib.intf_get_src
dnet_intf_get_src.restype = c_int
dnet_intf_get_src.argtypes = [ POINTER(intf_t), POINTER(dnet_intf_entry), POINTER(dnet_addr) ]

dnet_intf_get_dst = _lib.intf_get_dst
dnet_intf_get_dst.restype = c_int
dnet_intf_get_dst.argtypes = [ POINTER(intf_t), POINTER(dnet_intf_entry), POINTER(dnet_addr) ]

dnet_intf_set = _lib.intf_set
dnet_intf_set.restype = c_int
dnet_intf_set.argtypes = [ POINTER(intf_t), POINTER(dnet_intf_entry) ]

dnet_intf_loop = _lib.intf_loop
dnet_intf_loop.restype = POINTER(intf_t)
dnet_intf_loop.argtypes = [ POINTER(intf_t), dnet_intf_handler, c_void_p ]

dnet_intf_close = _lib.intf_close
dnet_intf_close.restype = POINTER(intf_t)
dnet_intf_close.argtypes = [ POINTER(intf_t) ]

dnet_ip_open = _lib.ip_open
dnet_ip_open.restype = POINTER(ip_t)
dnet_ip_open.argtypes = [ ]

dnet_ip_add_option = _lib.ip_add_option
dnet_ip_add_option.restype = ssize_t
dnet_ip_add_option.argtypes = [ POINTER(c_void_p), c_size_t, c_int, POINTER(c_void_p), c_size_t ]

dnet_ip_checksum = _lib.ip_checksum
dnet_ip_checksum.restype = None
dnet_ip_checksum.argtypes = [ POINTER(c_void_p), c_size_t ]

dnet_ip_send = _lib.ip_send
dnet_ip_send.restype = ssize_t
dnet_ip_send.argtypes = [ POINTER(ip_t), c_void_p, c_size_t ]

dnet_ip_close = _lib.ip_close
dnet_ip_close.restype = POINTER(ip_t)
dnet_ip_close.argtypes = [ POINTER(ip_t) ]

class dnet_eth:
  def __init__(self, iface):
    self.iface_b = create_string_buffer(iface.encode('ascii'))
    self.eth = dnet_eth_open(self.iface_b)
  def send(self, sx):
    dnet_eth_send(self.eth, sx, len(sx))
  def close(self):
    return dnet_eth_close(self.eth) 

class dnet_ip:
  def __init__(self):
    self.ip = dnet_ip_open()
  def send(self, sx):
    dnet_ip_send(self.ip, sx, len(sx))
  def close(self):
    return dnet_ip_close(self.ip) 

def dnet_intf_name_loop(entry, intf_list):
  l = cast(intf_list, POINTER(dnet_intf_list))
  if l.contents.length >= INTF_NAME_COUNT:
    return -1
  for i in enumerate(entry.contents.intf_name):
    l.contents.interfaces[l.contents.length][i[0]] = i[1]
  l.contents.length += 1
  return 0 

class dnet_intf:
  def __init__(self):
    self.intf = dnet_intf_open()
    intf_list = dnet_intf_list()
    intf_list.length = 0
    dnet_intf_loop(self.intf, dnet_intf_handler(dnet_intf_name_loop), pointer(intf_list))
    self.names = []
    for i in range(INTF_NAME_COUNT):
      if i >= intf_list.length:
        break
      self.names.append(intf_list.interfaces[i].value.decode('ascii').strip('\0'))

  def close(self):
    return dnet_intf_close(self.intf) 

  def get(self, iface):
    ret = {}
    entry = dnet_intf_entry()
    entry.intf_name = iface.encode('ascii')
    entry.intf_len = sizeof(entry)
    r = dnet_intf_get(self.intf, byref(entry))
    if r < 0:
      return {}
    ret['addr6'] = []
    for i in range(entry.intf_alias_num):
      if entry.intf_alias_addrs[i].addr_type == ADDR_TYPE_IP6:
        ret['addr6'].append(bytes(entry.intf_alias_addrs[i].data8[:16]))
    ret['type'] = entry.intf_type
    ret['addr'] = bytes(entry.intf_addr.data8[:4])
    ret['link_addr'] = bytes(entry.intf_link_addr.data8[:6])
    return ret

