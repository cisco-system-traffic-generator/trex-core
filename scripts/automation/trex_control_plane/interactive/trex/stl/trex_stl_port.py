from datetime import datetime, timedelta
from collections import OrderedDict

from ..utils.common import list_difference, list_intersect
from ..utils.text_opts import limit_string
from ..utils import text_tables

from ..common.trex_types import listify, RpcCmdData, RC, RC_OK, PortProfileID, DEFAULT_PROFILE_ID, ALL_PROFILE_ID
from ..common.trex_port import Port, owned, writeable, up

from .trex_stl_streams import STLStream


########## utlity ############
def mult_to_factor (mult, max_bps_l2, max_pps, line_util):
    if mult['type'] == 'raw':
        return mult['value']

    if mult['type'] == 'bps':
        return mult['value'] / max_bps_l2

    if mult['type'] == 'pps':
        return mult['value'] / max_pps

    if mult['type'] == 'percentage':
        return mult['value'] / line_util



class STLPort(Port):

    MASK_ALL = ((1 << 64) - 1)

    def __init__ (self, ctx, port_id, rpc, info, dynamic):
        Port.__init__(self, ctx, port_id, rpc, info)

        self.has_rx_streams = False
        self.streams = {}
        self.profile = None
        self.next_available_id = 1

        self.is_dynamic = dynamic
        self.profile_stream_list = {}
        self.profile_state_list = {}

############################   dynamic profile   #############################
############################   helper functions  #############################

    def __set_profile_stream_id (self, stream_id, profile_id = DEFAULT_PROFILE_ID, state = None):
        if not state:
            state = self.STATE_STREAMS
        self.profile_stream_list.setdefault(profile_id, [])
        if stream_id not in self.profile_stream_list.get(profile_id):
            self.profile_stream_list[profile_id].append(stream_id)
        self.profile_state_list.setdefault(profile_id, state)
        return self.profile_stream_list.get(profile_id)


    def __get_stream_profile(self, stream_id):
        if not self.profile_stream_list:
            return None
        for profile_id, streams in self.profile_stream_list.items():
            if stream_id in streams:
                return profile_id
        return None

    def __set_profile_state (self, state, profile_id = DEFAULT_PROFILE_ID):
        if state:
            if profile_id == ALL_PROFILE_ID:
                for key in self.profile_state_list.keys():
                    self.profile_state_list[key] = state
            elif type(profile_id) == list:
                 for key in profile_id:
                     self.profile_state_list[key] = state
            else:
                 self.profile_state_list[profile_id] = state
        #return None if not found
        return self.profile_state_list


    def __delete_profile (self, profile_id = DEFAULT_PROFILE_ID):
        stream_ids = self.profile_stream_list.get(profile_id)
        if profile_id == ALL_PROFILE_ID:
            stream_ids = self.profile_stream_list.keys()
            self.profile_stream_list.clear()
            self.profile_state_list.clear()
        elif stream_ids:
            del self.profile_stream_list[profile_id]
            del self.profile_state_list[profile_id]
        else:
            if profile_id in self.profile_state_list:
                del self.profile_state_list[profile_id]
            if profile_id in self.profile_stream_list:
                del self.profile_stream_list[profile_id]
            stream_ids = []
        #return None if not found
        return stream_ids


    def __delete_profile_stream (self, stream_id, profile_id = DEFAULT_PROFILE_ID):
        if self.profile_stream_list.get(profile_id):
            self.profile_stream_list[profile_id].remove(stream_id)
        #if empty, make it idle
        if not self.profile_stream_list.get(profile_id):
            del self.profile_stream_list[profile_id]
            del self.profile_state_list[profile_id]


    def __sync_port_state_from_profile (self):
        if self.STATE_PCAP_TX  in self.profile_state_list.values():
            self.state = self.STATE_PCAP_TX
        elif self.STATE_TX in self.profile_state_list.values():
            self.state = self.STATE_TX
        elif self.STATE_PAUSE in self.profile_state_list.values():
            self.state = self.STATE_PAUSE
        elif self.STATE_STREAMS in self.profile_state_list.values():
            self.state = self.STATE_STREAMS
        elif self.STATE_IDLE in self.profile_state_list.values() or not self.profile_state_list:
            self.state = self.STATE_IDLE
        else:
            raise Exception("port {0}: bad state received from server".format(self.port_id))


    def __get_profiles_from_state (self, state):
        result = []
        for pid, pstate in self.profile_state_list.items():
            if pstate == state:
                result.append(pid)
        return result


    def __state_from_name_dynamic(self, profile_state):
        if profile_state == "IDLE":
            return self.STATE_IDLE
        elif profile_state == "STREAMS":
            return self.STATE_STREAMS
        elif profile_state == "TX":
            return self.STATE_TX
        elif profile_state == "PAUSE":
            return self.STATE_PAUSE
        ##ASTF and PCAP not supported for profile
        else:
            raise Exception("port {0}: bad state received from server '{1}'".format(self.port_id, profile_state))


    def state_from_name_dynamic(self, profile_state):
        # dict is returned from dynamic profile server version(new)
        for profile_id,state in profile_state.items():
            profile_state = self.__state_from_name_dynamic(state)
            self.__set_profile_state(profile_state, profile_id)

############################     event      #############################
############################     handler    #############################
    # for PCAP_TX state
    def async_event_port_stopped (self):
        if not self.is_acquired():
            if self.state == self.STATE_PCAP_TX:
                self.__set_profile_state(self.STATE_STREAMS, DEFAULT_PROFILE_ID)
            self.state = self.STATE_STREAMS

    def async_event_port_job_done (self):
        # until thread is locked - order is important
        self.tx_stopped_ts = datetime.now()
        if self.state == self.STATE_PCAP_TX:
            self.__set_profile_state(self.STATE_STREAMS, DEFAULT_PROFILE_ID)
        self.state = self.STATE_STREAMS

        self.last_factor_type = None

    def async_event_profile_started (self, profile_id):
        if not self.is_acquired():
            self.__set_profile_state(self.STATE_TX, profile_id)
            self.__sync_port_state_from_profile()


    def async_event_profile_stopped (self, profile_id):
        if not self.is_acquired():
            self.__set_profile_state(self.STATE_STREAMS, profile_id)
            self.__sync_port_state_from_profile()


    def async_event_profile_paused (self, profile_id):
        if not self.is_acquired():
            self.__set_profile_state(self.STATE_PAUSE, profile_id)
            self.__sync_port_state_from_profile()


    def async_event_profile_resumed (self, profile_id):
        if not self.is_acquired():
            self.__set_profile_state(self.STATE_TX, profile_id)
            self.__sync_port_state_from_profile()


    def async_event_profile_job_done (self, profile_id):
        # until thread is locked - order is important
        self.tx_stopped_ts = datetime.now()
        self.__set_profile_state(self.STATE_STREAMS, profile_id)
        self.__sync_port_state_from_profile()

        self.last_factor_type = None


############################  STL PORT API  #############################
############################                #############################

    def sync_port_streams(self, rc_data):
        try:
            # dynamic profile server version (profiles in rc_data.keys())
            if self.is_dynamic:
                for profile_id, stream_value_list in rc_data['profiles'].items():
                    for stream_id, stream_value in stream_value_list.items():
                        self.__set_profile_stream_id(int(stream_id), str(profile_id))
                        self.streams[int(stream_id)] = STLStream.from_json(stream_value)
            # legacy server version (streams in rc_data.keys())
            else:
                for k, v in rc_data['streams'].items():
                    self.streams[int(k)] = STLStream.from_json(v)
                    self.__set_profile_stream_id(int(k))
        except Exception as e:
             print(e)
             raise Exception("invalid return from server, %s" % rc_data)

    def sync(self):
        params = {"port_id": self.port_id,
                  "profile_id": ALL_PROFILE_ID,'block': False}

        rc = self.transmit("get_port_status", params)
        if rc.bad():
            return self.err(rc.err())

        if self.is_dynamic:
            self.state_from_name_dynamic(rc.data()['state_profile'])

        if str(rc.data()['state']) == "PCAP_TX":
            self.__set_profile_state(self.STATE_PCAP_TX, DEFAULT_PROFILE_ID)

        self.__sync_port_state_from_profile()

        return self.sync_shared (rc.data())



    # sync all the streams with the server
    def sync_streams (self):

        self.streams = {}
        
        params = {"port_id": self.port_id,
                  "profile_id": ALL_PROFILE_ID}

        rc = self.transmit("get_all_streams", params)
        if rc.bad():
            return self.err(rc.err())

        self.sync_port_streams(rc.data())
        return self.ok()


    @owned
    def pause_streams(self, stream_ids, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("invalid profile_id [%s]" % profile_id)

        profile_state = self.profile_state_list.get(profile_id)
        if not profile_state :
            return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))

        if profile_state == self.STATE_PCAP_TX:
            return self.err('pause is not supported during PCAP TX')

        if profile_state != self.STATE_TX and profile_state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'profile_id': profile_id,
                  'stream_ids': stream_ids or []}

        rc  = self.transmit('pause_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    @owned
    def resume_streams(self, stream_ids, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("Invalid profile_id [%s]" % profile_id)

        profile_state = self.profile_state_list.get(profile_id)
        if not profile_state:
            return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))

        if profile_state == self.STATE_PCAP_TX:
            return self.err('resume is not supported during PCAP TX')

        if profile_state != self.STATE_TX and profile_state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'profile_id': profile_id,
                  'stream_ids': stream_ids or []}

        rc = self.transmit('resume_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()

    
    @owned
    def update_streams(self, mul, force, stream_ids, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("Invalid profile_id [%s]" % profile_id)

        profile_state = self.profile_state_list.get(profile_id)
        if not profile_state:
            return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))

        if profile_state == self.STATE_PCAP_TX:
            return self.err('update is not supported during PCAP TX')

        if profile_state != self.STATE_TX and profile_state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  "profile_id": profile_id,
                  'mul':        mul,
                  'force':      force,
                  'stream_ids': stream_ids or []}

        rc = self.transmit('update_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    def _allocate_stream_id (self):
        id = self.next_available_id
        self.next_available_id += 1
        return id

    # add streams
    @writeable
    def add_streams (self, streams_list, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("Invalid profile_id [%s]" % profile_id)

        # listify
        streams_list = listify(streams_list)
        lookup = {}

        # allocate IDs
        for stream in streams_list:

            # allocate stream id
            stream_id = stream.get_id() if stream.get_id() is not None else self._allocate_stream_id()
            if stream_id in self.streams:
                return self.err('Stream ID: {0} already exists'.format(stream_id))

            # name
            name = stream.get_name() if stream.get_name() is not None else id(stream)
            if name in lookup:
                return self.err("multiple streams with duplicate name: '{0}'".format(name))
            lookup[name] = stream_id

        batch = []
        for stream in streams_list:

            name = stream.get_name() if stream.get_name() is not None else id(stream)
            stream_id = lookup[name]
            next_id = -1

            next = stream.get_next()
            if next:
                if next not in lookup:
                    return self.err("stream dependency error - unable to find '{0}'".format(next))
                next_id = lookup[next]

            stream_json = stream.to_json()
            stream_json['next_stream_id'] = next_id

            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "profile_id": profile_id,
                      "stream_id": stream_id,
                      "stream": stream_json}


            cmd = RpcCmdData('add_stream', params, 'core')
            batch.append(cmd)


        rc = self.transmit_batch(batch)
        ret = RC()


        for i, single_rc in enumerate(rc):
            if single_rc:
                stream_id = batch[i].params['stream_id']
                self.streams[stream_id] = streams_list[i].clone()
                self.__set_profile_stream_id(int(stream_id), str(profile_id), self.STATE_STREAMS)

                ret.add(RC_OK(data = stream_id))

                self.has_rx_streams = self.has_rx_streams or streams_list[i].has_flow_stats()

            else:
                ret.add(single_rc)


        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        return ret if ret else self.err(str(ret))

    # remove stream from port
    @writeable
    def remove_streams (self, stream_id_list, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("Invalid profile_id [%s]" % profile_id)

        # single element to list
        stream_id_list = listify(stream_id_list)

        profile_streams = self.profile_stream_list.get(profile_id)

        # verify existance
        not_found = list_difference(stream_id_list, profile_streams)
        found     = list_intersect(stream_id_list, profile_streams)

        batch = []
        
        for stream_id in found:
            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "profile_id": profile_id,
                      "stream_id": stream_id}

            cmd = RpcCmdData('remove_stream', params, 'core')
            batch.append(cmd)


        if batch:
            rc = self.transmit_batch(batch)
            for i, single_rc in enumerate(rc):
                if single_rc:
                    id = batch[i].params['stream_id']
                    del self.streams[id]
                    self.__delete_profile_stream(id, profile_id)

            self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE
    
            # recheck if any RX stats streams present on the port
            self.has_rx_streams = any([stream.has_flow_stats() for stream in self.streams.values()])

            # did the batch send fail ?
            if not rc:
                return self.err(rc.err())
            
                
        # partially succeeded ?
        return self.err("stream(s) {0} do not exist".format(not_found)) if not_found else self.ok()


    # remove all the streams
    @writeable
    def remove_all_streams (self, profile_id = DEFAULT_PROFILE_ID):

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id": profile_id}

        rc = self.transmit("remove_all_streams", params)
        if not rc:
            return self.err(rc.err())

        streams_deleted = self.__delete_profile(profile_id)

        for stream_id in streams_deleted:
            if self.streams.get(stream_id):
                del self.streams[stream_id]

        if not self.streams:
            self.state = self.STATE_IDLE
            self.has_rx_streams = False

        return self.ok()


    # get a specific stream
    def get_stream (self, stream_id):
        if stream_id in self.streams:
            return self.streams[stream_id]
        else:
            return None

    def get_all_streams (self):
        return self.streams


    @owned
    def set_service_mode (self, enabled, filtered, mask):
        params = {"handler": self.handler,
                    "port_id": self.port_id,
                    "enabled": enabled,
                    "filtered": filtered}
        if filtered:
            params['mask'] = mask

        rc = self.transmit("service", params)
        if rc.bad():
            return self.err(rc.err())

        self.service_mode = enabled
        self.service_mode_filtered = filtered
        self.service_mask = mask
        return self.ok()
        

    def is_service_mode_on (self):
        if not self.is_acquired(): # update lazy
            self.sync()
        return self.service_mode

    def is_service_filtered_mode_on (self):
        if not self.is_acquired(): # update lazy
            self.sync()
        return self.service_mode_filtered

    # take the port
    def acquire(self, force = False):
        params = {"port_id":     self.port_id,
                  "user":        self.ctx.username,
                  "session_id":  self.ctx.session_id,
                  "force":       force}

        rc = self.transmit("acquire", params)
        if not rc:
            return self.err(rc.err())
        self._set_handler(rc.data())

        return self.ok()

    # release the port
    def release(self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        rc = self.transmit("release", params)

        if rc.good():

            self._clear_handler()

            return self.ok()
        else:
            return self.err(rc.err())

    @writeable
    def start (self, mul, duration, force, mask, start_at_ts = 0, profile_id = DEFAULT_PROFILE_ID):

        if profile_id == ALL_PROFILE_ID:
            return self.err("Invalid profile_id [%s]" % profile_id)

        profile_state = self.profile_state_list.get(profile_id)
        if not profile_state:
            return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))

        if profile_state == self.STATE_IDLE:
            return self.err("unable to start traffic - no streams attached to port")

        params = {"handler":     self.handler,
                  "port_id":     self.port_id,
                  "profile_id":  profile_id,
                  "mul":         mul,
                  "duration":    duration,
                  "force":       force,
                  "core_mask":   mask if mask is not None else self.MASK_ALL,
                  'start_at_ts': start_at_ts}
   
        # must set this before to avoid race with the async response
        last_state = self.state
        self.state = self.STATE_TX

        rc = self.transmit("start_traffic", params)

        if rc.bad():
            self.state = last_state
            return self.err(rc.err())

        self.__set_profile_state(self.state, profile_id)

        # save this for TUI
        self.last_factor_type = mul['type']
        
        return rc

    def is_writeable (self):
        if self.is_dynamic:
            # operations on port can be done on state idle or state streams
            return self.state in (self.STATE_IDLE, self.STATE_STREAMS, self.STATE_TX, self.STATE_PAUSE, self.STATE_ASTF_LOADED)
        else:
            return super(STLPort, self).is_writeable()

    # stop traffic
    # with force ignores the cached state and sends the command
    @owned
    def stop (self, force = False, profile_id = DEFAULT_PROFILE_ID):

        # if not is not active and not force - go back
        if not self.is_active() and not force:
            return self.ok()

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id":  profile_id}

        rc = self.transmit("stop_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.__set_profile_state(self.STATE_STREAMS, profile_id)
        self.__sync_port_state_from_profile()
        self.last_factor_type = None
        
        # timestamp for last tx
        self.tx_stopped_ts = datetime.now()
        
        return self.ok()


    @owned
    def pause (self, profile_id = DEFAULT_PROFILE_ID):

        profile_list = []
        if profile_id == ALL_PROFILE_ID:
            profile_list = self.__get_profiles_from_state(self.STATE_TX)
        else:
            profile_state = self.profile_state_list.get(profile_id)
            if not profile_state:
                return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))
            if (profile_state == self.STATE_PCAP_TX) :
                return self.err("pause is not supported during PCAP TX")
            if (profile_state != self.STATE_TX) :
                return self.err("port is not transmitting")
            profile_list.append(profile_id)

        if not profile_list :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id":  profile_id}

        rc  = self.transmit("pause_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.__set_profile_state(self.STATE_PAUSE, profile_list)
        self.__sync_port_state_from_profile()
        return self.ok()

    @owned
    def resume (self, profile_id = DEFAULT_PROFILE_ID):
        profile_list = []
        if profile_id == ALL_PROFILE_ID:
            profile_list = self.__get_profiles_from_state(self.STATE_PAUSE)
        else:
            profile_state = self.profile_state_list.get(profile_id)
            if not profile_state:
                return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))
            if profile_state != self.STATE_PAUSE:
                return self.err("port is not in pause mode")
            profile_list.append(profile_id)

        if not profile_list :
            return self.err("port is not in pause mode")


        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id": profile_id}

        # only valid state after stop

        rc = self.transmit("resume_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_TX
        self.__set_profile_state(self.state, profile_list)

        return self.ok()


    @owned
    def update (self, mul, force, profile_id = DEFAULT_PROFILE_ID):

        profile_list = []
        if profile_id == ALL_PROFILE_ID:
            profile_list = self.__get_profiles_from_state(self.STATE_TX)
        else:
            profile_state = self.profile_state_list.get(profile_id)
            if not profile_state :
                return self.err("profile [%s] does not exist in the port [%s]" %(profile_id, self.port_id))
            if (profile_state == self.STATE_PCAP_TX) :
                return self.err("update is not supported during PCAP TX")
            if (profile_state != self.STATE_TX) :
                return self.err("port is not transmitting")
            profile_list.append(profile_id)

        if not profile_list :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id":  profile_id,
                  "mul":     mul,
                  "force":   force}

        rc = self.transmit("update_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        # save this for TUI
        self.last_factor_type = mul['type']

        return self.ok()


    @writeable
    def push_remote (self, pcap_filename, ipg_usec, speedup, count, duration, is_dual, slave_handler, min_ipg_usec):

        if ((self.state == self.STATE_TX) or (self.state == self.STATE_PAUSE)):
            return self.err("push_remote is not allowed while transmitting traffic")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "pcap_filename": pcap_filename,
                  "ipg_usec": ipg_usec if ipg_usec is not None else -1,
                  "speedup": speedup,
                  "count": count,
                  "duration": duration,
                  "is_dual": is_dual,
                  "slave_handler": slave_handler,
                  "min_ipg_usec": min_ipg_usec if min_ipg_usec else 0}

        rc = self.transmit("push_remote", params, retry = 4)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_PCAP_TX
        self.__set_profile_state(self.state, DEFAULT_PROFILE_ID)

        return self.ok()


    @owned
    def validate (self):

        if (self.state == self.STATE_IDLE):
            return self.err("no streams attached to port")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("validate", params)
        if rc.bad():
            return self.err(rc.err())

        self.profile = rc.data()

        return self.ok()


    def get_profile (self):
        return self.profile

    def get_port_profiles (self, state = "all"):
        result = []
        for profile_id, profile_state in self.profile_state_list.items():
            port_profile = PortProfileID(str(self.port_id) + "." + str(profile_id))
            if state == "active":
                if (profile_state == self.STATE_TX ) or (profile_state == self.STATE_PAUSE) or (profile_state == self.STATE_PCAP_TX):
                    result.append(port_profile)
            elif state == "transmitting":
                if (profile_state == self.STATE_TX ) or (profile_state == self.STATE_PCAP_TX):
                    result.append(port_profile)
            elif state == "paused":
                if (profile_state == self.STATE_PAUSE):
                    result.append(port_profile)
            elif state == "streams":
                if (profile_state == self.STATE_STREAMS):
                    result.append(port_profile)
            elif state == "all":
                result.append(port_profile)
            else:
                raise Exception("invalid state input, %s" %state)
        return result

    def print_profile (self, mult, duration):
        if not self.get_profile():
            return

        rate = self.get_profile()['rate']
        graph = self.get_profile()['graph']

        print(format_text("Profile Map Per Port\n", 'underline', 'bold'))

        factor = mult_to_factor(mult, rate['max_bps_l2'], rate['max_pps'], rate['max_line_util'])

        print("Profile max BPS L2    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l2'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l2'] * factor, suffix = "bps")))

        print("Profile max BPS L1    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l1'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l1'] * factor, suffix = "bps")))

        print("Profile max PPS       (base / req):   {:^12} / {:^12}".format(format_num(rate['max_pps'], suffix = "pps"),
                                                                             format_num(rate['max_pps'] * factor, suffix = "pps"),))

        print("Profile line util.    (base / req):   {:^12} / {:^12}".format(format_percentage(rate['max_line_util']),
                                                                             format_percentage(rate['max_line_util'] * factor)))


        # duration
        exp_time_base_sec = graph['expected_duration'] / (1000 * 1000)
        exp_time_factor_sec = exp_time_base_sec / factor

        # user configured a duration
        if duration > 0:
            if exp_time_factor_sec > 0:
                exp_time_factor_sec = min(exp_time_factor_sec, duration)
            else:
                exp_time_factor_sec = duration


        print("Duration              (base / req):   {:^12} / {:^12}".format(format_time(exp_time_base_sec),
                                                                             format_time(exp_time_factor_sec)))

    ################# profiles printout ######################
    def generate_loaded_profiles(self):

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["c"] + ["c"])
        info_table.set_cols_width([15]  + [10]  + [10])
        info_table.header(["Profile ID", "state", "stream ID"])

        profile_id_list = self.profile_state_list.keys()
        for profile_id in profile_id_list:
            profile_streams = self.profile_stream_list.get(profile_id) or '-'
            profile_state = self.profile_state_list.get(profile_id)
            profile_state = self.__name_from_state(profile_state)
            info_table.add_row([
                profile_id,
                profile_state,
                profile_streams
                ])

        return info_table

    def __name_from_state(self, profile_state):
        if profile_state == self.STATE_IDLE:
            return "IDLE"
        elif profile_state == self.STATE_STREAMS:
            return "STREAMS"
        elif profile_state == self.STATE_TX:
            return "TX"
        elif profile_state == self.STATE_PAUSE:
            return "PAUSE"
        elif profile_state == self.STATE_PCAP_TX:
            return "PCAP_TX"
        ##ASTF is not supported
        else:
            return "UNKNOWN"

    ################# stream printout ######################
    def generate_loaded_streams_sum(self, stream_id_list, table_format = True):
        self.sync_streams()

        if stream_id_list:
            stream_ids = list_intersect(stream_id_list, self.streams.keys())
        else:
            stream_ids = self.streams.keys()

        data = OrderedDict([(k, self.streams[k]) for k in sorted(self.streams.keys()) if k in stream_ids])

        if not data or not table_format:
            return data

        pkt_types = {}
        p_type_field_len = 1
        for stream_id, stream in data.items():
            pkt_types[stream_id] = limit_string(stream.get_pkt_type(), 30)
            p_type_field_len = max(p_type_field_len, len(pkt_types[stream_id]))

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["c"] + ["c"] + ["c"] + ["r"] + ["c"] + ["c"] + ["c"] + ["c"])
        info_table.set_cols_width([10]  + [15] + [15] + [p_type_field_len]  + [8] + [16]  + [15] + [12] + [12])
        info_table.set_cols_dtype(["t"] * 9)
        info_table.header(["ID", "name", "profile", "packet type", "length", "mode", "rate", "PG ID", "next"])

        for stream_id, stream in data.items():
            if stream.has_flow_stats():
                pg_id = '{0}: {1}'.format(stream.get_flow_stats_type(), stream.get_pg_id())
            else:
                pg_id = '-'

            info_table.add_row([
                stream_id,
                stream.get_name() or '-',
                self.__get_stream_profile(stream_id) or '-',
                pkt_types[stream_id],
                len(stream.get_pkt())+ 4,
                stream.get_mode(),
                stream.get_rate(),
                pg_id,
                stream.get_next() or '-',
                ])

        return info_table


    # return True if port has any stream configured with RX stats
    def has_rx_enabled (self):
        return self.has_rx_streams

    # return True if profile has any stream configured with RX stats
    def has_profile_rx_enabled (self, profile_id):
        streams = [self.streams[stream_id] for stream_id in self.profile_stream_list.get(profile_id, []) if self.streams.get(stream_id)]
        return any([stream.has_flow_stats() for stream in streams])

    def is_profile_active(self, profile_id):
        return self.profile_state_list.get(profile_id) in (self.STATE_TX, self.STATE_PAUSE, self.STATE_PCAP_TX, self.STATE_ASTF_PARSE, self.STATE_ASTF_BUILD, self.STATE_ASTF_CLEANUP)

    # return true if rx_delay_ms has passed since the last profile stop
    def has_rx_delay_expired (self, profile_id, rx_delay_ms):
        assert(self.has_profile_rx_enabled(profile_id))

        # if active - it's not safe to remove RX filters
        if self.is_profile_active(profile_id):
            return False

        # either no timestamp present or time has already passed
        return not self.tx_stopped_ts or (datetime.now() - self.tx_stopped_ts) > timedelta(milliseconds = rx_delay_ms)


    @writeable
    def remove_rx_filters (self, profile_id = DEFAULT_PROFILE_ID):
        assert(self.has_profile_rx_enabled(profile_id))

        if self.state == self.STATE_IDLE:
            return self.ok()

        profile_state = self.profile_state_list.get(profile_id)
        if not profile_state:
            return self.ok()

        if profile_state == self.STATE_IDLE:
            return self.ok()

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "profile_id":  profile_id}

        rc = self.transmit("remove_rx_filters", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()

