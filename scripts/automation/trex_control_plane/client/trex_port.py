
from collections import namedtuple
from common.trex_types import *
from common import trex_stats

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


    def __init__ (self, port_id, speed, driver, user, session_id, comm_link):
        self.port_id = port_id
        self.state = self.STATE_IDLE
        self.handler = None
        self.comm_link = comm_link
        self.transmit = comm_link.transmit
        self.transmit_batch = comm_link.transmit_batch
        self.user = user
        self.session_id = session_id
        self.driver = driver
        self.speed = speed
        self.streams = {}
        self.profile = None

        self.port_stats = trex_stats.CPortStats(self)


    def err(self, msg):
        return RC_ERR("port {0} : {1}".format(self.port_id, msg))

    def ok(self, data = "ACK"):
        return RC_OK(data)

    def get_speed_bps (self):
        return (self.speed * 1000 * 1000 * 1000)

    # take the port
    def acquire(self, force = False):
        params = {"port_id":     self.port_id,
                  "user":        self.user,
                  "session_id":  self.session_id,
                  "force":       force}

        command = RpcCmdData("acquire", params)
        rc = self.transmit(command.method, command.params)
        if rc.success:
            self.handler = rc.data
            return self.ok()
        else:
            return self.err(rc.data)

    # release the port
    def release(self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        command = RpcCmdData("release", params)
        rc = self.transmit(command.method, command.params)
        self.handler = None

        if rc.success:
            return self.ok()
        else:
            return self.err(rc.data)

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
        if not rc.success:
            return self.err(rc.data)

        # sync the port
        port_state = rc.data['state']

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
            raise Exception("port {0}: bad state received from server '{1}'".format(self.port_id, sync_data['state']))

        return self.ok()


    # return TRUE if write commands
    def is_port_writable (self):
        # operations on port can be done on state idle or state streams
        return ((self.state == self.STATE_IDLE) or (self.state == self.STATE_STREAMS))

    # add stream to the port
    def add_stream (self, stream_id, stream_obj):

        if not self.is_port_writable():
            return self.err("Please stop port before attempting to add streams")


        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "stream_id": stream_id,
                  "stream": stream_obj}

        rc, data = self.transmit("add_stream", params)
        if not rc:
            r = self.err(data)
            print r.good()

        # add the stream
        self.streams[stream_id] = stream_obj

        # the only valid state now
        self.state = self.STATE_STREAMS

        return self.ok()

    # add multiple streams
    def add_streams (self, streams_list):
        batch = []

        for stream in streams_list:
            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream.stream_id,
                      "stream": stream.stream}

            cmd = RpcCmdData('add_stream', params)
            batch.append(cmd)

        rc, data = self.transmit_batch(batch)

        if not rc:
            return self.err(data)

        # add the stream
        for stream in streams_list:
            self.streams[stream.stream_id] = stream.stream

        # the only valid state now
        self.state = self.STATE_STREAMS

        return self.ok()

    # remove stream from port
    def remove_stream (self, stream_id):

        if not stream_id in self.streams:
            return self.err("stream {0} does not exists".format(stream_id))

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "stream_id": stream_id}


        rc, data = self.transmit("remove_stream", params)
        if not rc:
            return self.err(data)

        self.streams[stream_id] = None

        self.state = self.STATE_STREAMS if len(self.streams > 0) else self.STATE_IDLE

        return self.ok()

    # remove all the streams
    def remove_all_streams (self):

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("remove_all_streams", params)
        if not rc:
            return self.err(data)

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
    def start (self, mul, duration):
        if self.state == self.STATE_DOWN:
            return self.err("Unable to start traffic - port is down")

        if self.state == self.STATE_IDLE:
            return self.err("Unable to start traffic - no streams attached to port")

        if self.state == self.STATE_TX:
            return self.err("Unable to start traffic - port is already transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "mul": mul,
                  "duration": duration}

        rc, data = self.transmit("start_traffic", params)
        if not rc:
            return self.err(data)

        self.state = self.STATE_TX

        return self.ok()

    # stop traffic
    # with force ignores the cached state and sends the command
    def stop (self, force = False):

        if (not force) and (self.state != self.STATE_TX) and (self.state != self.STATE_PAUSE):
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("stop_traffic", params)
        if not rc:
            return self.err(data)

        # only valid state after stop
        self.state = self.STATE_STREAMS

        return self.ok()

    def pause (self):

        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("pause_traffic", params)
        if not rc:
            return self.err(data)

        # only valid state after stop
        self.state = self.STATE_PAUSE

        return self.ok()


    def resume (self):

        if (self.state != self.STATE_PAUSE) :
            return self.err("port is not in pause mode")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("resume_traffic", params)
        if not rc:
            return self.err(data)

        # only valid state after stop
        self.state = self.STATE_TX

        return self.ok()


    def update (self, mul):
        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "mul": mul}

        rc, data = self.transmit("update_traffic", params)
        if not rc:
            return self.err(data)

        return self.ok()


    def validate (self):

        if (self.state == self.STATE_DOWN):
            return self.err("port is down")

        if (self.state == self.STATE_IDLE):
            return self.err("no streams attached to port")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc, data = self.transmit("validate", params)
        if not rc:
            return self.err(data)

        self.profile = data

        return self.ok()

    def get_profile (self):
        return self.profile


    def print_profile (self, mult, duration):
        if not self.get_profile():
            return

        rate = self.get_profile()['rate']
        graph = self.get_profile()['graph']

        print format_text("Profile Map Per Port\n", 'underline', 'bold')

        factor = mult_to_factor(mult, rate['max_bps'], rate['max_pps'], rate['max_line_util'])

        print "Profile max BPS    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps'], suffix = "bps"),
                                                                          format_num(rate['max_bps'] * factor, suffix = "bps"))

        print "Profile max PPS    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_pps'], suffix = "pps"),
                                                                          format_num(rate['max_pps'] * factor, suffix = "pps"),)

        print "Profile line util. (base / req):   {:^12} / {:^12}".format(format_percentage(rate['max_line_util'] * 100),
                                                                          format_percentage(rate['max_line_util'] * factor * 100))


        # duration
        exp_time_base_sec = graph['expected_duration'] / (1000 * 1000)
        exp_time_factor_sec = exp_time_base_sec / factor

        # user configured a duration
        if duration > 0:
            if exp_time_factor_sec > 0:
                exp_time_factor_sec = min(exp_time_factor_sec, duration)
            else:
                exp_time_factor_sec = duration


        print "Duration           (base / req):   {:^12} / {:^12}".format(format_time(exp_time_base_sec),
                                                                          format_time(exp_time_factor_sec))
        print "\n"


    def get_port_state_name(self):
        return self.STATES_MAP.get(self.state, "Unknown")

    ################# stats handler ######################
    def generate_port_stats(self):
        return self.port_stats.generate_stats()
        pass

    def generate_port_status(self):
        return {"port-type": self.driver,
                "maximum": "{speed} Gb/s".format(speed=self.speed),
                "port-status": self.get_port_state_name()
                }

    def clear_stats(self):
        return self.port_stats.clear_stats()


    ################# events handler ######################
    def async_event_port_stopped (self):
        self.state = self.STATE_STREAMS


    def async_event_port_started (self):
        self.state = self.STATE_TX


    def async_event_port_paused (self):
        self.state = self.STATE_PAUSE


    def async_event_port_resumed (self):
        self.state = self.STATE_TX

    def async_event_forced_acquired (self):
        self.handler = None
