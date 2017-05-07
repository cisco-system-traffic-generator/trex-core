#!/router/bin/python

try:
    from . import outer_packages
except:
    import outer_packages
from simple_enum import SimpleEnum


# define the states in which a TRex can hold during its lifetime
TRexStatus = SimpleEnum('TRexStatus', 'Idle Starting Running')
