#!/router/bin/python

import trex_root_path
from client.trex_client import *
from common.trex_exceptions import *
import cmd
from python_lib.termstyle import termstyle
import os
from argparse import ArgumentParser
import socket
import errno


class InteractiveStatelessTRex(cmd.Cmd):

    intro = termstyle.green("\nInteractive shell to play with Cisco's TRex stateless API.\
                             \nType help to view available pre-defined scenarios\n(c) All rights reserved.\n")
    prompt = '> '

    def __init__(self, verbose_mode=False):
        cmd.Cmd.__init__(self)
        self.verbose = verbose_mode
        self.trex = None
        self.DEFAULT_RUN_PARAMS = dict(m=1.5,
                                       nc=True,
                                       p=True,
                                       d=100,
                                       f='avl/sfr_delay_10_1g.yaml',
                                       l=1000)
        self.run_params = dict(self.DEFAULT_RUN_PARAMS)

    def do_push_files(self, filepaths):
        """Pushes a custom file to be stored locally on T-Rex server.\
        \nPush multiple files by specifying their path separated by ' ' (space)."""
        try:
            filepaths = filepaths.split(' ')
            print termstyle.green("*** Starting pushing files ({trex_files}) to T-Rex. ***".format(
                trex_files=', '.join(filepaths))
            )
            ret_val = self.trex.push_files(filepaths)
            if ret_val:
                print termstyle.green("*** End of T-Rex push_files method (success) ***")
            else:
                print termstyle.magenta("*** End of T-Rex push_files method (failed) ***")

        except IOError as inst:
            print termstyle.magenta(inst)

if __name__ == "__main__":
    parser = ArgumentParser(description=termstyle.cyan('Run T-Rex client API demos and scenarios.'),
                            usage="client_interactive_example [options]")

    parser.add_argument('-v', '--version', action='version', version='%(prog)s 1.0 \t (C) Cisco Systems Inc.\n')

    # parser.add_argument("-t", "--trex-host", required = True, dest="trex_host",
    #     action="store", help="Specify the hostname or ip to connect with T-Rex server.",
    #     metavar="HOST" )
    # parser.add_argument("-p", "--trex-port", type=int, default = 8090, metavar="PORT", dest="trex_port",
    #     help="Select port on which the T-Rex server listens. Default port is 8090.", action="store")
    # parser.add_argument("-m", "--maxhist", type=int, default = 100, metavar="SIZE", dest="hist_size",
    #     help="Specify maximum history size saved at client side. Default size is 100.", action="store")
    parser.add_argument("--verbose", dest="verbose",
                        action="store_true",
                        help="Switch ON verbose option at T-Rex client. Default is: OFF.",
                        default=False)
    args = parser.parse_args()

    try:
        InteractiveStatelessTRex(args.verbose).cmdloop()

    except KeyboardInterrupt:
        print termstyle.cyan('Bye Bye!')
        exit(-1)
    except socket.error, e:
        if e.errno == errno.ECONNREFUSED:
            raise socket.error(errno.ECONNREFUSED,
                               "Connection from T-Rex server was terminated. \
                               Please make sure the server is up.")
