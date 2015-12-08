#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages

from client_utils.jsonrpc_client import JsonRpcClient, BatchMessage
from client_utils.packet_builder import CTRexPktBuilder
import json

from common.trex_streams import *
from collections import namedtuple
from common.text_opts import *
from common import trex_stats
from client_utils import parsing_opts, text_tables
import time
import datetime
import re

from trex_async_client import CTRexAsyncClient

RpcCmdData = namedtuple('RpcCmdData', ['method', 'params'])

class RpcResponseStatus(namedtuple('RpcResponseStatus', ['success', 'id', 'msg'])):
        __slots__ = ()
        def __str__(self):
            return "{id:^3} - {msg} ({stat})".format(id=self.id,
                                                  msg=self.msg,
                                                  stat="success" if self.success else "fail")

# simple class to represent complex return value
class RC():

    def __init__ (self, rc = None, data = None):
        self.rc_list = []

        if (rc != None) and (data != None):
            tuple_rc = namedtuple('RC', ['rc', 'data'])
            self.rc_list.append(tuple_rc(rc, data))

    def add (self, rc):
        self.rc_list += rc.rc_list

    def good (self):
        return all([x.rc for x in self.rc_list])

    def bad (self):
        return not self.good()

    def data (self):
        return [x.data if x.rc else "" for x in self.rc_list]

    def err (self):
        return [x.data if not x.rc else "" for x in self.rc_list]

    def annotate (self, desc = None):
        if desc:
            print format_text('\n{:<60}'.format(desc), 'bold'),

        if self.bad():
            # print all the errors
            print ""
            for x in self.rc_list:
                if not x.rc:
                    print format_text("\n{0}".format(x.data), 'bold')

            print ""
            print format_text("[FAILED]\n", 'red', 'bold')


        else:
            print format_text("[SUCCESS]\n", 'green', 'bold')


def RC_OK(data = ""):
    return RC(True, data)
def RC_ERR (err):
    return RC(False, err)

LoadedStreamList = namedtuple('LoadedStreamList', ['loaded', 'compiled'])

########## utlity ############
def mult_to_factor (mult, max_bps, max_pps, line_util):
    if mult['type'] == 'raw':
        return mult['value']

    if mult['type'] == 'bps':
        return mult['value'] / max_bps

    if mult['type'] == 'pps':
        return mult['value'] / max_pps

    if mult['type'] == 'percentage':
        return mult['value'] / line_util



# describes a stream DB
class CStreamsDB(object):

    def __init__(self):
        self.stream_packs = {}

    def load_yaml_file(self, filename):

        stream_pack_name = filename
        if stream_pack_name in self.get_loaded_streams_names():
            self.remove_stream_packs(stream_pack_name)

        stream_list = CStreamList()
        loaded_obj = stream_list.load_yaml(filename)

        try:
            compiled_streams = stream_list.compile_streams()
            rc = self.load_streams(stream_pack_name,
                                   LoadedStreamList(loaded_obj,
                                                    [StreamPack(v.stream_id, v.stream.dump())
                                                     for k, v in compiled_streams.items()]))

        except Exception as e:
            return None

        return self.get_stream_pack(stream_pack_name)

    def load_streams(self, name, LoadedStreamList_obj):
        if name in self.stream_packs:
            return False
        else:
            self.stream_packs[name] = LoadedStreamList_obj
            return True

    def remove_stream_packs(self, *names):
        removed_streams = []
        for name in names:
            removed = self.stream_packs.pop(name)
            if removed:
                removed_streams.append(name)
        return removed_streams

    def clear(self):
        self.stream_packs.clear()

    def get_loaded_streams_names(self):
        return self.stream_packs.keys()

    def stream_pack_exists (self, name):
        return name in self.get_loaded_streams_names()

    def get_stream_pack(self, name):
        if not self.stream_pack_exists(name):
            return None
        else:
            return self.stream_packs.get(name)


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
                  STATE_STREAMS: "STREAMS",
                  STATE_TX: "ACTIVE",
                  STATE_PAUSE: "PAUSE"}


    def __init__ (self, port_id, speed, driver, user, comm_link):
        self.port_id = port_id
        self.state = self.STATE_IDLE
        self.handler = None
        self.comm_link = comm_link
        self.transmit = comm_link.transmit
        self.transmit_batch = comm_link.transmit_batch
        self.user = user
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
        params = {"port_id": self.port_id,
                  "user":    self.user,
                  "force":   force}

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
        if rc.success:
            self.handler = rc.data
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


    def sync(self, sync_data):
        self.handler = sync_data['handler']
        port_state = sync_data['state'].upper()
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


class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""

    # verbose levels
    VERBOSE_SILENCE = 0
    VERBOSE_REGULAR = 1
    VERBOSE_HIGH    = 2
    
    def __init__(self, username, server="localhost", sync_port = 5050, async_port = 4500, virtual=False):
        super(CTRexStatelessClient, self).__init__()
        self.user = username
        self.comm_link = CTRexStatelessClient.CCommLink(server, sync_port, virtual)

        # default verbose level
        self.verbose = self.VERBOSE_REGULAR

        self.ports = {}
        self._connection_info = {"server": server,
                                 "sync_port": sync_port,
                                 "async_port": async_port}
        self.system_info = {}
        self.server_version = {}
        self.__err_log = None

        self.async_client = CTRexAsyncClient(server, async_port, self)

        self.streams_db = CStreamsDB()
        self.global_stats = trex_stats.CGlobalStats(self._connection_info,
                                                    self.server_version,
                                                    self.ports)
        self.stats_generator = trex_stats.CTRexStatsGenerator(self.global_stats,
                                                              self.ports)

        self.events = []

        self.connected = False



    # returns the port object
    def get_port (self, port_id):
        return self.ports.get(port_id, None)


    ################# events handler ######################
    def add_event_log (self, msg, ev_type, show = False):

        if ev_type == "server":
            prefix = "[server]"
        elif ev_type == "local":
            prefix = "[local]"

        ts = time.time()
        st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
        self.events.append("{:<10} - {:^8} - {:}".format(st, prefix, format_text(msg, 'bold')))

        if show and self.check_verbose(self.VERBOSE_REGULAR):
            print format_text("\n{:^8} - {:}".format(prefix, format_text(msg, 'bold')))
  

    def handle_async_stats_update(self, dump_data):
        global_stats = {}
        port_stats = {}

        # filter the values per port and general
        for key, value in dump_data.iteritems():
            # match a pattern of ports
            m = re.search('(.*)\-([0-8])', key)
            if m:
                port_id = int(m.group(2))
                field_name = m.group(1)
                if self.ports.has_key(port_id):
                    if not port_id in port_stats:
                        port_stats[port_id] = {}
                    port_stats[port_id][field_name] = value
                else:
                    continue
            else:
                # no port match - general stats
                global_stats[key] = value

        # update the general object with the snapshot
        self.global_stats.update(global_stats)
        # update all ports
        for port_id, data in port_stats.iteritems():
            self.ports[port_id].port_stats.update(data)



    def handle_async_event (self, type, data):
        # DP stopped

        show_event = False

        # port started
        if (type == 0):
            port_id = int(data['port_id'])
            ev = "Port {0} has started".format(port_id)

        # port stopped
        elif (type == 1):
            port_id = int(data['port_id'])
            ev = "Port {0} has stopped".format(port_id)

            # call the handler
            self.async_event_port_stopped(port_id)
            

        # server stopped
        elif (type == 2):
            ev = "Server has stopped"
            self.async_event_server_stopped()
            show_event = True

        # port finished traffic
        elif (type == 3):
            port_id = int(data['port_id'])
            ev = "Port {0} job done".format(port_id)

            # call the handler
            self.async_event_port_stopped(port_id)
            show_event = True

        else:
            # unknown event - ignore
            return


        self.add_event_log(ev, 'server', show_event)


    def async_event_port_stopped (self, port_id):
        self.ports[port_id].async_event_port_stopped()

    def async_event_server_stopped (self):
        self.connected = False


    def get_events (self):
        return self.events

    def clear_events (self):
        self.events = []

    ############# helper functions section ##############

    # measure time for functions
    def timing(f):
        def wrap(*args):
            time1 = time.time()
            ret = f(*args)

            # don't want to print on error
            if ret.bad():
                return ret

            delta = time.time() - time1
            print format_time(delta) + "\n"

            return ret

        return wrap


    def validate_port_list(self, port_id_list):
        if not isinstance(port_id_list, list):
            print type(port_id_list)
            return False

        # check each item of the sequence
        return all([ (port_id >= 0) and (port_id < self.get_port_count())
                      for port_id in port_id_list ])

    # some preprocessing for port argument
    def __ports (self, port_id_list):

        # none means all
        if port_id_list == None:
            return range(0, self.get_port_count())

        # always list
        if isinstance(port_id_list, int):
            port_id_list = [port_id_list]

        if not isinstance(port_id_list, list):
             raise ValueError("bad port id list: {0}".format(port_id_list))

        for port_id in port_id_list:
            if not isinstance(port_id, int) or (port_id < 0) or (port_id > self.get_port_count()):
                raise ValueError("bad port id {0}".format(port_id))

        return port_id_list

    ############ boot up section ################

    # connection sequence
    def connect(self):

        # clear this flag
        self.connected = False

        # connect sync channel
        rc, data = self.comm_link.connect()
        if not rc:
            return RC_ERR(data)

        # connect async channel
        rc, data = self.async_client.connect()
        if not rc:
            return RC_ERR(data)

        # version
        rc, data = self.transmit("get_version")
        if not rc:
            return RC_ERR(data)

        self.server_version = data
        self.global_stats.server_version = data

        # cache system info
        rc, data = self.transmit("get_system_info")
        if not rc:
            return RC_ERR(data)
        self.system_info = data

        # cache supported commands
        rc, data = self.transmit("get_supported_cmds")
        if not rc:
            return RC_ERR(data)

        self.supported_cmds = data

        # create ports
        for port_id in xrange(self.get_port_count()):
            speed = self.system_info['ports'][port_id]['speed']
            driver = self.system_info['ports'][port_id]['driver']

            self.ports[port_id] = Port(port_id, speed, driver, self.user, self.comm_link)


        # acquire all ports
        rc = self.acquire()
        if rc.bad():
            return rc

        rc = self.sync_with_server()
        if rc.bad():
            return rc

        self.connected = True

        return RC_OK()

    def is_connected (self):
        return self.connected and self.comm_link.is_connected


    def disconnect(self):
        self.comm_link.disconnect()
        self.async_client.disconnect()
        return RC_OK()


    def on_async_dead (self):
        if self.connected:
            msg = 'lost connection to server'
            self.add_event_log(msg, 'local', True)
            self.connected = False

    def on_async_alive (self):
        pass

    ########### cached queries (no server traffic) ###########

    def get_supported_cmds(self):
        return self.supported_cmds

    def get_version(self):
        return self.server_version

    def get_system_info(self):
        return self.system_info

    def get_port_count(self):
        return self.system_info.get("port_count")

    def get_port_ids(self, as_str=False):
        port_ids = range(self.get_port_count())
        if as_str:
            return " ".join(str(p) for p in port_ids)
        else:
            return port_ids

    def get_stats_async (self):
        return self.async_client.get_stats()

    def get_connection_port (self):
        return self.comm_link.port

    def get_connection_ip (self):
        return self.comm_link.server

    def get_acquired_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_acquired()]

    def get_active_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_active()]

    def get_paused_ports (self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_paused()]

    def get_transmitting_ports (self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_transmitting()]

    def set_verbose(self, mode):

        # on high - enable link verbose
        if mode == self.VERBOSE_HIGH:
            self.comm_link.set_verbose(True)
        else:
            self.comm_link.set_verbose(False)

        self.verbose = mode


    def check_verbose (self, level):
        return (self.verbose >= level)

    def get_verbose (self):
        return self.verbose

    ############# server actions ################

    # ping server
    def ping(self):
        rc, info = self.transmit("ping")
        return RC(rc, info)


    def sync_with_server(self, sync_streams=False):
        rc, data = self.transmit("sync_user", {"user": self.user, "sync_streams": sync_streams})
        if not rc:
            return RC_ERR(data)

        for port_info in data:
            rc = self.ports[port_info['port_id']].sync(port_info)
            if rc.bad():
                return rc

        return RC_OK()

    def get_global_stats(self):
        rc, info = self.transmit("get_global_stats")
        return RC(rc, info)


    ########## port commands ##############
    # acquire ports, if port_list is none - get all
    def acquire (self, port_id_list = None, force = False):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].acquire(force))
     
        return rc
    
    # release ports
    def release (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].release(force))
        
        return rc

 
    def add_stream(self, stream_id, stream_obj, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].add_stream(stream_id, stream_obj))
        
        return rc

      

    def add_stream_pack(self, stream_pack_list, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].add_streams(stream_pack_list))

        return rc



    def remove_stream(self, stream_id, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_stream(stream_id))
        
        return rc



    def remove_all_streams(self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_all_streams())
        
        return rc

    
    def get_stream(self, stream_id, port_id, get_pkt = False):

        return self.ports[port_id].get_stream(stream_id)


    def get_all_streams(self, port_id, get_pkt = False):

        return self.ports[port_id].get_all_streams()


    def get_stream_id_list(self, port_id):

        return self.ports[port_id].get_stream_id_list()


    def start_traffic (self, multiplier, duration, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].start(multiplier, duration))
        
        return rc


    def resume_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].resume())

        return rc

    def pause_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].pause())

        return rc

    def stop_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].stop(force))
        
        return rc


    def update_traffic (self, mult, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].update(mult))
        
        return rc


    def validate (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].validate())
     
        return rc


    def get_port_stats(self, port_id=None):
        pass

    def get_stream_stats(self, port_id=None):
        pass


    def transmit(self, method_name, params={}):
        return self.comm_link.transmit(method_name, params)


    def transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)

    ######################### Console (high level) API #########################

    @timing
    def cmd_ping(self):
        rc = self.ping()
        rc.annotate("Pinging the server on '{0}' port '{1}': ".format(self.get_connection_ip(), self.get_connection_port()))
        return rc

    def cmd_connect(self):
        rc = self.connect()
        rc.annotate()
        return rc

    def cmd_disconnect(self):
        rc = self.disconnect()
        rc.annotate()
        return rc

    # reset
    def cmd_reset(self):


        # sync with the server
        rc = self.sync_with_server()
        rc.annotate("Syncing with the server:")
        if rc.bad():
            return rc

        rc = self.acquire(force = True)
        rc.annotate("Force acquiring all ports:")
        if rc.bad():
            return rc


        # force stop all ports
        rc = self.stop_traffic(self.get_port_ids(), True)
        rc.annotate("Stop traffic on all ports:")
        if rc.bad():
            return rc


        # remove all streams
        rc = self.remove_all_streams(self.get_port_ids())
        rc.annotate("Removing all streams from all ports:")
        if rc.bad():
            return rc

        # TODO: clear stats
        return RC_OK()
        

    # stop cmd
    def cmd_stop (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            print format_text(msg, 'bold')
            return RC_ERR(msg)

        rc = self.stop_traffic(active_ports)
        rc.annotate("Stopping traffic on port(s) {0}:".format(port_id_list))
        if rc.bad():
            return rc

        return RC_OK()

    # update cmd
    def cmd_update (self, port_id_list, mult):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            print format_text(msg, 'bold')
            return RC_ERR(msg)

        rc = self.update_traffic(mult, active_ports)
        rc.annotate("Updating traffic on port(s) {0}:".format(port_id_list))

        return rc

    # clear stats
    def cmd_clear(self, port_id_list):

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        self.global_stats.clear_stats()

        return RC_OK()


    # pause cmd
    def cmd_pause (self, port_id_list):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            print format_text(msg, 'bold')
            return RC_ERR(msg)

        rc = self.pause_traffic(active_ports)
        rc.annotate("Pausing traffic on port(s) {0}:".format(port_id_list))
        return rc



    # resume cmd
    def cmd_resume (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on porvided ports"
            print format_text(msg, 'bold')
            return RC_ERR(msg)

        rc = self.resume_traffic(active_ports)
        rc.annotate("Resume traffic on port(s) {0}:".format(port_id_list))
        return rc


    # start cmd
    def cmd_start (self, port_id_list, stream_list, mult, force, duration, dry):

        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if active_ports:
            if not force:
                msg = "Port(s) {0} are active - please stop them or add '--force'".format(active_ports)
                print format_text(msg, 'bold')
                return RC_ERR(msg)
            else:
                rc = self.cmd_stop(active_ports)
                if not rc:
                    return rc


        rc = self.remove_all_streams(port_id_list)
        rc.annotate("Removing all streams from port(s) {0}:".format(port_id_list))
        if rc.bad():
            return rc


        rc = self.add_stream_pack(stream_list.compiled, port_id_list)
        rc.annotate("Attaching {0} streams to port(s) {1}:".format(len(stream_list.compiled), port_id_list))
        if rc.bad():
            return rc

        # when not on dry - start the traffic , otherwise validate only
        if not dry:
            rc = self.start_traffic(mult, duration, port_id_list)
            rc.annotate("Starting traffic on port(s) {0}:".format(port_id_list))

            return rc
        else:
            rc = self.validate(port_id_list)
            rc.annotate("Validating traffic profile on port(s) {0}:".format(port_id_list))

            if rc.bad():
                return rc

            # show a profile on one port for illustration
            self.ports[port_id_list[0]].print_profile(mult, duration)

            return rc


    # validate port(s) profile
    def cmd_validate (self, port_id_list):
        rc = self.validate(port_id_list)
        rc.annotate("Validating streams on port(s) {0}:".format(port_id_list))
        return rc


    # stats
    def cmd_stats(self, port_id_list, stats_mask=set()):
        stats_opts = trex_stats.ALL_STATS_OPTS.intersection(stats_mask)

        stats_obj = {}
        for stats_type in stats_opts:
            stats_obj.update(self.stats_generator.generate_single_statistic(port_id_list, stats_type))
        return stats_obj


    ############## High Level API With Parser ################
    @timing
    def cmd_start_line (self, line):
        '''Start selected traffic in specified ports on TRex\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "start",
                                         self.cmd_start_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.TOTAL,
                                         parsing_opts.FORCE,
                                         parsing_opts.STREAM_FROM_PATH_OR_FILE,
                                         parsing_opts.DURATION,
                                         parsing_opts.MULTIPLIER_STRICT,
                                         parsing_opts.DRY_RUN)

        opts = parser.parse_args(line.split())

        if opts is None:
            return RC_ERR("bad command line parameters")


        if opts.dry:
            print format_text("\n*** DRY RUN ***", 'bold')

        if opts.db:
            stream_list = self.streams_db.get_stream_pack(opts.db)
            rc = RC(stream_list != None)
            rc.annotate("Load stream pack (from DB):")
            if rc.bad():
                return RC_ERR("Failed to load stream pack")

        else:
            # load streams from file
            stream_list = self.streams_db.load_yaml_file(opts.file[0])
            rc = RC(stream_list != None)
            rc.annotate("Load stream pack (from file):")
            if stream_list == None:
                return RC_ERR("Failed to load stream pack")


        # total has no meaning with percentage - its linear
        if opts.total and (opts.mult['type'] != 'percentage'):
            # if total was set - divide it between the ports
            opts.mult['value'] = opts.mult['value'] / len(opts.ports)

        return self.cmd_start(opts.ports, stream_list, opts.mult, opts.force, opts.duration, opts.dry)

    @timing
    def cmd_resume_line (self, line):
        '''Resume active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "resume",
                                         self.cmd_stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line parameters")

        return self.cmd_resume(opts.ports)


    @timing
    def cmd_stop_line (self, line):
        '''Stop active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "stop",
                                         self.cmd_stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line parameters")

        return self.cmd_stop(opts.ports)


    @timing
    def cmd_pause_line (self, line):
        '''Pause active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "pause",
                                         self.cmd_stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line parameters")

        return self.cmd_pause(opts.ports)


    @timing
    def cmd_update_line (self, line):
        '''Update port(s) speed currently active\n'''
        parser = parsing_opts.gen_parser(self,
                                         "update",
                                         self.cmd_update_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.MULTIPLIER,
                                         parsing_opts.TOTAL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line paramters")

        # total has no meaning with percentage - its linear
        if opts.total and (opts.mult['type'] != 'percentage'):
            # if total was set - divide it between the ports
            opts.mult['value'] = opts.mult['value'] / len(opts.ports)

        return self.cmd_update(opts.ports, opts.mult)

    @timing
    def cmd_reset_line (self, line):
        return self.cmd_reset()


    def cmd_clear_line (self, line):
        '''Clear cached local statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "clear",
                                         self.cmd_clear_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())

        if opts is None:
            return RC_ERR("bad command line parameters")
        return self.cmd_clear(opts.ports)


    def cmd_stats_line (self, line):
        '''Fetch statistics from TRex server by port\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.cmd_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STATS_MASK)

        opts = parser.parse_args(line.split())

        if opts is None:
            return RC_ERR("bad command line parameters")

        # determine stats mask
        mask = self._get_mask_keys(**self._filter_namespace_args(opts, trex_stats.ALL_STATS_OPTS))
        if not mask:
            # set to show all stats if no filter was given
            mask = trex_stats.ALL_STATS_OPTS

        stats = self.cmd_stats(opts.ports, mask)

        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


        return RC_OK()



    @timing
    def cmd_validate_line (self, line):
        '''validates port(s) stream configuration\n'''

        parser = parsing_opts.gen_parser(self,
                                         "validate",
                                         self.cmd_validate_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line paramters")

        rc = self.cmd_validate(opts.ports)
        return rc


    def cmd_exit_line (self, line):
        print format_text("Exiting\n", 'bold')
        # a way to exit
        return RC_ERR("exit")


    def cmd_wait_line (self, line):
        '''wait for a period of time\n'''

        parser = parsing_opts.gen_parser(self,
                                         "wait",
                                         self.cmd_wait_line.__doc__,
                                         parsing_opts.DURATION)

        opts = parser.parse_args(line.split())
        if opts is None:
            return RC_ERR("bad command line parameters")

        delay_sec = opts.duration if (opts.duration > 0) else 1

        print format_text("Waiting for {0} seconds...\n".format(delay_sec), 'bold')
        time.sleep(delay_sec)

        return RC_OK()

    # run a script of commands
    def run_script_file (self, filename):

        print format_text("\nRunning script file '{0}'...".format(filename), 'bold')

        rc = self.cmd_connect()
        if rc.bad():
            return

        with open(filename) as f:
            script_lines = f.readlines()

        cmd_table = {}

        # register all the commands
        cmd_table['start'] = self.cmd_start_line
        cmd_table['stop']  = self.cmd_stop_line
        cmd_table['reset'] = self.cmd_reset_line
        cmd_table['wait']  = self.cmd_wait_line
        cmd_table['exit']  = self.cmd_exit_line

        for index, line in enumerate(script_lines, start = 1):
            line = line.strip()
            if line == "":
                continue
            if line.startswith("#"):
                continue

            sp = line.split(' ', 1)
            cmd = sp[0]
            if len(sp) == 2:
                args = sp[1]
            else:
                args = ""

            print format_text("Executing line {0} : '{1}'\n".format(index, line))

            if not cmd in cmd_table:
                print "\n*** Error at line {0} : '{1}'\n".format(index, line)
                print format_text("unknown command '{0}'\n".format(cmd), 'bold')
                return False

            rc = cmd_table[cmd](args)
            if rc.bad():
                return False

        print format_text("\n[Done]", 'bold')

        return True


    #################################
    # ------ private methods ------ #
    @staticmethod
    def _get_mask_keys(ok_values={True}, **kwargs):
        masked_keys = set()
        for key, val in kwargs.iteritems():
            if val in ok_values:
                masked_keys.add(key)
        return masked_keys

    @staticmethod
    def _filter_namespace_args(namespace, ok_values):
        return {k: v for k, v in namespace.__dict__.items() if k in ok_values}


    #################################
    # ------ private classes ------ #
    class CCommLink(object):
        """describes the connectivity of the stateless client method"""
        def __init__(self, server="localhost", port=5050, virtual=False):
            super(CTRexStatelessClient.CCommLink, self).__init__()
            self.virtual = virtual
            self.server = server
            self.port = port
            self.verbose = False
            self.rpc_link = JsonRpcClient(self.server, self.port)

        @property
        def is_connected(self):
            if not self.virtual:
                return self.rpc_link.connected
            else:
                return True

        def set_verbose(self, mode):
            self.verbose = mode
            return self.rpc_link.set_verbose(mode)

        def connect(self):
            if not self.virtual:
                return self.rpc_link.connect()

        def disconnect(self):
            if not self.virtual:
                return self.rpc_link.disconnect()

        def transmit(self, method_name, params={}):
            if self.virtual:
                self._prompt_virtual_tx_msg()
                _, msg = self.rpc_link.create_jsonrpc_v2(method_name, params)
                print msg
                return
            else:
                return self.rpc_link.invoke_rpc_method(method_name, params)

        def transmit_batch(self, batch_list):
            if self.virtual:
                self._prompt_virtual_tx_msg()
                print [msg
                       for _, msg in [self.rpc_link.create_jsonrpc_v2(command.method, command.params)
                                      for command in batch_list]]
            else:
                batch = self.rpc_link.create_batch()
                for command in batch_list:
                    batch.add(command.method, command.params)
                # invoke the batch
                return batch.invoke()

        def _prompt_virtual_tx_msg(self):
            print "Transmitting virtually over tcp://{server}:{port}".format(server=self.server,
                                                                             port=self.port)


if __name__ == "__main__":
    pass
