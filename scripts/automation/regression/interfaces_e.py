#!/router/bin/python

import outer_packages

# define the states in which a TRex can hold during its lifetime
class IFType:
    Client = 0
    Server = 1
    All = 2
