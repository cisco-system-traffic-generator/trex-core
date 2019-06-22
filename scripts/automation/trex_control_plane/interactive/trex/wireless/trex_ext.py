import sys
import os
import warnings
import platform

# if not set - set it to default
CURRENT_PATH = os.path.dirname(os.path.realpath(__file__))
# trex-core/scripts/external_libs
TREX_EXT_PATH = os.path.normpath(os.path.join(CURRENT_PATH, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs'))

if not os.path.exists(TREX_EXT_PATH):
    raise Exception('Could not determine path of external_libs, try setting TREX_EXT_PATH variable')

# the modules required
# py-dep requires python2/python3 directories
# arch-dep requires intel/arm, ucs2/ucs4, 32bit/64bit directories
CLIENT_UTILS_MODULES = [ {'name': 'texttable-0.8.4'},
                         {'name': 'pyyaml-3.11', 'py-dep': True},
                         {'name': 'scapy-2.3.1', 'py-dep': True},
                         {'name': 'simpy-3.0.10'},
                         {'name': 'trex-openssl'},
                        ]


def generate_module_path (module, is_python3, is_64bit, is_ucs2):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('arm' if os.uname()[4] == 'aarch64' else 'intel')
        platform_path.append('ucs2' if is_ucs2 else 'ucs4')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(TREX_EXT_PATH, *platform_path))


def import_module_list(modules_list):

    # platform data
    is_64bit   = platform.architecture()[0] == '64bit'
    is_python3 = (sys.version_info >= (3, 0))
    is_ucs2    = (sys.maxunicode == 65535)

    # regular modules
    for p in modules_list:
        full_path = generate_module_path(p, is_python3, is_64bit, is_ucs2)

        if not os.path.exists(full_path):
            print("Unable to find required module library: '{0}'".format(p['name']))
            print("Please provide the correct path using TREX_EXT_PATH variable")
            print("current path used: '{0}'".format(full_path))
            exit(1)

        sys.path.insert(1, full_path)





import_module_list(CLIENT_UTILS_MODULES)
