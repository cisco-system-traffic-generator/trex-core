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
import subprocess
import cmd
import json
import ast
import argparse
import random
import readline
import string
import os
import sys
import tty, termios
import trex_root_path
from common.trex_streams import *
from client.trex_stateless_client import CTRexStatelessClient, LoggerApi, STLError
from common.text_opts import *
from client_utils.general_utils import user_input, get_current_user
from client_utils import parsing_opts
import trex_tui
from functools import wraps


__version__ = "1.1"

# console custom logger
class ConsoleLogger(LoggerApi):
    def __init__ (self):
        self.prompt_redraw = None

    def write (self, msg, newline = True):
        if newline:
            print msg
        else:
            print msg,

    def flush (self):
        sys.stdout.flush()

    # override this for the prompt fix
    def async_log (self, msg, level = LoggerApi.VERBOSE_REGULAR, newline = True):
        self.log(msg, level, newline)
        if self.prompt_redraw:
            self.prompt_redraw()
            self.flush()


def set_window_always_on_top (title):
    # we need the GDK module, if not available - ignroe this command
    try:
        import gtk.gdk
    except ImportError:
        return

    # search the window and set it as above
    root = gtk.gdk.get_default_root_window()

    for id in root.property_get('_NET_CLIENT_LIST')[2]:
        w = gtk.gdk.window_foreign_new(id)
        if w:
            name = w.property_get('WM_NAME')[2]
            if name == title:
                w.set_keep_above(True)
                gtk.gdk.window_process_all_updates()
                break


class TRexGeneralCmd(cmd.Cmd):
    def __init__(self):
        cmd.Cmd.__init__(self)
        # configure history behaviour
        self._history_file_dir = "/tmp/trex/console/"
        self._history_file = self.get_history_file_full_path()
        readline.set_history_length(100)
        # load history, if any
        self.load_console_history()


    def get_console_identifier(self):
        return self.__class__.__name__

    def get_history_file_full_path(self):
        return "{dir}{filename}.hist".format(dir=self._history_file_dir,
                                             filename=self.get_console_identifier())

    def load_console_history(self):
        if os.path.exists(self._history_file):
            readline.read_history_file(self._history_file)
        return

    def save_console_history(self):
        if not os.path.exists(self._history_file_dir):
            # make the directory available for every user
            try:
                original_umask = os.umask(0)
                os.makedirs(self._history_file_dir, mode = 0777)
            finally:
                os.umask(original_umask)

            
        # os.mknod(self._history_file)
        readline.write_history_file(self._history_file)
        return

    def print_history (self):
        
        length = readline.get_current_history_length()

        for i in xrange(1, length + 1):
            cmd = readline.get_history_item(i)
            print "{:<5}   {:}".format(i, cmd)

    def get_history_item (self, index):
        length = readline.get_current_history_length()
        if index > length:
            print format_text("please select an index between {0} and {1}".format(0, length))
            return None

        return readline.get_history_item(index)


    def emptyline(self):
        """Called when an empty line is entered in response to the prompt.

        This overriding is such that when empty line is passed, **nothing happens**.
        """
        return

    def completenames(self, text, *ignored):
        """
        This overriding is such that a space is added to name completion.
        """
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]


#
# main console object
class TRexConsole(TRexGeneralCmd):
    """Trex Console"""

    def __init__(self, stateless_client, verbose = False):

        self.stateless_client = stateless_client

        TRexGeneralCmd.__init__(self)

        self.tui = trex_tui.TrexTUI(stateless_client)
        self.terminal = None

        self.verbose = verbose

        self.intro  = "\n-=TRex Console v{ver}=-\n".format(ver=__version__)
        self.intro += "\nType 'help' or '?' for supported actions\n"

        self.postcmd(False, "")


    ################### internal section ########################

    def prompt_redraw (self):
        sys.stdout.write("\n" + self.prompt + readline.get_line_buffer())
        sys.stdout.flush()


    def verify_connected(f):
        @wraps(f)
        def wrap(*args):
            inst = args[0]
            func_name = f.__name__
            if func_name.startswith("do_"):
                func_name = func_name[3:]

            if not inst.stateless_client.is_connected():
                print format_text("\n'{0}' cannot be executed on offline mode\n".format(func_name), 'bold')
                return

            ret = f(*args)
            return ret

        return wrap

    # TODO: remove this ugly duplication
    def verify_connected_and_rw (f):
        @wraps(f)
        def wrap(*args):
            inst = args[0]
            func_name = f.__name__
            if func_name.startswith("do_"):
                func_name = func_name[3:]

            if not inst.stateless_client.is_connected():
                print format_text("\n'{0}' cannot be executed on offline mode\n".format(func_name), 'bold')
                return

            if inst.stateless_client.is_all_ports_acquired():
                print format_text("\n'{0}' cannot be executed on read only mode\n".format(func_name), 'bold')
                return

            rc = f(*args)
            return rc

        return wrap


    def get_console_identifier(self):
        return "{context}_{server}".format(context=self.__class__.__name__,
                                           server=self.stateless_client.get_connection_info()['server'])
    
    def register_main_console_methods(self):
        main_names = set(self.trex_console.get_names()).difference(set(dir(self.__class__)))
        for name in main_names:
            for prefix in 'do_', 'help_', 'complete_':
                if name.startswith(prefix):
                    self.__dict__[name] = getattr(self.trex_console, name)

    def precmd(self, line):
        # before doing anything, save history snapshot of the console
        # this is done before executing the command in case of ungraceful application exit
        self.save_console_history()

        lines = line.split(';')

        for line in lines:
            stop = self.onecmd(line)
            stop = self.postcmd(stop, line)
            if stop:
                return "quit"

        return ""


    def postcmd(self, stop, line):

        if not self.stateless_client.is_connected():
            self.prompt = "TRex (offline) > "
            self.supported_rpc = None
            return stop

        if self.stateless_client.is_all_ports_acquired():
            self.prompt = "TRex (read only) > "
            return stop


        self.prompt = "TRex > "

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
        
        targets = []

        for x in os.listdir(path):
            if x.startswith(start_string):
                y = os.path.join(path, x)
                if os.path.isfile(y):
                    targets.append(x + ' ')
                elif os.path.isdir(y):
                    targets.append(x + '/')

        return targets


    ####################### shell commands #######################
    @verify_connected
    def do_ping (self, line):
        '''Ping the server\n'''
        rc = self.stateless_client.ping()
        if rc.bad():
            return


    # set verbose on / off
    def do_verbose(self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print "\nverbose is " + ("on\n" if self.verbose else "off\n")

        elif line == "on":
            self.verbose = True
            self.stateless_client.set_verbose(self.stateless_client.logger.VERBOSE_HIGH)
            print format_text("\nverbose set to on\n", 'green', 'bold')

        elif line == "off":
            self.verbose = False
            self.stateless_client.set_verbose(self.stateless_client.logger.VERBOSE_REGULAR)
            print format_text("\nverbose set to off\n", 'green', 'bold')

        else:
            print format_text("\nplease specify 'on' or 'off'\n", 'bold')

    # show history
    def help_history (self):
        self.do_history("-h")

    def do_history (self, line):
        '''Manage the command history\n'''

        item = parsing_opts.ArgumentPack(['item'],
                                         {"nargs": '?',
                                          'metavar': 'item',
                                          'type': parsing_opts.check_negative,
                                          'help': "an history item index",
                                          'default': 0})

        parser = parsing_opts.gen_parser(self,
                                         "history",
                                         self.do_history.__doc__,
                                         item)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        if opts.item == 0:
            self.print_history()
        else:
            cmd = self.get_history_item(opts.item)
            if cmd == None:
                return

            self.onecmd(cmd)



    ############### connect
    def do_connect (self, line):
        '''Connects to the server\n'''

        self.stateless_client.connect_line(line)


    def do_disconnect (self, line):
        '''Disconnect from the server\n'''

        self.stateless_client.disconnect_line(line)

 
    ############### start

    def complete_start(self, text, line, begidx, endidx):
        s = line.split()
        l = len(s)

        file_flags = parsing_opts.get_flags(parsing_opts.FILE_PATH)

        if (l > 1) and (s[l - 1] in file_flags):
            return TRexConsole.tree_autocomplete("")

        if (l > 2) and (s[l - 2] in file_flags):
            return TRexConsole.tree_autocomplete(s[l - 1])

    @verify_connected_and_rw
    def do_start(self, line):
        '''Start selected traffic in specified port(s) on TRex\n'''

        self.stateless_client.start_line(line)

        


    def help_start(self):
        self.do_start("-h")

    ############# stop
    @verify_connected_and_rw
    def do_stop(self, line):
        '''stops port(s) transmitting traffic\n'''

        self.stateless_client.stop_line(line)

    def help_stop(self):
        self.do_stop("-h")

    ############# update
    @verify_connected_and_rw
    def do_update(self, line):
        '''update speed of port(s)currently transmitting traffic\n'''

        self.stateless_client.update_line(line)

    def help_update (self):
        self.do_update("-h")

    ############# pause
    @verify_connected_and_rw
    def do_pause(self, line):
        '''pause port(s) transmitting traffic\n'''

        self.stateless_client.pause_line(line)

    ############# resume
    @verify_connected_and_rw
    def do_resume(self, line):
        '''resume port(s) transmitting traffic\n'''

        self.stateless_client.resume_line(line)

   

    ########## reset
    @verify_connected_and_rw
    def do_reset (self, line):
        '''force stop all ports\n'''
        self.stateless_client.reset_line(line)


    ######### validate
    @verify_connected
    def do_validate (self, line):
        '''validates port(s) stream configuration\n'''

        self.stateless_client.validate_line(line)


    @verify_connected
    def do_stats(self, line):
        '''Fetch statistics from TRex server by port\n'''
        self.stateless_client.show_stats_line(line)


    def help_stats(self):
        self.do_stats("-h")

    @verify_connected
    def do_streams(self, line):
        '''Fetch statistics from TRex server by port\n'''
        self.stateless_client.show_streams_line(line)


    def help_streams(self):
        self.do_streams("-h")

    @verify_connected
    def do_clear(self, line):
        '''Clear cached local statistics\n'''
        self.stateless_client.clear_stats_line(line)


    def help_clear(self):
        self.do_clear("-h")

  
    def help_events (self):
        self.do_events("-h")

    def do_events (self, line):
        '''shows events recieved from server\n'''

        x = parsing_opts.ArgumentPack(['-c','--clear'],
                                      {'action' : "store_true",
                                       'default': False,
                                       'help': "clear the events log"})

        parser = parsing_opts.gen_parser(self,
                                         "events",
                                         self.do_events.__doc__,
                                         x)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        events = self.stateless_client.get_events()
        for ev in events:
            print ev

        if opts.clear:
            self.stateless_client.clear_events()
            print format_text("\n\nEvent log was cleared\n\n")


    # tui
    @verify_connected
    def do_tui (self, line):
        '''Shows a graphical console\n'''

        parser = parsing_opts.gen_parser(self,
                                         "tui",
                                         self.do_tui.__doc__,
                                         parsing_opts.XTERM)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        if opts.xterm:

            info = self.stateless_client.get_connection_info()

            exe = './trex-console -t -q -s {0} -p {1} --async_port {2}'.format(info['server'], info['sync_port'], info['async_port'])
            cmd = ['xterm', '-geometry', '111x42', '-sl', '0', '-title', 'trex_tui', '-e', exe]
            self.terminal = subprocess.Popen(cmd)

            return


        with self.stateless_client.logger.supress():
            self.tui.show()


    def help_tui (self):
        do_tui("-h")


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
             if ( (cmd == "EOF") or (cmd == "q") or (cmd == "exit") or (cmd == "h")):
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

    # a custorm cmdloop wrapper
    def start(self):
        while True:
            try:
                self.cmdloop()
                break
            except KeyboardInterrupt as e:
                if not readline.get_line_buffer():
                    raise KeyboardInterrupt
                else:
                    print ""
                    self.intro = None
                    continue

        if self.terminal:
            self.terminal.kill()

    # aliases
    do_exit = do_EOF = do_q = do_quit
    do_h = do_history


# run a script of commands
def run_script_file (self, filename, stateless_client):

    self.logger.log(format_text("\nRunning script file '{0}'...".format(filename), 'bold'))

    with open(filename) as f:
        script_lines = f.readlines()

    cmd_table = {}

    # register all the commands
    cmd_table['start'] = stateless_client.start_line
    cmd_table['stop']  = stateless_client.stop_line
    cmd_table['reset'] = stateless_client.reset_line

    for index, line in enumerate(script_lines, start = 1):
        line = line.strip()
        if line == "":
            continue
        if line.startswith("#"):
            continue

        sp = line.split(' ', 1)
        cmd = sp[0]
        if len(sp) == 2:
            args = sp[1]
        else:
            args = ""

        stateless_client.logger.log(format_text("Executing line {0} : '{1}'\n".format(index, line)))

        if not cmd in cmd_table:
            print "\n*** Error at line {0} : '{1}'\n".format(index, line)
            stateless_client.logger.log(format_text("unknown command '{0}'\n".format(cmd), 'bold'))
            return False

        rc = cmd_table[cmd](args)
        if rc.bad():
            return False

    stateless_client.logger.log(format_text("\n[Done]", 'bold'))

    return True


#
def is_valid_file(filename):
    if not os.path.isfile(filename):
        raise argparse.ArgumentTypeError("The file '%s' does not exist" % filename)

    return filename



def setParserOptions():
    parser = argparse.ArgumentParser(prog="trex_console.py")

    parser.add_argument("-s", "--server", help = "TRex Server [default is localhost]",
                        default = "localhost",
                        type = str)

    parser.add_argument("-p", "--port", help = "TRex Server Port  [default is 4501]\n",
                        default = 4501,
                        type = int)

    parser.add_argument("--async_port", help = "TRex ASync Publisher Port [default is 4500]\n",
                        default = 4500,
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

    parser.add_argument("--batch", dest="batch",
                        nargs = 1,
                        type = is_valid_file,
                        help = "Run the console in a batch mode with file",
                        default = None)

    parser.add_argument("-t", "--tui", dest="tui",
                        action="store_true", help="Starts with TUI mode",
                        default = False)


    parser.add_argument("-q", "--quiet", dest="quiet",
                        action="store_true", help="Starts with all outputs suppressed",
                        default = False)

    return parser

    
def main():
    parser = setParserOptions()
    options = parser.parse_args()

    # Stateless client connection
    if options.quiet:
        verbose_level = LoggerApi.VERBOSE_QUIET
    elif options.verbose:
        verbose_level = LoggerApi.VERBOSE_HIGH
    else:
        verbose_level = LoggerApi.VERBOSE_REGULAR

    # Stateless client connection
    logger = ConsoleLogger()
    stateless_client = CTRexStatelessClient(username = options.user,
                                            server = options.server,
                                            sync_port = options.port,
                                            async_port = options.pub,
                                            verbose_level = verbose_level,
                                            logger = logger)

    # TUI or no acquire will give us READ ONLY mode
    try:
        stateless_client.connect("RO")
    except STLError as e:
        logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
        return

    if not options.tui and options.acquire:
        try:
            stateless_client.acquire()
        except STLError as e:
            logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
            logger.log(format_text("\nSwitching to read only mode - only few commands will be available", 'bold'))

  
    # a script mode
    if options.batch:
        cont = run_script_file(options.batch[0], stateless_client)
        if not cont:
            return
        
    # console
    try:
        console = TRexConsole(stateless_client, options.verbose)
        logger.prompt_redraw = console.prompt_redraw

        if options.tui:
            set_window_always_on_top('trex_tui')
            console.do_tui("")
        else:
            console.start()
            
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"

    finally:
        stateless_client.teardown(stop_traffic = False)

if __name__ == '__main__':
    
    main()

