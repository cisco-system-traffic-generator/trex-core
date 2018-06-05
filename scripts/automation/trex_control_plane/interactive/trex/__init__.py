
import sys
import os
import warnings
import platform

# because of ZMQ and other built external libraries we support only those versions
__supported_python_versions = ['2.7', '3.4', '3.5']

# check for python version
if "{0}.{1}".format(sys.version_info[0], sys.version_info[1]) not in __supported_python_versions:
    raise Exception("\n*** TRex library supported Python versions: {0} ***\n".format(__supported_python_versions))


def __load ():

    # file path
    CURRENT_PATH = os.path.dirname(os.path.realpath(__file__))

    # give priority to the enviorment variable
    if os.getenv('TREX_EXT_LIBS'):
        ext_libs_path = os.environ['TREX_EXT_LIBS']
    else:
        # fallback to default ../../../../external_libs
        ext_libs_path = os.path.normpath(os.path.join(CURRENT_PATH, os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs'))

        # last try - two up
        if not os.path.exists(ext_libs_path):
            ext_libs_path = os.path.normpath(os.path.join(CURRENT_PATH, os.pardir, os.pardir, 'external_libs'))


    if not os.path.exists(ext_libs_path):
        raise Exception('Could not determine path of external_libs, try setting TREX_EXT_LIBS variable')


    # the modules required
    # py-dep requires python2/python3 directories
    # arch-dep requires intel/arm, ucs2/ucs4 and 32bit/64bit directories
    ext_libs = [ {'name': 'texttable-0.8.4'},
                 {'name': 'pyyaml-3.11', 'py-dep': True},
                 {'name': 'scapy-2.3.1', 'py-dep': True},
                 {'name': 'pyzmq-14.5.0', 'py-dep': True, 'arch-dep': True},
                 {'name': 'simpy-3.0.10'},
                 {'name': 'trex-openssl'},
                 {'name': 'dpkt-1.9.1'},
                 {'name': 'repoze'},
               ]

    __import_ext_libs(ext_libs, ext_libs_path)


def __generate_module_path (module, ext_libs_path, is_python3, is_64bit, is_ucs2):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('arm' if os.uname()[4] == 'aarch64' else 'intel')
        platform_path.append('ucs2' if is_ucs2 else 'ucs4')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(ext_libs_path, *platform_path))


def __import_ext_libs(ext_libs, ext_libs_path):

    # platform data
    is_64bit   = platform.architecture()[0] == '64bit'
    is_python3 = (sys.version_info >= (3, 0))
    is_ucs2    = (sys.maxunicode == 65535)

    # regular modules
    for p in ext_libs:
        full_path = __generate_module_path(p, ext_libs_path, is_python3, is_64bit, is_ucs2)

        if not os.path.exists(full_path):
            err_msg  =  "\n\nUnable to find required external library: '{0}'\n".format(p['name'])
            err_msg +=  "Please provide the correct path using TREX_EXT_LIBS variable\n\n" 
            err_msg +=  "Current path used: '{0}'".format(full_path)

            raise Exception(err_msg)


        sys.path.insert(1, full_path)

# load the library
__load()

