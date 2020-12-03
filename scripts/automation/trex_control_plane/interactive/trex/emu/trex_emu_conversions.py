''' This class aggregates all conversions functions used in EMU '''
from ..common.trex_exceptions import *
from trex.utils.common import compress_ipv6, mac_str_to_num, int2mac, ip2int, int2ip, ipv62int, int2ipv6
from scapy.utils import mac2str

import copy
import socket
import struct

try:
  basestring
except NameError:
  basestring = str

EMPTY_IPV4 = '0.0.0.0'
EMPTY_IPV6 = '::'
EMPTY_MAC = '00:00:00:00:00:00'

##############################
# KEYS AND HEADERS META-DATA #
##############################
# key: key name
# header: header name
# empty_val: empty value(will not be shown if all col is empty)
# type: type of the record, used in conversion to/from bytes i.e: ipv4, ipv6, mac..
# relates: will be shown only if this tag will be passed to print function
# hide_from_table: will not show the record, default is False.

CLIENT_KEYS_AND_HEADERS = [
    # ipv4
    {'key': 'ipv4'    , 'header': 'IPv4' , 'empty_val': EMPTY_IPV4, 'type': 'ipv4'},
    {'key': 'ipv4_dg' , 'header': 'DG-IPv4', 'empty_val': EMPTY_IPV4, 'type': 'ipv4'},
    {'key': 'ipv4_mtu', 'header': 'MTU' , 'empty_val': 0, 'key_dependent': ['ipv4', 'ipv4_dg']},

    # ipv6
    {'key': 'ipv6'      , 'header': 'IPv6', 'empty_val': EMPTY_IPV6, 'relates': 'ipv6', 'type': 'ipv6'},
    {'key': 'dg_ipv6'   , 'header': 'DG-IPv6', 'empty_val': EMPTY_IPV6, 'relates': 'ipv6', 'type': 'ipv6'},
    {'key': 'dhcp_ipv6' , 'header': 'DHCPv6', 'empty_val': EMPTY_IPV6, 'relates': 'ipv6', 'type': 'ipv6'},
    {'key': 'ipv6_local', 'header': 'IPv6-Local', 'empty_val': EMPTY_IPV6, 'relates': 'ipv6', 'type': 'ipv6'},
    {'key': 'ipv6_slaac', 'header': 'IPv6-Slaac', 'empty_val': EMPTY_IPV6, 'relates': 'ipv6', 'type': 'ipv6'},

    # plugins
    {'key': 'plug_names' , 'header': 'Plugins', 'empty_val': '', 'type': 'list_of_string'},

    # forced
    {'key': 'ipv4_force_dg', 'header': 'IPv4 Force DG' , 'empty_val': False},
    {'key': 'ipv4_force_mac', 'header': 'IPv4 Force MAC' , 'empty_val': EMPTY_MAC, 'type': 'mac'},
    {'key': 'ipv6_force_dg' , 'header': 'IPv6 Force DG', 'empty_val': False},
    {'key': 'ipv6_force_mac', 'header': 'IPv6 Force MAC', 'empty_val': EMPTY_MAC, 'type': 'mac'},

    # structured
    {'key': 'dgw', 'header': '', 'type': 'structured', 'hide_from_table': True},
    {'key': 'ipv6_router' , 'header': '', 'type': 'structured', 'hide_from_table': True},
    {'key': 'ipv6_dgw', 'header': '', 'type': 'structured', 'hide_from_table': True},
]

DG_KEYS_AND_HEADERS = [
    {'key': 'resolve', 'header': 'Resolve', 'empty_val': False},
    {'key': 'rmac'   , 'header': 'Resolved-Mac', 'empty_val': EMPTY_MAC, 'type': 'mac'},
]

IPV6_ND_KEYS_AND_HEADERS = [
    {'key': 'mtu'  , 'header': 'MTU', 'empty_val': 0},
    {'key': 'dgmac', 'header': 'DG-MAC', 'empty_val': EMPTY_MAC, 'type': 'mac'},
    {'key': 'prefix', 'header': 'Prefix', 'empty_val': EMPTY_IPV6, 'type': 'ipv6'},
    {'key': 'prefix_len', 'header': 'Prefix Length', 'empty_val': 0},
]

ALL_KEYS_AND_HEADERS = CLIENT_KEYS_AND_HEADERS + DG_KEYS_AND_HEADERS + IPV6_ND_KEYS_AND_HEADERS


def _get_keys_with_type(t):
    return {record['key'] for record in ALL_KEYS_AND_HEADERS if record.get('type') == t}

# taken from emu server RPC
IPV4_TYPES = _get_keys_with_type('ipv4')
IPV6_TYPES = _get_keys_with_type('ipv6')
MAC_TYPES  = _get_keys_with_type('mac')

# these keys are structs
STRUCTURED_KEYS = _get_keys_with_type('structured') 

# these keys are lists of strings
LIST_OF_STR = _get_keys_with_type('list_of_string')

# these keys are lists of hex
LIST_OF_HEX = {'tpid'}

# these keys will not be shown in case of zero values
REMOVE_ZERO_VALS = {'tpid', 'tci'}

TYPES_DICT = {'ipv4': {'delim': '.', 'pad_zeros': False, 'format_type': 'd'},
                'mac': {'delim': ':', 'pad_zeros': True},
                'ipv6': {'delim': ':', 'pad_zeros': True, 'group_bytes': 2}
            }

class EMUTypeBuilder(object):

    @staticmethod
    def build_type(key, val):
        """
        Return a new EMUType object correspond to key. Created for emu client __init__ **kwargs.   

            :parameters:
                key: string
                    Client's key name.
                val: Anything
                    Val of key. Might be IPv4, IPv6 e.t.c.. but also can be none of those.
                :return:
                    EMUType or val type: The wanted EMUtype for key, if key is not recognized, the function will just return `val`.  
        """
        key_type = get_key_type(key)
        if key_type == 'ipv4':
            return Ipv4(val)
        elif key_type == 'ipv6':
            return Ipv6(val)
        elif key_type == 'mac':
            return Mac(val)
        else:
            return val  # key is not recognized

class EMUType(object):

    def __init__(self, val):
        """
        Base class for all emu types: mac, ipv4, ipv6.. 
        
            :parameters:
                val: string / list of bytes / any EMUType object
                    val can be any one of the above. In case of EMUType object the method works as copy c'tor.
        
            :raises:
                + :exe:'TRexError': In case val isn't valid in the wanted format(mac, ipv4..).
        """        
        if isinstance(val, EMUType):
            self.num_val = val.num_val  # copy c'tor
            self.s_val = val.s_val
            self.v_val = val.v_val
        else:
            self.num_val = self._conv_unknown_to_val(val)  # numeric value of the object
            self.s_val = None  # used for caching
            self.v_val = None  # used for caching

    def __str__(self):
        return self.S()

    def __getitem__(self, key):
        raise NotImplementedError()

    def _conv_unknown_to_val(self, val):
        """
        Convert unknown value to a numeric number. 
        
            :parameters:
                val: str or list of bytes
                    Value of mac address.
        """
        if self._is_list_of_bytes(val):
            fixed_val = 0
            for i, byte in enumerate(reversed(val)):
                fixed_val += (byte << (8 * i)) 
            self.v_val = val  # cache it
            return fixed_val
        elif isinstance(val, basestring):
            self.s_val = val  # cache it
            return self._conv_str_to_val(val)
        else:
            raise TRexError('Cannot convert value: "{0}" with type: "{1}" not in str / list of bytes'.format(val, type(val)))

    def _conv_num_to_bytes(self, val):
        """
        Convert a given value to list of bytes. 
        
            :parameters:
                val: int
                    Given numeric value.
            :returns:
                list: list of bytes representing the number.
        """
        res = [0 for _ in range (self.BYTES_NUM)]
        for i in range(self.BYTES_NUM - 1, -1, -1):
            byte = int(val & 0xFF)
            res[i] = byte
            val >>= 8
        return res

    def _calc_num_val_plus_key(self, key):
        """
        Calculate your inner value + key (for __getitem__). Might cause Over/Underflow.
        
            :parameters:
                key: int
                    Number to add to object's inner value.
        
            :raises:
                + :exe:'TRexError': Overflow
                + :exe:'TRexError': Underflow
        
            :returns:
                int: The result of object's inner value + key.
        """        
        inc_v = self.num_val + key
        if inc_v < 0:
            raise TRexError("Underflow! cannot decrease mac: {0}[{1}]".format(self.S(), key))
        if inc_v > self.MAX_VAL:
            raise TRexError("Overflow! cannot increase mac: {0}[{1}]".format(self.S(), key))
        inc_v = self._conv_num_to_bytes(inc_v)
        return inc_v

    def V(self):
        """
        Convert the object to a list of bytes. 

            :returns:
                list: list of bytes representing the object.
        """        
        if self.v_val is not None:
            return self.v_val

        self.v_val = self._conv_num_to_bytes(self.num_val)
        return self.v_val

    def S(self):
        raise NotImplementedError()

    def _is_list_of_bytes(self, val):
        """
        Check if val is a list of bytes fitting to the object. 
        
            :parameters:
                val: unknown type
                    Value of unknown type.
        
            :raises:
                + :exe:'TRexError': If val is a list but with a different legnth.
                + :exe:'TRexError': If val is a list but one of the elements isn't fit to byte.
        
            :returns:
                bool: If val is a valid list of bytes for the object. 
        """        
        if type(val) is list:
            if len(val) == self.BYTES_NUM:
                if all([type(v) is int and 0 <= v <= 255 for v in val]):
                    return True
                else:
                    raise TRexError('All bytes in list must be in range: 0 <= x <= 255, given list is: {0}'.format(val))
            else:
                raise TRexError('Got list with len: {0} where the wanted type requires: {1}'.format(len(val), self.BYTES_NUM))
        return False

class Mac(EMUType):

    BYTES_NUM = 6
    MAX_VAL   = (2 ** (6 * 8)) - 1

    def __init__(self, mac):
        """
        Creating a Mac object.

            :parameters:
                mac: string / list of bytes / Mac object
                    Valid mac representation. i.e: '00:00:00:00:00:00', [0, 0, 0, 0, 0, 1] or Mac('00:00:00:00:00:00')

            :raises:
                + :exe:'TRexError': If mac is not valid.
        """
        super(Mac, self).__init__(mac)

    def __getitem__(self, key):
        """
        | Increse mac value by key.
        | i.e: mac = Mac('00:00:00:00:00:ff')[2] -> Mac('00:00:00:00:01:01')

            :parameters:
                key: int
                    How much to increase(can be negative, positive or zero).   
            :raises:
                + :exe:'TRexError': Overflow
                + :exe:'TRexError': Underflow
            :returns:
                Mac: new mac object with the new val.
        """
        return Mac(self._calc_num_val_plus_key(key))

    def _conv_str_to_val(self, mac_str):
        """
        Convert a given mac string to numeric value. 
        
            :parameters:
                mac_str: string
                    String representing mac address.
        
            :raises:
                + :exe:'TRexError': In case of invalid string.
            :returns:
                int: mac as numeric value.
        """        
        if ':' in mac_str:
            mac_str = mac2str(mac_str)
        return mac_str_to_num(mac_str)

    def S(self):
        """
        Convert object to string representation.
            :returns:
                string: string representation of the mac address
        """        
        if self.s_val is not None:
            return self.s_val
        self.s_val = int2mac(self.num_val)
        return self.s_val

    @classmethod
    def is_valid(cls, mac):
        return isinstance(mac, list) and len(mac) == cls.BYTES_NUM and all([type(v) is int and 0 <= v <= 255 for v in mac])

class Ipv4(EMUType):

    BYTES_NUM = 4
    MAX_VAL   = (2 ** (4 * 8)) - 1

    def __init__(self, ipv4, mc = False):
        """
        Creating a ipv4 object. 
        
            :parameters:
                ipv4: string / list of bytes / ipv4 object
                    Valid ipv4 representation. i.e: '224.0.1.2', [224, 0, 1, 2] or ipv4('224.0.1.2')
                mc: bool
                    Check if the address is a valid IPv4 multicast.
            :raises:
                + :exe:'TRexError': If ipv4 is not valid.            
        """             
        super(Ipv4, self).__init__(ipv4)
        if isinstance(ipv4, Ipv4):
            self.mc = ipv4.mc  # copy c'tor
        else:
            self.mc = mc
        self._validate_mc()

    def __getitem__(self, key):
        """
        | Increse ipv4 value by key.
        | i.e: ipv4 = Ipv4('10.0.0.255')[2] -> Ipv4('10.0.1.1')
        
            :parameters:
                key: int
                    How much to increase(can be negative, positive or zero).   
            :raises:
                + :exe:'TRexError': Overflow
                + :exe:'TRexError': Underflow
            :returns:
                ipv4: new ipv4 object with the new val.
        """
        return Ipv4(self._calc_num_val_plus_key(key), mc = self.mc)

    def _validate_mc(self):
        """
        Validate object is Ipv4 multicast. 
                
            :raises:
                + :exe:'TRexError': If self.mc is on and the inner address isn't multicast.
        """        
        if self.mc:
            v = self.V()
            if (v[0] & 0xF0) != 0xE0:
                raise TRexError('Value: "%s" is not a valid ipv4 multicast address' % self.S())
    
    def _conv_str_to_val(self, val):
        """
        Convert a given ipv4 string to numeric value. 
        
            :parameters:
                ipv4_str: string
                    String representing ipv4 address.
        
            :raises:
                + :exe:'TRexError': In case of invalid string.
            :returns:
                int: ipv4 as numeric value.
        """    
        return ip2int(val)

    def S(self):
        """
        Convert object to string representation.         
            :returns:
                string: string representation of the ipv4 address
        """   
        if self.s_val is not None:
            return self.s_val

        self.s_val = int2ip(self.num_val) 
        return self.s_val

    @classmethod
    def is_valid(cls, ipv4, mc = False):
        res = isinstance(ipv4, list) and len(ipv4) == cls.BYTES_NUM and all([type(v) is int and 0 <= v <= 255 for v in ipv4])
        if mc:
            res = res and (ipv4[0] & 0xF0) == 0xE0
        return res

class Ipv6(EMUType):
    BYTES_NUM = 16
    MAX_VAL   = (2 ** (16 * 8)) - 1

    def __init__(self, ipv6, mc = False):
        """
        Creating a ipv6 object. 
        
            :parameters:
                ipv6: string / list of bytes / ipv6 object
                    Valid ipv6 representation. i.e: '::FF00', [0, .., 255, 255, 0, 0] or ipv6('::FF00')
                mc: bool
                    Check if the address is a valid ipv6 multicast.
            :raises:
                + :exe:'TRexError': If ipv6 is not valid.            
        """
        super(Ipv6, self).__init__(ipv6)
        if isinstance(ipv6, Ipv6):
            self.mc = ipv6.mc  # copy c'tor
        else:
            self.mc = mc
        self._validate_mc()

    def __getitem__(self, key):
        """
        | Increse ipv6 value by key.
        | i.e: ipv6 = ipv6('::10FF')[2] -> ipv6('::1101')
        
            :parameters:
                key: int
                    How much to increase(can be negative, positive or zero).   
            :raises:
                + :exe:'TRexError': Overflow
                + :exe:'TRexError': Underflow
            :returns:
                ipv6: new ipv6 object with the new val.
        """    
        return Ipv6(self._calc_num_val_plus_key(key), mc = self.mc)

    def _validate_mc(self):
        """
        Validate object is Ipv6 multicast. 
                
            :raises:
                + :exe:'TRexError': If self.mc is on and the inner address isn't multicast.
        """  
        if self.mc:
            v = self.V()
            if not (v[0] == 255):
                raise TRexError('Value: "%s" is not a valid ipv6 multicast address' % self.S())

    def _conv_str_to_val(self, val):
        """
        Convert a given ipv6 string to numeric value. 
        
            :parameters:
                ipv6_str: string
                    String representing ipv6 address.
        
            :raises:
                + :exe:'TRexError': In case of invalid string.
            :returns:
                int: ipv6 as numeric value.
        """ 
        high, low = ipv62int(val)
        return (high << 64) | low 

    def S(self):
        """
        Convert object to string representation.         
            :returns:
                string: string representation of the ipv6 address
        """
        if self.s_val is not None:
            return self.s_val

        b = self.num_val & 0xFFFFFFFFFFFFFFFF
        a = (self.num_val & 0xFFFFFFFFFFFFFFFF0000000000000000) >> 64

        self.s_val = int2ipv6(a, b)
        return self.s_val

    @classmethod
    def is_valid(cls, ipv6, mc = False):
        res = isinstance(ipv6, list) and len(ipv6) == cls.BYTES_NUM and all([type(v) is int and 0 <= v <= 255 for v in ipv6])
        if mc:
            res = res and (ipv6[0] == 255)
        return res


class HostPort():

    def __init__(self, ip, port):
        """
        HostPort represents an object that is a combination of host and port. For example, 127.0.0.1:80, [2001:db8::1]:8080.

            :parameters:
                ip: string
                    IPv4 or IPv6

                port: string
                    Port number, must be between 0 and 0xFFFF.
        """
        if ':' in ip:
            self.ip = Ipv6(ip)
            self.is_ipv6 = True
        elif '.' in ip:
            self.ip = Ipv4(ip)
            self.is_ipv6 = False
        else:
            raise TRexError("Value {} is not a valid IPv4/IPv6.".format(ip))
        HostPort._verify_port(port)
        self.port = int(port)

    @staticmethod
    def _verify_port(port):
        """
            Verify port string is a valid transport port.

            :parameters:
                port: string
                    Transport Port

            :raises:
                + :exe:'TRexError': If port is not a valid port
        """
        port_int = 0 
        try: 
            port_int = int(port)
        except: 
           raise TRexError("{} is not a numeric value.".format(port))  

        if port_int < 0 or port_int > 0xFFFF:
            raise TRexError("{} is not a valid port. Port must be in [0-65535].".format(port_int))

    def encode(self):
        """
            Encodes a HostPort into a string.

            :returns:
                String from the HostPort object.
        """
        if self.is_ipv6:
            return "[{}]:{}".format(self.ip.S(), self.port)
        else:
            return "{}:{}".format(self.ip.S(), self.port)

    __str__ = encode

    @staticmethod
    def decode(string):
        """
            Decodes a host port string of type ipv4:port or [ipv6]:port into a tuple of (ip, port).
            Validates the Ips and port are valid.

            :returns: 
                Tuple of (IP, Port)

            :raises:
                + :exe:'TRexError': If port is not a valid port

        """
        if "]" in string:
            # IPv6 String
            # [ipv6]:port
            splitted = string.split(']')
            attempted_ipv6 = splitted[0][1:] # don't need the [
            attempted_port = splitted[1][1:] # don't need the :
            ipv6 = Ipv6(attempted_ipv6)
            HostPort._verify_port(attempted_port)
            return ipv6, int(attempted_port)
        elif ':' in string:
            # IPv4 string
            attempted_ipv4, attempted_port = string.split(':')
            ipv4 = Ipv4(attempted_ipv4)
            HostPort._verify_port(attempted_port)
            return ipv4, int(attempted_port)
        else:
            raise TRexError("Invalid host port string {}".format(string))


def conv_to_str(val, key):
    
    def _add_comma(lst):
        return ', '.join(lst)

    if val is None:
        return None

    val_type = get_key_type(key)

    if val_type in REMOVE_ZERO_VALS and all(v == 0 for v in val):
        return None

    if val_type in TYPES_DICT.keys():
        val = _conv_to_str(val, val_type, **TYPES_DICT[val_type])
    elif val_type in STRUCTURED_KEYS:
        for k, v in val.items():
            val[k] = conv_to_str(v, k)

    elif val_type in LIST_OF_STR:
        sorted_list = sorted([str(v) for v in val])
        val = _add_comma(sorted_list)
    elif val_type in LIST_OF_HEX:
        val = _add_comma([hex(v) for v in val])

    # replace [data1, data2] -> data1, data2
    if type(val) is list:
        val = _add_comma([str(v) for v in val])

    return val

def _conv_to_str(val, val_type, delim, group_bytes = 1, pad_zeros = False, format_type = 'x'):
    """
        Convert bytes format to str.

        :parameters:
            val: dictionary
                Dictionary with the form: {'vport': 4040, tci': [10, 11], 'tpid': [0x8100, 0x8100]}
            
            delim: list
                list of dictionaries with the form: {'mac': '00:01:02:03:04:05', 'ipv4': '1.1.1.3', 'ipv4_dg':'1.1.1.2', 'ipv6': '00:00:01:02:03:04'}
                `mac` is the only required field.
            
            group_bytes: int
                Number of bytes to group together between delimiters.
                i.e: for mac address is 2 but for IPv6 is 4
            
            pad_zeros: bool
                True if each byte in string should be padded with zeros.
                i.e: 2 -> 02 
            
            format_type: str
                Type to convert using format, default is 'x' for hex. i.e for ipv4 format = 'd'

        :return:
            human readable string representation of val. 
    """
    bytes_as_str = ['0%s' % format(v, format_type) if pad_zeros and len(format(v, format_type)) < 2 else format(v, format_type) for v in val]
    
    n = len(bytes_as_str)
    grouped = ['' for _ in range(n // group_bytes)]
    for i in range(len(grouped)):
        for j in range(group_bytes):
            grouped[i] += bytes_as_str[j + (i * group_bytes)]

    res = delim.join(grouped)

    if val_type == 'ipv6':
        res = compress_ipv6(res)

    return res

def get_key_type(val_key):
    if val_key in IPV4_TYPES: 
        return 'ipv4'
    elif val_key in IPV6_TYPES:
        return 'ipv6'
    elif val_key in MAC_TYPES:
        return 'mac'
    else:
        return val_key

def get_val_types():
    copy = set(IPV4_TYPES)
    copy.update(IPV6_TYPES)
    copy.update(MAC_TYPES)
    return copy

def conv_unknown_to_str(val):
    len_val = len(val)
    if len_val == 4:
        return conv_to_str(val, 'ipv4')
    elif len_val == 6:
        return conv_to_str(val, 'mac')
    elif len_val == 16:
        return conv_to_str(val, 'ipv6')
    else:
        return val
