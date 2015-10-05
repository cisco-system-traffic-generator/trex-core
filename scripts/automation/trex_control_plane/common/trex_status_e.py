#!/router/bin/python

# import outer_packages
from enum import Enum


# define the states in which a TRex can hold during its lifetime
TRexStatus = Enum('TRexStatus', 'Idle Starting Running')
