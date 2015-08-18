#!/usr/bin/python
# -*- coding: utf-8 -*- 
import cmd
from trex_rpc_client import RpcClient

class TrexConsole(cmd.Cmd):
    """Trex Console"""
   
    def __init__(self, rpc_client):
        cmd.Cmd.__init__(self)
        self.prompt = "TRex > "
        self.intro  = "\n-=TRex Console V1.0=-\n" 
        self.rpc_client = rpc_client
        self.verbose = False

    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

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


    def do_query_server(self, line):
        '''\nquery the RPC server for supported remote commands\n'''
        rc, msg = self.rpc_client.query_rpc_server()
        if not rc:
            print "\n*** Failed to query RPC server: " + str(msg)

        print "RPC server supports the following commands: \n\n" + msg

    def do_ping (self, line):
        '''pings the RPC server \n'''
        print "Pinging RPC server"

        rc, msg = self.rpc_client.ping_rpc_server()
        if rc:
            print "[SUCCESS]\n"
        else:
            print "[FAILED]\n"

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
        rpc_client = RpcClient(5050)
        rpc_client.connect()
    except Exception as e:
        print "\n*** " + str(e) + "\n"
        exit(-1)

    # console
    console = TrexConsole(rpc_client)
    console.cmdloop()

if __name__ == '__main__':
    main()
