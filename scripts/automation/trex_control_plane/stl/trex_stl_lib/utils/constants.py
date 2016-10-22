from collections import OrderedDict

ON_OFF_DICT = OrderedDict([
    ('on', True),
    ('off', False),
])

UP_DOWN_DICT = OrderedDict([
    ('up', True),
    ('down', False),
])

FLOW_CTRL_DICT = OrderedDict([
    ('none', 0),     # Disable flow control
    ('tx',   1),     # Enable flowctrl on TX side (RX pause frames)
    ('rx',   2),     # Enable flowctrl on RX side (TX pause frames)
    ('full', 3),     # Enable flow control on both sides
])



# generate reverse dicts

for var_name in list(vars().keys()):
    if var_name.endswith('_DICT'):
        exec('{0}_REVERSED = OrderedDict([(val, key) for key, val in {0}.items()])'.format(var_name))
