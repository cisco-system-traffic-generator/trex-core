#!/router/bin/python

import sys
import os
python_ver = 'python%s' % sys.version_info.major
ucs_ver = 'ucs2' if sys.maxunicode == 65535 else 'ucs4'

CURRENT_PATH         = os.path.dirname(os.path.realpath(__file__))                
ROOT_PATH            = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB   = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir, 'external_libs'))
ZMQ_PATH             = os.path.abspath(os.path.join(PATH_TO_PYTHON_LIB, 'pyzmq-14.5.0', python_ver, ucs_ver, '64bit'))
YAML_PATH            = os.path.abspath(os.path.join(PATH_TO_PYTHON_LIB, 'pyyaml-3.11', python_ver))

SERVER_MODULES = [
                  'simple_enum',
                  'zmq',
                  'jsonrpclib-pelix-0.2.5',
                  'python-daemon-2.0.5',
                  'lockfile-0.10.2',
                  'termstyle'
                  ]


def import_server_modules():
    # must be in a higher priority
    if PATH_TO_PYTHON_LIB not in sys.path:
        sys.path.insert(0, PATH_TO_PYTHON_LIB)
    for path in (ROOT_PATH, ZMQ_PATH, YAML_PATH):
        if path not in sys.path:
            sys.path.insert(0, path)
    import_module_list(SERVER_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        if full_path not in sys.path:
            sys.path.insert(1, full_path)


import_server_modules()

