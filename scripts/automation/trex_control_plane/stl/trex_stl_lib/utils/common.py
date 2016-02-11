import os
import sys
import string
import random

try:
    import pwd
except ImportError:
    import getpass
    pwd = None

using_python_3 = True if sys.version_info.major == 3 else False

def get_current_user():
  if pwd:
      return pwd.getpwuid(os.geteuid()).pw_name
  else:
      return getpass.getuser()


def user_input():
    if using_python_3:
        return input()
    else:
        # using python version 2
        return raw_input()


def random_id_gen(length=8):
    """
    A generator for creating a random chars id of specific length

    :parameters:
        length : int
            the desired length of the generated id

            default: 8

    :return:
        a random id with each next() request.
    """
    id_chars = string.ascii_lowercase + string.digits
    while True:
        return_id = ''
        for i in range(length):
            return_id += random.choice(id_chars)
        yield return_id

# try to get number from input, return None in case of fail
def get_number(input):
    try:
        if type(input) in (int, long):
            return input
        return int(input)
    except:
        return None
