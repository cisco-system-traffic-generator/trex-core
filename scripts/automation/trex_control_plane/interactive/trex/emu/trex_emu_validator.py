from trex.common.trex_exceptions import TRexError
from trex.common.trex_types import listify
from trex.emu.trex_emu_conversions import Mac, Ipv4, Ipv6
try:
  basestring
except NameError:
  basestring = str

def is_valid_mac(mac):
    return Mac.is_valid(mac) 

def is_valid_ipv4(addr):
    return Ipv4.is_valid(addr, mc = False)

def is_valid_ipv4_mc(addr):
    return Ipv4.is_valid(addr, mc = True)

def is_valid_ipv6(addr):
    return Ipv6.is_valid(addr, mc = False)

def is_valid_ipv6_mc(addr):
    return Ipv6.is_valid(addr, mc = True)

def is_valid_tci_tpid(tci):
    return isinstance(tci, list) and 0 <= len(tci) <= 2 and all([0 <= v <= EMUValidator.MAX_16_BITS for v in tci])

def is_valid_tunables(t):
    return isinstance(t, list) and all([isinstance(s, basestring)for s in t])

class EMUValidator(object):
    
    MAX_16_BITS = (2 ** 16) - 1

    # all emu types with their validations
    EMU_VAL_DICT = {
        'mac': is_valid_mac,
        'ipv4': is_valid_ipv4,
        'ipv4_mc': is_valid_ipv4_mc,
        'ipv6': is_valid_ipv6,
        'ipv6_mc': is_valid_ipv6_mc,
        'mtu': lambda x: 256 <= x <= 9000,
        'vport': lambda x: 0 <= x <= EMUValidator.MAX_16_BITS,
        'tci': is_valid_tci_tpid,
        'tpid': is_valid_tci_tpid,
        'tunables': is_valid_tunables,
    }


    @staticmethod
    def verify(list_of_args):
        """
        Check if list_of_args is valid. 
        
            :parameters:
                list_of_args: list
                    List of dictionary with data about the arguments.
                    | list_of_args = [{'name': 'ipv4_mc_arg', 'value': ipv4_mc_arg, 'type': 'ipv4_mc', must: 'False', 'allow_list': True}]
                    | the example above will verify: None, '224.0.0.0', ['224.0.0.0'] but raise exception for: 42, 'FF00::', ['224.0.0.0', 'FF00::']
                    | name: string (Mandatory)
                    |   Name of the argument(for error messages).
                    | arg: Anything (Mandatory)
                    |   The actual variable to validate.
                    | type: string or class instance (Mandatory)
                    |   Might be a string from `EMU_VAL_DICT`('mac', 'ipv4'..) or just the wanted class instance.
                    |   `type` might also be a list of types and `value` should be 1 one them.  
                    | must: bool
                    |   True will validate value is not None, defaults to True.
                    | allow_list: bool
                    |   True will allow `value` to be a list of anything from `types`. 
        
            :raises:
                + :exe:'TRexError': In any case of wrong parameters.
        """

        def _check_types_for_val(types, arg_name, arg_val):
            for t in types:
                if not isinstance(t, str):
                    # type is a class
                    if isinstance(arg_val, t):
                        break
                else:
                    # type is a string, look for it in database
                    test_func = database.get(t, None)
                    if test_func is None:
                        err(arg_name, arg_val, 'Unknown type to EMUValidator "{0}"'.format(t))
                    else:
                        if not test_func(arg_val):
                                err(arg_name, arg_val, 'Argument is not valid for "{0}" type'.format(t))
                        break
            else:
                # got here if code did not break
                err(arg_name, arg_val, 'Not matching type, got: "{0}"'.format(type(arg_val)))

        def err(name, val, reason):
            raise TRexError('Validation error, argument "{name}" with value "{val}"\nReason: {reason}'.format(name=name,
                                                                                                                val=val,
                                                                                                                reason=reason))

        for arg in list_of_args:
            database    = EMUValidator.EMU_VAL_DICT
            arg_name    = arg.get('name')
            arg_val     = arg.get('arg')
            arg_type    = arg.get('t')
            is_must     = arg.get('must', True)
            allow_list  = arg.get('allow_list', False)

            # check if arg is None
            if arg_val is None:
                if is_must:
                    err(arg_name, arg_val, 'Cannot be None')
                else:
                    continue

            arg_types = listify(arg_type)
            if allow_list and isinstance(arg_val, list):
                for val in arg_val:
                    _check_types_for_val(arg_types, arg_name, val)    
            else:
                _check_types_for_val(arg_types, arg_name, arg_val)    
