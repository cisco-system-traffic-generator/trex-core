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

import collections
import subprocess
import inspect
import cmd
import json
import argparse
import random
import readline
import string
import os
import sys
import tty, termios
from threading import Lock
from functools import wraps, partial
import threading
import atexit
import tempfile

if __package__ == None:
    print("TRex console must be launched as a module")
    sys.exit(1)

from ..stl.api import *
from ..astf.api import *

from ..common.trex_client import TRexClient

from ..utils.text_opts import *
from ..utils.common import user_input, get_current_user, set_window_always_on_top
from ..utils import parsing_opts

from .trex_capture import CaptureManager
from .plugins_mngr import PluginsManager

from . import trex_tui


__version__ = "3.0"

# readline.write_history_file can fail with IOError in Python2
def write_history_file(hist_file):
    hist_end   = readline.get_current_history_length()
    hist_start = max(0, hist_end - readline.get_history_length())
    with open(hist_file, 'w') as f:
        for i in range(hist_start, hist_end):
            f.write('%s\n' % readline.get_history_item(i + 1))

# console custom logger
class ConsoleLogger(Logger):
    def __init__ (self):
        Logger.__init__(self)
        self.prompt_redraw = lambda: None
        self.tid = threading.current_thread().ident

    def _write (self, msg, newline = True):

        # if printed from another thread - handle it specifcially
        if threading.current_thread().ident != self.tid:
            self._write_async(msg, newline)
        else:
            self._write_sync(msg, newline)


    def _write_sync (self, msg, newline):

        if newline:
            print(msg)
        else:
            print(msg, end=' ')


    def _write_async (self, msg, newline):
        print('\n')
        self._write_sync(msg, newline)
        self.prompt_redraw()
        self._flush()


    def _flush (self):
        sys.stdout.flush()


class TRexGeneralCmd(cmd.Cmd):
    def __init__(self):
        cmd.Cmd.__init__(self)
        # configure history behaviour
        self._history_file_dir = "/tmp/trex/console/"
        self._history_file = self.get_history_file_full_path()
        readline.set_history_length(100)
        # load history, if any
        self.load_console_history()
        atexit.register(self.save_console_history)


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
        try:
            write_history_file(self._history_file)
        except BaseException as e:
            print(bold('\nCould not save history file: %s\nError: %s\n' % (self._history_file, e)))

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



# main console object
class TRexConsole(TRexGeneralCmd):
    """Trex Console"""

    def __init__(self, client, verbose = False):

        # cmd lock is used to make sure background job
        # of the console is not done while the user excutes commands
        self.cmd_lock = Lock()
        
        self.client = client

        TRexGeneralCmd.__init__(self)

        self.tui = trex_tui.TrexTUI(self)
        self.terminal = None

        self.verbose = verbose

        self.intro  = "\n-=TRex Console v{ver}=-\n".format(ver=__version__)
        self.intro += "\nType 'help' or '?' for supported actions\n"

        self.cap_mngr = CaptureManager(client, self.cmd_lock)
        self.plugins_mngr = PluginsManager(self)

        self.postcmd(False, "")

        self.load_client_console_functions()


    ################### internal section ########################

    def verify_connected(f):
        @wraps(f)
        def wrap(*args):
            inst = args[0]
            func_name = f.__name__
            if func_name.startswith("do_"):
                func_name = func_name[3:]
                
            if not inst.client.is_connected():
                print(format_text("\n'{0}' cannot be executed on offline mode\n".format(func_name), 'bold'))
                return

            ret = f(*args)
            return ret

        return wrap

    def history_preserver(self, func, line):
        filename = self._push_history()
        try:
            func(line)
        finally:
            self._pop_history(filename)


    def load_client_console_functions (self):
        for cmd_name, cmd_func in self.client.get_console_methods().items():
            
            # register the function and its help
            if cmd_func.preserve_history:
                f = partial(self.history_preserver, cmd_func)
                f.__doc__ = cmd_func.__doc__
                setattr(self.__class__, 'do_' + cmd_name, f)
            else:
                setattr(self.__class__, 'do_' + cmd_name, cmd_func)
            setattr(self.__class__, 'help_' + cmd_name, lambda _, func = cmd_func: func('-h'))



    def generate_prompt (self, prefix = 'trex'):
        if not self.client.is_connected():
            return "{0}(offline)>".format(prefix)

        elif not self.client.get_acquired_ports():
            return "{0}(read-only)>".format(prefix)

        elif self.client.is_all_ports_acquired():
            
            p = prefix
            
            # HACK
            if self.client.get_mode() == "STL" and self.client.get_service_enabled_ports():
                if self.client.get_service_enabled_ports() == self.client.get_acquired_ports():
                    p += '(service)'
                else:
                    p += '(service: {0})'.format(', '.join(map(str, self.client.get_service_enabled_ports())))
                
            return "{0}>".format(p)

        else:
            return "{0} (ports: {1})>".format(prefix, ', '.join(map(str, self.client.get_acquired_ports())))


    def prompt_redraw (self):
        self.postcmd(False, "")
        sys.stdout.write("\n" + self.prompt + readline.get_line_buffer())
        sys.stdout.flush()


    def get_console_identifier(self):
        return "{context}_{server}".format(context=get_current_user(),
                                           server=self.client.get_connection_info()['server'])
    
    def register_main_console_methods(self):
        main_names = set(self.trex_console.get_names()).difference(set(dir(self.__class__)))
        for name in main_names:
            for prefix in 'do_', 'help_', 'complete_':
                if name.startswith(prefix):
                    self.__dict__[name] = getattr(self.trex_console, name)

    def precmd(self, line):
        lines = line.split(';')
        try:
            self.cmd_lock.acquire()
            for line in lines:
                stop = self.onecmd(line)
                stop = self.postcmd(stop, line)
                if stop:
                    return "quit"
    
            return ""
        except KeyboardInterrupt:
            print(bold('Interrupted by a keyboard signal (probably ctrl + c)'))

        except TRexError as e:
            print(e)

        finally:
            self.cmd_lock.release()
        return ''



    def postcmd(self, stop, line):
        self.prompt = self.generate_prompt(prefix = 'trex')
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

    # set verbose on / off
    def do_verbose(self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print("\nverbose is " + ("on\n" if self.verbose else "off\n"))

        elif line == "on":
            self.verbose = True
            self.client.set_verbose("debug")
            print(format_text("\nverbose set to on\n", 'green', 'bold'))

        elif line == "off":
            self.verbose = False
            self.client.set_verbose("info")
            print(format_text("\nverbose set to off\n", 'green', 'bold'))

        else:
            print(format_text("\nplease specify 'on' or 'off'\n", 'bold'))

    # show history
    def help_history (self):
        self.do_history("-h")

    def do_shell (self, line):
        self.do_history(line)

    def help_plugins(self):
        self.do_plugins('-h')

    @verify_connected
    def do_capture (self, line):
        '''Manage PCAP captures'''
        self.cap_mngr.parse_line(line)

    def help_capture (self):
        self.do_capture("-h")

    # save current history to a temp file
    def _push_history(self):
        tmp_file = tempfile.NamedTemporaryFile()
        write_history_file(tmp_file.name)
        readline.clear_history()
        return tmp_file

    # restore history from a temp file
    def _pop_history(self, tmp_file):
        readline.clear_history()
        readline.read_history_file(tmp_file.name)
        tmp_file.close()

    def do_debug(self, line):
        '''Internal debugger for development.
           Requires IPython module installed
        '''

        parser = parsing_opts.gen_parser(self.client,
                                         "debug",
                                         self.do_debug.__doc__)

        opts = parser.parse_args(line.split())

        try:
            from IPython.terminal.ipapp import load_default_config
            from IPython.terminal.embed import InteractiveShellEmbed
            from IPython import embed

        except ImportError:
            embed = None

        if not embed:
            try:
                import code
            except ImportError:
                self.client.logger.info(format_text("\n*** 'IPython' and 'code' library are not available ***\n", 'bold'))
                return

        auto_completer = readline.get_completer()
        console_history_file = self._push_history()
        client = self.client

        descr = 'IPython' if embed else "'code' library"
        self.client.logger.info(format_text("\n*** Starting Python shell (%s)... use 'client' as client object, Ctrl + D to exit ***\n" % descr, 'bold'))

        try:
            if embed:
                cfg = load_default_config()
                cfg['TerminalInteractiveShell']['confirm_exit'] = False
                embed(config = cfg, display_banner = False)
                #InteractiveShellEmbed.clear_instance()
            else:
                ns = {}
                ns.update(globals())
                ns.update(locals())
                code.InteractiveConsole(ns).interact('')

        finally:
            readline.set_completer(auto_completer)
            self._pop_history(console_history_file)

        self.client.logger.info(format_text("\n*** Leaving Python shell ***\n"))


    def do_history (self, line):
        '''Manage the command history\n'''

        item = parsing_opts.ArgumentPack(['item'],
                                         {"nargs": '?',
                                          'metavar': 'item',
                                          'type': parsing_opts.check_negative,
                                          'help': "an history item index",
                                          'default': 0})

        parser = parsing_opts.gen_parser(self.client,
                                         "history",
                                         self.do_history.__doc__,
                                         item)

        try:
            opts = parser.parse_args(line.split())
        except TRexError:
            return

        if opts.item == 0:
            self.print_history()
        else:
            cmd = self.get_history_item(opts.item)
            if cmd == None:
                return

            print("Executing '{0}'".format(cmd))

            return self.onecmd(cmd)


    def do_plugins(self, line):
        '''Show / load / use plugins\n'''
        self.plugins_mngr.do_plugins(line)


    def complete_plugins(self, text, line, start_index, end_index):
        return self.plugins_mngr.complete_plugins(text, line, start_index, end_index)


    ############### connect
    def do_connect (self, line):
        '''Connects to the server and acquire ports\n'''

        self.client.connect_line(line)

    def do_disconnect (self, line):
        '''Disconnect from the server\n'''
        
        # stop any monitors before disconnecting
        self.plugins_mngr._unload_plugin()
        self.cap_mngr.stop()
        self.client.disconnect_line(line)


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
    complete_hello = complete_start


    def complete_profile(self, text, line, begidx, endidx):
        return self.complete_start(text,line, begidx, endidx)

    # tui
    @verify_connected
    def do_tui (self, line):
        '''Shows a graphical console\n'''
        parser = parsing_opts.gen_parser(self.client,
                                         "tui",
                                         self.do_tui.__doc__,
                                         parsing_opts.XTERM,
                                         parsing_opts.LOCKED)

        try:
            opts = parser.parse_args(line.split())
        except TRexError:
            return

        if opts.xterm:
            if not os.path.exists('/usr/bin/xterm'):
                print(format_text("XTERM does not exists on this machine", 'bold'))
                return

            info = self.client.get_connection_info()

            exe = './trex-console --top -t -q -s {0} -p {1} --async_port {2}'.format(info['server'], info['sync_port'], info['async_port'])
            cmd = ['/usr/bin/xterm', '-geometry', '{0}x{1}'.format(self.tui.MIN_COLS, self.tui.MIN_ROWS), '-sl', '0', '-title', 'trex_tui', '-e', exe]

            # detach child
            self.terminal = subprocess.Popen(cmd, preexec_fn = os.setpgrp)

            return

        
        try:
            with self.client.logger.supress(verbose = 'none'):
                self.tui.show(self.client, self.save_console_history, locked = opts.locked)

        except self.tui.ScreenSizeException as e:
            print(format_text(str(e) + "\n", 'bold'))


    def help_tui (self):
        do_tui("-h")


    # quit function
    def do_quit(self, line):
        '''Exit the console\n'''
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
    
    
         cmds = [x[3:] for x in self.get_names() if x.startswith("do_")]
         hidden = ['EOF', 'q', 'exit', 'h', 'shell']

         categories = collections.defaultdict(list)
         
         for cmd in cmds:
             if cmd in hidden:
                 continue

             category = getattr(getattr(self, 'do_' + cmd), 'group', 'basic')
             categories[category].append(cmd)
         
         # basic commands
         if 'basic' in categories:
             self._help_cmds('Console Commands', categories['basic'])

         # common
         if 'common' in categories:
             self._help_cmds('Common Commands', categories['common'])

         if 'STL' in categories:
             self._help_cmds('Stateless Commands', categories['STL'])

         if 'ASTF' in categories:
             self._help_cmds('Advanced Stateful Commands', categories['ASTF'])


    def _help_cmds (self, title, cmds):

        print(format_text("\n{0}:\n".format(title), 'bold', 'underline'))

        for cmd in cmds:
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
        try:
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
    
        finally:
            # capture manager is not presistent - kill it before going out
            self.plugins_mngr._unload_plugin()
            self.cap_mngr.stop()

        if self.terminal:
            self.terminal.kill()

    # aliases
    do_exit = do_EOF = do_q = do_quit
    do_h = do_history


# run a script of commands
def run_script_file(filename, client):

    client.logger.info(format_text("\nRunning script file '{0}'...".format(filename), 'bold'))

    with open(filename) as f:
        script_lines = f.readlines()

    # register all the commands
    cmd_table = {}

    for cmd_name, cmd_func in client.get_console_methods().items():
        cmd_table[cmd_name] = cmd_func

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

        client.logger.info(format_text("Executing line {0} : '{1}'\n".format(index, line)))

        if cmd not in cmd_table:
            client.logger.error(format_text("Unknown command '%s', available commands are:\n%s" % (cmd, '\n'.join(sorted(cmd_table.keys()))), 'bold'))
            return False

        rc = cmd_table[cmd](args)
        if isinstance(rc, RC) and not rc:
            return False

    client.logger.info(format_text("\n[Done]", 'bold'))

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
    modes = {'STL': 'Stateless', 'ASTF': 'Advanced Stateful'}
    
    x    = c.get_server_system_info()
    ver  = c.get_server_version().get('version', 'N/A')
    mode = c.get_server_version().get('mode', 'N/A')
    
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

    logger.info(format_text("\nServer Info:\n", 'underline'))
    logger.info("Server version:   {:>}".format(format_text(ver + ' @ ' + mode, 'bold')))
    logger.info("Server mode:      {:>}".format(format_text(modes.get(mode, 'N/A'), 'bold')))
    logger.info("Server CPU:       {:>}".format(format_text("{:>} x {:>}".format(x.get('dp_core_count'), x.get('core_type')), 'bold')))
    logger.info("Ports count:      {:>}".format(format_text(port_line, 'bold')))




def probe_server_mode (options):
    # first we create a 'dummy' client and probe the server
    client = TRexClient(username = options.user,
                        server = options.server,
                        sync_port = options.port,
                        async_port = options.pub,
                        logger = ConsoleLogger(),
                        verbose_level = 'error')

    return client.probe_server()['mode']


def main():
    parser = setParserOptions()
    options = parser.parse_args()

    if options.xtui:
        options.tui = True

    # always on top
    if options.top:
        set_window_always_on_top('trex_tui')


    # verbose
    if options.quiet:
        verbose_level = "none"

    elif options.verbose:
        verbose_level = "debug"

    else:
        verbose_level = "info"


    logger = ConsoleLogger()

    # determine server mode
    try:
        mode = probe_server_mode(options)
    except TRexError as e:
        logger.error("Log:\n" + format_text(e.brief() + "\n", 'bold'))
        return


    if mode == 'STL':
        client = STLClient(username = options.user,
                           server = options.server,
                           sync_port = options.port,
                           async_port = options.pub,
                           logger = logger,
                           verbose_level = verbose_level)
    elif mode == 'ASTF':
        client = ASTFClient(username = options.user,
                            server = options.server,
                            sync_port = options.port,
                            async_port = options.pub,
                            logger = logger,
                            verbose_level = verbose_level)

    else:
        logger.critical("Unknown server mode: '{0}'".format(mode))
        return


    try:
        client.connect()
    except TRexError as e:
        logger.error("Log:\n" + format_text(e.brief() + "\n", 'bold'))
        return

    # TUI or no acquire will give us READ ONLY mode
    if not options.tui and not options.readonly:
        try:
            # acquire all ports
            client.acquire(options.acquire, force = options.force)
        except TRexError as e:
            logger.error("Log:\n" + format_text(e.brief() + "\n", 'bold'))
            
            logger.error("\n*** Failed to acquire all required ports ***\n")
            return

    if options.readonly:
        logger.info(format_text("\nRead only mode - only few commands will be available", 'bold'))

    # console
    try:
        show_intro(logger, client)

        # a script mode
        if options.batch:
            cont = run_script_file(options.batch[0], client)
            if not cont:
                return

        console = TRexConsole(client, options.verbose)
        logger.prompt_redraw = console.prompt_redraw

        # TUI
        if options.tui:
            console.do_tui("-x" if options.xtui else "-l")

        else:
            console.start()
            
    except KeyboardInterrupt as e:
        print("\n\n*** Caught Ctrl + C... Exiting...\n\n")

    finally:
        with client.logger.supress():
            client.disconnect(stop_traffic = False)


if __name__ == '__main__':
    main()

