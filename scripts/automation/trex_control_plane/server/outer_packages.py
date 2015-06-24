#!/router/bin/python

import sys,site
import platform,os
import tarfile
import errno
import pwd

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))                
ROOT_PATH           = os.path.abspath(os.path.join(CURRENT_PATH, os.pardir))      # path to trex_control_plane directory
PATH_TO_PYTHON_LIB  = os.path.abspath(os.path.join(ROOT_PATH, 'python_lib')) 

SERVER_MODULES = ['enum34-1.0.4',
            # 'jsonrpclib-0.1.3',
            'jsonrpclib-pelix-0.2.5',
            'zmq',
            'python-daemon-2.0.5',
            'lockfile-0.10.2',
            'termstyle'
            ]

def extract_zmq_package ():
    """make sure zmq package is available"""

    os.chdir(PATH_TO_PYTHON_LIB)
    if not os.path.exists('zmq'):
        if os.path.exists('zmq_fedora.tar.gz'): # make sure tar file is available for extraction
            try:
                tar = tarfile.open("zmq_fedora.tar.gz")
                # finally, extract the tarfile locally
                tar.extractall()
            except OSError as err:
                if err.errno == errno.EACCES:
                    # fall back. try extracting using currently logged in user
                    stat_info = os.stat(PATH_TO_PYTHON_LIB)
                    uid = stat_info.st_uid
                    logged_user = pwd.getpwuid(uid).pw_name
                    if logged_user != 'root':
                        try:
                            os.system("sudo -u {user} tar -zxvf zmq_fedora.tar.gz".format(user = logged_user))
                        except:
                            raise OSError(13, 'Permission denied: Please make sure that logged user have sudo access and writing privileges to `python_lib` directory.')
                    else:
                        raise OSError(13, 'Permission denied: Please make sure that logged user have sudo access and writing privileges to `python_lib` directory.')
            finally:
                tar.close()
        else:
            raise IOError("File 'zmq_fedora.tar.gz' couldn't be located at python_lib directory.")
    os.chdir(CURRENT_PATH)

def import_server_modules ():
    # must be in a higher priority 
    sys.path.insert(0, PATH_TO_PYTHON_LIB)
    sys.path.append(ROOT_PATH)
    extract_zmq_package()
    import_module_list(SERVER_MODULES)

def import_module_list (modules_list):
    assert(isinstance(modules_list, list))
    for p in modules_list:
        full_path   = os.path.join(PATH_TO_PYTHON_LIB, p)
        fix_path    = os.path.normcase(full_path) 
        site.addsitedir(full_path)


import_server_modules()
