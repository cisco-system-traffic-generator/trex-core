import argparse
from collections import namedtuple
import sys

ArgumentPack = namedtuple('ArgumentPack', ['name_or_flags', 'options'])
ArgumentGroup = namedtuple('ArgumentGroup', ['type', 'args', 'options'])


# list of available parsing options
MULTIPLIER = 1
PORT_LIST = 2
ALL_PORTS = 3
PORT_LIST_WITH_ALL = 4
FILE_PATH = 5
FILE_FROM_DB = 6
STREAM_FROM_PATH_OR_FILE = 7

# list of ArgumentGroup types
MUTEX = 1




OPTIONS_DB = {MULTIPLIER: ArgumentPack(['-m', '--multiplier'],
                                 {'help': "Set multiplier for stream", 'dest': "mult", 'type': float}),
              PORT_LIST: ArgumentPack(['--port'],
                                        {"nargs": '+',
                                         # "action": "store_"
                                         'help': "A list of ports on which to apply the command",
                                         'default': []}),
              ALL_PORTS: ArgumentPack(['-a'],
                                        {"action": "store_true",
                                         "dest": "all",
                                         'help': "Set this flag to apply the command on all available ports"}),

              FILE_PATH: ArgumentPack(['-f'],
                                      {'help': "File path to YAML file that describes a stream pack"}),
              FILE_FROM_DB: ArgumentPack(['--db'],
                                         {'help': "A stream pack which already loaded into console cache."}),
              # advanced options
              PORT_LIST_WITH_ALL: ArgumentGroup(MUTEX, [PORT_LIST,
                                                        ALL_PORTS],
                                                {'required': True}),
              STREAM_FROM_PATH_OR_FILE: ArgumentGroup(MUTEX, [FILE_PATH,
                                                              FILE_FROM_DB],
                                                      {'required': True})
              }


class CCmdArgParser(argparse.ArgumentParser):

    def __init__(self, *args, **kwargs):
        super(CCmdArgParser, self).__init__(*args, **kwargs)
        pass

    # def error(self, message):
    #     try:
    #         super(CCmdArgParser, self).error(message)   # this will trigger system exit!
    #     except SystemExit:
    #         return -1
    #
    #     # self.print_usage(sys.stderr)
    #     # print ('%s: error: %s\n') % (self.prog, message)
    #     # self.print_help()
    #     return

    def exit(self, status=0, message=None):
        try:
            super(CCmdArgParser, self).exit(status, message)   # this will trigger system exit!
        except SystemExit:
            return -1
        return

def gen_parser(op_name, description, *args):
    parser = CCmdArgParser(prog=op_name, conflict_handler='resolve',
                           # add_help=False,
                           description=description)
    for param in args:
        try:
            argument = OPTIONS_DB[param]
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