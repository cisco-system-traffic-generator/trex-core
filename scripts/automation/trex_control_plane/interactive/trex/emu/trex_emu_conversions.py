''' This class aggregates all conversions functions used in EMU '''
from ..common.trex_exceptions import *

import socket


ipv4_types = {'ipv4', 'ipv4_dg'}
ipv6_types = {'ipv6', 'dg_ipv6', 'dhcp_dg_ipv6', 'prefix'}
mac_types  = {'mac', 'dmac', 'dgmac', 'rmac', 'ipv4_force_mac', 'ipv6_force_mac'}

TYPES_DICT = {'ipv4': {'delim': '.', 'pad_zeros': False, 'format_type': 'd'},
                'mac': {'delim': ':', 'pad_zeros': True},
                'ipv6': {'delim': ':', 'pad_zeros': True, 'group_bytes': 2}
            }

def conv_to_bytes(val, key):
    """ 
        Convert `val` to []bytes for EMU server. 
        i.e: '00:aa:aa:aa:aa:aa' -> [0, 170, 170, 170, 170, 170]
    """
    val_type = _get_key_type(key)

    if val_type == 'ipv4': 
        return [ord(v) for v in socket.inet_pton(socket.AF_INET, val)]
    elif val_type == 'ipv6':
        return [ord(v) for v in socket.inet_pton(socket.AF_INET6, val)]
    elif val_type == 'mac':
        return [int(v, 16) for v in val.split(':')]
    else:
        return val

def conv_to_str(val, key):
    val_type = _get_key_type(key)
    if val_type not in TYPES_DICT.keys():
        print('Unknown type: "%s"' % val_type) 
    return _conv_to_str(val, **TYPES_DICT[val_type])

def _conv_to_str(val, delim, group_bytes = 1, pad_zeros = False, format_type = 'x'):
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
    return delim.join(grouped)

def _get_key_type(val_key):
    if val_key in ipv4_types: 
        return 'ipv4'
    elif val_key in ipv6_types:
        return 'ipv6'
    elif val_key in mac_types:
        return 'mac'
    else:
        return 'Unknown'

def get_val_types():
    copy = set(ipv4_types)
    copy.update(ipv6_types)
    copy.update(mac_types)
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

def conv_ns(port, vlans, tpids, allow_none = False):
    if not allow_none:
        # convert tpids from string to numbers
        if tpids is not None and all([type(t) == str for t in tpids]):
            tpids = [int(t, base = 16) for t in listify(tpids)]
        else:
            tpids = [0, 0] 

    vlans = vlans if vlans is not None else [0, 0]

    return {'vport': port, 'tci': vlans, 'tpid': tpids}

def conv_ns_for_tunnel(port, vlans, tpids, allow_none = False):
    if port is None:
        raise TRexError("Must specify at least a port number or run with --all-ns")
    return {'tun': conv_ns(port, vlans, tpids, allow_none)}

def conv_client(mac, ipv4, dg, ipv6):
    return {'mac': mac, 'ipv4': ipv4, 'ipv4_dg': dg, 'ipv6': ipv6}
