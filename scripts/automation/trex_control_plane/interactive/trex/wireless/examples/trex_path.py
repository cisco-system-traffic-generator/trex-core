import sys, os

cur_dir = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(cur_dir, os.pardir))
sys.path.insert(0, os.path.join(cur_dir, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(cur_dir, os.pardir, os.pardir, os.pardir))