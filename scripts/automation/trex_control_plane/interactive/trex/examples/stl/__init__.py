# allow import of stl_path from same directory

from trex.examples.stl import stl_path

import sys
sys.modules['stl_path'] = stl_path

