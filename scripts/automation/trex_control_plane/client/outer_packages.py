#!/router/bin/python

import sys
import site
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir,
                                                   os.pardir, 'external_libs', 'python'))

CLIENT_MODULES = ['enum34-1.0.4',
                  'jsonrpclib-pelix-0.2.5',
                  'termstyle',
                  'rpc_exceptions-0.1'
                  ]


def import_client_modules():
    sys.path.append(ROOT_PATH)
    import_module_list(CLIENT_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)  # (CURRENT_PATH+p)
        site.addsitedir(full_path)

import_client_modules()
