#!/router/bin/python

import sys
import os
import warnings

python_ver = 'python%s' % sys.version_info.major
ucs_ver = 'ucs2' if sys.maxunicode == 65535 else 'ucs4'

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir, 'external_libs'))
ZMQ_PATH            = os.path.abspath(os.path.join(PATH_TO_PYTHON_LIB, 'pyzmq-14.5.0', python_ver, ucs_ver, '64bit'))

CLIENT_UTILS_MODULES = ['dpkt-1.8.6',
                        'yaml-3.11',
                        'texttable-0.8.4',
                        'scapy-2.3.1'
                        'zmq',
                        ]

def import_client_utils_modules():

    # must be in a higher priority
    if PATH_TO_PYTHON_LIB not in sys.path:
        sys.path.insert(0, PATH_TO_PYTHON_LIB)

    if ROOT_PATH not in sys.path:
        sys.path.append(ROOT_PATH)

    if ZMQ_PATH not in sys.path:
        sys.path.append(ZMQ_PATH)

    import_module_list(CLIENT_UTILS_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        if full_path not in sys.path:
            sys.path.insert(1, full_path)


import_client_utils_modules()
