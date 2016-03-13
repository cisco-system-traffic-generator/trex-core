
from collections import namedtuple, OrderedDict

from trex_stl_packet_builder_scapy import CScapyTRexPktBuilder
from trex_stl_streams import STLStream
import base64
import trex_stl_stats
from trex_stl_types import *
import time
import copy

StreamOnPort = namedtuple('StreamOnPort', ['compiled_stream', 'metadata'])

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


# describes a single port
class Port(object):
    STATE_DOWN       = 0
    STATE_IDLE       = 1
    STATE_STREAMS    = 2
    STATE_TX         = 3
    STATE_PAUSE      = 4
    PortState = namedtuple('PortState', ['state_id', 'state_name'])
    STATES_MAP = {STATE_DOWN: "DOWN",
                  STATE_IDLE: "IDLE",
                  STATE_STREAMS: "IDLE",
                  STATE_TX: "ACTIVE",
                  STATE_PAUSE: "PAUSE"}


    def __init__ (self, port_id, user, comm_link, session_id, info):
        self.port_id = port_id
        self.state = self.STATE_IDLE
        self.handler = None
        self.comm_link = comm_link
        self.transmit = comm_link.transmit
        self.transmit_batch = comm_link.transmit_batch
        self.user = user

        self.info = dict(info)

        self.streams = {}
        self.profile = None
        self.session_id = session_id
        self.attr = {}

        self.port_stats = trex_stl_stats.CPortStats(self)

        self.next_available_id = long(1)


    def err(self, msg):
        return RC_ERR("port {0} : {1}\n".format(self.port_id, msg))

    def ok(self, data = ""):
        return RC_OK(data)

    def get_speed_bps (self):
        return (self.info['speed'] * 1000 * 1000 * 1000)

    # take the port
    def acquire(self, force = False):
        params = {"port_id":     self.port_id,
                  "user":        self.user,
                  "session_id":  self.session_id,
                  "force":       force}

        command = RpcCmdData("acquire", params)
        rc = self.transmit(command.method, command.params)
        if rc.good():
            self.handler = rc.data()
            return self.ok()
        else:
            return self.err(rc.err())

    # release the port
    def release(self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        command = RpcCmdData("release", params)
        rc = self.transmit(command.method, command.params)
        self.handler = None

        if rc.good():
            return self.ok()
        else:
            return self.err(rc.err())

    def is_acquired(self):
        return (self.handler != None)

    def is_active(self):
        return(self.state == self.STATE_TX ) or (self.state == self.STATE_PAUSE)

    def is_transmitting (self):
        return (self.state == self.STATE_TX)

    def is_paused (self):
        return (self.state == self.STATE_PAUSE)


    def sync(self):
        params = {"port_id": self.port_id}

        command = RpcCmdData("get_port_status", params)
        rc = self.transmit(command.method, command.params)
        if rc.bad():
            return self.err(rc.err())

        # sync the port
        port_state = rc.data()['state']

        if port_state == "DOWN":
            self.state = self.STATE_DOWN
        elif port_state == "IDLE":
            self.state = self.STATE_IDLE
        elif port_state == "STREAMS":
            self.state = self.STATE_STREAMS
        elif port_state == "TX":
            self.state = self.STATE_TX
        elif port_state == "PAUSE":
            self.state = self.STATE_PAUSE
        else:
            raise Exception("port {0}: bad state received from server '{1}'".format(self.port_id, port_state))


        self.next_available_id = long(rc.data()['max_stream_id']) + 1

        # attributes
        self.attr = rc.data()['attr']

        # sync the streams
        params = {"port_id": self.port_id}

        command = RpcCmdData("get_all_streams", params)
        rc = self.transmit(command.method, command.params)
        if rc.bad():
            return self.err(rc.err())

        for k, v in rc.data()['streams'].iteritems():
            self.streams[k] = {'next_id': v['next_stream_id'],
                               'pkt'    : base64.b64decode(v['packet']['binary']),
                               'mode'   : v['mode']['type'],
                               'rate'   : STLStream.get_rate_from_field(v['mode']['rate'])}

        return self.ok()


    # return TRUE if write commands
    def is_port_writable (self):
        # operations on port can be done on state idle or state streams
        return ((self.state == self.STATE_IDLE) or (self.state == self.STATE_STREAMS))


    def __allocate_stream_id (self):
        id = self.next_available_id
        self.next_available_id += 1
        return id


    # add streams
    def add_streams (self, streams_list):

        if not self.is_acquired():
            return self.err("port is not owned")

        if not self.is_port_writable():
            return self.err("Please stop port before attempting to add streams")

        # listify
        streams_list = streams_list if isinstance(streams_list, list) else [streams_list]
        
        lookup = {}

        # allocate IDs
        for stream in streams_list:

            # allocate stream id
            stream_id = stream.get_id() if stream.get_id() is not None else self.__allocate_stream_id()
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
                if not next in lookup:
                    return self.err("stream dependency error - unable to find '{0}'".format(next))
                next_id = lookup[next]

            stream_json = stream.to_json()
            stream_json['next_stream_id'] = next_id

            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream_id,
                      "stream": stream_json}

            cmd = RpcCmdData('add_stream', params)
            batch.append(cmd)


        rc = self.transmit_batch(batch)

        ret = RC()
        for i, single_rc in enumerate(rc):
            if single_rc.rc:
                stream_id = batch[i].params['stream_id']
                next_id   = batch[i].params['stream']['next_stream_id']
                self.streams[stream_id] = {'next_id' : next_id,
                                           'pkt'  : streams_list[i].get_pkt(),
                                           'mode' : streams_list[i].get_mode(),
                                           'rate' : streams_list[i].get_rate()}

                ret.add(RC_OK(data = stream_id))
            else:
                ret.add(RC(*single_rc))

        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        return ret if ret else self.err(str(ret))



    # remove stream from port
    def remove_streams (self, stream_id_list):

        if not self.is_acquired():
            return self.err("port is not owned")

        if not self.is_port_writable():
            return self.err("Please stop port before attempting to remove streams")

        # single element to list
        stream_id_list = stream_id_list if isinstance(stream_id_list, list) else [stream_id_list]

        # verify existance
        if not all([stream_id in self.streams for stream_id in stream_id_list]):
            return self.err("stream {0} does not exists".format(stream_id))

        batch = []

        for stream_id in stream_id_list:
            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream_id}

            cmd = RpcCmdData('remove_stream', params)
            batch.append(cmd)


        rc = self.transmit_batch(batch)
        for i, single_rc in enumerate(rc):
            if single_rc:
                id = batch[i].params['stream_id']
                del self.streams[stream_id]

        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        return self.ok() if rc else self.err(rc.err())


    # remove all the streams
    def remove_all_streams (self):

        if not self.is_acquired():
            return self.err("port is not owned")

        if not self.is_port_writable():
            return self.err("Please stop port before attempting to remove streams")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("remove_all_streams", params)
        if not rc:
            return self.err(rc.err())

        self.streams = {}

        self.state = self.STATE_IDLE

        return self.ok()


    # get a specific stream
    def get_stream (self, stream_id):
        if stream_id in self.streams:
            return self.streams[stream_id]
        else:
            return None

    def get_all_streams (self):
        return self.streams


    # start traffic
    def start (self, mul, duration, force):
        if not self.is_acquired():
            return self.err("port is not owned")

        if self.state == self.STATE_DOWN:
            return self.err("Unable to start traffic - port is down")

        if self.state == self.STATE_IDLE:
            return self.err("Unable to start traffic - no streams attached to port")

        if self.state == self.STATE_TX:
            return self.err("Unable to start traffic - port is already transmitting")

        params = {"handler":  self.handler,
                  "port_id":  self.port_id,
                  "mul":      mul,
                  "duration": duration,
                  "force":    force}
        
        rc = self.transmit("start_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_TX

        return self.ok()

    # stop traffic
    # with force ignores the cached state and sends the command
    def stop (self, force = False):

        if not self.is_acquired():
            return self.err("port is not owned")

        # port is already stopped
        if not force:
            if (self.state == self.STATE_IDLE) or (self.state == self.state == self.STATE_STREAMS):
                return self.ok()


        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("stop_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_STREAMS

        return self.ok()

    def pause (self):

        if not self.is_acquired():
            return self.err("port is not owned")

        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc  = self.transmit("pause_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_PAUSE

        return self.ok()


    def resume (self):

        if not self.is_acquired():
            return self.err("port is not owned")

        if (self.state != self.STATE_PAUSE) :
            return self.err("port is not in pause mode")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        # only valid state after stop

        rc = self.transmit("resume_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_TX

        return self.ok()


    def update (self, mul, force):

        if not self.is_acquired():
            return self.err("port is not owned")

        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "mul":     mul,
                  "force":   force}

        rc = self.transmit("update_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    def validate (self):

        if not self.is_acquired():
            return self.err("port is not owned")

        if (self.state == self.STATE_DOWN):
            return self.err("port is down")

        if (self.state == self.STATE_IDLE):
            return self.err("no streams attached to port")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("validate", params)
        if rc.bad():
            return self.err(rc.err())

        self.profile = rc.data()

        return self.ok()


    def set_attr (self, attr_dict):
        if not self.is_acquired():
            return self.err("port is not owned")

        if (self.state == self.STATE_DOWN):
            return self.err("port is down")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "attr": attr_dict}

        rc = self.transmit("set_port_attr", params)
        if rc.bad():
            return self.err(rc.err())


        self.attr.update(attr_dict)

        return self.ok()


    def get_profile (self):
        return self.profile

    
    def print_profile (self, mult, duration):
        if not self.get_profile():
            return

        rate = self.get_profile()['rate']
        graph = self.get_profile()['graph']

        print format_text("Profile Map Per Port\n", 'underline', 'bold')

        factor = mult_to_factor(mult, rate['max_bps_l2'], rate['max_pps'], rate['max_line_util'])

        print "Profile max BPS L2    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l2'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l2'] * factor, suffix = "bps"))

        print "Profile max BPS L1    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l1'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l1'] * factor, suffix = "bps"))

        print "Profile max PPS       (base / req):   {:^12} / {:^12}".format(format_num(rate['max_pps'], suffix = "pps"),
                                                                             format_num(rate['max_pps'] * factor, suffix = "pps"),)

        print "Profile line util.    (base / req):   {:^12} / {:^12}".format(format_percentage(rate['max_line_util']),
                                                                             format_percentage(rate['max_line_util'] * factor))


        # duration
        exp_time_base_sec = graph['expected_duration'] / (1000 * 1000)
        exp_time_factor_sec = exp_time_base_sec / factor

        # user configured a duration
        if duration > 0:
            if exp_time_factor_sec > 0:
                exp_time_factor_sec = min(exp_time_factor_sec, duration)
            else:
                exp_time_factor_sec = duration


        print "Duration              (base / req):   {:^12} / {:^12}".format(format_time(exp_time_base_sec),
                                                                             format_time(exp_time_factor_sec))
        print "\n"

    # generate port info
    def get_info (self):
        info = dict(self.info)

        info['status']       = self.get_port_state_name()

        if self.attr.get('promiscuous'):
            info['prom'] = "on" if self.attr['promiscuous']['enabled'] else "off"
        else:
            info['prom'] = "N/A"

        return info


    def get_port_state_name(self):
        return self.STATES_MAP.get(self.state, "Unknown")

    ################# stats handler ######################
    def generate_port_stats(self):
        return self.port_stats.generate_stats()

    def generate_port_status(self):

        info = self.get_info()

        return {"driver":        info['driver'],
                "HW src mac":  info['hw_macaddr'],
                "SW src mac":  info['src_macaddr'],
                "SW dst mac":  info['dst_macaddr'],
                "PCI Address": info['pci_addr'],
                "NUMA Node":   info['numa'],
                "--": "",
                "---": "",
                "maximum": "{speed} Gb/s".format(speed=info['speed']),
                "status": info['status'],
                "promiscuous" : info['prom']
                }

    def clear_stats(self):
        return self.port_stats.clear_stats()


    def get_stats (self):
        return self.port_stats.get_stats()


    def invalidate_stats(self):
        return self.port_stats.invalidate()

    ################# stream printout ######################
    def generate_loaded_streams_sum(self):
        if self.state == self.STATE_DOWN:
            return {}

        data = {}
        for id, obj in self.streams.iteritems():

            # lazy build scapy repr.
            if not 'pkt_type' in obj:
                obj['pkt_type'] = CScapyTRexPktBuilder.pkt_layers_desc_from_buffer(obj['pkt'])
            
            data[id] = OrderedDict([ ('id',  id),
                                     ('packet_type',  obj['pkt_type']),
                                     ('L2 len',       len(obj['pkt']) + 4),
                                     ('mode',         obj['mode']),
                                     ('rate',         obj['rate']),
                                     ('next_stream',  obj['next_id'])
                                    ])
    
        return {"streams" : OrderedDict(sorted(data.items())) }
    


  ################# events handler ######################
    def async_event_port_job_done (self):
        self.state = self.STATE_STREAMS

    # rest of the events are used for TUI / read only sessions
    def async_event_port_stopped (self):
        if not self.is_acquired():
            self.state = self.STATE_STREAMS

    def async_event_port_paused (self):
        if not self.is_acquired():
            self.state = self.STATE_PAUSE

    def async_event_port_started (self):
        if not self.is_acquired():
            self.state = self.STATE_TX

    def async_event_port_resumed (self):
        if not self.is_acquired():
            self.state = self.STATE_TX

    def async_event_forced_acquired (self):
        self.handler = None

