"""
Manage port state validation

Author:
  Itay Marom 

"""
from functools import wraps
from .trex_exceptions import TRexError, TRexTypeError
from .trex_types import listify, PortProfileID
from ..utils.common import *

# type of states to be enforced
# types starting with _ are internally used
_PSV_ALL, PSV_UP, PSV_ACQUIRED, PSV_IDLE, PSV_TX, PSV_PAUSED, PSV_RESOLVED, PSV_SERVICE, PSV_NON_SERVICE, PSV_L3 = range(0, 10)


__all__ = ['PSV_UP',
           'PSV_ACQUIRED',
           'PSV_IDLE',
           'PSV_TX',
           'PSV_PAUSED',
           'PSV_RESOLVED',
           'PSV_SERVICE',
           'PSV_NON_SERVICE',
           'PSV_L3',
           'PortStateValidator']


def convert_profile_to_port(port_arg):
    '''
       Decorator to convert profile type argument to port ids only.
    '''
    def wrap (func):
        @wraps(func)
        def wrapper(self, *args, **kwargs):
            code = func.__code__
            fname = func.__name__
            names = code.co_varnames[:code.co_argcount]
            argname = port_arg
            try:
                port_index = names.index(argname) - 1
                argval = args[port_index]
                args = list(args)
                args[port_index] = parse_ports_from_profiles(argval)
                args = tuple(args)
            except (ValueError, IndexError):
                argval = kwargs.get(argname)
                kwargs[argname] = parse_ports_from_profiles(argval)

            supermeth = getattr(super(self.__class__, self), fname, None)
            return supermeth(*args, **kwargs)
        return wrapper
    return wrap


class PortState(object):
    '''
        abstract class to create a state validation
    '''

    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        # Port state comparison
        port_comparison_list = self.get_valid_ports(client)
        if ports:
            if isinstance(ports[0], PortProfileID):
                # Profile state comparison(IDLE/TX/Paused)
                port_comparison_list = self.get_valid_profiles(client)

        invalid_ports = list_difference(ports, port_comparison_list)
        if invalid_ports:
            self.print_err_msg(invalid_ports, custom_err_msg)

    def print_err_msg (self, invalid_ports, custom_err_msg = None):
        err_msg = self.def_err_msg()
        if custom_err_msg:
            err_msg = '{0} - {1}'.format(err_msg, custom_err_msg)
        raise TRexError('port(s) {0}: {1}'.format(invalid_ports, err_msg))

    def get_valid_ports (self, client):
        raise NotImplementedError

    def get_valid_profiles (self, client):
        raise NotImplementedError


class PortStateAll(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'invalid port IDs'

    def get_valid_ports (self, client):
        return client.get_all_ports()


class PortStateUp(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'link is DOWN'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_up()]

        
class PortStateAcquired(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'must be acquired'

    def get_valid_ports (self, client):
        return client.get_acquired_ports()


class PortStateIdle(PortState):
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        # Profile state comparison
        if ports:
            if isinstance(ports[0], PortProfileID):
                invalid_ports = list_intersect(ports, client.get_profiles_with_state("active"))
                if invalid_ports:
                    self.print_err_msg(invalid_ports, custom_err_msg)
                return

        # Port state comparison
        super(PortStateIdle, self).validate(client, cmd_name, ports, custom_err_msg)

    def def_err_msg (self):
        return 'are active'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if not client.ports[port_id].is_active()]


class PortStateTX(PortState):
    def def_err_msg (self):
        return 'are not active'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_active()]

    def get_valid_profiles (self, client):
        return client.get_profiles_with_state("active")


class PortStatePaused(PortState):
    def def_err_msg (self):
        return 'are not paused'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_paused()]

    def get_valid_profiles (self, client):
        return client.get_profiles_with_state("paused")


class PortStateService(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'must be under service mode'
        
    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_service_mode_on()]
    

class PortStateNonService(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'cannot be under service mode'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if not client.ports[port_id].is_service_mode_on()]

        
class PortStateL3(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'does not have a valid IPv4 address'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_l3_mode()]


class PortStateResolved(PortState):
    @convert_profile_to_port("ports")
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        pass

    def def_err_msg (self):
        return 'must have resolved destination address'

    def get_valid_ports (self, client):
        return client.get_resolved_ports()


class PortStateValidator(object):
    '''
        port state validator

        used to validate different groups of states
        required for 'ports'
    '''
    
    def __init__ (self, client):
        self.client = client

        self.validators = {}
        
        self.validators[_PSV_ALL]         = PortStateAll()
        self.validators[PSV_UP]           = PortStateUp()
        self.validators[PSV_ACQUIRED]     = PortStateAcquired()
        self.validators[PSV_IDLE]         = PortStateIdle()
        self.validators[PSV_TX]           = PortStateTX()
        self.validators[PSV_PAUSED]       = PortStatePaused()
        self.validators[PSV_RESOLVED]     = PortStateResolved()
        self.validators[PSV_SERVICE]      = PortStateService()
        self.validators[PSV_L3]           = PortStateL3()
        self.validators[PSV_NON_SERVICE]  = PortStateNonService()
    
            
    def validate (self, cmd_name, ports, states = None, allow_empty = False):
        '''
            main validator
            
        '''
        
        # listify
        if isinstance(ports, (int, str, PortProfileID)):
            ports = listify(ports)

        if not isinstance(ports, (set, list, tuple)):
            raise TRexTypeError('ports', type(ports), list)

        if has_dup(ports):
            raise TRexError('duplicate port(s) are not allowed')

        if not ports and not allow_empty:
            raise TRexError('action requires at least one port')

        # translate * profiles for validation purposes
        if ports:
            if isinstance(ports[0], PortProfileID):
                ports = self.client.validate_profile_input(ports)
            
        # default checks for every command
        states_map = {_PSV_ALL: None}
        
        # eventually states is a map from every mandatory state to it's error message
        if isinstance(states, int):
            states_map[states] = None
        elif isinstance(states, (list, tuple)):
            for s in states:
                states_map[s] = None
        elif isinstance(states, dict):
            states_map.update(states)
        else:
            pass

        for state, err_msg in states_map.items():
            self.validators[state].validate(self.client, cmd_name, ports, err_msg)

        # returns the ports listify
        return ports

