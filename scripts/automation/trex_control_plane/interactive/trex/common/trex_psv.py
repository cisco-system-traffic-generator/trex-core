"""
Manage port state validation

Author:
  Itay Marom 

"""

from .trex_exceptions import TRexError, TRexTypeError
from .trex_types import listify, STLDynamicProfile
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


def check_args(func):
    """Decorator to check port argument types.
    """
    def wrapper(self, *args, **kwargs):
        code = func.__code__
        fname = func.__name__
        names = code.co_varnames[:code.co_argcount]
        argname = "ports"
        port_index = names.index(argname) - 1
        try:
            argval = args[port_index]
        except IndexError:
            argval = kwargs.get(argname)
        if argval is None:
            raise TypeError("%s(...): arg '%s' is null"
                            % (fname, argname))
        if not isinstance(argval[0], STLDynamicProfile):
            supermeth = getattr(super(self.__class__, self), fname, None)
            if supermeth is not None:
                return supermeth(*args, **kwargs)
        return func(self, *args, **kwargs)
    return wrapper


class PortState(object):
    '''
        abstract class to create a state validation
    '''
    
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        ports = parse_ports_from_profiles(ports)
        invalid_ports = list_difference(ports, self.get_valid_ports(client))
        if invalid_ports:
            self.print_err_msg(invalid_ports, custom_err_msg)
            
    def get_valid_ports (self, client):
        raise NotImplementedError
        
    def print_err_msg (self, invalid_ports, custom_err_msg = None):
        err_msg = self.def_err_msg()
        if custom_err_msg:
            err_msg = '{0} - {1}'.format(err_msg, custom_err_msg)
        raise TRexError('port(s) {0}: {1}'.format(invalid_ports, err_msg))


class PortStateAll(PortState):
    def def_err_msg (self):
        return 'invalid port IDs'

    def get_valid_ports (self, client):
        return client.get_all_ports()
    

class PortStateUp(PortState):
    def def_err_msg (self):
        return 'link is DOWN'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_up()]

        
class PortStateAcquired(PortState):
    def def_err_msg (self):
        return 'must be acquired'

    def get_valid_ports (self, client):
        return client.get_acquired_ports()


class PortStateIdle(PortState):
    @check_args
    def validate (self, client, cmd_name, ports, custom_err_msg = None):

        invalid_ports = []
        dup_port = []

        for port in ports:
            if port.port_id in dup_port:
                self.print_err_msg(ports, "Cannot use * with other profile_ids")

            is_idle = False
            if port.profile_id == "*":
                dup_port.append(port.port_id)
                profile_list = client.ports[port.port_id].profile_manager.get_all_profiles()

                if not profile_list:
                    is_idle = True

                for pid in profile_list:
                    if not client.ports[port.port_id].profile_manager.is_active(pid):
                        is_idle = True
            else:
                if not client.ports[port.port_id].profile_manager.is_active(port.profile_id):
                    is_idle = True

            if not is_idle :
                invalid_ports.append(port)

        if invalid_ports:
            self.print_err_msg (invalid_ports, custom_err_msg)

    def def_err_msg (self):
        return 'are active'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if not client.ports[port_id].is_active()]


class PortStateTX(PortState):
    @check_args
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        invalid_ports = []
        dup_port = []

        for port in ports:
            if port.port_id in dup_port:
                self.print_err_msg(ports, "Cannot use * with other profile_ids")

            is_active = False
            if port.profile_id == "*":
                dup_port.append(port.port_id)
                is_active = client.ports[port.port_id].profile_manager.has_active()
            else:
                is_active = client.ports[port.port_id].profile_manager.is_active(port.profile_id)

            if not is_active :
                invalid_ports.append(port)

        if invalid_ports:
            self.print_err_msg (invalid_ports, custom_err_msg)

    def def_err_msg (self):
        return 'are not active'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_active()]

class PortStatePaused(PortState):
    @check_args
    def validate (self, client, cmd_name, ports, custom_err_msg = None):

        invalid_ports = []
        dup_port = []

        for port in ports:
            if port.port_id in dup_port:
                self.print_err_msg(ports, "Cannot use * with other profile_ids")

            is_paused = False
            if port.profile_id == "*":
                dup_port.append(port.port_id)
                is_paused = client.ports[port.port_id].profile_manager.has_paused()
            else:
                is_paused = client.ports[port.port_id].profile_manager.is_paused(port.profile_id)

            if not is_paused :
                invalid_ports.append(port)

        if invalid_ports:
            self.print_err_msg (invalid_ports, custom_err_msg)

    def def_err_msg (self):
        return 'are not paused'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_paused()]


class PortStateService(PortState):
    def def_err_msg (self):
        return 'must be under service mode'
        
    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_service_mode_on()]
    

class PortStateNonService(PortState):
    def def_err_msg (self):
        return 'cannot be under service mode'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if not client.ports[port_id].is_service_mode_on()]

        
class PortStateL3(PortState):
    def def_err_msg (self):
        return 'does not have a valid IPv4 address'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_l3_mode()]


class PortStateResolved(PortState):
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
        ports = listify(ports)

        if not isinstance(ports, (set, list, tuple)):
            raise TRexTypeError('ports', type(ports), list)

        if has_dup(ports):
            raise TRexError('duplicate port(s) are not allowed')

        if not ports and not allow_empty:
            raise TRexError('action requires at least one port')
            
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

