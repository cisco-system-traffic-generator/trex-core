import sys, os
from scapy.all import *
from scapy.layers.dot11 import *
import hashlib

#######################
# Based on RFC 5415
#######################

############################### Global CAPWAP structure ##############################
#
#       CAPWAP Control Packet (Discovery Request/Response):
#       +-------------------------------------------+
#       | IP  | UDP | CAPWAP | Control | Message    |
#       | Hdr | Hdr | Header | Header  | Element(s) |
#       +-------------------------------------------+
#
#    CAPWAP Control Packet (DTLS Security Required):
#    +------------------------------------------------------------------+
#    | IP  | UDP | CAPWAP   | DTLS | CAPWAP | Control| Message   | DTLS |
#    | Hdr | Hdr | DTLS Hdr | Hdr  | Header | Header | Element(s)| Trlr |
#    +------------------------------------------------------------------+
#                           \---------- authenticated -----------/
#                                  \------------- encrypted ------------/
#
######################################################################################

################################# CAPWAP Header ######################################
#
#        0                   1                   2                   3
#        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#       |CAPWAP Preamble|  HLEN   |   RID   | WBID    |T|F|L|W|M|K|Flags|
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#       |          Fragment ID          |     Frag Offset         |Rsvd |
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#       |                 (optional) Radio MAC Address                  |
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#       |            (optional) Wireless Specific Information           |
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#       |                        Payload ....                           |
#       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#
######################################################################################

def warn(msg):
    warning('CAPWAP error: %s' % msg)


#################### Defines ##################

capwap_header_types = {0: 'CAPWAP Header', 1: 'CAPWAP DTLS Header'}
capwap_wbid_types = {0: 'Reserved', 1: 'IEEE 802.11', 2: 'Reserved', 3: 'EPCGlobal'}
capwap_mac_radio_sizes = {6: 'EUI-48 MAC', 8: 'EUI-64 MAC'}
capwap_mac_types = {0: 'Local MAC', 1: 'Split MAC', 2: 'Both'}
capwap_wtp_descriptor_types = {
    0: 'Hardware Version',
    1: 'Active Software Version',
    2: 'Boot Version',
    3: 'Other Software Version'
    }
capwap_board_data_types = {
    0: 'WTP Model Number',
    1: 'WTP Serial Number',
    2: 'Board ID',
    3: 'Board Revision',
    4: 'Base MAC Address'
    }
capwap_discovery_types = {
    0: 'Unknown',
    1: 'Static Configuration',
    2: 'DHCP',
    3: 'DNS',
    4: 'AC Referral'
    }
capwap_ecn_supports = {0: 'Limited', 1: 'Full and Limited'}
capwap_reserved_enabled_disabled = {0: 'Reserved', 1: 'Enabled', 2: 'Disabled'}
capwap_radio_mac_address_field = {0: 'Reserved', 1: 'Supported', 2: 'Not Supported'}
capwap_ac_information_types = {4: 'Hardware Version', 5: 'Software Version'}
capwap_draft8_tunnel_modes = {
    1: 'Local Bridging',
    2: '802.3 Frame Tunnel Mode',
    4: 'Native Frame Tunnel Mode',
    7: 'All',
    }
capwap_state_cause = {
    0: 'Normal',
    1: 'Radio Failure',
    2: 'Software Failure',
    3: 'Administratively Set',
    }
capwap_reboot_statistics_last_failures = {
    0:   'Not Supported',
    1:   'AC Initiated',
    2:   'Link Failure',
    3:   'Software Failure',
    4:   'Hardware Failure',
    5:   'Other Failure',
    255: 'Unknown',
    }
capwap_result_codes = {
    0:  'Success',
    1:  'Failure (AC List Message Element MUST Be Present)',
    2:  'Success (NAT Detected)',
    3:  'Join Failure (Unspecified)',
    4:  'Join Failure (Resource Depletion)',
    5:  'Join Failure (Unknown Source)',
    6:  'Join Failure (Incorrect Data)',
    7:  'Join Failure (Session ID Already in Use)',
    8:  'Join Failure (WTP Hardware Not Supported)',
    9:  'Join Failure (Binding Not Supported)',
    10: 'Reset Failure (Unable to Reset)',
    11: 'Reset Failure (Firmware Write Error)',
    12: 'Configuration Failure (Unable to Apply Requested Configuration - Service Provided Anyhow)',
    13: 'Configuration Failure (Unable to Apply Requested Configuration - Service Not Provided)',
    14: 'Image Data Error (Invalid Checksum)',
    15: 'Image Data Error (Invalid Data Length)',
    16: 'Image Data Error (Other Error)',
    17: 'Image Data Error (Image Already Present)',
    18: 'Message Unexpected (Invalid in Current State)',
    19: 'Message Unexpected (Unrecognized Request)',
    20: 'Failure - Missing Mandatory Message Element',
    21: 'Failure - Unrecognized Message Element',
    22: 'Data Transfer Error (No Information to Transfer)'
    }

capwap_message_elements_types = {
    # CAPWAP Protocol Message Elements                   1 - 1023
    0:  'Unknown (0)',
    1:  'AC Descriptor',
    2:  'AC IPv4 List',
    3:  'AC IPv6 List',
    4:  'AC Name',
    5:  'AC Name with Priority',
    6:  'AC Timestamp',
    7:  'Add MAC ACL Entry',
    8:  'Add Station',
    9:  '9 (Reserved)',
    10: 'CAPWAP Control IPV4 Address',
    11: 'CAPWAP Control IPV6 Address',
    30: 'CAPWAP Local IPV4 Address',
    50: 'CAPWAP Local IPV6 Address',
    12: 'CAPWAP Timers',
    51: 'CAPWAP Transport Protocol',
    13: 'Data Transfer Data',
    14: 'Data Transfer Mode',
    15: 'Decryption Error Report',
    16: 'Decryption Error Report Period',
    17: 'Delete MAC ACL Entry',
    18: 'Delete Station',
    19: '19 (Reserved)',
    20: 'Discovery Type',
    21: 'Duplicate IPv4 Address',
    22: 'Duplicate IPv6 Address',
    53: 'ECN Support',
    23: 'Idle Timeout',
    24: 'Image Data',
    25: 'Image Identifier',
    26: 'Image Information',
    27: 'Initiate Download',
    28: 'Location Data',
    29: 'Maximum Message Length',
    31: 'Radio Administrative State',
    32: 'Radio Operational State',
    33: 'Result Code',
    34: 'Returned Message Element',
    35: 'Session ID',
    36: 'Statistics Timer',
    37: 'Vendor Specific Payload',
    38: 'WTP Board Data',
    39: 'WTP Descriptor',
    40: 'WTP Fallback',
    41: 'WTP Frame Tunnel Mode',
    42: 'WTP IPv4 IP Address', # draft8
    43: 'WTP IPv6 IP Address', # draft8
    44: 'WTP MAC Type',
    45: 'WTP Name',
    46: '46 (Unused/Reserved)',
    47: 'WTP Radio Statistics',
    48: 'WTP Reboot Statistics',
    49: 'WTP Static IP Address Information',
    52: 'MTU Discovery Padding',
    # IEEE 802.11 Message Elements                    1024 - 2047
    1024: 'IEEE 802.11 Add WLAN',
    1025: 'IEEE 802.11 Antenna',
    1026: 'IEEE 802.11 Assigned WTP BSSID',
    1027: 'IEEE 802.11 Delete WLAN',
    1028: 'IEEE 802.11 Direct Sequence Control',
    1029: 'IEEE 802.11 Information Element',
    1030: 'IEEE 802.11 MAC Operation',
    1031: 'IEEE 802.11 MIC Countermeasures',
    1032: 'IEEE 802.11 Multi-Domain Capability',
    1033: 'IEEE 802.11 OFDM Control',
    1034: 'IEEE 802.11 Rate Set',
    1035: 'IEEE 802.11 RSNA Error Report From Station',
    1036: 'IEEE 802.11 Station',
    1037: 'IEEE 802.11 Station QoS Profile',
    1038: 'IEEE 802.11 Station Session Key',
    1039: 'IEEE 802.11 Statistics',
    1040: 'IEEE 802.11 Supported Rates',
    1041: 'IEEE 802.11 Tx Power',
    1042: 'IEEE 802.11 Tx Power Level',
    1043: 'IEEE 802.11 Update Station QoS',
    1044: 'IEEE 802.11 Update WLAN',
    1045: 'IEEE 802.11 WTP Quality of Service',
    1046: 'IEEE 802.11 WTP Radio Configuration',
    1047: 'IEEE 802.11 WTP Radio Fail Alarm Indication',
    1048: 'IEEE 802.11 WTP Radio Information',
    #Reserved for Future Use                         2048 - 3071
    #EPCGlobal Message Elements                      3072 - 4095
    #Reserved for Future Use                         4096 - 65535
    }

capwap_control_message_types = {
    0:  'Unknown',
    1:  'Discovery Request',
    2:  'Discovery Response',
    3:  'Join Request',
    4:  'Join Response',
    5:  'Configuration Status Request',
    6:  'Configuration Status Response',
    7:  'Configuration Update Request',
    8:  'Configuration Update Response',
    9:  'WTP Event Request',
    10: 'WTP Event Response',
    11: 'Change State Event Request',
    12: 'Change State Event Response',
    13: 'Echo Request',
    14: 'Echo Response',
    15: 'Image Data Request',
    16: 'Image Data Response',
    17: 'Reset Request',
    18: 'Reset Response',
    19: 'Primary Discovery Request',
    20: 'Primary Discovery Response',
    21: 'Data Transfer Request',
    22: 'Data Transfer Response',
    23: 'Clear Configuration Request',
    24: 'Clear Configuration Response',
    25: 'Station Configuration Request',
    26: 'Station Configuration Response',
    }

#################### Fields ###################

class IEEE80211RateSetField(StrLenField):
    def i2h(self, pkt, x):
        return ', '.join(['%gMb/s' % (c * 0.5) for c in map(ord, sorted(x))])


#################### Classes ###################

# Dot11 over CAPWAP has FC bytes swapped
class Dot11_swapped(Dot11):
    fields_desc = [
        FlagsField('FCfield', 0, 8, ['to-DS', 'from-DS', 'MF', 'retry', 'pw-mgt', 'MD', 'wep', 'order']),
        BitField('subtype', 0, 4),
        BitEnumField('type', 0, 2, ['Management', 'Control', 'Data', 'Reserved']),
        BitField('proto', 0, 2),
        ShortField('ID',0),
        MACField('addr1', ETHER_ANY),
        Dot11Addr2MACField('addr2', ETHER_ANY),
        Dot11Addr3MACField('addr3', ETHER_ANY),
        Dot11SCField('SC', 0),
        Dot11Addr4MACField('addr4', ETHER_ANY) 
        ]


# Dummy
class DTLS(Raw):
    name = 'DTLS stub'


# extra data is payload
class Layer(Packet):
    def extract_padding(self, s):
        if type(s) is tuple:
            raise Exception('Packet size is not whole number of bytes!')
        return s, None


# extra data is inner-fields
class Container(Packet):
    def extract_padding(self, s):
        if type(s) is tuple:
            raise Exception('Packet size is not whole number of bytes!')
        return None, s


class CAPWAP_Radio_MAC(Container):
    name = 'Radio MAC'
    fields_desc = [ ByteEnumField('length', 6, capwap_mac_radio_sizes),
                    MACField('mac_address', '12:12:12:12:12:12'),
                    XByteField('padding', 0),
                    ]


class CAPWAP_Wireless_Specific_Information(Container):
    name = 'Wireless Specific Information (General)'
    fields_desc = [ ByteField('length', 6),
                    StrLenField('data', '', length_from = lambda pkt:pkt.length),
                    StrLenField('padding', '', length_from = lambda pkt: (3 - pkt.length) % 4) ]

    def post_dissect(self, s):
        if self.length != len(self.data):
            warn('%s: length is %s, size of data: %s' % (self.name, self.length, len(self.data)))
        return s


class CAPWAP_Wireless_Specific_Information_IEEE802_11(Container):
    name = 'Wireless Specific Information (IEEE 802.11)'
    fields_desc = [ ByteField('length', 4),
                    ByteField('rssi', 0),
                    ByteField('snr', 0),
                    ShortField('data_rate', 0), # multiplier of 0.1 Mbps
                    X3BytesField('padding', 0),
                    ]


# CAPWAP ME sub-fields

class CAPWAP_Encryption_Sub_Element(Container):
    name = 'Encryption Sub-Element'
    fields_desc = [ BitField('reserved', 0, 3),
                    BitEnumField('wbid', 0, 5, capwap_wbid_types),
                    BitField('encryption_capabilities', 0, 16) ]

    def post_dissect(self, s):
        if self.reserved:
            warn('%s: reserved field should be zero according to RFC' % self.name)
        return s


class CAPWAP_Descriptor_Sub_Element(Container):
    name = 'Descriptor Sub-Element'
    fields_desc = [ IntField('descriptor_vendor_identifier', 0),
                    ShortEnumField('descriptor_type', 0, capwap_wtp_descriptor_types),
                    BitFieldLenField('descriptor_length', None, 16, length_of = 'descriptor_data'),
                    StrLenField('descriptor_data', '', length_from = lambda p: p.descriptor_length),
                    ]

    def post_dissect(self, s):
        if self.descriptor_length > 1024:
            warn('%s: descriptor_length field should not exceed 1024 (got %s) according to RFC' % (self.name, self.descriptor_length))
        if self.descriptor_length != len(self.descriptor_data):
            warn('%s: descriptor_length value %s mismatches size of descriptor_data %s!' % (self.name, self.descriptor_length, len(self.descriptor_data)))
        return s


class CAPWAP_AC_Information_Sub_Element(Container):
    name = 'AC Information Sub-Element'
    fields_desc = [ IntField('ac_information_vendor_identifier', 0),
                    ShortEnumField('ac_information_type', 0, capwap_ac_information_types),
                    ShortField('ac_information_length', 0),
                    StrLenField('ac_information_data', '', length_from = lambda p: p.ac_information_length),
                    ]

    def post_dissect(self, s):
        if self.ac_information_length > 1024:
            warn('%s: ac_information_length field should not exceed 1024 (got %s) according to RFC' % (self.name, self.ac_information_length))
        if self.ac_information_length != len(self.ac_information_data):
            warn('%s: ac_information_length value %s mismatches size of ac_information_data %s!' % (self.name, self.ac_information_length, len(self.ac_information_data)))
        return s


class CAPWAP_Board_Data_Sub_Element(Container):
    name = 'Board Data Sub-Element'
    fields_desc = [ ShortEnumField('board_data_type', 0, capwap_board_data_types),
                    BitFieldLenField('board_data_length', None, 16, length_of = 'board_data_value'),
                    StrLenField('board_data_value', '', length_from = lambda p: p.board_data_length),
                    ]

    def post_dissect(self, s):
        if self.board_data_length > 1024:
            warn('%s: board_data_length field should not exceed 1024 (got %s) according to RFC' % (self.name, self.board_data_length))
        if self.board_data_length != len(self.board_data_value):
            warn('%s: board_data_length value %s mismatches size of board_data_value %s!' % (self.name, self.board_data_length, len(self.board_data_value)))
        return s


class _CAPWAP_ME_HDR(Container):
    fields_desc = [ ShortEnumField('type', None, capwap_message_elements_types),
                    ShortField('length', None) ]


class CAPWAP_ME(Container):
    type = 'Unknown'
    ignore_msg_types = [1022]
    def __init__(self, *a, **k):
        Container.__init__(self, *a, **k)
        name_by_type = capwap_message_elements_types.get(self.type, 'Unknown')
        if self.__class__ == CAPWAP_ME:
            self.name = 'Message element - Not implemented (raw)'
            if self.type not in self.ignore_msg_types:
                if 'Reserved' in name_by_type:
                    warn('Usage of reserved Message element type %s (%s)' % (self.type, name_by_type))
                else:
                    warn('Not implemented Message element %s (%s).' % (self.type, name_by_type))
        elif self.type == 37:
            if self.__class__ == CAPWAP_ME_Vendor_Specific_Payload:
                self.name = 'Message element - Vendor Specific Payload (raw)'
            else:
                self.name = 'Message element - Vendor Specific Payload - %s' % capwap_vendor_specific_payload_names.get(self.element_id, 'raw')
        else:
            self.name = 'Message element - %s' % name_by_type

    fields_desc = [ _CAPWAP_ME_HDR,
                    StrLenField('raw', '', length_from = lambda pkt: pkt.length)]

    def post_dissect(self, s):
        data_len = self._length - 4
        if self.length != data_len:
            msg = 'Message element - %s: length is %s, size of data: %s' % (
                        capwap_message_elements_types.get(self.type, 'Code %s' % self.type),
                        self.length, data_len)
            #raise Exception(msg)
            warn(msg)
        return s

    def post_build(self, pkt, pay):
        l = self.length
        if l is None:
            l = len(pkt) - 4
        elif l != len(pkt) - 4:
            warn('Length in %s (%s) does not match size of data (%s)' % (self.name, l, len(pkt) - 4))
        return int2str(self.type, 2) + int2str(l, 2) + pkt[4:] + pay

    _registered_me_types = {}
    _registered_vendor_specific_payloads = {}
    @classmethod
    def register_variant(cls):
        assert hasattr(cls, 'type'), 'ME should have type'
        cls._registered_me_types[cls.type.default] = cls
        if cls.type.default == 37:
            assert hasattr(cls, 'element_id'), 'ME Vendor Specific Payload should have element_id'
            cls._registered_vendor_specific_payloads[cls.element_id.default] = cls

    @classmethod
    def dispatch_hook(cls, pkt=None, *a, **k):
        if pkt and len(pkt) >= 4:
            me_type = str2int(pkt[:2])
            me_len = str2int(pkt[2:4])
            if me_type in cls._registered_me_types:
                if me_type == 35 and me_len == 4:
                    return CAPWAP_ME_Session_ID_draft8
                if me_type == 37:
                    try:
                        element_id = str2int(pkt[8:10])
                        return cls._registered_vendor_specific_payloads[element_id]
                    except Exception as e:
                        return CAPWAP_ME_Vendor_Specific_Payload
                if me_type == 39 and len(pkt) > 6 and ord(pkt[6:7]) == 0:
                    return CAPWAP_ME_WTP_Descriptor_draft8
                return cls._registered_me_types[me_type]
            elif me_type not in cls.ignore_msg_types:
                warn('Got Message element type %s which is not supported' % me_type)
        return cls


class CAPWAP_ME_AC_Descriptor(CAPWAP_ME):
    type = 1
    fields_desc = [ _CAPWAP_ME_HDR,
                    ShortField('stations', 0),
                    ShortField('limit', 0),
                    ShortField('active_wtps', 0),
                    ShortField('max_wtps', 0),
                    BitField('reserved1', 0, 5),
                    FlagsField('authentication_credential_flags', 0, 3, ['Reserved', 'X.509 Certificate', 'Pre-shared secret']),
                    ByteEnumField('r_mac_field', 0, capwap_radio_mac_address_field),
                    BitField('reserved2', 0, 13),
                    FlagsField('dtls_policy_flags', 0, 3, ['Reserved', 'Clear Text Data Channel Supported', 'DTLS-Enabled Data Channel Supported']),
                    PacketListField('ac_information_sub_element', [], CAPWAP_AC_Information_Sub_Element, length_from = lambda p:p.length - 12),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 12:
            warn('%s: length should be at least 12 (got %s) according to RFC' % (self.name, self.length))
        return s


class CAPWAP_ME_AC_IPv4_List(CAPWAP_ME):
    type = 2
    fields_desc = [ _CAPWAP_ME_HDR,
                    FieldListField('ap_ip_address', [], IPField('', '0.0.0.0'), length_from = lambda pkt: pkt.length),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length > 1024:
            warn('%s: length should be at most 1024 (got %s) according to RFC' % (self.name, self.length))
        if self.length % 4:
            warn('%s: length should be whole multiplier of 4 (got %s)' % (self.name, self.length))
        return s


class CAPWAP_ME_AC_IPv6_List(CAPWAP_ME):
    type = 3
    fields_desc = [ _CAPWAP_ME_HDR,
                    FieldListField('ap_ip_address', [], IP6Field('', '::1'), length_from = lambda pkt: pkt.length),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length > 1024:
            warn('%s: length should be at most 1024 (got %s) according to RFC' % (self.name, self.length))
        if self.length % 16:
            warn('%s: length should be whole multiplier of 16 (got %s)' % (self.name, self.length))
        return s


class CAPWAP_ME_AC_Name(CAPWAP_ME):
    type = 4
    fields_desc = [ _CAPWAP_ME_HDR,
                    UTF8LenField('ac_name', '', length_from = lambda pkt: pkt.length),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 1:
            warn('%s: length should be at least 1 (got %s) according to RFC' % (self.name, self.length))
        return s


class CAPWAP_ME_Add_Station(CAPWAP_ME):
    type = 8
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    ByteEnumField('mac_length', 6, capwap_mac_radio_sizes),
                    MACField('mac_address', '12:12:12:12:12:12'),
                    UTF8LenField('vlan_name', '', length_from = lambda pkt:pkt.length - 8),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 8:
            warn('%s: length should be at least 8 (got %s) according to RFC' % (self.name, self.length))
        return s


class CAPWAP_ME_CAPWAP_Control_IPv4_Address(CAPWAP_ME):
    type = 10
    fields_desc = [ _CAPWAP_ME_HDR,
                    IPField('ip_address', '0.0.0.0'),
                    ShortField('wtp_count', 0),
                    ]


class CAPWAP_ME_CAPWAP_Timers(CAPWAP_ME):
    type = 12
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('discovery', 0),
                    ByteField('echo_request', 0),
                    ]


class CAPWAP_ME_Decryption_Error_Report_Period(CAPWAP_ME):
    type = 16
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    ShortField('report_interval', 0),
                    ]


class CAPWAP_ME_Delete_Station(CAPWAP_ME):
    type = 18
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    ByteEnumField('mac_length', 6, capwap_mac_radio_sizes),
                    MACField('mac_address', '12:12:12:12:12:12'),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 8:
            warn('%s: length should be at least 8 (got %s) according to RFC' % (self.name, self.length))
        return s


class CAPWAP_ME_Discovery_Type(CAPWAP_ME):
    type = 20
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteEnumField('discovery_type', 0, capwap_discovery_types),
                    ]


class CAPWAP_ME_Idle_Timeout(CAPWAP_ME):
    type = 23
    fields_desc = [ _CAPWAP_ME_HDR,
                    IntField('timeout', 0),
                    ]


class CAPWAP_ME_Image_Identifier(CAPWAP_ME):
    type = 25
    fields_desc = [ _CAPWAP_ME_HDR,
                    IntField('vendor_identifier', 0),
                    UTF8LenField('data', '', length_from = lambda pkt: pkt.length - 4)
                    ]


class CAPWAP_ME_Location_Data(CAPWAP_ME):
    type = 28
    fields_desc = [ _CAPWAP_ME_HDR,
                    UTF8LenField('location', '', length_from = lambda pkt: pkt.length)
                    ]


class CAPWAP_ME_Maximum_Message_Length(CAPWAP_ME):
    type = 29
    fields_desc = [ _CAPWAP_ME_HDR,
                    ShortField('maximum_message_length', 0)
                    ]


class CAPWAP_ME_Local_IPv4_Address(CAPWAP_ME):
    type = 30
    fields_desc = [ _CAPWAP_ME_HDR,
                    IPField('ip_address', '0.0.0.0')
                    ]


class CAPWAP_ME_Radio_Administrative_State(CAPWAP_ME):
    type = 31
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    ByteEnumField('admin_state', 0, capwap_reserved_enabled_disabled),
                    ]


class CAPWAP_ME_Radio_Operational_State(CAPWAP_ME):
    type = 32
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    ByteEnumField('state', 0, capwap_reserved_enabled_disabled),
                    ByteEnumField('cause', 0, capwap_state_cause),
                    ]


class CAPWAP_ME_Result_Code(CAPWAP_ME):
    type = 33
    fields_desc = [ _CAPWAP_ME_HDR,
                    BitEnumField('result_code', 0, 32, capwap_result_codes),
                    ]


# Cisco uses in some cases draft8 version of this header https://tools.ietf.org/html/draft-ietf-capwap-protocol-specification-08
class CAPWAP_ME_Session_ID_draft8(CAPWAP_ME):
    type = 35
    fields_desc = [ _CAPWAP_ME_HDR,
                    IntField('session_id', 0) ]

    #def post_dissect(self, s):
    #    CAPWAP_ME.post_dissect(self, s)
    #    if self.length != 4:
    #        warn('%s: length should be exactly 4 (got %s) according to RFC draft8' % (self.name, self.length))
    #    return s


class CAPWAP_ME_Session_ID(CAPWAP_ME):
    type = 35
    fields_desc = [ _CAPWAP_ME_HDR,
                    BitField('session_id', 0, 128) ]

    #def post_dissect(self, s):
    #    CAPWAP_ME.post_dissect(self, s)
    #    if self.length != 16:
    #        warn('%s: length should be exactly 16 (got %s) according to RFC' % (self.name, self.length))
    #    return s


class CAPWAP_ME_Statistics_Timer(CAPWAP_ME):
    type = 36
    fields_desc = [ _CAPWAP_ME_HDR,
                    ShortField('statistics_timer', 120) ]


def check_file(path):
    if path and os.path.isfile(path):
        return True
    return False


def get_capwap_privkey_path():
    priv_key_file = os.getenv('CAPWAP_PRIVKEY')
    if check_file(priv_key_file):
        return priv_key_file
    priv_key_file = os.path.join(os.getenv('HOME'), 'capwap_keys', 'privatekey.pem')
    if check_file(priv_key_file):
        return priv_key_file
    raise Exception('Either put privatekey.pem into $HOME/capwap_keys/, or define full path to file via CAPWAP_PRIVKEY envvar.')


def get_capwap_internal():
    current_dir = os.path.dirname(__file__)
    encrypted_file = os.path.join(current_dir, 'capwap_internal.enc')
    with open(encrypted_file, 'rb') as f:
        file_cont = f.read()
    decrypted_file = '/tmp/%s.py' % hashlib.sha256(file_cont).hexdigest()
    if not os.path.isfile(decrypted_file):
        priv_key_file = get_capwap_privkey_path()
        ret = os.system('openssl smime -decrypt -in %s -binary -inform DEM -inkey %s -out %s' % (encrypted_file, priv_key_file, decrypted_file))
        if ret:
            raise Exception('Decryption of capwap_internal failed with error code: %s' % ret)
    with open(decrypted_file) as f:
        file_cont = f.read()
    return file_cont


try:
    exec(get_capwap_internal())
except Exception as e:
    import traceback
    traceback.print_exc()
    capwap_vendor_specific_payload_names = {}
    warn('Could not load custom Vendor Specific Payloads, will use raw. %s' % e)


class CAPWAP_ME_Vendor_Specific_Payload(CAPWAP_ME):
    type = 37
    fields_desc = [ _CAPWAP_ME_HDR,
                    IntField('vendor_identifier', 0),
                    ShortEnumField('element_id', 0, capwap_vendor_specific_payload_names),
                    StrLenField('data', '', length_from = lambda pkt: pkt.length - 6) ]

    def post_dissect(self, s):
        if self.length < 7:
            warn('%s: length should be at least 7 (got %s) according to RFC' % (self.name, self.length))
        return s


class CAPWAP_ME_WTP_Board_Data(CAPWAP_ME):
    type = 38
    fields_desc = [ _CAPWAP_ME_HDR,
                    IntField('vendor_identifier', 0),
                    PacketListField('board_data_sub_element', [], CAPWAP_Board_Data_Sub_Element, length_from = lambda p:p.length - 4),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 14:
            warn('%s: length should be at least 14 according to RFC' % self.name)
        types_present = [False] * 2
        for d in self.board_data_sub_element:
            if d.board_data_type < 2:
                types_present[d.board_data_type] = True
        for i in range(2):
            if not types_present[i]:
                warn('%s: %s must be present according to RFC' % (self.name, capwap_board_data_types[i]))
        return s


# Cisco uses in some cases draft8 version of this header https://tools.ietf.org/html/draft-ietf-capwap-protocol-specification-08
class CAPWAP_ME_WTP_Descriptor_draft8(CAPWAP_ME):
    type = 39
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('max_radios', 1),
                    ByteField('radios_in_use', 1),
                    BitField('encryption_capabilities', 0, 16),
                    PacketListField('descriptor_sub_element', [], CAPWAP_Descriptor_Sub_Element, length_from = lambda p:p.length - 4),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if self.length < 31:
            warn('%s: length should be at least 33 according to RFC' % self.name)
        versions_present = [False] * 3
        for d in self.descriptor_sub_element:
            if d.descriptor_type < 3:
                versions_present[d.descriptor_type] = True
        for i in range(3):
            if not versions_present[i]:
                warn('%s: %s must be present according to RFC' % (self.name, capwap_wtp_descriptor_types[i]))
        return s


class CAPWAP_ME_WTP_Descriptor(CAPWAP_ME):
    type = 39
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('max_radios', 1),
                    ByteField('radios_in_use', 1),
                    BitFieldLenField('num_encrypt', None, 8, count_of = 'encryption_sub_element'),
                    PacketListField('encryption_sub_element', [], CAPWAP_Encryption_Sub_Element, count_from = lambda p:p.num_encrypt),
                    PacketListField('descriptor_sub_element', [], CAPWAP_Descriptor_Sub_Element, length_from = lambda p:p.length - 3 - sum(map(len, p.encryption_sub_element))),
                    ]

    def post_dissect(self, s):
        CAPWAP_ME.post_dissect(self, s)
        if not self.num_encrypt:
            warn('%s: num_encrypt should be positive according to RFC' % self.name)
        if self.length < 33:
            warn('%s: length should be at least 33 according to RFC' % self.name)
        versions_present = [False] * 3
        for d in self.descriptor_sub_element:
            if d.descriptor_type < 3:
                versions_present[d.descriptor_type] = True
        for i in range(3):
            if not versions_present[i]:
                warn('%s: %s must be present according to RFC' % (self.name, capwap_wtp_descriptor_types[i]))
        return s


class CAPWAP_ME_WTP_Fallback(CAPWAP_ME):
    type = 40
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteEnumField('mode', 0, capwap_reserved_enabled_disabled) ]


class CAPWAP_ME_WTP_Frame_Tunnel_Mode_draft8(CAPWAP_ME):
    type = 41
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteEnumField('tunnel_mode', 4, capwap_draft8_tunnel_modes)
                    ]

# I have no idea how to distinguish which of them is used, will keep one what works for us
#class CAPWAP_ME_WTP_Frame_Tunnel_Mode(CAPWAP_ME):
#    type = 41
#    fields_desc = [ _CAPWAP_ME_HDR,
#                    BitField('reserved', 0, 4),
#                    FlagsField('flags', 0, 4, 'RLEN') # actual order is NELR
#                    ]


class CAPWAP_ME_WTP_IPv4_IP_Address(CAPWAP_ME):
    type = 42
    fields_desc = [ _CAPWAP_ME_HDR,
                    IPField('wtp_ipv4_ip_address', '0.0.0.0'),
                    ]


class CAPWAP_ME_WTP_IPv6_IP_Address(CAPWAP_ME):
    type = 43
    fields_desc = [ _CAPWAP_ME_HDR,
                    IP6Field('wtp_ipv6_ip_address', '::1'),
                    ]


class CAPWAP_ME_WTP_MAC_Type(CAPWAP_ME):
    type = 44
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteEnumField('mac_type', 0, capwap_mac_types) ]


class CAPWAP_ME_WTP_Name(CAPWAP_ME):
    type = 45
    fields_desc = [ _CAPWAP_ME_HDR,
                    UTF8LenField('wtp_name', '', length_from = lambda pkt: pkt.length)
                    ]


class CAPWAP_ME_WTP_Reboot_Statistics(CAPWAP_ME):
    type = 48
    fields_desc = [ _CAPWAP_ME_HDR,
                    ShortField('reboot_count', 0),
                    ShortField('ac_initiated_count', 0),
                    ShortField('link_failure_count', 0),
                    ShortField('sw_failure_count', 0),
                    ShortField('hw_failure_count', 0),
                    ShortField('other_failure_count', 0),
                    ShortField('unknown_failure_count', 0),
                    ByteEnumField('reboot_statistics_last_failure', 0, capwap_reboot_statistics_last_failures)
                    ]


class CAPWAP_ME_MTU_Discovery_Padding(CAPWAP_ME):
    type = 52
    fields_desc = [ _CAPWAP_ME_HDR,
                    StrField('padding', '')
                    ]


class CAPWAP_ME_ECN_Support(CAPWAP_ME):
    type = 53
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteEnumField('ecn_support', 0, capwap_ecn_supports)
                    ]


class CAPWAP_ME_Rate_Set(CAPWAP_ME):
    type = 1034
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    IEEE80211RateSetField('supported_rates', '', length_from = lambda pkt: pkt.length - 1),
                    ]


class CAPWAP_ME_WTP_Radio_Information(CAPWAP_ME):
    type = 1048
    fields_desc = [ _CAPWAP_ME_HDR,
                    ByteField('radio_id', 0),
                    BitField('reserved', 0, 28),
                    FlagsField('flags', 0, 4, ['802.11b', '802.11a', '802.11g', '802.11n'])
                    ]


class CAPWAP_KeepAlive(Container):
    name = 'Keep alive'
    fields_desc = [ BitFieldLenField('message_elements_length', None, 16, length_of = 'message_elements', adjust = lambda pkt,x: x + 2),
                    PacketListField('message_elements', [], CAPWAP_ME, length_from = lambda p: p.message_elements_length - 2) ]

    #def post_dissect(self, s):
    #    '''
    #    me_actual_len = sum(map(len, self.message_elements))
    #    if self.message_elements_length > me_actual_len:
    #        warn('Message elements total length "%s" is less than size of data: "%s"' % (self.message_elements_length, me_actual_len))
    #    '''
    #    return s


class CAPWAP_Preamble(Container):
    name = 'Preamble'
    fields_desc = [ BitField('version', 0, 4),
                    BitEnumField('type', 0, 4, capwap_header_types),
                    ConditionalField(BitField('reserved', 0, 24), lambda pkt:pkt.type == 1) ] # padding in case of DTLS Header


class CAPWAP_Header(Container):
    name = 'Header'
    fields_desc = [ BitField('hlen', None, 5), # header len
                    BitField('rid', 0, 5),  # radio id
                    BitEnumField('wbid', 0, 5, capwap_wbid_types),  # wireless binding id
                    FlagsField('flags', 0, 6, 'KMWLFT'), # actual order is TFLWMK
                    BitField('reserved_flags', 0, 3),
                    BitField('fragment_id', 0, 16),
                    BitField('fragment_offset', 0, 13),
                    BitField('reserved_bits', 0, 3),
                    ConditionalField(PacketField('radio_mac', CAPWAP_Radio_MAC(), CAPWAP_Radio_MAC), lambda pkt:pkt.has_radio_mac()),
                    ConditionalField(PacketField('wireless_info', CAPWAP_Wireless_Specific_Information(), CAPWAP_Wireless_Specific_Information), lambda pkt:pkt.has_binding() and pkt.wbid != 1),
                    ConditionalField(PacketField('wireless_info_802', CAPWAP_Wireless_Specific_Information_IEEE802_11(), CAPWAP_Wireless_Specific_Information_IEEE802_11), lambda pkt:pkt.has_binding() and pkt.wbid == 1),
                    ]


    def post_build(self, pkt, pay):
        # fix hlen
        if not self.hlen:
            length = (len(pkt) + 1) >> 2
            pkt = int2str((length << 3) | (self.rid & 0b11100) >> 2) + pkt[1:]
        return pkt + pay

    def is_fragment(self):
        return bool(self.flags & 0b10000)

    def is_last_fragment(self):
        return bool(self.flags & 0b1000)

    def has_binding(self):
        return bool(self.flags & 0b100)

    def has_radio_mac(self):
        return bool(self.flags & 0b10)


class CAPWAP_Control_Header(Layer):
    name = 'Control Header'
    fields_desc = [ ThreeBytesField('iana_enterprise_number', 0),
                    ByteEnumField('type', 0, capwap_control_message_types),
                    ByteField('sequence_number', 0),
                    BitFieldLenField('message_elements_length', None, 16, length_of = 'message_elements', adjust = lambda pkt,x: x + 3),
                    ByteField('flags', 0),
                    PacketListField('message_elements', [], CAPWAP_ME, length_from = lambda p: p.message_elements_length - 3),
                    ]

    def post_dissect(self, s):
        me_actual_len = sum(map(len, self.message_elements)) + 3
        if self.message_elements_length != me_actual_len:
            warn('%s message elements total length "%s" not equal actual data: "%s"' % (self.name, self.message_elements_length, me_actual_len))
        if self.iana_enterprise_number != 0:
            warn('RFC 5415 requires IANA Enterprise Number = 0\n'
                  'Provided number: %s' % self.iana_enterprise_number)
        if self.flags != 0:
            warn('RFC 5415 requires flags = 0\n'
                  'Provided flags value: %s' % self.flags)
        return ''

    def post_build(self, pkt, pay):
        if self.type == 0:
            warn('Control message type is 0!')
        if self.iana_enterprise_number != 0:
            warn('RFC 5415 requires IANA Enterprise Number = 0\n'
                  'Provided number: %s' % self.iana_enterprise_number)
        if self.flags != 0:
            warn('RFC 5415 requires flags = 0\n'
                  'Provided flags value: %s' % self.flags)
        if len(pay): # ignore payload (raw etc.)
            pay = 'removed raw (len %s)' % len(pay)
        return Layer.post_build(self, pkt, pay)
            


class CAPWAP_Control_Header_Fragment(Raw):
    name = 'Control Header Fragment'
    fields_desc = [ StrField('raw', '') ]


class CAPWAP_CTRL(Layer):
    name = 'CAPWAP Control'
    fields_desc = [ PacketField('preamble', CAPWAP_Preamble(), CAPWAP_Preamble),
                    ConditionalField(PacketField('header', CAPWAP_Header(), CAPWAP_Header), lambda pkt: pkt.is_plain()),
                    ConditionalField(PacketField('control_header', CAPWAP_Control_Header(), CAPWAP_Control_Header), lambda pkt: pkt.is_plain() and not pkt.is_fragment()),
                    ConditionalField(PacketField('control_header_fragment', CAPWAP_Control_Header_Fragment(), CAPWAP_Control_Header_Fragment), lambda pkt: pkt.is_fragment()),
                    ]

    def post_dissect(self, s):
        if self.is_first_fragment():
            self.control_header_fragment.name = 'Control Header Fragment (%s)' % capwap_control_message_types.get(ord(self.control_header_fragment.raw[3:4]), 'Unknown')
        return s

    #def post_build(self, pkt, pay):
    #    if self.header and self.header.fragment_offset == 0 and self.header.is_fragment():
    #        ctrl_data = self.control_header_fragment.raw
    #        self.control_header_fragment.name = 'Control Header Fragment (%s)' % capwap_control_message_types.get(ord(ctrl_data[3:4]), 'Unknown')
    #    #    self.header.hlen = len(self.preamble) + len(self.header)
    #    return pkt + pay

    def guess_payload_class(self, payload):
        if self.preamble.type == 1:
            return DTLS
        return Layer.guess_payload_class(self, payload)

    def is_dtls(self):
        return self.preamble.type == 1

    def is_plain(self):
        return self.preamble.type == 0

    def is_fragment(self):
        return self.is_plain() and self.header.is_fragment()

    def is_last_fragment(self):
        return self.is_plain() and self.header.is_last_fragment()

    def is_first_fragment(self):
        return self.is_fragment() and self.header.fragment_offset == 0


class CAPWAP_DATA(Layer):
    name = 'CAPWAP Data'
    fields_desc = [ PacketField('preamble', CAPWAP_Preamble(), CAPWAP_Preamble),
                    ConditionalField(PacketField('header', CAPWAP_Header(), CAPWAP_Header), lambda pkt: pkt.preamble.type == 0),
                    ConditionalField(PacketField('keep_alive', CAPWAP_KeepAlive(), CAPWAP_KeepAlive), lambda pkt: pkt.header and bool(pkt.header.flags & 0b1)),
                    ]

    def is_keepalive(self):
        return self.header.flags & 0b1

    def guess_payload_class(self, payload):
        if self.is_keepalive():
            return Raw
        if self.header.flags & 0b100000: # type
            #if self.is_keepalive():
            #    warn('Flag of type should be zero in case of keepalive')
            if self.header.wbid:
                return Dot11_swapped
            return Raw
        return Dot3



bind_layers(UDP,                    CAPWAP_CTRL,          sport = 5246)
bind_layers(UDP,                    CAPWAP_CTRL,          dport = 5246)
bind_layers(CAPWAP_CTRL,            DTLS,                 preamble = CAPWAP_Preamble(b'\1\0\0\0'))
bind_layers(UDP,                    CAPWAP_DATA,          sport = 5247)
bind_layers(UDP,                    CAPWAP_DATA,          dport = 5247)
bind_layers(CAPWAP_DATA,            DTLS,                 preamble = CAPWAP_Preamble(b'\1\0\0\0'))



