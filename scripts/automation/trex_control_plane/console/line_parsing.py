import argparse
from collections import namedtuple
import sys

ArgumentPack = namedtuple('ArgumentPack', ['name_or_flags', 'options'])
# class ArgumentPack(namedtuple('ArgumentPack', ['name_or_flags', 'options'])):
#
#     @property
#     def name_or_flags(self):
#         return self.name_or_flags
#
#     @name_or_flags.setter
#     def name_or_flags(self, val):
#         print "bla"
#         if not isinstance(val, list):
#             self.name_or_flags = [val]
#         else:
#             self.name_or_flags = val


OPTIONS_DB = {'-m': ArgumentPack(['-m', '--multiplier'],
                                 {'help': "Set multiplier for stream", 'dest':"mult"}),
              'file_path': ArgumentPack(['file'],
                                 {'help': "File path to yaml file"})}


class CCmdArgParser(argparse.ArgumentParser):

    def __init__(self, *args, **kwargs):
        super(CCmdArgParser, self).__init__(*args, **kwargs)
        pass

    def error(self, message):
        # self.print_usage(sys.stderr)
        self.print_help()
        return

def gen_parser(op_name, *args):
    parser = CCmdArgParser(prog=op_name, conflict_handler='resolve')
    for param in args:
        try:
            parser.add_argument(*OPTIONS_DB[param].name_or_flags,
                                **OPTIONS_DB[param].options)
        except KeyError, e:
            cause = e.args[0]
            raise KeyError("The attribute '{0}' is missing as a field of the {1} option.\n".format(cause, param))
    return parser

if __name__ == "__main__":
    pass