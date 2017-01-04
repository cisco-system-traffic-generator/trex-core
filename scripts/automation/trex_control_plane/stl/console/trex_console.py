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
from __future__ import print_function

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

try:
    import stl_path
except:
    from . import stl_path
from trex_stl_lib.api import *

from trex_stl_lib.utils.text_opts import *
from trex_stl_lib.utils.common import user_input, get_current_user
from trex_stl_lib.utils import parsing_opts

try:
    import trex_tui
except:
    from . import trex_tui

from functools import wraps

__version__ = "2.0"

# console custom logger
class ConsoleLogger(LoggerApi):
    def __init__ (self):
        self.prompt_redraw = None

    def write (self, msg, newline = True):
        if newline:
            print(msg)
        else:
            print(msg, end=' ')

    def flush (self):
        sys.stdout.flush()

    # override this for the prompt fix
    def async_log (self, msg, level = LoggerApi.VERBOSE_REGULAR, newline = True):
        self.log(msg, level, newline)
        if ( (self.level >= LoggerApi.VERBOSE_REGULAR) and self.prompt_redraw ):
            self.prompt_redraw()
            self.flush()


def set_window_always_on_top (title):
    # we need the GDK module, if not available - ignroe this command
    try:
        if sys.version_info < (3,0):
            from gtk import gdk
        else:
            #from gi.repository import Gdk as gdk
            return

    except ImportError:
        return

    # search the window and set it as above
    root = gdk.get_default_root_window()

    for id in root.property_get('_NET_CLIENT_LIST')[2]:
        w = gdk.window_foreign_new(id)
        if w:
            name = w.property_get('WM_NAME')[2]
            if name == title:
                w.set_keep_above(True)
                gdk.window_process_all_updates()
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
                os.makedirs(self._history_file_dir, mode = 0o777)
            finally:
                os.umask(original_umask)

            
        # os.mknod(self._history_file)
        readline.write_history_file(self._history_file)
        return

    def print_history (self):
        
        length = readline.get_current_history_length()

        for i in range(1, length + 1):
            cmd = readline.get_history_item(i)
            print("{:<5}   {:}".format(i, cmd))

    def get_history_item (self, index):
        length = readline.get_current_history_length()
        if index > length:
            print(format_text("please select an index between {0} and {1}".format(0, length)))
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
        self.postcmd(False, "")
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
                print(format_text("\n'{0}' cannot be executed on offline mode\n".format(func_name), 'bold'))
                return

            ret = f(*args)
            return ret

        return wrap

    
    def get_console_identifier(self):
        return "{context}_{server}".format(context=get_current_user(),
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
        try:
            for line in lines:
                stop = self.onecmd(line)
                stop = self.postcmd(stop, line)
                if stop:
                    return "quit"
    
            return ""
        except STLError as e:
            print(e)
            return ''


    def postcmd(self, stop, line):
        self.prompt = self.stateless_client.generate_prompt(prefix = 'trex')
        return stop


    def default(self, line):
        print("'{0}' is an unrecognized command. type 'help' or '?' for a list\n".format(line))

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
        self.stateless_client.ping_line(line)


    @verify_connected
    def do_shutdown (self, line):
        '''Sends the server a shutdown request\n'''
        self.stateless_client.shutdown_line(line)

    # set verbose on / off
    def do_verbose(self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print("\nverbose is " + ("on\n" if self.verbose else "off\n"))

        elif line == "on":
            self.verbose = True
            self.stateless_client.set_verbose("high")
            print(format_text("\nverbose set to on\n", 'green', 'bold'))

        elif line == "off":
            self.verbose = False
            self.stateless_client.set_verbose("normal")
            print(format_text("\nverbose set to off\n", 'green', 'bold'))

        else:
            print(format_text("\nplease specify 'on' or 'off'\n", 'bold'))

    # show history
    def help_history (self):
        self.do_history("-h")

    def do_shell (self, line):
        self.do_history(line)

    @verify_connected
    def do_push (self, line):
        '''Push a local PCAP file\n'''
        self.stateless_client.push_line(line)

    def help_push (self):
        self.do_push("-h")

    @verify_connected
    def do_portattr (self, line):
        '''Change/show port(s) attributes\n'''
        self.stateless_client.set_port_attr_line(line)

    def help_portattr (self):
        self.do_portattr("-h")

    @verify_connected
    def do_l2 (self, line):
        '''Configures a port in L2 mode'''
        self.stateless_client.set_l2_mode_line(line)
        
    def help_l2 (self):
        self.do_l2("-h")
    
    @verify_connected
    def do_l3 (self, line):
        '''Configures a port in L3 mode'''
        self.stateless_client.set_l3_mode_line(line)

    def help_l3 (self):
        self.do_l3("-h")

        
    @verify_connected
    def do_set_rx_sniffer (self, line):
        '''Sets a port sniffer on RX channel as PCAP recorder'''
        self.stateless_client.set_rx_sniffer_line(line)

    def help_sniffer (self):
        self.do_set_rx_sniffer("-h")

    @verify_connected
    def do_resolve (self, line):
        '''Resolve ARP for ports'''
        self.stateless_client.resolve_line(line)

    def help_resolve (self):
        self.do_resolve("-h")

    do_arp = do_resolve
    help_arp = help_resolve
    
    @verify_connected
    def do_map (self, line):
        '''Maps ports topology\n'''
        ports = self.stateless_client.get_acquired_ports()
        if not ports:
            print("No ports acquired\n")
            return

        
        try:    
            with self.stateless_client.logger.supress():
                table = stl_map_ports(self.stateless_client, ports = ports)
        except STLError as e:
            print(format_text(e.brief() + "\n", 'bold'))
            return

        
        print(format_text('\nAcquired ports topology:\n', 'bold', 'underline'))

        # bi-dir ports
        print(format_text('Bi-directional ports:\n','underline'))
        for port_a, port_b in table['bi']:
            print("port {0} <--> port {1}".format(port_a, port_b))

        print("")

        # unknown ports
        print(format_text('Mapping unknown:\n','underline'))
        for port in table['unknown']:
            print("port {0}".format(port))
        print("")

       
      

    def do_history (self, line):
        '''Manage the command history\n'''

        item = parsing_opts.ArgumentPack(['item'],
                                         {"nargs": '?',
                                          'metavar': 'item',
                                          'type': parsing_opts.check_negative,
                                          'help': "an history item index",
                                          'default': 0})

        parser = parsing_opts.gen_parser(self.stateless_client,
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

            print("Executing '{0}'".format(cmd))

            return self.onecmd(cmd)



    ############### connect
    def do_connect (self, line):
        '''Connects to the server and acquire ports\n'''

        self.stateless_client.connect_line(line)

    def help_connect (self):
        self.do_connect("-h")

    def do_disconnect (self, line):
        '''Disconnect from the server\n'''

        self.stateless_client.disconnect_line(line)


    @verify_connected
    def do_acquire (self, line):
        '''Acquire ports\n'''

        self.stateless_client.acquire_line(line)


    @verify_connected
    def do_release (self, line):
        '''Release ports\n'''
        self.stateless_client.release_line(line)

    @verify_connected
    def do_reacquire (self, line):
        '''reacquire all the ports under your logged user name'''
        self.stateless_client.reacquire_line(line)

    def help_acquire (self):
        self.do_acquire("-h")

    def help_release (self):
        self.do_release("-h")

    def help_reacquire (self):
        self.do_reacquire("-h")

    ############### start

    def complete_start(self, text, line, begidx, endidx):
        s = line.split()
        l = len(s)

        file_flags = parsing_opts.get_flags(parsing_opts.FILE_PATH)

        if (l > 1) and (s[l - 1] in file_flags):
            return TRexConsole.tree_autocomplete("")

        if (l > 2) and (s[l - 2] in file_flags):
            return TRexConsole.tree_autocomplete(s[l - 1])

    complete_push = complete_start

    @verify_connected
    def do_start(self, line):
        '''Start selected traffic in specified port(s) on TRex\n'''

        self.stateless_client.start_line(line)



    def help_start(self):
        self.do_start("-h")

    ############# stop
    @verify_connected
    def do_stop(self, line):
        '''stops port(s) transmitting traffic\n'''

        self.stateless_client.stop_line(line)

    def help_stop(self):
        self.do_stop("-h")

    ############# update
    @verify_connected
    def do_update(self, line):
        '''update speed of port(s) currently transmitting traffic\n'''

        self.stateless_client.update_line(line)

    def help_update (self):
        self.do_update("-h")

    ############# pause
    @verify_connected
    def do_pause(self, line):
        '''pause port(s) transmitting traffic\n'''

        self.stateless_client.pause_line(line)

    ############# resume
    @verify_connected
    def do_resume(self, line):
        '''resume port(s) transmitting traffic\n'''

        self.stateless_client.resume_line(line)

   

    ########## reset
    @verify_connected
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

    @verify_connected
    def do_service (self, line):
        '''Sets port(s) service mode state'''
        self.stateless_client.service_line(line)
        
    def help_service (self, line):
        self.do_service("-h")

    def help_clear(self):
        self.do_clear("-h")

  
    def help_events (self):
        self.do_events("-h")

    def do_events (self, line):
        '''shows events recieved from server\n'''
        self.stateless_client.get_events_line(line)


    def complete_profile(self, text, line, begidx, endidx):
        return self.complete_start(text,line, begidx, endidx)

    def do_profile (self, line):
        '''shows information about a profile'''
        self.stateless_client.show_profile_line(line)

    # tui
    @verify_connected
    def do_tui (self, line):
        '''Shows a graphical console\n'''
        parser = parsing_opts.gen_parser(self.stateless_client,
                                         "tui",
                                         self.do_tui.__doc__,
                                         parsing_opts.XTERM,
                                         parsing_opts.LOCKED)

        opts = parser.parse_args(line.split())

        if not opts:
            return opts
        if opts.xterm:
            if not os.path.exists('/usr/bin/xterm'):
                print(format_text("XTERM does not exists on this machine", 'bold'))
                return

            info = self.stateless_client.get_connection_info()

            exe = './trex-console --top -t -q -s {0} -p {1} --async_port {2}'.format(info['server'], info['sync_port'], info['async_port'])
            cmd = ['/usr/bin/xterm', '-geometry', '{0}x{1}'.format(self.tui.MIN_COLS, self.tui.MIN_ROWS), '-sl', '0', '-title', 'trex_tui', '-e', exe]

            # detach child
            self.terminal = subprocess.Popen(cmd, preexec_fn = os.setpgrp)

            return

        
        try:
            with self.stateless_client.logger.supress():
                self.tui.show(self.stateless_client, self.save_console_history, locked = opts.locked)

        except self.tui.ScreenSizeException as e:
            print(format_text(str(e) + "\n", 'bold'))


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
    
         print("\nSupported Console Commands:")
         print("----------------------------\n")
    
         cmds =  [x[3:] for x in self.get_names() if x.startswith("do_")]
         hidden = ['EOF', 'q', 'exit', 'h', 'shell']
         for cmd in cmds:
             if cmd in hidden:
                 continue
    
             try:
                 doc = getattr(self, 'do_' + cmd).__doc__
                 if doc:
                     help = str(doc)
                 else:
                     help = "*** Undocumented Function ***\n"
             except AttributeError:
                 help = "*** Undocumented Function ***\n"

             l=help.splitlines()
             print("{:<30} {:<30}".format(cmd + " - ",l[0] ))

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
                    print("")
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
            print("\n*** Error at line {0} : '{1}'\n".format(index, line))
            stateless_client.logger.log(format_text("unknown command '{0}'\n".format(cmd), 'bold'))
            return False

        cmd_table[cmd](args)

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

    parser.add_argument("-v", "--verbose", dest="verbose",
                        action="store_true", help="Switch ON verbose option. Default is: OFF.",
                        default = False)


    group = parser.add_mutually_exclusive_group()

    group.add_argument("-a", "--acquire", dest="acquire",
                       nargs = '+',
                       type = int,
                       help="Acquire ports on connect. default is all available ports",
                       default = None)

    group.add_argument("-r", "--readonly", dest="readonly",
                       action="store_true",
                       help="Starts console in a read only mode",
                       default = False)


    parser.add_argument("-f", "--force", dest="force",
                        action="store_true",
                        help="Force acquire the requested ports",
                        default = False)

    parser.add_argument("--batch", dest="batch",
                        nargs = 1,
                        type = is_valid_file,
                        help = "Run the console in a batch mode with file",
                        default = None)

    parser.add_argument("-t", "--tui", dest="tui",
                        action="store_true", help="Starts with TUI mode",
                        default = False)

    parser.add_argument("-x", "--xtui", dest="xtui",
                        action="store_true", help="Starts with XTERM TUI mode",
                        default = False)

    parser.add_argument("--top", dest="top",
                        action="store_true", help="Set the window as always on top",
                        default = False)

    parser.add_argument("-q", "--quiet", dest="quiet",
                        action="store_true", help="Starts with all outputs suppressed",
                        default = False)

    return parser

# a simple info printed on log on
def show_intro (logger, c):
    x   = c.get_server_system_info()
    ver = c.get_server_version().get('version', 'N/A')

    # find out which NICs the server has
    port_types = {}
    for port in x['ports']:
        if 'supp_speeds' in port and port['supp_speeds']:
            speed = max(port['supp_speeds']) // 1000
        else:
            speed = c.ports[port['index']].get_speed_gbps()
        key = (speed, port.get('description', port['driver']))
        if key not in port_types:
            port_types[key] = 0
        port_types[key] += 1

    port_line = ''
    for k, v in port_types.items():
        port_line += "{0} x {1}Gbps @ {2}\t".format(v, k[0], k[1])

    logger.log(format_text("\nServer Info:\n", 'underline'))
    logger.log("Server version:   {:>}".format(format_text(ver, 'bold')))
    logger.log("Server CPU:       {:>}".format(format_text("{:>} x {:>}".format(x.get('dp_core_count'), x.get('core_type')), 'bold')))
    logger.log("Ports count:      {:>}".format(format_text(port_line, 'bold')))


def main():
    parser = setParserOptions()
    options = parser.parse_args()

    if options.xtui:
        options.tui = True

    # always on top
    if options.top:
        set_window_always_on_top('trex_tui')


    # Stateless client connection
    if options.quiet:
        verbose_level = LoggerApi.VERBOSE_QUIET
    elif options.verbose:
        verbose_level = LoggerApi.VERBOSE_HIGH
    else:
        verbose_level = LoggerApi.VERBOSE_REGULAR

    # Stateless client connection
    logger = ConsoleLogger()
    stateless_client = STLClient(username = options.user,
                                 server = options.server,
                                 sync_port = options.port,
                                 async_port = options.pub,
                                 verbose_level = verbose_level,
                                 logger = logger)

    # TUI or no acquire will give us READ ONLY mode
    try:
        stateless_client.connect()
    except STLError as e:
        logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
        return

    if not options.tui and not options.readonly:
        try:
            # acquire all ports
            stateless_client.acquire(options.acquire, force = options.force)
        except STLError as e:
            logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
            
            logger.log("\n*** Failed to acquire all required ports ***\n")
            return

    if options.readonly:
        logger.log(format_text("\nRead only mode - only few commands will be available", 'bold'))

    # console
    try:
        show_intro(logger, stateless_client)

        # a script mode
        if options.batch:
            cont = run_script_file(options.batch[0], stateless_client)
            if not cont:
                return

        console = TRexConsole(stateless_client, options.verbose)
        logger.prompt_redraw = console.prompt_redraw

        # TUI
        if options.tui:
            console.do_tui("-x" if options.xtui else "-l")

        else:
            console.start()
            
    except KeyboardInterrupt as e:
        print("\n\n*** Caught Ctrl + C... Exiting...\n\n")

    finally:
        with stateless_client.logger.supress():
            stateless_client.disconnect(stop_traffic = False)

if __name__ == '__main__':
    
    main()

