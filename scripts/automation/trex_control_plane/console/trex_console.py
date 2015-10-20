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

import sys
import tty, termios
import trex_root_path
from common.trex_streams import *


from client_utils.jsonrpc_client import TrexStatelessClient
import trex_status
from collections import namedtuple

LoadedStreamList = namedtuple('LoadedStreamList', ['loaded', 'compiled'])

#

def readch (choices = []):
        
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        while True:
            ch = sys.stdin.read(1)
            if (ord(ch) == 3) or (ord(ch) == 4):
                return None
            if ch in choices:
                return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

    return None

class YesNoMenu(object):
    def __init__ (self, caption):
        self.caption = caption

    def show (self):
        print "{0}".format(self.caption)
        sys.stdout.write("[Y/y/N/n] : ")
        ch = readch(choices = ['y', 'Y', 'n', 'N'])
        if ch == None:
            return None
    
        print "\n"    
        if ch == 'y' or ch == 'Y':
            return True
        else:
            return False

# multi level cmd menu
class CmdMenu(object):
    def __init__ (self):
        self.menus = []


    def add_menu (self, caption, options):
        menu = {}
        menu['caption'] = caption
        menu['options'] = options
        self.menus.append(menu)

    def show (self):
        cur_level = 0
        print "\n"

        selected_path = []
        for menu in self.menus:
            # show all the options
            print "{0}\n".format(menu['caption'])
            for i, option in enumerate(menu['options']):
                print "{0}. {1}".format(i + 1, option)

            #print "\nPlease select an option: "

            choices = range(0, len(menu['options']))
            choices = [ chr(x + 48) for x in choices]

            print ""
            ch = readch(choices)
            print ""

            if ch == None:
                return None

            selected_path.append(int(ch) - 1)

        return selected_path


class AddStreamMenu(CmdMenu):
    def __init__ (self):
        super(AddStreamMenu, self).__init__()
        self.add_menu('Please select type of stream', ['a', 'b', 'c'])
        self.add_menu('Please select ISG', ['d', 'e', 'f'])

# main console object
class TrexConsole(cmd.Cmd):
    """Trex Console"""
   
    def __init__(self, rpc_client):
        cmd.Cmd.__init__(self)

        self.rpc_client = rpc_client

        self.do_connect("")

        self.intro  = "\n-=TRex Console V1.0=-\n"
        self.intro += "\nType 'help' or '?' for supported actions\n" 

        self.verbose = False

        self.postcmd(False, "")

        self.user_streams = {}
      

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    # set verbose on / off
    def do_verbose (self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print "\nverbose is " + ("on\n" if self.verbose else "off\n")

        elif line == "on":
            self.verbose = True
            self.rpc_client.set_verbose(True)
            print "\nverbose set to on\n"

        elif line == "off":
            self.verbose = False
            self.rpc_client.set_verbose(False)
            print "\nverbose set to off\n"

        else:
            print "\nplease specify 'on' or 'off'\n"

    # query the server for registered commands
    def do_query_server(self, line):
        '''query the RPC server for supported remote commands\n'''

        rc, msg = self.rpc_client.query_rpc_server()
        if not rc:
            print "\n*** " + msg + "\n"
            return

        print "\nRPC server supports the following commands: \n\n"
        for func in msg:
            if func:
                print func
        print "\n"

    def do_ping (self, line):
        '''Pings the RPC server\n'''

        print "\n-> Pinging RPC server"

        rc, msg = self.rpc_client.ping_rpc_server()
        if rc:
            print "[SUCCESS]\n"
        else:
            print "\n*** " + msg + "\n"
            return

    def do_force_acquire (self, line):
        '''Acquires ports by force\n'''

        self.do_acquire(line, True)

    def parse_ports_from_line (self, line):
        port_list = set()

        if line:
            for port_id in line.split(' '):
                if (not port_id.isdigit()) or (int(port_id) < 0) or (int(port_id) >= self.rpc_client.get_port_count()):
                    print "Please provide a list of ports seperated by spaces between 0 and {0}".format(self.rpc_client.get_port_count() - 1)
                    return None

                port_list.add(int(port_id))

            port_list = list(port_list)

        else:
            port_list = [i for i in xrange(0, self.rpc_client.get_port_count())]

        return port_list

    def do_acquire (self, line, force = False):
        '''Acquire ports\n'''

        # make sure that the user wants to acquire all
        if line == "":
            ask = YesNoMenu('Do you want to acquire all ports ? ')
            rc = ask.show()
            if rc == False:
                return

        port_list = self.parse_ports_from_line(line)
        if not port_list:
            return

        print "\nTrying to acquire ports: " + (" ".join(str(x) for x in port_list)) + "\n"

        rc, resp_list = self.rpc_client.take_ownership(port_list, force)

        if not rc:
            print "\n*** " + resp_list + "\n"
            return

        for i, rc in enumerate(resp_list):
            if rc[0]:
                print "Port {0} - Acquired".format(port_list[i])
            else:
                print "Port {0} - ".format(port_list[i]) + rc[1]

        print "\n"

    def do_release (self, line):
        '''Release ports\n'''

        if line:
            port_list = self.parse_ports_from_line(line)
        else:
            port_list = self.rpc_client.get_owned_ports()

        if not port_list:
            return

        rc, resp_list = self.rpc_client.release_ports(port_list)


        print "\n"

        for i, rc in enumerate(resp_list):
            if rc[0]:
                print "Port {0} - Released".format(port_list[i])
            else:
                print "Port {0} - Failed to release port, probably not owned by you or port is under traffic"

        print "\n"

    def do_get_port_stats (self, line):
        '''Get ports stats\n'''

        port_list = self.parse_ports_from_line(line)
        if not port_list:
            return

        rc, resp_list = self.rpc_client.get_port_stats(port_list)

        if not rc:
            print "\n*** " + resp_list + "\n"
            return

        for i, rc in enumerate(resp_list):
            if rc[0]:
                print "\nPort {0} stats:\n{1}\n".format(port_list[i], self.rpc_client.pretty_json(json.dumps(rc[1])))
            else:
                print "\nPort {0} - ".format(i) + rc[1] + "\n"

        print "\n"


    def do_connect (self, line):
        '''Connects to the server\n'''

        if line == "":
            rc, msg = self.rpc_client.connect()
        else:
            sp = line.split()
            if (len(sp) != 2):
                print "\n[usage] connect [server] [port] or without parameters\n"
                return

            rc, msg = self.rpc_client.connect(sp[0], sp[1])

        if rc:
            print "[SUCCESS]\n"
        else:
            print "\n*** " + msg + "\n"
            return

        self.supported_rpc = self.rpc_client.get_supported_cmds()

    def do_rpc (self, line):
        '''Launches a RPC on the server\n'''

        if line == "":
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        sp = line.split(' ', 1)
        method = sp[0]

        params = None
        bad_parse = False
        if len(sp) > 1:

            try:
                params = ast.literal_eval(sp[1])
                if not isinstance(params, dict):
                    bad_parse = True

            except ValueError as e1:
                bad_parse = True
            except SyntaxError as e2:
                bad_parse = True

        if bad_parse:
            print "\nValue should be a valid dict: '{0}'".format(sp[1])
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        rc, msg = self.rpc_client.invoke_rpc_method(method, params)
        if rc:
            print "\nServer Response:\n\n" + self.rpc_client.pretty_json(json.dumps(msg)) + "\n"
        else:
            print "\n*** " + msg + "\n"
            #print "Please try 'reconnect' to reconnect to server"


    def complete_rpc (self, text, line, begidx, endidx):
        return [x for x in self.supported_rpc if x.startswith(text)]

    def do_status (self, line):
        '''Shows a graphical console\n'''

        self.do_verbose('off')
        trex_status.show_trex_status(self.rpc_client)

    def do_quit(self, line):
        '''Exit the client\n'''
        return True

    def do_disconnect (self, line):
        '''Disconnect from the server\n'''
        if not self.rpc_client.is_connected():
            print "Not connected to server\n"
            return

        rc, msg = self.rpc_client.disconnect()
        if rc:
            print "[SUCCESS]\n"
        else:
            print msg + "\n"

    def do_whoami (self, line):
        '''Prints console user name\n'''
        print "\n" + self.rpc_client.whoami() + "\n"
        
    def postcmd(self, stop, line):
        if self.rpc_client.is_connected():
            self.prompt = "TRex > "
        else:
            self.supported_rpc = None
            self.prompt = "TRex (offline) > "

        return stop

    def default(self, line):
        print "'{0}' is an unrecognized command. type 'help' or '?' for a list\n".format(line)

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
            if cmd == "EOF":
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

    def do_load_stream_list(self, line):
        '''Loads a YAML stream list serialization into user console \n'''
        args = line.split()
        if args >= 2:
            name = args[0]
            yaml_path = args[1]
            try:
                multiplier = args[2]
            except IndexError:
                multiplier = 1
            stream_list = CStreamList()
            loaded_obj = stream_list.load_yaml(yaml_path, multiplier)
            # print self.rpc_client.pretty_json(json.dumps(loaded_obj))
            if name in self.user_streams:
                print "Picked name already exist. Please pick another name."
            else:
                try:
                    compiled_streams = stream_list.compile_streams()
                    self.user_streams[name] = LoadedStreamList(loaded_obj,
                                                               [StreamPack(v.stream_id, v.stream.dump_compiled())
                                                                for k, v in compiled_streams.items()])

                    print "Stream '{0}' loaded successfully".format(name)
                except Exception as e:
                    raise
            return
        else:
            print "please provide load name and YAML path, separated by space.\n" \
                  "Optionally, you may provide a third argument to specify multiplier."

    def do_show_stream_list(self, line):
        '''Shows the loaded stream list named [name] \n'''
        args = line.split()
        if args:
            list_name = args[0]
            try:
                stream = self.user_streams[list_name]
                if len(args) >= 2 and args[1] == "full":
                    print self.rpc_client.pretty_json(json.dumps(stream.compiled))
                else:
                    print self.rpc_client.pretty_json(json.dumps(stream.loaded))
            except KeyError as e:
                print "Unknown stream list name provided"
        else:
            print "\nAvailable stream lists:\n{0}".format(', '.join([x
                                                                  for x in self.user_streams.keys()]))

    def complete_show_stream_list (self, text, line, begidx, endidx):
        return [x
                for x in self.user_streams.keys()
                if x.startswith(text)]

    def do_attach(self, line):
        args = line.split()
        if len(args) >= 1:
            try:
                stream_list = self.user_streams[args[0]]
                port_list = self.parse_ports_from_line(' '.join(args[1:]))
                owned = set(self.rpc_client.get_owned_ports())
                if set(port_list).issubset(owned):
                    rc, resp_list = self.rpc_client.add_stream(port_list, stream_list.compiled)
                    if not rc:
                        print "\n*** " + resp_list + "\n"
                        return
                else:
                    print "Not all desired ports are aquired.\n" \
                          "Acquired ports are: {acq}\n" \
                          "Requested ports:    {req}\n" \
                          "Missing ports:      {miss}".format(acq=list(owned),
                                                              req=port_list,
                                                              miss=list(set(port_list).difference(owned)))
            except KeyError as e:
                cause = e.args[0]
                print "Provided stream list name '{0}' doesn't exists.".format(cause)
        else:
            print "Please provide list name and ports to attach to, or leave empty to attach to all ports."











    # adds a very simple stream
    def do_add_simple_stream (self, line):
        if line == "":
            add_stream = AddStreamMenu()
            add_stream.show()
            return

        params = line.split()
        port_id = int(params[0])
        stream_id = int(params[1])

        packet = [0xFF,0xFF,0xFF]
        rc, msg = self.rpc_client.add_stream(port_id = port_id, stream_id = stream_id, isg = 1.1, next_stream_id = -1, packet = packet)
        if rc:
            print "\nServer Response:\n\n" + self.rpc_client.pretty_json(json.dumps(msg)) + "\n"
        else:
            print "\n*** " + msg + "\n"

    # aliasing
    do_exit = do_EOF = do_q = do_quit

def setParserOptions ():
    parser = argparse.ArgumentParser(prog="trex_console.py")

    parser.add_argument("-s", "--server", help = "TRex Server [default is localhost]",
                        default = "localhost",
                        type = str)

    parser.add_argument("-p", "--port", help = "TRex Server Port  [default is 5050]\n",
                        default = 5050,
                        type = int)

    parser.add_argument("-u", "--user", help = "User Name  [default is random generated]\n",
                        default = 'user_' + ''.join(random.choice(string.digits) for _ in range(5)),
                        type = str)

    return parser

def main ():
    parser = setParserOptions()
    options = parser.parse_args(sys.argv[1:])

    # RPC client
    rpc_client = TrexStatelessClient(options.server, options.port, options.user)

    # console
    try:
        console = TrexConsole(rpc_client)
        console.cmdloop()
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        return

if __name__ == '__main__':
    main()

