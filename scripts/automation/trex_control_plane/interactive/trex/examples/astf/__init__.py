# allow import of astf_path from same directory

from trex.examples.astf import astf_path

import sys
sys.modules['astf_path'] = astf_path
