''' This class aggregates all conversions functions used in EMU '''

import socket

def conv_to_bytes(val, val_type):
    """ 
        Convert `val` to []bytes for EMU server. 
        i.e: '00:aa:aa:aa:aa:aa' -> [0, 170, 170, 170, 170, 170]
    """
    if val_type == 'ipv4' or val_type == 'ipv4_dg':
        return [ord(v) for v in socket.inet_pton(socket.AF_INET, val)]
    elif val_type == 'ipv6':
        return [ord(v) for v in socket.inet_pton(socket.AF_INET6, val)]
    elif val_type == 'mac':
        return [int(v, 16) for v in val.split(':')]
    else:
        raise TRexError("Unsupported key for convertion: %s" % val_type)

def conv_to_str(val, delim, group_bytes = 1, pad_zeros = False, format_type = 'x'):
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
    return {'tun': conv_ns(port, vlans, tpids, allow_none)}

def conv_client(mac, ipv4, dg, ipv6):
    return {'mac': mac, 'ipv4': ipv4, 'ipv4_dg': dg, 'ipv6': ipv6}
