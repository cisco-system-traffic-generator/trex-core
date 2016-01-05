#!/router/bin/python

__copyright__ = "Copyright 2014"



import os
import sys
import outer_packages
import nose
from nose.plugins import Plugin
import logging
from rednose import RedNose
import termstyle




def set_report_dir (report_dir):
    if not os.path.exists(report_dir):
        os.mkdir(report_dir)

if __name__ == "__main__":
    
    # setting defaults. By default we run all the test suite
    specific_tests    = False
    disableLogCapture = False
    long_test         = False
    report_dir        = "reports"

    nose_argv= sys.argv + ['-s', '-v', '--exe', '--rednose', '--detailed-errors']
    
#   for arg in sys.argv:
#       if 'unit_tests/' in arg:
#           specific_tests = True
#       if 'log-path' in arg:
#           disableLogCapture = True
#       if arg=='--collect-only':   # this is a user trying simply to view the available tests. removing xunit param from nose args
#           nose_argv[5:7] = []
            


    try:
        result = nose.run(argv = nose_argv, addplugins = [RedNose()])
        
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
        
        
    



                        

