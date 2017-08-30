import sys, os

# FIXME to the right path for trex_nstf_lib
cur_dir = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(cur_dir, os.pardir))

ASTF_PROFILES_PATH = os.path.join(os.pardir, os.pardir, os.pardir, os.pardir, 'nstf')

