# allow import of emu_path from same directory

from trex.examples.emu import emu_path

import sys
sys.modules['emu_path'] = emu_path
