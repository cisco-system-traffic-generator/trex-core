#!/usr/bin/python
# -*- coding: utf-8 -*- 
import cmd
import json

from trex_rpc_client import RpcClient
import trex_status

class TrexConsole(cmd.Cmd):
    """Trex Console"""
   
    def __init__(self, rpc_client):
        cmd.Cmd.__init__(self)
        self.prompt = "TRex > "
        self.intro  = "\n-=TRex Console V1.0=-\n" 
        self.rpc_client = rpc_client
        self.verbose = False
        
        rc, msg = self.rpc_client.query_rpc_server()

        if rc:
            lst = msg.split('\n')
            self.supported_rpc = [str(x) for x in lst if x]

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    # set verbose on / off
    def do_verbose (self, line):
        '''shows or set verbose mode\nusage: verbose [on/off]\n'''
        if line == "":
            print "verbose is " + ("on" if self.verbose else "off")
        elif line == "on":
            self.verbose = True
            self.rpc_client.set_verbose(True)
            print "verbose set to on\n"

        elif line == "off":
            self.verbose = False
            self.rpc_client.set_verbose(False)
            print "verbose set to off\n"
        else:
            print "please specify 'on' or 'off'\n"

    # query the server for registered commands
    def do_query_server(self, line):
        '''\nquery the RPC server for supported remote commands\n'''
        rc, msg = self.rpc_client.query_rpc_server()
        if not rc:
            print "\n*** Failed to query RPC server: " + str(msg)

        print "RPC server supports the following commands: \n\n" + msg

    def do_ping (self, line):
        '''pings the RPC server\n'''
        print "Pinging RPC server"

        rc, msg = self.rpc_client.ping_rpc_server()
        if rc:
            print "[SUCCESS]\n"
        else:
            print "[FAILED]\n"

    def do_rpc (self, line):
        '''Launches a RPC on the server\n'''
        if line == "":
            print "Please provide method name\n"
            return

        rc, msg = self.rpc_client.invoke_rpc_method(line)
        if rc:
            print "[SUCCESS]\n"
        else:
            print "[FAILED]\n"

        print "Server Response:\n\n{0}\n".format(json.dumps(msg))

    def complete_rpc (self, text, line, begidx, endidx):
        return [x for x in self.supported_rpc if x.startswith(text)]

    def do_status (self, line):
        '''Shows a graphical console\n'''
        trex_status.show_trex_status(self.rpc_client)

    def do_quit(self, line):
        '''\nexit the client\n'''
        return True

    def default(self, line):
        print "'{0}' is an unrecognized command\n".format(line)

    # aliasing
    do_EOF = do_q = do_quit 

def main ():
    # RPC client
    try:
        rpc_client = RpcClient("localhost", 5050)
        rpc_client.connect()
    except Exception as e:
        print "\n*** " + str(e) + "\n"
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

