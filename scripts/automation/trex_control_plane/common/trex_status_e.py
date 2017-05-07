#!/router/bin/python

import outer_packages  # import this to overcome doc building import error by sphinx
from simple_enum import SimpleEnum


# define the states in which a TRex can hold during its lifetime
TRexStatus = SimpleEnum('TRexStatus', 'Idle Starting Running')
