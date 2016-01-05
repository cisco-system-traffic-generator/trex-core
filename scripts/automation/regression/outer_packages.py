#!/router/bin/python

import sys, site
import platform, os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__)) # alternate use  with: os.getcwd()
TREX_PATH           = os.getenv('TREX_UNDER_TEST')     # path to <trex-core>/scripts directory, env. variable TREX_UNDER_TEST should override it.
if not TREX_PATH or not os.path.isfile('%s/trex_daemon_server' % TREX_PATH):
    TREX_PATH       = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, os.pardir))
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(TREX_PATH, 'external_libs'))
PATH_TO_CTRL_PLANE  = os.path.abspath(os.path.join(TREX_PATH, 'automation', 'trex_control_plane')) 

NIGHTLY_MODULES = ['enum34-1.0.4',
                   'nose-1.3.4',
                   'rednose-0.4.1',
                   'progressbar-2.2',
                   'termstyle',
                   'dpkt-1.8.6',
                   'yaml-3.11',
                    ]

def import_nightly_modules ():
    sys.path.append(TREX_PATH)
    sys.path.append(PATH_TO_CTRL_PLANE)
    import_module_list(NIGHTLY_MODULES)

def import_module_list (modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path   = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path    = os.path.normcase(full_path) #CURRENT_PATH+p)
        sys.path.insert(1, full_path)

import_nightly_modules()


if __name__ == "__main__":
    pass
