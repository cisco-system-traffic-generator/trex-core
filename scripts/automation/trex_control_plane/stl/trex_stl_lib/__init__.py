import sys

if sys.version_info < (2, 7):
    print("\n**** TRex STL pacakge requires Python version >= 2.7 ***\n")
    exit(-1)

if sys.version_info >= (3, 0):
    print("\n**** TRex STL pacakge does not support Python 3 (yet) ***\n")
    exit(-1)

import trex_stl_ext
