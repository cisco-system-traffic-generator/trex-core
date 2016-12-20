import argparse
from collections import namedtuple, OrderedDict
from .common import list_intersect, list_difference, is_valid_ipv4, is_valid_mac, list_remove_dup
from .text_opts import format_text
from ..trex_stl_types import *
from .constants import ON_OFF_DICT, UP_DOWN_DICT, FLOW_CTRL_DICT

import sys
import re
import os

ArgumentPack = namedtuple('ArgumentPack', ['name_or_flags', 'options'])
ArgumentGroup = namedtuple('ArgumentGroup', ['type', 'args', 'options'])


# list of available parsing options
MULTIPLIER = 1
MULTIPLIER_STRICT = 2
PORT_LIST = 3
ALL_PORTS = 4
PORT_LIST_WITH_ALL = 5
FILE_PATH = 6
FILE_FROM_DB = 7
SERVER_IP = 8
STREAM_FROM_PATH_OR_FILE = 9
DURATION = 10
FORCE = 11
DRY_RUN = 12
XTERM = 13
TOTAL = 14
FULL_OUTPUT = 15
IPG = 16
SPEEDUP = 17
COUNT = 18
PROMISCUOUS = 19
LINK_STATUS = 20
LED_STATUS = 21
TUNABLES = 22
REMOTE_FILE = 23
LOCKED = 24
PIN_CORES = 25
CORE_MASK = 26
DUAL = 27
FLOW_CTRL = 28
SUPPORTED = 29
FILE_PATH_NO_CHECK = 30

OUTPUT_FILENAME = 31
LIMIT = 33
PORT_RESTART   = 34

RETRIES = 37

SINGLE_PORT = 38
DST_MAC = 39

PING_IPV4 = 40
PING_COUNT = 41
PKT_SIZE = 42

SERVICE_OFF = 43

SRC_IPV4 = 44
DST_IPV4 = 45

GLOBAL_STATS = 50
PORT_STATS = 51
PORT_STATUS = 52
STREAMS_STATS = 53
STATS_MASK = 54
CPU_STATS = 55
MBUF_STATS = 56
EXTENDED_STATS = 57
EXTENDED_INC_ZERO_STATS = 58

STREAMS_MASK = 60
CORE_MASK_GROUP = 61

# ALL_STREAMS = 61
# STREAM_LIST_WITH_ALL = 62



# list of ArgumentGroup types
MUTEX = 1

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


def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename

def check_ipv4_addr (ipv4_str):
    if not is_valid_ipv4(ipv4_str):
        raise argparse.ArgumentTypeError("invalid IPv4 address: '{0}'".format(ipv4_str))

    return ipv4_str

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


              SPEEDUP: ArgumentPack(['-s', '--speedup'],
                                   {'help': "Factor to accelerate the injection. effectively means IPG = IPG / SPEEDUP",
                                    'dest': "speedup",
                                    'default':  1.0,
                                    'type': float}),

              COUNT: ArgumentPack(['-n', '--count'],
                                  {'help': "How many times to perform action [default is 1, 0 means forever]",
                                   'dest': "count",
                                   'default':  1,
                                   'type': int}),

              PROMISCUOUS: ArgumentPack(['--prom'],
                                        {'help': "Set port promiscuous on/off",
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
                                             'required': True,
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
              
              PING_IPV4: ArgumentPack(['-d'],
                                      {'help': 'which IPv4 to ping',
                                      'dest': 'ping_ipv4',
                                      'required': True,
                                      'type': check_ipv4_addr}),
              
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

              FORCE: ArgumentPack(['--force'],
                                        {"action": "store_true",
                                         'default': False,
                                         'help': "Set if you want to stop active ports before appyling command."}),

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
                
              # advanced options
              PORT_LIST_WITH_ALL: ArgumentGroup(MUTEX, [PORT_LIST,
                                                        ALL_PORTS],
                                                {'required': False}),


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

    def __init__(self, stateless_client, *args, **kwargs):
        super(CCmdArgParser, self).__init__(*args, **kwargs)
        self.stateless_client = stateless_client
        self.cmd_name = kwargs.get('prog')
        self.register('action', 'merge', _MergeAction)

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


def get_flags (opt):
    return OPTIONS_DB[opt].name_or_flags

def gen_parser(stateless_client, op_name, description, *args):
    parser = CCmdArgParser(stateless_client, prog=op_name, conflict_handler='resolve',
                           description=description)
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
    return parser


if __name__ == "__main__":
    pass
