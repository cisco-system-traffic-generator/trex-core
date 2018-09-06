#!/router/bin/python

import sys
import os
import warnings

cpu_vendor = 'arm' if os.uname()[4] == 'aarch64' else 'intel'
cpu_bits   = '64bit' if sys.maxsize > 0xffffffff else '32bit'
python_ver = 'python%s' % sys.version_info.major

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
EXT_LIBS_PATH       = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir, 'external_libs'))
ZMQ_PATH            = os.path.join(EXT_LIBS_PATH, 'pyzmq-ctypes', cpu_vendor, cpu_bits)
YAML_PATH           = os.path.join(EXT_LIBS_PATH, 'pyyaml-3.11', python_ver)

CLIENT_UTILS_MODULES = [
                        'texttable-0.8.4',
                        'scapy-2.3.1'
                        'zmq',
                        ]

def import_client_utils_modules():

    # must be in a higher priority
    if PATH_TO_PYTHON_LIB not in sys.path:
        sys.path.insert(0, PATH_TO_PYTHON_LIB)

    for path in (ROOT_PATH, ZMQ_PATH, YAML_PATH):
        if path not in sys.path:
            sys.path.append(path)

    import_module_list(CLIENT_UTILS_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        if full_path not in sys.path:
            sys.path.insert(1, full_path)


import_client_utils_modules()
