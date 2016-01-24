#!/router/bin/python

import trex_root_path
from client.trex_stateless_client import *
from common.trex_exceptions import *
import cmd
from termstyle import termstyle
# import termstyle
import os
from argparse import ArgumentParser
import socket
import errno
import ast
import json


class InteractiveStatelessTRex(cmd.Cmd):

    intro = termstyle.green("\nInteractive shell to play with Cisco's TRex stateless API.\
                             \nType help to view available pre-defined scenarios\n(c) All rights reserved.\n")
    prompt = '> '

    def __init__(self, trex_host, trex_port, virtual, verbose):
        cmd.Cmd.__init__(self)

        self.verbose = verbose
        self.virtual = virtual
        self.trex = STLClient(trex_host, trex_port, self.virtual)
        self.DEFAULT_RUN_PARAMS = dict(m=1.5,
                                       nc=True,
                                       p=True,
                                       d=100,
                                       f='avl/sfr_delay_10_1g.yaml',
                                       l=1000)
        self.run_params = dict(self.DEFAULT_RUN_PARAMS)

    def do_transmit(self, line):
        """Transmits a request over using a given link to server.\
        \nuse: transmit [method_name] [method_params]"""
        if line == "":
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        args = line.split(' ', 1)   # args will have max length of 2
        method_name = args[0]
        params = None
        bad_parse = False

        try:
            params = ast.literal_eval(args[1])
            if not isinstance(params, dict):
                bad_parse = True
        except ValueError as e1:
            bad_parse = True
        except SyntaxError as e2:
            bad_parse = True

        if bad_parse:
            print "\nValue should be a valid dict: '{0}'".format(args[1])
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        response = self.trex.transmit(method_name, params)
        if not self.virtual:
            # expect response
            rc, msg = response
            if rc:
                print "\nServer Response:\n\n" + json.dumps(msg) + "\n"
            else:
                print "\n*** " + msg + "\n"





    def do_push_files(self, filepaths):
        """Pushes a custom file to be stored locally on TRex server.\
        \nPush multiple files by specifying their path separated by ' ' (space)."""
        try:
            filepaths = filepaths.split(' ')
            print termstyle.green("*** Starting pushing files ({trex_files}) to TRex. ***".format(
                trex_files=', '.join(filepaths))
            )
            ret_val = self.trex.push_files(filepaths)
            if ret_val:
                print termstyle.green("*** End of TRex push_files method (success) ***")
            else:
                print termstyle.magenta("*** End of TRex push_files method (failed) ***")

        except IOError as inst:
            print termstyle.magenta(inst)

if __name__ == "__main__":
    parser = ArgumentParser(description=termstyle.cyan('Run TRex client stateless API demos and scenarios.'),
                            usage="client_interactive_example [options]")

    parser.add_argument('-v', '--version', action='version', version='%(prog)s 1.0 \t (C) Cisco Systems Inc.\n')

    parser.add_argument("-t", "--trex-host", required = True, dest="trex_host",
        action="store", help="Specify the hostname or ip to connect with TRex server.",
        metavar="HOST" )
    parser.add_argument("-p", "--trex-port", type=int, default = 5050, metavar="PORT", dest="trex_port",
        help="Select port on which the TRex server listens. Default port is 5050.", action="store")
    # parser.add_argument("-m", "--maxhist", type=int, default = 100, metavar="SIZE", dest="hist_size",
    #     help="Specify maximum history size saved at client side. Default size is 100.", action="store")
    parser.add_argument("--virtual", dest="virtual",
                        action="store_true",
                        help="Switch ON virtual option at TRex client. Default is: OFF.",
                        default=False)
    parser.add_argument("--verbose", dest="verbose",
                        action="store_true",
                        help="Switch ON verbose option at TRex client. Default is: OFF.",
                        default=False)
    args = parser.parse_args()

    try:
        InteractiveStatelessTRex(**vars(args)).cmdloop()

    except KeyboardInterrupt:
        print termstyle.cyan('Bye Bye!')
        exit(-1)
    except socket.error, e:
        if e.errno == errno.ECONNREFUSED:
            raise socket.error(errno.ECONNREFUSED,
                               "Connection from TRex server was terminated. \
                               Please make sure the server is up.")
