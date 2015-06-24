#!/router/bin/python

import sys,site
import os

try:
    import pwd
except ImportError:
    import getpass
    pwd = None

using_python_3 = True if sys.version_info.major == 3 else False


def user_input():
    if using_python_3:
        return input()
    else:
        # using python version 2
        return raw_input()

def get_current_user():
  if pwd:
      return pwd.getpwuid( os.geteuid() ).pw_name
  else:
      return getpass.getuser()

def import_module_list_by_path (modules_list):
    assert(isinstance(modules_list, list))
    for full_path in modules_list:
        site.addsitedir(full_path)

def find_path_to_pardir (pardir, base_path = os.getcwd() ):
    """
    Finds the absolute path for some parent dir `pardir`, starting from base_path

    The request is only valid if the stop intitiator is the same client as the T-Rex run intitiator.
            
    :parameters:        
        pardir : str
            name of an upper-level directory to which we want to find an absolute path for
        base_path : str
            a full (usually nested) path from which we want to find a parent folder.

            default value : **current working dir**

    :return: 
        string representation of the full path to 

    """
    components = base_path.split(os.sep)
    return str.join(os.sep, components[:components.index(pardir)+1])
    


if __name__ == "__main__":
    pass
