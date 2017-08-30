#!/router/bin/python
import sys, site
import platform, os
import pprint

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__)) # alternate use  with: os.getcwd()
TREX_PATH           = os.getenv('TREX_UNDER_TEST')     # path to <trex-core>/scripts directory, env. variable TREX_UNDER_TEST should override it.
if not TREX_PATH or not os.path.isfile('%s/trex_daemon_server' % TREX_PATH):
    TREX_PATH       = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, os.pardir))
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(TREX_PATH, 'external_libs'))
PATH_TO_CTRL_PLANE  = os.path.abspath(os.path.join(TREX_PATH, 'automation', 'trex_control_plane')) 
PATH_STF_API        = os.path.abspath(os.path.join(PATH_TO_CTRL_PLANE, 'stf')) 
PATH_STL_API        = os.path.abspath(os.path.join(PATH_TO_CTRL_PLANE, 'stl'))
PATH_ASTF_API       = os.path.abspath(os.path.join(PATH_TO_CTRL_PLANE, 'astf'))


NIGHTLY_MODULES = [ {'name': 'ansi2html'},
                    {'name': 'rednose-0.4.1'},
                    {'name': 'progressbar-2.2'},
                    {'name': 'termstyle'},
                    {'name': 'urllib3'},
                    {'name': 'simple_enum'},
                    {'name': 'elasticsearch'},
                    {'name': 'requests'},
                    {'name': 'pyyaml-3.11', 'py-dep': True},
                    {'name': 'nose-1.3.4', 'py-dep': True},
                    ]


def generate_module_path (module, is_python3, is_64bit, is_ucs2):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('ucs2' if is_ucs2 else 'ucs4')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(PATH_TO_PYTHON_LIB, *platform_path))


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
            print("Please provide the correct path using PATH_TO_PYTHON_LIB variable")
            print("current path used: '{0}'".format(full_path))
            exit(0)
        if full_path not in sys.path:
            sys.path.insert(1, full_path)


def import_nightly_modules ():
    #sys.path.append(PATH_TO_CTRL_PLANE)
    for path in (TREX_PATH, PATH_STL_API, PATH_STF_API, PATH_ASTF_API):
        if path not in sys.path:
            sys.path.append(path)
    import_module_list(NIGHTLY_MODULES)
    #pprint.pprint(sys.path)


import_nightly_modules()


if __name__ == "__main__":
    pass
