"""
Manage port state validation

Author:
  Itay Marom 

"""

from .trex_stl_exceptions import *
from .utils.common import *

# type of states to be enforced
# types starting with _ are internally used
_PSV_ALL, PSV_UP, PSV_ACQUIRED, PSV_IDLE, PSV_RESOLVED, PSV_SERVICE, PSV_NON_SERVICE, PSV_L3 = range(0, 8)


class PortState(object):
    '''
        abstract class to create a state validation
    '''
    
    def validate (self, client, cmd_name, ports, custom_err_msg = None):
        invalid_ports = list_difference(ports, self.get_valid_ports(client))
        if invalid_ports:
            err_msg = self.def_err_msg()
            if custom_err_msg:
                err_msg = '{0} - {1}'.format(err_msg, custom_err_msg)
            
            raise STLError('{0} - port(s) {1}: {2}'.format(cmd_name, invalid_ports, err_msg))
        
            
    def get_valid_ports (self, client):
        raise NotImplementedError
        

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
    def def_err_msg (self):
        return 'are active'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if not client.ports[port_id].is_active()]
  

class PortStateService(PortState):
    def def_err_msg (self):
        return 'must be under service mode'
        
    def get_valid_ports (self, client):
        return client.get_service_enabled_ports()
    

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
        self.validators[PSV_RESOLVED]     = PortStateResolved()
        self.validators[PSV_SERVICE]      = PortStateService()
        self.validators[PSV_L3]           = PortStateL3()
        self.validators[PSV_NON_SERVICE]  = PortStateNonService()
    
            
    def validate (self, cmd_name, ports, states = None, allow_empty = False):
        '''
            main validator
            
        '''
        
        # listify
        ports = ports if isinstance(ports, (list, tuple)) else [ports]
        ports = list_remove_dup(ports)
        
        if not ports and not allow_empty:
            raise STLError('{0} - action requires at least one port'.format(cmd_name))
            
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

