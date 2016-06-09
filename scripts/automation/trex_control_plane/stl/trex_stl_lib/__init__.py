import sys

if sys.version_info < (2, 7):
    print("\n**** TRex STL package requires Python version >= 2.7 ***\n")
    exit(-1)

from . import trex_stl_ext
