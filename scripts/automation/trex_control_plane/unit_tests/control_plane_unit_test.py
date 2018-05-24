#!/router/bin/python

__copyright__ = "Copyright 2014"



import os
import sys
import nose_outer_packages
import nose
from nose.plugins import Plugin
from rednose import RedNose
import termstyle
import control_plane_general_test

class TRexCPConfiguringPlugin(Plugin):
    def options(self, parser, env = os.environ):
        super(TRexCPConfiguringPlugin, self).options(parser, env)
        parser.add_option('-t', '--trex-server', action='store',
                            dest='trex_server', default='trex-dan',
                            help='Specify TRex server hostname. This server will be used to test control-plane functionality.')

    def configure(self, options, conf):
        if options.trex_server:
            self.trex_server = options.trex_server

    def begin (self):
        # initialize CTRexCP global testing class, to be used by and accessible all tests
        print "assigned trex_server name"
        control_plane_general_test.CTRexCP.trex_server = self.trex_server
    
    def finalize(self, result):
        pass



if __name__ == "__main__":
    
    # setting defaults. By default we run all the test suite
    specific_tests    = False
    disableLogCapture = False
    long_test         = False
    report_dir        = "reports"

    nose_argv= sys.argv + ['-s', '-v', '--exe', '--rednose', '--detailed-errors']

    try:
        result = nose.run(argv = nose_argv, addplugins = [RedNose(), TRexCPConfiguringPlugin()])
        if (result == True):
            print termstyle.green("""
                     ..::''''::..
                   .;''        ``;.
                  ::    ::  ::    ::
                 ::     ::  ::     ::
                 ::     ::  ::     ::
                 :: .:' ::  :: `:. ::
                 ::  :          :  ::
                  :: `:.      .:' ::
                   `;..``::::''..;'
                     ``::,,,,::''

                   ___  ___   __________
                  / _ \/ _ | / __/ __/ /
                 / ___/ __ |_\ \_\ \/_/ 
                /_/  /_/ |_/___/___(_)  

            """)
            sys.exit(0)
        else:
            sys.exit(-1)
    
    finally:
        pass
