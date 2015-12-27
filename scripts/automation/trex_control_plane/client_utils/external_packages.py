#!/router/bin/python

import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))     # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, os.pardir, os.pardir, 'external_libs'))

CLIENT_UTILS_MODULES = ['dpkt-1.8.6',
                        'PyYAML-3.01/lib',
                        'texttable-0.8.4'
                        ]

def import_client_utils_modules():

    # must be in a higher priority
    sys.path.insert(0, PATH_TO_PYTHON_LIB)

    sys.path.append(ROOT_PATH)
    import_module_list(CLIENT_UTILS_MODULES)


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path = os.path.normcase(full_path)
        sys.path.insert(1, full_path)


    import_platform_dirs()
  


def import_platform_dirs ():
    # handle platform dirs

    # try fedora 18 first and then cel5.9
    # we are using the ZMQ module to determine the right platform

    full_path = os.path.join(PATH_TO_PYTHON_LIB, 'platform/fedora18')
    fix_path = os.path.normcase(full_path)
    sys.path.insert(0, full_path)
    try:
        # try to import and delete it from the namespace
        import zmq
        del zmq
        return
    except:
        pass

    full_path = os.path.join(PATH_TO_PYTHON_LIB, 'platform/cel59')
    fix_path = os.path.normcase(full_path)
    sys.path.insert(0, full_path)
    try:
        # try to import and delete it from the namespace
        import zmq
        del zmq
        return

    except:
        raise Exception("unable to determine platform type for ZMQ import")



import_client_utils_modules()

