from collections import OrderedDict, defaultdict
from ..common.trex_types import listify
from .trex_stl_streams import STLStream

class STLProfileManager(object):
    (STATE_IDLE,
    STATE_STREAMS,
    STATE_TX,
    STATE_PAUSE,
    STATE_PCAP_TX,
    STATE_ASTF_LOADED,
    STATE_ASTF_PARSE,
    STATE_ASTF_BUILD,
    STATE_ASTF_CLEANUP) = range(9)

    STATES_MAP = {STATE_IDLE:         'IDLE',
                  STATE_STREAMS:      'IDLE',
                  STATE_TX:           'TRANSMITTING',
                  STATE_PAUSE:        'PAUSE',
                  STATE_PCAP_TX :     'TRANSMITTING',
                  STATE_ASTF_LOADED:  'LOADED',
                  STATE_ASTF_PARSE:   'PARSING',
                  STATE_ASTF_BUILD:   'BUILDING',
                  STATE_ASTF_CLEANUP: 'CLEANUP'}

    def __init__ (self, port_id):
        self.port_id = port_id
        self.stream_list = {}
        self.state_list = {}

    def add_profile_stream (self, stream_id, profile_id = "_", state = STATE_STREAMS):
        self.stream_list.setdefault(profile_id, [])
        self.stream_list[profile_id].append(stream_id)
        self.state_list.setdefault(profile_id, state)
        return self.stream_list.get(profile_id)

    def update_profile_state (self, state, profile_id = "_"):
        if state:
            if profile_id == "*":
                for key in self.stream_list.keys():
                    self.state_list[key] = state
            elif type(profile_id) == list:
                 for key in profile_id:
                     self.state_list[key] = state
            else:
                 self.state_list[profile_id] = state
        #return None if not found
        return self.state_list

    def delete_profile (self, profile_id = "_"):
        stream_ids = self.stream_list.get(profile_id)
        if profile_id == "*":
            stream_ids = self.stream_list.keys()
            self.stream_list.clear()
            self.state_list.clear()
        elif stream_ids:
            del self.stream_list[profile_id]
            del self.state_list[profile_id]
        else:
            stream_ids = []

        return stream_ids

    def get_all_profiles(self):
        return self.state_list.keys()

    def get_profiles (self, state = None):
        profile_list = []
        for profile_id, profile_state in self.state_list.items():
            if profile_state == state:
                profile_list.append(profile_id)
        #returns list of profiles with the state
        return profile_list

    def get_all_profile_streams (self):
        return self.stream_list.values()

    def get_profile_streams (self, profile_id = "_"):
        return self.stream_list.get(profile_id)

    def get_profile_state_list (self):
        return self.state_list

    def get_all_profile_state (self):
        return self.state_list.values()

    def get_profile_state (self, profile_id = "_"):
        return self.state_list.get(profile_id)

    def get_port_state (self):
        if self.has_state(self.STATE_PCAP_TX):
            return self.STATE_PCAP_TX
        elif self.has_state(self.STATE_TX):
            return self.STATE_TX
        elif self.has_state(self.STATE_PAUSE):
            return self.STATE_PAUSE
        elif self.has_state(self.STATE_STREAMS):
            return self.STATE_STREAMS
        elif self.has_state(self.STATE_IDLE):
            return self.STATE_IDLE
        else:
            raise Exception("port {0}: bad state received from server".format(self.port_id))

    def has_state(self, state):
        if not self.state_list and state == self.STATE_IDLE:
            return True
        return state in self.state_list.values()

    def has_active (self):
        result = False
        for profile_id in self.state_list.keys():
            if self.is_active(profile_id):
                result = True
        return result

    def has_paused (self):
        result = False
        for profile_id in self.state_list.keys():
            if self.is_paused(profile_id):
                result = True
        return result

    def is_active (self, profile_id = "_"):
        profile_state = self.get_profile_state(profile_id)
        if profile_state == None:
            return False
        return (profile_state == self.STATE_TX ) or (profile_state == self.STATE_PAUSE) or (profile_state == self.STATE_PCAP_TX)

    def is_paused (self, profile_id = "_"):
        profile_state = self.get_profile_state(profile_id)
        if profile_state == None:
            return False
        return (profile_state == self.STATE_PAUSE)

