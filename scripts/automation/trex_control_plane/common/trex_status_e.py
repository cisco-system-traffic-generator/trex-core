#!/router/bin/python

# import outer_packages
from enum import Enum


# define the states in which a T-Rex can hold during its lifetime
TRexStatus = Enum('TRexStatus', 'Idle Starting Running')
