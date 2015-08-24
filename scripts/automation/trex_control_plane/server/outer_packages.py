#!/router/bin/python

import sys
import site
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))                
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir,
                                                   os.pardir, 'external_libs', 'python'))

SERVER_MODULES = ['enum34-1.0.4',
                  'jsonrpclib-pelix-0.2.5',
                  'zmq',
                  'pyzmq-14.7.0',
                  'python-daemon-2.0.5',
                  'lockfile-0.10.2',
                  'termstyle'
                  ]


def import_server_modules():
    # must be in a higher priority 
    sys.path.insert(0, PATH_TO_PYTHON_LIB)
    sys.path.append(ROOT_PATH)
    import_module_list(SERVER_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        site.addsitedir(full_path)

import_server_modules()
