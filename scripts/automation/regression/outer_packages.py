#!/router/bin/python

import sys, site
import platform, os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__)) # alternate use  with: os.getcwd()
TREX_PATH           = os.getenv('TREX_UNDER_TEST')     # path to <trex-core>/scripts directory, env. variable TREX_UNDER_TEST should override it.
if not TREX_PATH or not os.path.isfile('%s/trex_daemon_server' % TREX_PATH):
    TREX_PATH       = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, os.pardir))
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(TREX_PATH, 'external_libs'))
PATH_TO_CTRL_PLANE  = os.path.abspath(os.path.join(TREX_PATH, 'automation', 'trex_control_plane')) 
PATH_STF_API        = os.path.abspath(os.path.join(PATH_TO_CTRL_PLANE, 'stf')) 
PATH_STL_API        = os.path.abspath(os.path.join(PATH_TO_CTRL_PLANE, 'stl')) 


NIGHTLY_MODULES = [ {'name': 'ansi2html'},
                    {'name': 'enum34-1.0.4'},
                    {'name': 'rednose-0.4.1'},
                    {'name': 'progressbar-2.2'},
                    {'name': 'termstyle'},
                    {'name': 'pyyaml-3.11', 'py-dep': True},
                    {'name': 'nose-1.3.4', 'py-dep': True}
                    ]


def generate_module_path (module, is_python3, is_64bit, is_cel):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('cel59' if is_cel else 'fedora18')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(PATH_TO_PYTHON_LIB, *platform_path))


def import_module_list(modules_list):

    # platform data
    is_64bit   = platform.architecture()[0] == '64bit'
    is_python3 = (sys.version_info >= (3, 0))
    is_cel     = os.path.exists('/etc/system-profile')

    # regular modules
    for p in modules_list:
        full_path = generate_module_path(p, is_python3, is_64bit, is_cel)

        if not os.path.exists(full_path):
            print("Unable to find required module library: '{0}'".format(p['name']))
            print("Please provide the correct path using PATH_TO_PYTHON_LIB variable")
            print("current path used: '{0}'".format(full_path))
            exit(0)

        sys.path.insert(1, full_path)


def import_nightly_modules ():
    sys.path.append(TREX_PATH)
    #sys.path.append(PATH_TO_CTRL_PLANE)
    sys.path.append(PATH_STL_API)
    sys.path.append(PATH_STF_API)
    import_module_list(NIGHTLY_MODULES)


import_nightly_modules()


if __name__ == "__main__":
    pass
