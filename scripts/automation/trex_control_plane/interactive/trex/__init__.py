
import sys
import os
import warnings


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
    # arch-dep requires intel/arm, 32bit/64bit directories
    ext_libs = [ {'name': 'texttable-0.8.4'},
                 {'name': 'pyyaml-3.11', 'py-dep': True},
                 {'name': 'scapy-2.3.1', 'py-dep': True},
                 {'name': 'pyzmq-ctypes', 'arch-dep': True},
                 {'name': 'simpy-3.0.10'},
                 {'name': 'trex-openssl'},
                 {'name': 'dpkt-1.9.1'},
                 {'name': 'repoze'},
               ]

    __import_ext_libs(ext_libs, ext_libs_path)


def __generate_module_path (module, ext_libs_path, is_python3, is_64bit):
    platform_path = [module['name']]

    if module.get('py-dep'):
        platform_path.append('python3' if is_python3 else 'python2')

    if module.get('arch-dep'):
        platform_path.append('arm' if os.uname()[4] == 'aarch64' else 'intel')
        platform_path.append('64bit' if is_64bit else '32bit')

    return os.path.normcase(os.path.join(ext_libs_path, *platform_path))


def __import_ext_libs(ext_libs, ext_libs_path):

    # platform data
    is_64bit   = sys.maxsize > 0xffffffff
    is_python3 = sys.version_info >= (3, 0)

    # regular modules
    for p in ext_libs:
        full_path = __generate_module_path(p, ext_libs_path, is_python3, is_64bit)

        if not os.path.exists(full_path):
            err_msg  =  "\n\nUnable to find required external library: '{0}'\n".format(p['name'])
            err_msg +=  "Please provide the correct path using TREX_EXT_LIBS variable\n\n" 
            err_msg +=  "Current path used: '{0}'".format(full_path)

            raise Exception(err_msg)


        sys.path.insert(1, full_path)

# load the library
__load()

