#!/router/bin/python

import sys
import os


CURRENT_PATH = os.path.dirname(os.path.realpath(__file__))
PARENT_PATH  = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, 'external_libs'))
SCRIPTS_PATH = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir, os.pardir, os.pardir, 'external_libs'))

CLIENT_MODULES = ['enum34-1.0.4',
                  'jsonrpclib-pelix-0.2.5',
#                  'termstyle',
#                  'yaml-3.11'
                  ]


def import_module_list(ext_libs_path):
    for p in CLIENT_MODULES:
        full_path = os.path.join(ext_libs_path, p)
        if not os.path.exists(full_path):
            raise Exception('Library %s is absent in path %s' % (p, ext_libs_path))
        sys.path.insert(1, full_path)

if os.path.exists(PARENT_PATH):
    import_module_list(PARENT_PATH)
elif os.path.exists(SCRIPTS_PATH):
    import_module_list(SCRIPTS_PATH)
else:
    raise Exception('Could not find external libs in path: %s' % [PARENT_PATH, SCRIPTS_PATH])
