#!/usr/bin/python
# -*- coding: utf-8 -*- 
import cmd
import json
import ast

from trex_rpc_client import RpcClient
import trex_status

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
      

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    # set verbose on / off
    def do_verbose (self, line):
        '''shows or set verbose mode\n'''
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

    def do_connect (self, line):
        '''Connects to the server\n'''
        rc, msg = self.rpc_client.connect()
        if rc:
            print "[SUCCESS]\n"
        else:
            print "\n*** " + msg + "\n"
            return

        rc, msg = self.rpc_client.query_rpc_server()

        if rc:
            self.supported_rpc = [str(x) for x in msg if x]

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
            print "\nServer Response:\n\n" + json.dumps(msg) + "\n"
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
        '''exit the client\n'''
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


    # aliasing
    do_exit = do_EOF = do_q = do_quit

def main ():
    # RPC client
    rpc_client = RpcClient("localhost", 5050)

    # console
    try:
        console = TrexConsole(rpc_client)
        console.cmdloop()
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        return

if __name__ == '__main__':
    main()

