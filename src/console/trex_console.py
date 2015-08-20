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
        self.prompt = "TRex > "

        self.intro  = "\n-=TRex Console V1.0=-\n"
        self.intro += "\nType 'help' or '?' for supported actions\n" 

        self.rpc_client = rpc_client
        self.verbose = False
        
        # before starting query the RPC server and add the methods
        rc, msg = self.rpc_client.query_rpc_server()

        if rc:
            self.supported_rpc = [str(x) for x in msg if x]

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    # set verbose on / off
    def do_verbose (self, line):
        '''\nshows or set verbose mode\nusage: verbose [on/off]\n'''
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
        '''\nquery the RPC server for supported remote commands\n'''
        rc, msg = self.rpc_client.query_rpc_server()
        if not rc:
            print "\n*** Failed to query RPC server: " + str(msg)

        print "\nRPC server supports the following commands: \n\n"
        for func in msg:
            if func:
                print func
        print "\n"

    def do_ping (self, line):
        '''\npings the RPC server\n'''
        print "\n-> Pinging RPC server"

        rc, msg = self.rpc_client.ping_rpc_server()
        if rc:
            print "[SUCCESS]\n"
        else:
            print "[FAILED]\n"

    def do_rpc (self, line):
        '''\nLaunches a RPC on the server\n'''
        if line == "":
            print "\nUsage: [method name] [param dict as string]\n"
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
            return

        rc, msg = self.rpc_client.invoke_rpc_method(method, params)
        if rc:
            print "[SUCCESS]\n"
        else:
            print "[FAILED]\n"


    def complete_rpc (self, text, line, begidx, endidx):
        return [x for x in self.supported_rpc if x.startswith(text)]

    def do_status (self, line):
        '''\nShows a graphical console\n'''

        self.do_verbose('off')
        trex_status.show_trex_status(self.rpc_client)

    def do_quit(self, line):
        '''\nexit the client\n'''
        return True

    def default(self, line):
        print "'{0}' is an unrecognized command. type 'help' or '?' for a list\n".format(line)

    # aliasing
    do_exit = do_EOF = do_q = do_quit

def main ():
    # RPC client
    rpc_client = RpcClient("localhost", 5050)
    rc, msg = rpc_client.connect()
    if not rc:
        print "\n*** " + msg + "\n"
        exit(-1)

    # console
    try:
        console = TrexConsole(rpc_client)
        console.cmdloop()
    except KeyboardInterrupt as e:
        print "\n\n*** Caught Ctrl + C... Exiting...\n\n"
        return

if __name__ == '__main__':
    main()

