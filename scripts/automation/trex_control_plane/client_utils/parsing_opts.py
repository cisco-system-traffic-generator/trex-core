import argparse
from collections import namedtuple
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

GLOBAL_STATS = 50
PORT_STATS = 51
PORT_STATUS = 52
STATS_MASK = 53

# list of ArgumentGroup types
MUTEX = 1

def check_negative(value):
    ivalue = int(value)
    if ivalue < 0:
        raise argparse.ArgumentTypeError("non positive value provided: '{0}'".format(value))
    return ivalue

def match_time_unit(val):
    '''match some val against time shortcut inputs '''
    match = re.match("^(\d+)([m|h]?)$", val)
    if match:
        digit = int(match.group(1))
        unit = match.group(2)
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
                          [number][<empty> | bps | kbps | mbps | gbps | pps | kpps | mpps | %% ].
                          no suffix will provide an absoulute factor and percentage 
                          will provide a percentage of the line rate. examples
                          : '-m 10', '-m 10kbps', '-m 10mpps', '-m 23%%' """

def match_multiplier_common(val, strict_abs = True):

    # on strict absolute we do not allow +/-
    if strict_abs:
        match = re.match("^(\d+(\.\d+)?)(bps|kbps|mbps|gbps|pps|kpps|mpps|%?)$", val)
        op = None
    else:
        match = re.match("^(\d+(\.\d+)?)(bps|kbps|mbps|gbps|pps|kpps|mpps|%?)([\+\-])?$", val)
        if match:
            op  = match.group(4)
        else:
            op = None

    result = {}

    if match:

        value = float(match.group(1))
        unit = match.group(3)
        

        
        # raw type (factor)
        if not unit:
            result['type'] = 'raw'
            result['value'] = value

        elif unit == 'bps':
            result['type'] = 'bps'
            result['value'] = value

        elif unit == 'kbps':
            result['type'] = 'bps'
            result['value'] = value * 1000

        elif unit == 'mbps':
            result['type'] = 'bps'
            result['value'] = value * 1000 * 1000

        elif unit == 'gbps':
            result['type'] = 'bps'
            result['value'] = value * 1000 * 1000 * 1000

        elif unit == 'pps':
            result['type'] = 'pps'
            result['value'] = value

        elif unit == "kpps":
            result['type'] = 'pps'
            result['value'] = value * 1000

        elif unit == "mpps":
            result['type'] = 'pps'
            result['value'] = value * 1000 * 1000

        elif unit == "%":
            result['type'] = 'percentage'
            result['value']  = value


        if op == "+":
            result['op'] = "add"
        elif op == "-":
            result['op'] = "sub"
        else:
            result['op'] = "abs"

        return result

    else:
        raise argparse.ArgumentTypeError(match_multiplier_help)


def match_multiplier(val):
    '''match some val against multiplier  shortcut inputs '''
    return match_multiplier_common(val, strict_abs = False)

def match_multiplier_strict(val):
    '''match some val against multiplier  shortcut inputs '''
    return match_multiplier_common(val, strict_abs = True)

def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename


OPTIONS_DB = {MULTIPLIER: ArgumentPack(['-m', '--multiplier'],
                                 {'help': match_multiplier_help,
                                  'dest': "mult",
                                  'default': {'type':'raw', 'value':1, 'op': 'abs'},
                                  'type': match_multiplier}),

              MULTIPLIER_STRICT: ArgumentPack(['-m', '--multiplier'],
                               {'help': match_multiplier_help,
                                  'dest': "mult",
                                  'default': {'type':'raw', 'value':1, 'op': 'abs'},
                                  'type': match_multiplier_strict}),

              TOTAL: ArgumentPack(['-t', '--total'],
                                 {'help': "traffic will be divided between all ports specified",
                                  'dest': "total",
                                  'default': False,
                                  'action': "store_true"}),

              PORT_LIST: ArgumentPack(['--port'],
                                        {"nargs": '+',
                                         'dest':'ports',
                                         'metavar': 'PORTS',
                                          'type': int,
                                         'help': "A list of ports on which to apply the command",
                                         'default': []}),

              ALL_PORTS: ArgumentPack(['-a'],
                                        {"action": "store_true",
                                         "dest": "all_ports",
                                         'help': "Set this flag to apply the command on all available ports"}),
              DURATION: ArgumentPack(['-d'],
                                        {'action': "store",
                                         'metavar': 'TIME',
                                         'dest': 'duration',
                                         'type': match_time_unit,
                                         'default': -1.0,
                                         'help': "Set duration time for TRex."}),

              FORCE: ArgumentPack(['--force'],
                                        {"action": "store_true",
                                         'default': False,
                                         'help': "Set if you want to stop active ports before applying new TRex run on them."}),

              FILE_PATH: ArgumentPack(['-f'],
                                      {'metavar': 'FILE',
                                       'dest': 'file',
                                       'nargs': 1,
                                       'type': is_valid_file,
                                       'help': "File path to YAML file that describes a stream pack. "}),

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

              GLOBAL_STATS: ArgumentPack(['-g'],
                                         {'action': 'store_true',
                                          'help': "Fetch only global statistics"}),

              PORT_STATS: ArgumentPack(['-p'],
                                       {'action': 'store_true',
                                        'help': "Fetch only port statistics"}),

              PORT_STATUS: ArgumentPack(['--ps'],
                                        {'action': 'store_true',
                                         'help': "Fetch only port status data"}),


              # advanced options
              PORT_LIST_WITH_ALL: ArgumentGroup(MUTEX, [PORT_LIST,
                                                        ALL_PORTS],
                                                {'required': True}),
              STREAM_FROM_PATH_OR_FILE: ArgumentGroup(MUTEX, [FILE_PATH,
                                                              FILE_FROM_DB],
                                                      {'required': True}),
              STATS_MASK: ArgumentGroup(MUTEX, [GLOBAL_STATS,
                                                PORT_STATS,
                                                PORT_STATUS],
                                        {})
              }


class CCmdArgParser(argparse.ArgumentParser):

    def __init__(self, stateless_client, *args, **kwargs):
        super(CCmdArgParser, self).__init__(*args, **kwargs)
        self.stateless_client = stateless_client

    def parse_args(self, args=None, namespace=None):
        try:
            opts = super(CCmdArgParser, self).parse_args(args, namespace)
            if opts is None:
                return None

            if getattr(opts, "all_ports", None):
                opts.ports = self.stateless_client.get_port_ids()

            if getattr(opts, "ports", None):
                for port in opts.ports:
                    if not self.stateless_client.validate_port_list([port]):
                        self.error("port id '{0}' is not a valid port id\n".format(port))

            return opts

        except SystemExit:
            # recover from system exit scenarios, such as "help", or bad arguments.
            return None


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