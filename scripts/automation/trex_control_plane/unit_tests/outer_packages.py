import sys
import os

EXT_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, 'external_libs'))
if not os.path.exists(EXT_PATH):
    raise Exception('Wrong path to external libs: %s' % EXT_PATH)

CLIENT_UTILS_MODULES = [
                         {'name': 'scapy-2.3.1', 'py-dep': True},
                         {'name': 'texttable-0.8.4'},
                        ]


def generate_module_path (module, is_python3, is_64bit):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('arm' if os.uname()[4] == 'aarch64' else 'intel')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(EXT_PATH, *platform_path))


def import_module_list(modules_list):

    # platform data
    is_64bit   = sys.maxsize > 0xffffffff
    is_python3 = sys.version_info >= (3, 0)

    # regular modules
    for p in modules_list:
        full_path = generate_module_path(p, is_python3, is_64bit)

        if not os.path.exists(full_path):
            print("Unable to find required module library: '{0}'".format(p['name']))
            print("Please provide the correct path using TREX_STL_EXT_PATH variable")
            print("current path used: '{0}'".format(full_path))
            exit(1)
        if full_path not in sys.path:
            sys.path.insert(1, full_path)


import_module_list(CLIENT_UTILS_MODULES)
