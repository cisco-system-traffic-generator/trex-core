import sys
import os
import warnings
import platform

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


CLIENT_PLATFORM_MODULES = ['pyzmq-14.5.0']


def import_module_list(modules_list, platform_modules_list):

    assert(isinstance(modules_list, list))
    assert(isinstance(platform_modules_list, list))

    # regular modules
    for p in modules_list:
        full_path = os.path.join(TREX_STL_EXT_PATH, p)
        fix_path = os.path.normcase(full_path)

        if not os.path.exists(fix_path):
            print "Unable to find required module library: '{0}'".format(p)
            print "Please provide the correct path using TREX_STL_EXT_PATH variable"
            print "current path used: '{0}'".format(fix_path)
            exit(0)

        sys.path.insert(1, full_path)

    # platform depdendant modules
    is_64bit   = platform.architecture()[0] == '64bit'
    is_python3 = (sys.version_info >= (3, 0))
    is_cel     = os.path.exists('/etc/system-profile')

    platform_path = "{0}/{1}/{2}".format('cel59' if is_cel else 'fedora18',
                                         'python3' if is_python3 else 'python2',
                                         '64bit' if is_64bit else '32bit')

    for p in platform_modules_list:
        full_path = os.path.join(TREX_STL_EXT_PATH, p, platform_path)
        fix_path = os.path.normcase(full_path)

        if not os.path.exists(fix_path):
            print "Unable to find required platfrom dependant module library: '{0}'".format(p)
            print "platform dependant path used was '{0}'".format(fix_path)
            exit(0)

        sys.path.insert(1, full_path)



import_module_list(CLIENT_UTILS_MODULES, CLIENT_PLATFORM_MODULES)
