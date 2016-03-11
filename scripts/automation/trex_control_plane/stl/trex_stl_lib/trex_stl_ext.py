import sys
import os
import warnings

# if not set - set it to default
TREX_STL_EXT_PATH = os.environ.get('TREX_STL_EXT_PATH')

# take default
if not TREX_STL_EXT_PATH:
    CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
    # ../../../../external_libs
    TREX_STL_EXT_PATH   = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs'))


# the modules required
CLIENT_UTILS_MODULES = ['dpkt-1.8.6',
                        'yaml-3.11',
                        'texttable-0.8.4',
                        'scapy-2.3.1'
                        ]


def import_module_list(modules_list):
    assert(isinstance(modules_list, list))

    for p in modules_list:
        full_path = os.path.join(TREX_STL_EXT_PATH, p)
        fix_path = os.path.normcase(full_path)

        if not os.path.exists(fix_path):
            print "Unable to find required module library: '{0}'".format(p)
            print "Please provide the correct path using TREX_STL_EXT_PATH variable"
            print "current path used: '{0}'".format(TREX_STL_EXT_PATH)
            exit(0)

        sys.path.insert(1, full_path)


# TODO; REFACTOR THIS....it looks horrible
def import_platform_dirs ():
    # handle platform dirs

    # try fedora 18 first and then cel5.9
    # we are using the ZMQ module to determine the right platform

    full_path = os.path.join(TREX_STL_EXT_PATH, 'platform/fedora18')
    fix_path = os.path.normcase(full_path)
    sys.path.insert(0, full_path)
    try:
        # try to import and delete it from the namespace
        import zmq
        del zmq
        return
    except:
        sys.path.pop(0)
        pass

    full_path = os.path.join(TREX_STL_EXT_PATH, 'platform/cel59')
    fix_path = os.path.normcase(full_path)
    sys.path.insert(0, full_path)
    try:
        # try to import and delete it from the namespace
        import zmq
        del zmq
        return
    except:
        sys.path.pop(0)
        pass

    full_path = os.path.join(TREX_STL_EXT_PATH, 'platform/cel59/32bit')
    fix_path = os.path.normcase(full_path)
    sys.path.insert(0, full_path)
    try:
        # try to import and delete it from the namespace
        import zmq
        del zmq
        return

    except:
        sys.path.pop(0)
        sys.modules['zmq'] = None
        warnings.warn("unable to determine platform type for ZMQ import")

        
import_module_list(CLIENT_UTILS_MODULES)
import_platform_dirs()
