#!/router/bin/python

import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, 'python_lib')) 


TEST_MODULES = ['nose-1.3.4',
            'rednose-0.4.1',
            'termstyle'
            ]

def import_test_modules ():
    sys.path.append(ROOT_PATH)
    import_module_list(TEST_MODULES)

def import_module_list (modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path   = os.path.join(PATH_TO_PYTHON_LIB, p)
        if full_path not in sys.path:
            sys.path.append(full_path)

import_test_modules()
