#!/router/bin/python

import sys
import os
python_ver = 'python%s' % sys.version_info.major

CURRENT_PATH         = os.path.dirname(os.path.realpath(__file__))                
ROOT_PATH            = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB   = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir, 'external_libs'))
PATH_TO_PLATFORM_LIB = os.path.abspath(os.path.join(PATH_TO_PYTHON_LIB, 'pyzmq-14.5.0', python_ver , 'fedora18', '64bit'))

SERVER_MODULES = ['enum34-1.0.4',
                  'zmq',
                  'jsonrpclib-pelix-0.2.5',
                  'python-daemon-2.0.5',
                  'lockfile-0.10.2',
                  'termstyle'
                  ]


def import_server_modules():
    # must be in a higher priority 
    sys.path.insert(0, PATH_TO_PYTHON_LIB)
    sys.path.insert(0, PATH_TO_PLATFORM_LIB)
    sys.path.append(ROOT_PATH)
    import_module_list(SERVER_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        sys.path.insert(1, full_path)


import_server_modules()

