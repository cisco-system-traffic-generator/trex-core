from .trex_stl_exceptions import *
from .utils.common import *

# type of states to be enforced
# types starting with _ are internally used
_PSV_ALL, PSV_ACQUIRED, PSV_SERVICE, PSV_L3, PSV_ACTIVE  = range(0, 5)


class PortState(object):
        
    def validate (self, client, cmd_name, ports, err_msg = None):
        invalid_ports = list_difference(ports, self.get_valid_ports(client))
        if invalid_ports:
            raise STLError('{0} - port(s) {1}: {2}'.format(cmd_name, invalid_ports,
                                                           err_msg if err_msg is not None else self.def_err_msg()))
        
            
    def get_valid_ports (self, client):
        raise NotImplementedError
        

class PortStateAll(PortState):
    def def_err_msg (self):
        return 'invalid port IDs'

    def get_valid_ports (self, client):
        return client.get_all_ports()
    

class PortStateAcquired(PortState):
    def def_err_msg (self):
        return 'must be acquired'

    def get_valid_ports (self, client):
        return client.get_acquired_ports()
    

class PortStateService(PortState):
    def def_err_msg (self):
        return 'must be under service mode'
        
    def get_valid_ports (self, client):
        return client.get_service_enabled_ports()
            

class PortStateL3(PortState):
    def def_err_msg (self):
        return 'does not have a valid IPv4 address'

    def get_valid_ports (self, client):
        return [port_id for port_id in client.get_all_ports() if client.ports[port_id].is_l3_mode()]

        
class PortStateValidator(object):
    def __init__ (self, client):
        self.client = client

        self.validators = {}
        
        self.validators[_PSV_ALL]      = PortStateAll()
        self.validators[PSV_ACQUIRED]  = PortStateAcquired()
        self.validators[PSV_SERVICE]   = PortStateService()
        self.validators[PSV_L3]        = PortStateL3()
        
        
    def chk_port_state (self, cmd_name, ports, states):
        
        # listify
        ports = ports if isinstance(ports, (list, tuple)) else [ports]
        ports = list_remove_dup(ports)
        
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
            assert(0)
        
        for state, err_msg in states_map.items():
            self.validators[state].validate(self.client, cmd_name, ports, err_msg)
            

        # returns the ports listify
        return ports
