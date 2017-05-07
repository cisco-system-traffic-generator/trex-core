#!/router/bin/python

import outer_packages
from simple_enum import SimpleEnum


# define the states in which a TRex can hold during its lifetime
IFType = SimpleEnum('IFType', 'Client Server All')
