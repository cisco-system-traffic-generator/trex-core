#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Dan Klein, Itay Marom
Cisco Systems, Inc.

Copyright (c) 2015-2015 Cisco Systems, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""


import cmd
import json
import ast
import argparse
import random
import string
import os
import sys
import tty, termios
import trex_root_path
from common.trex_streams import *
from client.trex_stateless_client import CTRexStatelessClient
from client.trex_stateless_client import RpcResponseStatus
from common.text_opts import *
from client_utils.general_utils import user_input, get_current_user
import parsing_opts
import trex_status
from collections import namedtuple

__version__ = "1.0"

LoadedStreamList = namedtuple('LoadedStreamList', ['loaded', 'compiled'])

class CStreamsDB(object):

    def __init__(self):
        self.stream_packs = {}

    def load_yaml_file (self, filename):

        stream_pack_name = filename
        if stream_pack_name in self.get_loaded_streams_names():
            self.remove_stream_packs(stream_pack_name)

        stream_list = CStreamList()
        loaded_obj = stream_list.load_yaml(filename)

        try:
            compiled_streams = stream_list.compile_streams()
            rc = self.load_streams(stream_pack_name,
                                   LoadedStreamList(loaded_obj,
                                                    [StreamPack(v.stream_id, v.stream.dump())
                                                     for k, v in compiled_streams.items()]))

        except Exception as e:
            return None

        return self.get_stream_pack(stream_pack_name)

    def load_streams(self, name, LoadedStreamList_obj):
        if name in self.stream_packs:
            return False
        else:
            self.stream_packs[name] = LoadedStreamList_obj
            return True

    def remove_stream_packs(self, *names):
        removed_streams = []
        for name in names:
            removed = self.stream_packs.pop(name)
            if removed:
                removed_streams.append(name)
        return removed_streams

    def clear(self):
        self.stream_packs.clear()

    def get_loaded_streams_names(self):
        return self.stream_packs.keys()

    def stream_pack_exists (self, name):
        return name in self.get_loaded_streams_names()

    def get_stream_pack(self, name):
        if not self.stream_pack_exists(name):
            return None
        else:
            return self.stream_packs.get(name)


#
# main console object
class TRexConsole(cmd.Cmd):
    """Trex Console"""

    def __init__(self, stateless_client, acquire_all_ports = True, verbose = False):
        cmd.Cmd.__init__(self)

        self.stateless_client = stateless_client

        self.verbose = verbose
        self.acquire_all_ports = acquire_all_ports

        self.do_connect("")

        self.intro  = "\n-=TRex Console v{ver}=-\n".format(ver=__version__)
        self.intro += "\nType 'help' or '?' for supported actions\n"

        self.postcmd(False, "")

        self.streams_db = CStreamsDB()


    ################### internal section ########################

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    
    def register_main_console_methods(self):
        main_names = set(self.trex_console.get_names()).difference(set(dir(self.__class__)))
        for name in main_names:
            for prefix in 'do_', 'help_', 'complete_':
                if name.startswith(prefix):
                    self.__dict__[name] = getattr(self.trex_console, name)

    def postcmd(self, stop, line):
        if self.stateless_client.is_connected():
            self.prompt = "TRex > "
        else:
            self.supported_rpc = None
            self.prompt = "TRex (offline) > "

        return stop

    def default(self, line):
        print "'{0}' is an unrecognized command. type 'help' or '?' for a list\n".format(line)

    @staticmethod
    def tree_autocomplete(text):
        dir = os.path.dirname(text)
        if dir:
            path = dir
        else:
            path = "."
        start_string = os.path.basename(text)
        return [x
                for x in os.listdir(path)
                if x.startswith(start_string)]

    # annotation method
    @staticmethod
    def annotate (desc, rc = None, err_log = None, ext_err_msg = None):
        print format_text('\n{:<40}'.format(desc), 'bold'),
        if rc == None:
            print "\n"
            return

        if rc == False:
            # do we have a complex log object ?
            if isinstance(err_log, list):
                print ""
                for func in err_log:
                    if func:
                        print func
                print ""

            elif isinstance(err_log, str):
                print "\n" + err_log + "\n"

            print format_text("[FAILED]\n", 'red', 'bold')
            if ext_err_msg:
                print format_text(ext_err_msg + "\n", 'blue', 'bold')

            return False

        else:
            print format_text("[SUCCESS]\n", 'green', 'bold')
            return True


    ####################### shell commands #######################
    def do_ping (self, line):
        '''Ping the server\n'''

        rc = self.stateless_client.ping()
        if rc.good():
            print format_text("[SUCCESS]\n", 'green', 'bold')
        else:
            print "\n*** " + rc.err() + "\n"
            print format_text("[FAILED]\n", 'red', 'bold')
            return

    def do_test (self, line):
        print self.stateless_client.get_acquired_ports()

    # set verbose on / off
    def do_verbose(self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print "\nverbose is " + ("on\n" if self.verbose else "off\n")

        elif line == "on":
            self.verbose = True
            self.stateless_client.set_verbose(True)
            print format_text("\nverbose set to on\n", 'green', 'bold')

        elif line == "off":
            self.verbose = False
            self.stateless_client.set_verbose(False)
            print format_text("\nverbose set to off\n", 'green', 'bold')

        else:
            print format_text("\nplease specify 'on' or 'off'\n", 'bold')


    ############### connect
    def do_connect (self, line):
        '''Connects to the server\n'''

        rc = self.stateless_client.connect()
        if rc.good():
            print format_text("[SUCCESS]\n", 'green', 'bold')
        else:
            print "\n*** " + rc.err() + "\n"
            print format_text("[FAILED]\n", 'red', 'bold')
            return


    def do_disconnect (self, line):
        '''Disconnect from the server\n'''

        if not self.stateless_client.is_connected():
            print "Not connected to server\n"
            return

        self.stateless_client.disconnect()
        print format_text("[SUCCESS]\n", 'green', 'bold')

 
    ############### start

    def complete_start(self, text, line, begidx, endidx):
        s = line.split()
        l = len(s)

        file_flags = parsing_opts.get_flags(parsing_opts.FILE_PATH)

        if (l > 1) and (s[l - 1] in file_flags):
            return TRexConsole.tree_autocomplete("")

        if (l > 2) and (s[l - 2] in file_flags):
            return TRexConsole.tree_autocomplete(s[l - 1])

    def do_start(self, line):
        '''Start selected traffic in specified ports on TRex\n'''

        # make sure that the user wants to acquire all
        parser = parsing_opts.gen_parser(self.stateless_client,
                                         "start",
                                         self.do_start.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.FORCE,
                                         parsing_opts.STREAM_FROM_PATH_OR_FILE,
                                         parsing_opts.DURATION,
                                         parsing_opts.MULTIPLIER)

        opts = parser.parse_args(line.split())

        if opts is None:
            return

        if opts.db:
            stream_list = self.stream_db.get_stream_pack(opts.db)
            self.annotate("Load stream pack (from DB):", (stream_list != None))
            if stream_list == None:
                return

        else:
            # load streams from file
            stream_list = self.streams_db.load_yaml_file(opts.file[0])
            self.annotate("Load stream pack (from file):", (stream_list != None))
            if stream_list == None:
                return


        self.stateless_client.cmd_start(opts.ports, stream_list, opts.mult, opts.force, self.annotate)
        return


    def help_start(self):
        self.do_start("-h")

    ############# stop
    def do_stop(self, line):
        '''Stop active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self.stateless_client,
                                         "stop",
                                         self.do_stop.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        self.stateless_client.cmd_stop(opts.ports, self.annotate)
        return

    def help_stop(self):
        self.do_stop("-h")

    ########## reset
    def do_reset (self, line):
        '''force stop all ports\n'''
        self.stateless_client.cmd_reset(self.annotate)

  
    # tui
    def do_tui (self, line):
        '''Shows a graphical console\n'''

        if not self.stateless_client.is_connected():
            print "Not connected to server\n"
            return

        self.do_verbose('off')
        trex_status.show_trex_status(self.stateless_client)

    # quit function
    def do_quit(self, line):
        '''Exit the client\n'''
        return True

    
    def do_help (self, line):
         '''Shows This Help Screen\n'''
         if line:
             try:
                 func = getattr(self, 'help_' + line)
             except AttributeError:
                 try:
                     doc = getattr(self, 'do_' + line).__doc__
                     if doc:
                         self.stdout.write("%s\n"%str(doc))
                         return
                 except AttributeError:
                     pass
                 self.stdout.write("%s\n"%str(self.nohelp % (line,)))
                 return
             func()
             return
    
         print "\nSupported Console Commands:"
         print "----------------------------\n"
    
         cmds =  [x[3:] for x in self.get_names() if x.startswith("do_")]
         for cmd in cmds:
             if ( (cmd == "EOF") or (cmd == "q") or (cmd == "exit")):
                 continue
    
             try:
                 doc = getattr(self, 'do_' + cmd).__doc__
                 if doc:
                     help = str(doc)
                 else:
                     help = "*** Undocumented Function ***\n"
             except AttributeError:
                 help = "*** Undocumented Function ***\n"
    
             print "{:<30} {:<30}".format(cmd + " - ", help)

    do_exit = do_EOF = do_q = do_quit


def setParserOptions():
    parser = argparse.ArgumentParser(prog="trex_console.py")

    parser.add_argument("-s", "--server", help = "TRex Server [default is localhost]",
                        default = "localhost",
                        type = str)

    parser.add_argument("-p", "--port", help = "TRex Server Port  [default is 5505]\n",
                        default = 5505,
                        type = int)

    parser.add_argument("--async_port", help = "TRex ASync Publisher Port [default is 4505]\n",
                        default = 4505,
                        dest='pub',
                        type = int)

    parser.add_argument("-u", "--user", help = "User Name  [default is currently logged in user]\n",
                        default = get_current_user(),
                        type = str)

    parser.add_argument("--verbose", dest="verbose",
                        action="store_true", help="Switch ON verbose option. Default is: OFF.",
                        default = False)


    parser.add_argument("--no_acquire", dest="acquire",
                        action="store_false", help="Acquire all ports on connect. Default is: ON.",
                        default = True)

    return parser


def main():
    parser = setParserOptions()
    options = parser.parse_args()

    # Stateless client connection
    stateless_client = CTRexStatelessClient(options.user, options.server, options.port, options.pub)

    # console
    try:
        console = TRexConsole(stateless_client, options.acquire, options.verbose)
        console.cmdloop()
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        return

if __name__ == '__main__':
    main()

