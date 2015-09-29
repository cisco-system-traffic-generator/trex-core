#!/router/bin/python

import outer_packages  # import this to overcome doc building import error by sphinx
from enum import Enum


# define the states in which a T-Rex can hold during its lifetime
TRexStatus = Enum('TRexStatus', 'Idle Starting Running')
