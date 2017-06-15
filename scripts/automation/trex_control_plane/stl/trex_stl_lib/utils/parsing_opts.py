import argparse
from collections import namedtuple, OrderedDict
from .common import list_intersect, list_difference, is_valid_ipv4, is_valid_ipv6, is_valid_mac, list_remove_dup
from .text_opts import format_text
from ..trex_stl_vlan import VLAN
from ..trex_stl_types import *
from .constants import ON_OFF_DICT, UP_DOWN_DICT, FLOW_CTRL_DICT

import sys
import re
import os
import inspect


ArgumentPack = namedtuple('ArgumentPack', ['name_or_flags', 'options'])
ArgumentGroup = namedtuple('ArgumentGroup', ['type', 'args', 'options'])


# list of available parsing options
_constants = '''

MULTIPLIER
MULTIPLIER_STRICT
PORT_LIST
ALL_PORTS
PORT_LIST_WITH_ALL
FILE_PATH
FILE_FROM_DB
SERVER_IP
STREAM_FROM_PATH_OR_FILE
DURATION
TIMEOUT
FORCE
READONLY
DRY_RUN
SYNCHRONIZED
XTERM
TOTAL
FULL_OUTPUT
IPG
MIN_IPG
SPEEDUP
COUNT
PROMISCUOUS
MULTICAST
LINK_STATUS
LED_STATUS
TUNABLES
REMOTE_FILE
LOCKED
PIN_CORES
CORE_MASK
DUAL
FLOW_CTRL
SUPPORTED
FILE_PATH_NO_CHECK

OUTPUT_FILENAME
LIMIT
PORT_RESTART

RETRIES

SINGLE_PORT
DST_MAC

PING_IP
PING_COUNT
PKT_SIZE

SERVICE_OFF

TX_PORT_LIST
RX_PORT_LIST

SRC_IPV4
DST_IPV4

CAPTURE_ID

SCAPY_PKT
SHOW_LAYERS
SCAPY_PKT_CMD

GLOBAL_STATS
PORT_STATS
PORT_STATUS
STREAMS_STATS
STATS_MASK
CPU_STATS
MBUF_STATS
EXTENDED_STATS
EXTENDED_INC_ZERO_STATS

STREAMS_MASK
CORE_MASK_GROUP
CAPTURE_PORTS_GROUP

MONITOR_TYPE_VERBOSE
MONITOR_TYPE_PIPE
MONITOR_TYPE

VLAN_TAGS
CLEAR_VLAN
VLAN_CFG

# ALL_STREAMS
# STREAM_LIST_WITH_ALL

# list of ArgumentGroup types
MUTEX
NON_MUTEX

'''

for index, line in enumerate(_constants.splitlines()):
    var = line.strip().split()
    if not var or '#' in var[0]:
        continue
    exec('%s = %s' % (var[0], index))


def check_negative(value):
    ivalue = int(value)
    if ivalue < 0:
        raise argparse.ArgumentTypeError("non positive value provided: '{0}'".format(value))
    return ivalue

def match_time_unit(val):
    '''match some val against time shortcut inputs '''
    match = re.match("^(\d+(\.\d+)?)([m|h]?)$", val)
    if match:
        digit = float(match.group(1))
        unit = match.group(3)
        if not unit:
            return digit
        elif unit == 'm':
            return digit*60
        else:
            return digit*60*60
    else:
        raise argparse.ArgumentTypeError("Duration should be passed in the following format: \n"
                                         "-d 100 : in sec \n"
                                         "-d 10m : in min \n"
                                         "-d 1h  : in hours")


match_multiplier_help = """Multiplier should be passed in the following format:
                          [number][<empty> | bps | kbps | mbps |  gbps | pps | kpps | mpps | %% ]. 

                          no suffix will provide an absoulute factor and percentage 
                          will provide a percentage of the line rate. examples

                          '-m 10', 
                          '-m 10kbps', 
                          '-m 10kbpsl1', 
                          '-m 10mpps', 
                          '-m 23%% '
                     
                          '-m 23%%' : is 23%% L1 bandwidth 
                          '-m 23mbps': is 23mbps in L2 bandwidth (including FCS+4)
                          '-m 23mbpsl1': is 23mbps in L1 bandwidth 

                          """


# decodes multiplier
# if allow_update - no +/- is allowed
# divide states between how many entities the
# value should be divided
def decode_multiplier(val, allow_update = False, divide_count = 1):

    factor_table = {None: 1, 'k': 1e3, 'm': 1e6, 'g': 1e9}
    pattern = "^(\d+(\.\d+)?)(((k|m|g)?(bpsl1|pps|bps))|%)?"

    # do we allow updates ?  +/-
    if not allow_update:
        pattern += "$"
        match = re.match(pattern, val)
        op = None
    else:
        pattern += "([\+\-])?$"
        match = re.match(pattern, val)
        if match:
            op  = match.group(7)
        else:
            op = None

    result = {}

    if not match:
        return None

    # value in group 1
    value = float(match.group(1))

    # decode unit as whole
    unit = match.group(3)

    # k,m,g
    factor = match.group(5)

    # type of multiplier
    m_type = match.group(6)

    # raw type (factor)
    if not unit:
        result['type'] = 'raw'
        result['value'] = value

    # percentage
    elif unit == '%':
        result['type'] = 'percentage'
        result['value']  = value

    elif m_type == 'bps':
        result['type'] = 'bps'
        result['value'] = value * factor_table[factor]

    elif m_type == 'pps':
        result['type'] = 'pps'
        result['value'] = value * factor_table[factor]

    elif m_type == 'bpsl1':
        result['type'] = 'bpsl1'
        result['value'] = value * factor_table[factor]


    if op == "+":
        result['op'] = "add"
    elif op == "-":
        result['op'] = "sub"
    else:
        result['op'] = "abs"

    if result['op'] != 'percentage':
        result['value'] = result['value'] / divide_count

    return result



def match_multiplier(val):
    '''match some val against multiplier  shortcut inputs '''
    result = decode_multiplier(val, allow_update = True)
    if not result:
        raise argparse.ArgumentTypeError(match_multiplier_help)

    return val


def match_multiplier_strict(val):
    '''match some val against multiplier  shortcut inputs '''
    result = decode_multiplier(val, allow_update = False)
    if not result:
        raise argparse.ArgumentTypeError(match_multiplier_help)

    return val

def hex_int (val):
    pattern = r"0x[1-9a-fA-F][0-9a-fA-F]*"

    if not re.match(pattern, val):
        raise argparse.ArgumentTypeError("{0} is not a valid positive HEX formatted number".format(val))
    
    return int(val, 16)
    

def action_check_vlan():
    class VLANCheck(argparse.Action):
        def __call__(self, parser, args, values, option_string=None):
            try:
                vlan = VLAN(values)
            except STLError as e:
                parser.error(e.brief())
            
            setattr(args, self.dest, vlan.get_tags())
            
            return
       
            
    return VLANCheck
      

def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename

# scapy decoder class for parsing opts
class ScapyDecoder(object):
    scapy_layers = None
    
    @staticmethod
    def init ():
        # one time
        if ScapyDecoder.scapy_layers:
            return
        
        
            
        import scapy.all
        import scapy.layers.dhcp
        
        raw = {}
        
        # default layers
        raw.update(scapy.all.__dict__)
        
        # extended layers - add here
        raw.update(scapy.layers.dhcp.__dict__)
        
        ScapyDecoder.scapy_layers = {k: v for k, v in raw.items() if inspect.isclass(v) and issubclass(v, scapy.all.Packet)}
        
        
    @staticmethod
    def to_scapy(scapy_str):
        ScapyDecoder.init()
        
        try:
            scapy_obj = eval(scapy_str, {'__builtins__': {}, 'True': True, 'False': False}, ScapyDecoder.scapy_layers)
            len(scapy_obj)
            return scapy_obj
        except Exception as e:
            raise argparse.ArgumentTypeError("invalid scapy expression: '{0}' - {1}".format(scapy_str, str(e)))

        
    @staticmethod
    def formatted_layers ():
        ScapyDecoder.init()
        
        output = ''
        for k, v in sorted(ScapyDecoder.scapy_layers.items()):
            name    = format_text("'{}'".format(k), 'bold')
            descr   = v.name
            #fields  = ", ".join(["'%s'" % f.name for f in v.fields_desc])
            #print("{:<50} - {} ({})".format(name ,descr, fields))
            output += "{:<50} - {}\n".format(name ,descr)
            
        return output
        
        
def check_ipv4_addr (ipv4_str):
    if not is_valid_ipv4(ipv4_str):
        raise argparse.ArgumentTypeError("invalid IPv4 address: '{0}'".format(ipv4_str))

    return ipv4_str

def check_ip_addr(addr):
    if not (is_valid_ipv4(addr) or is_valid_ipv6(addr)):
        raise argparse.ArgumentTypeError("invalid IPv4/6 address: '{0}'".format(addr))

    return addr

def check_pkt_size (pkt_size):
    try:
        pkt_size = int(pkt_size)
    except ValueError:
        raise argparse.ArgumentTypeError("invalid packet size type: '{0}'".format(pkt_size))
        
    if (pkt_size < 64) or (pkt_size > 9216):
        raise argparse.ArgumentTypeError("invalid packet size: '{0}' - valid range is 64 to 9216".format(pkt_size))
    
    return pkt_size
    
def check_mac_addr (addr):
    if not is_valid_mac(addr):
        raise argparse.ArgumentTypeError("not a valid MAC address: '{0}'".format(addr))
        
    return addr

    
def decode_tunables (tunable_str):
    tunables = {}

    # split by comma to tokens
    tokens = tunable_str.split(',')

    # each token is of form X=Y
    for token in tokens:
        m = re.search('(\S+)=(.+)', token)
        if not m:
            raise argparse.ArgumentTypeError("bad syntax for tunables: {0}".format(token))
        val = m.group(2)           # string
        if val.startswith(("'", '"')) and val.endswith(("'", '"')) and len(val) > 1: # need to remove the quotes from value
            val = val[1:-1]
        elif val.startswith('0x'): # hex
            val = int(val, 16)
        else:
            try:
                if '.' in val:     # float
                    val = float(val)
                else:              # int
                    val = int(val)
            except:
                pass
        tunables[m.group(1)] = val

    return tunables



OPTIONS_DB = {MULTIPLIER: ArgumentPack(['-m', '--multiplier'],
                                 {'help': match_multiplier_help,
                                  'dest': "mult",
                                  'default': "1",
                                  'type': match_multiplier}),

              MULTIPLIER_STRICT: ArgumentPack(['-m', '--multiplier'],
                               {'help': match_multiplier_help,
                                  'dest': "mult",
                                  'default': "1",
                                  'type': match_multiplier_strict}),

              TOTAL: ArgumentPack(['-t', '--total'],
                                 {'help': "traffic will be divided between all ports specified",
                                  'dest': "total",
                                  'default': False,
                                  'action': "store_true"}),

              IPG: ArgumentPack(['-i', '--ipg'],
                                {'help': "IPG value in usec between packets. default will be from the pcap",
                                 'dest': "ipg_usec",
                                 'default':  None,
                                 'type': float}),

              MIN_IPG: ArgumentPack(['--min-ipg'],
                                {'help': "Minimal IPG value in usec between packets. Used to guard from too small IPGs.",
                                 'dest': "min_ipg_usec",
                                 'default':  None,
                                 'type': float}),

              SPEEDUP: ArgumentPack(['-s', '--speedup'],
                                   {'help': "Factor to accelerate the injection. effectively means IPG = IPG / SPEEDUP",
                                    'dest': "speedup",
                                    'default':  1.0,
                                    'type': float}),

              COUNT: ArgumentPack(['-c', '--count'],
                                  {'help': "How many times to perform action [default is 1, 0 means forever]",
                                   'dest': "count",
                                   'default':  1,
                                   'type': int}),

              PROMISCUOUS: ArgumentPack(['--prom'],
                                        {'help': "Set port promiscuous on/off",
                                         'choices': ON_OFF_DICT}),

              MULTICAST: ArgumentPack(['--mult'],
                                      {'help': "Set port multicast on/off",
                                       'choices': ON_OFF_DICT}),

              LINK_STATUS: ArgumentPack(['--link'],
                                     {'help': 'Set link status up/down',
                                      'choices': UP_DOWN_DICT}),

              LED_STATUS: ArgumentPack(['--led'],
                                   {'help': 'Set LED status on/off',
                                    'choices': ON_OFF_DICT}),

              FLOW_CTRL: ArgumentPack(['--fc'],
                                   {'help': 'Set Flow Control type',
                                    'dest': 'flow_ctrl',
                                    'choices': FLOW_CTRL_DICT}),

              SRC_IPV4: ArgumentPack(['--src'],
                                     {'help': 'Configure source IPv4 address',
                                      'dest': 'src_ipv4',
                                      'required': True,
                                      'type': check_ipv4_addr}),
              
              DST_IPV4: ArgumentPack(['--dst'],
                                     {'help': 'Configure destination IPv4 address',
                                      'dest': 'dst_ipv4',
                                      'required': True,
                                      'type': check_ipv4_addr}),
              

              DST_MAC: ArgumentPack(['--dst'],
                                    {'help': 'Configure destination MAC address',
                                     'dest': 'dst_mac',
                                     'required': True,
                                     'type': check_mac_addr}),
              
              RETRIES: ArgumentPack(['-r', '--retries'],
                                    {'help': 'retries count [default is zero]',
                                     'dest': 'retries',
                                     'default':  0,
                                     'type': int}),
                

              OUTPUT_FILENAME: ArgumentPack(['-o', '--output'],
                                            {'help': 'Output PCAP filename',
                                             'dest': 'output_filename',
                                             'default': None,
                                             'type': str}),


              PORT_RESTART: ArgumentPack(['-r', '--restart'],
                                         {'help': 'hard restart port(s)',
                                          'dest': 'restart',
                                          'default': False,
                                          'action': 'store_true'}),

              LIMIT: ArgumentPack(['-l', '--limit'],
                                  {'help': 'Limit the packet count to be written to the file',
                                   'dest': 'limit',
                                   'default':  1000,
                                   'type': int}),


              SUPPORTED: ArgumentPack(['--supp'],
                                   {'help': 'Show which attributes are supported by current NICs',
                                    'default': None,
                                    'action': 'store_true'}),

              TUNABLES: ArgumentPack(['-t'],
                                     {'help': "Sets tunables for a profile. Example: '-t fsize=100,pg_id=7'",
                                      'metavar': 'T1=VAL[,T2=VAL ...]',
                                      'dest': "tunables",
                                      'default': None,
                                      'action': 'merge',
                                      'type': decode_tunables}),

              PORT_LIST: ArgumentPack(['--port', '-p'],
                                        {"nargs": '+',
                                         'dest':'ports',
                                         'metavar': 'PORTS',
                                         'action': 'merge',
                                         'type': int,
                                         'help': "A list of ports on which to apply the command",
                                         'default': []}),

              
              SINGLE_PORT: ArgumentPack(['--port', '-p'],
                                        {'dest':'ports',
                                         'type': int,
                                         'metavar': 'PORT',
                                         'help': 'source port for the action',
                                         'required': True}),
              
              PING_IP: ArgumentPack(['-d'],
                                      {'help': 'which IPv4/6 to ping',
                                      'dest': 'ping_ip',
                                      'required': True,
                                      'type': check_ip_addr}),
              
              PING_COUNT: ArgumentPack(['-n', '--count'],
                                       {'help': 'How many times to ping [default is 5]',
                                        'dest': 'count',
                                        'default':  5,
                                        'type': int}),
                  
              PKT_SIZE: ArgumentPack(['-s'],
                                     {'dest':'pkt_size',
                                      'help': 'packet size to use',
                                      'default': 64,
                                      'type': check_pkt_size}),
              
              
              ALL_PORTS: ArgumentPack(['-a'],
                                        {"action": "store_true",
                                         "dest": "all_ports",
                                         'help': "Set this flag to apply the command on all available ports",
                                         'default': False},),

              DURATION: ArgumentPack(['-d'],
                                        {'action': "store",
                                         'metavar': 'TIME',
                                         'dest': 'duration',
                                         'type': match_time_unit,
                                         'default': -1.0,
                                         'help': "Set duration time for job."}),

              TIMEOUT: ArgumentPack(['-t'],
                                        {'action': "store",
                                         'metavar': 'TIMEOUT',
                                         'dest': 'timeout',
                                         'type': int,
                                         'default': None,
                                         'help': "Timeout for operation in seconds."}),

              FORCE: ArgumentPack(['--force'],
                                        {"action": "store_true",
                                         'default': False,
                                         'help': "Set if you want to stop active ports before appyling command."}),

              READONLY: ArgumentPack(['-r'],
                                        {'action': 'store_true',
                                         'dest': 'readonly',
                                         'help': 'Do not acquire ports, connect as read-only.'}),

              REMOTE_FILE: ArgumentPack(['-r', '--remote'],
                                        {"action": "store_true",
                                         'default': False,
                                         'help': "file path should be interpeted by the server (remote file)"}),

              DUAL: ArgumentPack(['--dual'],
                                 {"action": "store_true",
                                  'default': False,
                                  'help': "Transmit in a dual mode - requires ownership on the adjacent port"}),

              FILE_PATH: ArgumentPack(['-f'],
                                      {'metavar': 'FILE',
                                       'dest': 'file',
                                       'nargs': 1,
                                       'required': True,
                                       'type': is_valid_file,
                                       'help': "File path to load"}),

              FILE_PATH_NO_CHECK: ArgumentPack(['-f'],
                                      {'metavar': 'FILE',
                                       'dest': 'file',
                                       'nargs': 1,
                                       'required': True,
                                       'type': str,
                                       'help': "File path to load"}),

              FILE_FROM_DB: ArgumentPack(['--db'],
                                         {'metavar': 'LOADED_STREAM_PACK',
                                          'help': "A stream pack which already loaded into console cache."}),

              SERVER_IP: ArgumentPack(['--server'],
                                      {'metavar': 'SERVER',
                                       'help': "server IP"}),

              DRY_RUN: ArgumentPack(['-n', '--dry'],
                                    {'action': 'store_true',
                                     'dest': 'dry',
                                     'default': False,
                                     'help': "Dry run - no traffic will be injected"}),

              SYNCHRONIZED: ArgumentPack(['--sync'],
                                    {'action': 'store_true',
                                     'dest': 'sync',
                                     'default': False,
                                     'help': 'Run the traffic with syncronized time at adjacent ports. Need to ensure effective ipg is at least 1000 usec.'}),

              XTERM: ArgumentPack(['-x', '--xterm'],
                                  {'action': 'store_true',
                                   'dest': 'xterm',
                                   'default': False,
                                   'help': "Starts TUI in xterm window"}),

              LOCKED: ArgumentPack(['-l', '--locked'],
                                   {'action': 'store_true',
                                    'dest': 'locked',
                                    'default': False,
                                    'help': "Locks TUI on legend mode"}),

              FULL_OUTPUT: ArgumentPack(['--full'],
                                         {'action': 'store_true',
                                          'help': "Prompt full info in a JSON format"}),

              GLOBAL_STATS: ArgumentPack(['-g'],
                                         {'action': 'store_true',
                                          'help': "Fetch only global statistics"}),

              PORT_STATS: ArgumentPack(['-p'],
                                       {'action': 'store_true',
                                        'help': "Fetch only port statistics"}),

              PORT_STATUS: ArgumentPack(['--ps'],
                                        {'action': 'store_true',
                                         'help': "Fetch only port status data"}),

              STREAMS_STATS: ArgumentPack(['-s'],
                                          {'action': 'store_true',
                                           'help': "Fetch only streams stats"}),

              CPU_STATS: ArgumentPack(['-c'],
                                      {'action': 'store_true',
                                       'help': "Fetch only CPU utilization stats"}),

              MBUF_STATS: ArgumentPack(['-m'],
                                       {'action': 'store_true',
                                        'help': "Fetch only MBUF utilization stats"}),

              EXTENDED_STATS: ArgumentPack(['-x'],
                                       {'action': 'store_true',
                                        'help': "Fetch xstats of port, excluding lines with zero values"}),

              EXTENDED_INC_ZERO_STATS: ArgumentPack(['--xz'],
                                       {'action': 'store_true',
                                        'help': "Fetch xstats of port, including lines with zero values"}),

              STREAMS_MASK: ArgumentPack(['--streams'],
                                         {"nargs": '+',
                                          'dest':'streams',
                                          'metavar': 'STREAMS',
                                          'type': int,
                                          'help': "A list of stream IDs to query about. Default: analyze all streams",
                                          'default': []}),


              PIN_CORES: ArgumentPack(['--pin'],
                                      {'action': 'store_true',
                                       'dest': 'pin_cores',
                                       'default': False,
                                       'help': "Pin cores to interfaces - cores will be divided between interfaces (performance boot for symetric profiles)"}),

              CORE_MASK: ArgumentPack(['--core_mask'],
                                      {'action': 'store',
                                       'nargs': '+',
                                       'type': hex_int,
                                       'dest': 'core_mask',
                                       'default': None,
                                       'help': "Core mask - only cores responding to the bit mask will be active"}),

              SERVICE_OFF: ArgumentPack(['--off'],
                                        {'action': 'store_false',
                                         'dest': 'enabled',
                                         'default': True,
                                         'help': 'Deactivates services on port(s)'}),
                
              TX_PORT_LIST: ArgumentPack(['--tx'],
                                         {'nargs': '+',
                                          'dest':'tx_port_list',
                                          'metavar': 'TX',
                                          'action': 'merge',
                                          'type': int,
                                          'help': 'A list of ports to capture on the TX side',
                                          'default': []}),
               
              
              RX_PORT_LIST: ArgumentPack(['--rx'],
                                         {'nargs': '+',
                                          'dest':'rx_port_list',
                                          'metavar': 'RX',
                                          'action': 'merge',
                                          'type': int,
                                          'help': 'A list of ports to capture on the RX side',
                                          'default': []}),
              
              
              MONITOR_TYPE_VERBOSE: ArgumentPack(['-v', '--verbose'],
                                                 {'action': 'store_true',
                                                  'dest': 'verbose',
                                                  'default': False,
                                                  'help': 'output to screen as verbose'}),
              
              MONITOR_TYPE_PIPE: ArgumentPack(['-p', '--pipe'],
                                              {'action': 'store_true',
                                               'dest': 'pipe',
                                               'default': False,
                                               'help': 'forward packets to a pipe'}),


              CAPTURE_ID: ArgumentPack(['-i', '--id'],
                                  {'help': "capture ID to remove",
                                   'dest': "capture_id",
                                   'type': int,
                                   'required': True}),

              
              SCAPY_PKT: ArgumentPack(['-s'],
                                      {'dest':'scapy_pkt',
                                       'metavar': 'PACKET',
                                       'type': ScapyDecoder.to_scapy,
                                       'help': 'A scapy notation packet (e.g.: Ether()/IP())'}),
               
              SHOW_LAYERS: ArgumentPack(['--layers', '-l'],
                                        {'action': 'store_true',
                                         'dest': 'layers',
                                         'help': "Show all registered layers / inspect a specific layer"}),
              
              
              VLAN_TAGS: ArgumentPack(['--vlan', '-v'],
                                      {'dest':'vlan',
                                       'action': action_check_vlan(),
                                       'type': int,
                                       'nargs': '*',
                                       'metavar': 'VLAN',
                                       'help': 'single or double VLAN tags'}),
               
              CLEAR_VLAN: ArgumentPack(['-c'],
                                      {'action': 'store_true',
                                       'dest': 'clear_vlan',
                                       'default': False,
                                       'help': "clear any VLAN configuration"}),
              

              SCAPY_PKT_CMD: ArgumentGroup(MUTEX, [SCAPY_PKT,
                                                   SHOW_LAYERS],
                                           {'required': True}),
              
              # advanced options
              PORT_LIST_WITH_ALL: ArgumentGroup(MUTEX, [PORT_LIST,
                                                        ALL_PORTS],
                                                {'required': False}),


              VLAN_CFG: ArgumentGroup(MUTEX, [VLAN_TAGS,
                                              CLEAR_VLAN],
                                      {'required': True}),

              
              STREAM_FROM_PATH_OR_FILE: ArgumentGroup(MUTEX, [FILE_PATH,
                                                              FILE_FROM_DB],
                                                      {'required': True}),
              STATS_MASK: ArgumentGroup(MUTEX, [GLOBAL_STATS,
                                                PORT_STATS,
                                                PORT_STATUS,
                                                STREAMS_STATS,
                                                CPU_STATS,
                                                MBUF_STATS,
                                                EXTENDED_STATS,
                                                EXTENDED_INC_ZERO_STATS,],
                                        {}),


              CORE_MASK_GROUP:  ArgumentGroup(MUTEX, [PIN_CORES,
                                                      CORE_MASK],
                                              {'required': False}),

              CAPTURE_PORTS_GROUP: ArgumentGroup(NON_MUTEX, [TX_PORT_LIST, RX_PORT_LIST], {}),
              
              
              MONITOR_TYPE: ArgumentGroup(MUTEX, [MONITOR_TYPE_VERBOSE,
                                                  MONITOR_TYPE_PIPE],
                                          {'required': False}),
              
              }

class _MergeAction(argparse._AppendAction):
    def __call__(self, parser, namespace, values, option_string=None):
        items = getattr(namespace, self.dest)
        if not items:
            items = values
        elif type(items) is list and type(values) is list:
            items.extend(values)
        elif type(items) is dict and type(values) is dict: # tunables are dict
            items.update(values)
        else:
            raise Exception("Argparser 'merge' option should be used on dict or list.")

        setattr(namespace, self.dest, items)

class CCmdArgParser(argparse.ArgumentParser):

    def __init__(self, stateless_client = None, *args, **kwargs):
        super(CCmdArgParser, self).__init__(*args, **kwargs)
        self.stateless_client = stateless_client
        self.cmd_name = kwargs.get('prog')
        self.register('action', 'merge', _MergeAction)


        
    def add_arg_list (self, *args):
        populate_parser(self, *args)

        
    # a simple hook for add subparsers to add stateless client
    def add_subparsers(self, *args, **kwargs):
        sub = super(CCmdArgParser, self).add_subparsers(*args, **kwargs)

        # save pointer to the original add parser method
        add_parser = sub.add_parser
        stateless_client = self.stateless_client

        def add_parser_hook (self, *args, **kwargs):
            parser = add_parser(self, *args, **kwargs)
            parser.stateless_client = stateless_client
            return parser

        # override with the hook
        sub.add_parser = add_parser_hook
        
        return sub

        
    # hook this to the logger
    def _print_message(self, message, file=None):
        self.stateless_client.logger.log(message)

        
    def error(self, message):
        self.print_usage()
        self._print_message(('%s: error: %s\n') % (self.prog, message))
        raise ValueError(message)

    def has_ports_cfg (self, opts):
        return hasattr(opts, "all_ports") or hasattr(opts, "ports")

    def parse_args(self, args=None, namespace=None, default_ports=None, verify_acquired=False):
        try:
            opts = super(CCmdArgParser, self).parse_args(args, namespace)
            if opts is None:
                return RC_ERR("'{0}' - invalid arguments".format(self.cmd_name))

            if not self.has_ports_cfg(opts):
                return opts

            opts.ports = listify(opts.ports)
            
            # if all ports are marked or 
            if (getattr(opts, "all_ports", None) == True) or (getattr(opts, "ports", None) == []):
                if default_ports is None:
                    opts.ports = self.stateless_client.get_acquired_ports()
                else:
                    opts.ports = default_ports

            opts.ports = list_remove_dup(opts.ports)
            
            # so maybe we have ports configured
            invalid_ports = list_difference(opts.ports, self.stateless_client.get_all_ports())
            if invalid_ports:
                
                if len(invalid_ports) > 1:
                    msg = "{0}: port(s) {1} are not valid port IDs".format(self.cmd_name, invalid_ports)
                else:
                    msg = "{0}: port {1} is not a valid port ID".format(self.cmd_name, invalid_ports[0])
                    
                self.stateless_client.logger.log(format_text(msg, 'bold'))
                return RC_ERR(msg)

            # verify acquired ports
            if verify_acquired:
                acquired_ports = self.stateless_client.get_acquired_ports()

                diff = list_difference(opts.ports, acquired_ports)
                if diff:
                    msg = "{0} - port(s) {1} are not acquired".format(self.cmd_name, diff)
                    self.stateless_client.logger.log(format_text(msg, 'bold'))
                    return RC_ERR(msg)

                # no acquire ports at all
                if not acquired_ports:
                    msg = "{0} - no acquired ports".format(self.cmd_name)
                    self.stateless_client.logger.log(format_text(msg, 'bold'))
                    return RC_ERR(msg)


            return opts

        except ValueError as e:
            return RC_ERR("'{0}' - {1}".format(self.cmd_name, str(e)))

        except SystemExit:
            # recover from system exit scenarios, such as "help", or bad arguments.
            return RC_ERR("'{0}' - {1}".format(self.cmd_name, "no action"))


    def formatted_error (self, msg):
        self.print_usage()
        self._print_message(('%s: error: %s\n') % (self.prog, msg))


def get_flags (opt):
    return OPTIONS_DB[opt].name_or_flags

def populate_parser (parser, *args):
    for param in args:
        try:

            if isinstance(param, int):
                argument = OPTIONS_DB[param]
            else:
                argument = param

            if isinstance(argument, ArgumentGroup):
                if argument.type == MUTEX:
                    # handle as mutually exclusive group
                    group = parser.add_mutually_exclusive_group(**argument.options)
                    for sub_argument in argument.args:
                        group.add_argument(*OPTIONS_DB[sub_argument].name_or_flags,
                                           **OPTIONS_DB[sub_argument].options)

                elif argument.type == NON_MUTEX:
                    group = parser.add_argument_group(**argument.options)
                    for sub_argument in argument.args:
                        group.add_argument(*OPTIONS_DB[sub_argument].name_or_flags,
                                           **OPTIONS_DB[sub_argument].options)
                else:
                    # ignore invalid objects
                    continue
            elif isinstance(argument, ArgumentPack):
                parser.add_argument(*argument.name_or_flags,
                                    **argument.options)
            else:
                # ignore invalid objects
                continue
        except KeyError as e:
            cause = e.args[0]
            raise KeyError("The attribute '{0}' is missing as a field of the {1} option.\n".format(cause, param))

def gen_parser(stateless_client, op_name, description, *args):
    parser = CCmdArgParser(stateless_client, prog=op_name, conflict_handler='resolve',
                           description=description)

    populate_parser(parser, *args)
    return parser


if __name__ == "__main__":
    pass
