import os
import sys
import inspect

if sys.version_info < (3, 4):
    raise ImportError('Python < 3.4 is unsupported.')

# set stl (parent directory) in python path for imports
currentdir = os.path.dirname(os.path.abspath(
    inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0, parentdir)

from .trex_ext import *
