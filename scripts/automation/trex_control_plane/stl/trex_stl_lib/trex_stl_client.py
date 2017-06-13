#!/router/bin/python

# for API usage the path name must be full
from .trex_stl_exceptions import *
from .trex_stl_streams import *
from .trex_stl_psv import *

from .trex_stl_conn import Connection

from . import trex_stl_stats

from .trex_stl_port import Port
from .trex_stl_types import *

from .services.trex_stl_service_icmp import STLServiceICMP
from .services.trex_stl_service_arp import STLServiceARP

from .utils import parsing_opts, text_tables, common
from .utils.common import *
from .utils.text_tables import TRexTextTable
from .utils.text_opts import *

from functools import wraps
from texttable import ansi_len

from .services.trex_stl_service_int import STLServiceCtx
from collections import namedtuple, defaultdict
from yaml import YAMLError
from contextlib import contextmanager
import time
import datetime
import re
import random
import json
import traceback
import tempfile
import readline
import os.path


############################     logger     #############################
############################                #############################
############################                #############################

# logger API for the client
class LoggerApi(object):
    # verbose levels
    VERBOSE_QUIET         = 0
    VERBOSE_REGULAR_SYNC  = 1
    VERBOSE_REGULAR       = 2
    VERBOSE_HIGH          = 3

    def __init__(self):
        self.level = LoggerApi.VERBOSE_REGULAR

    # implemented by specific logger
    def write(self, msg, newline = True):
        raise Exception("Implement this")

    # implemented by specific logger
    def flush(self):
        raise Exception("Implement this")

    def set_verbose (self, level):
        if not level in range(self.VERBOSE_QUIET, self.VERBOSE_HIGH + 1):
            raise ValueError("Bad value provided for logger")

        self.level = level

    def get_verbose (self):
        return self.level


    def check_verbose (self, level):
        return (self.level >= level)


    # simple log message with verbose
    def log (self, msg, level = VERBOSE_REGULAR_SYNC, newline = True):
        if not self.check_verbose(level):
            return

        self.write(msg, newline)

    # logging that comes from async event
    def async_log (self, msg, level = VERBOSE_REGULAR, newline = True):
        self.log(msg, level, newline)


    # urgent async log - will log event under quiet
    def urgent_async_log (self, msg, newline = True):
        # log even on quiet mode
        self.async_log(msg, level = LoggerApi.VERBOSE_QUIET, newline = newline)
        self.flush()
        
        
    def pre_cmd (self, desc):
        self.log(format_text('\n{:<60}'.format(desc), 'bold'), newline = False)
        self.flush()

        
    def post_cmd (self, rc):
        if rc:
            self.log(format_text("[SUCCESS]\n", 'green', 'bold'))
        else:
            self.log(format_text("[FAILED]\n", 'red', 'bold'))


    def log_cmd (self, desc):
        self.pre_cmd(desc)
        self.post_cmd(True)


    # supress object getter
    def supress (self, level = VERBOSE_QUIET):
        class Supress(object):
            def __init__ (self, logger, level):
                self.logger = logger
                self.level = level

            def __enter__ (self):
                self.saved_level = self.logger.get_verbose()
                if self.level < self.saved_level:
                    self.logger.set_verbose(self.level)

            def __exit__ (self, type, value, traceback):
                self.logger.set_verbose(self.saved_level)

        return Supress(self, level)



# default logger - to stdout
class DefaultLogger(LoggerApi):

    def __init__ (self):
        super(DefaultLogger, self).__init__()

    def write (self, msg, newline = True):
        if newline:
            print(msg)
        else:
            print (msg),

    def flush (self):
        sys.stdout.flush()



############################     async event hander     #############################
############################                            #############################
############################                            #############################

# an event
class Event(object):

    def __init__ (self, origin, ev_type, msg):
        self.origin = origin
        self.ev_type = ev_type
        self.msg = msg

        self.ts = datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')

    def __str__ (self):

        prefix = "[{:^}][{:^}]".format(self.origin, self.ev_type)

        return "{:<10} - {:18} - {:}".format(self.ts, prefix, format_text(self.msg, 'bold'))


# handles different async events given to the client
class EventsHandler(object):
    EVENT_PORT_STARTED   = 0
    EVENT_PORT_STOPPED   = 1
    EVENT_PORT_PAUSED    = 2
    EVENT_PORT_RESUMED   = 3
    EVENT_PORT_JOB_DONE  = 4
    EVENT_PORT_ACQUIRED  = 5
    EVENT_PORT_RELEASED  = 6
    EVENT_PORT_ERROR     = 7
    EVENT_PORT_ATTR_CHG  = 8
    
    EVENT_SERVER_STOPPED = 100

    
    def __init__ (self, client):
        self.client = client
        self.logger = self.client.logger

        self.events = []

        # events are disabled by default until explicitly enabled
        self.enabled = False
        
        
    # will start handling events
    def enable (self):
        self.enabled = True
        
    def disable (self):
        self.enabled = False
        
    def is_enabled (self):
        return self.enabled
        
        
    # public functions

    def get_events (self, ev_type_filter = None):
        if ev_type_filter:
            return [ev for ev in self.events if ev.ev_type in listify(ev_type_filter)]
        else:
            return [ev for ev in self.events]


    def clear_events (self):
        self.events = []


    def log_warning (self, msg):
        self.__add_event_log('local', 'warning', msg)


    # events called internally

    def on_async_timeout (self, timeout_sec):
        if self.client.conn.is_connected():
            msg = 'Connection lost - Subscriber timeout: no data from TRex server for more than {0} seconds'.format(timeout_sec)
            self.log_warning(msg)
            
            # we cannot simply disconnect the connection - we mark it for disconnection
            # later on, the main thread will execute an ordered disconnection
            self.client.conn.mark_for_disconnect(msg)
            
            

    def on_async_crash (self):
        msg = 'subscriber thread has crashed:\n\n{0}'.format(traceback.format_exc())
        self.log_warning(msg)
        
        # if connected, mark as disconnected
        if self.client.conn.is_connected():
            self.client.conn.mark_for_disconnect(msg)
        
        
    def on_async_alive (self):
        pass

    
    def on_async_rx_stats_event (self, data, baseline):
        if not self.is_enabled():
            return
            
        self.client.flow_stats.update(data, baseline)

    def on_async_latency_stats_event (self, data, baseline):
        if not self.is_enabled():
            return
            
        self.client.latency_stats.update(data, baseline)

    # handles an async stats update from the subscriber
    def on_async_stats_update(self, dump_data, baseline):
        if not self.is_enabled():
            return
            
        global_stats = {}
        port_stats = {}

        # filter the values per port and general
        for key, value in dump_data.items():
            # match a pattern of ports
            m = re.search('(.*)\-(\d+)', key)
            if m:
                port_id = int(m.group(2))
                field_name = m.group(1)
                if port_id in self.client.ports:
                    if not port_id in port_stats:
                        port_stats[port_id] = {}
                    port_stats[port_id][field_name] = value
                else:
                    continue
            else:
                # no port match - general stats
                global_stats[key] = value

        # update the general object with the snapshot
        self.client.global_stats.update(global_stats, baseline)

        # update all ports
        for port_id, data in port_stats.items():
            self.client.ports[port_id].port_stats.update(data, baseline)



    # dispatcher for server async events (port started, port stopped and etc.)
    def on_async_event (self, event_id, data):
        if not self.is_enabled():
            return
            
        # default type info and do not show
        ev_type = 'info'
        show_event = False
        
        # port started
        if (event_id == self.EVENT_PORT_STARTED):
            port_id = int(data['port_id'])
            ev = "Port {0} has started".format(port_id)
            self.__async_event_port_started(port_id)

        # port stopped
        elif (event_id == self.EVENT_PORT_STOPPED):
            port_id = int(data['port_id'])
            ev = "Port {0} has stopped".format(port_id)

            # call the handler
            self.__async_event_port_stopped(port_id)


        # port paused
        elif (event_id == self.EVENT_PORT_PAUSED):
            port_id = int(data['port_id'])
            ev = "Port {0} has paused".format(port_id)

            # call the handler
            self.__async_event_port_paused(port_id)

            
        # port resumed
        elif (event_id == self.EVENT_PORT_RESUMED):
            port_id = int(data['port_id'])
            ev = "Port {0} has resumed".format(port_id)

            # call the handler
            self.__async_event_port_resumed(port_id)

            
        # port finished traffic
        elif (event_id == self.EVENT_PORT_JOB_DONE):
            port_id = int(data['port_id'])
            ev = "Port {0} job done".format(port_id)

            # call the handler
            self.__async_event_port_job_done(port_id)
            
            # mark the event for show
            show_event = True

            
        # port was acquired - maybe stolen...
        elif (event_id == self.EVENT_PORT_ACQUIRED):
            session_id = data['session_id']

            port_id = int(data['port_id'])
            who     = data['who']
            force   = data['force']

            # if we hold the port and it was not taken by this session - show it
            if port_id in self.client.get_acquired_ports() and session_id != self.client.session_id:
                ev_type = 'warning'

            # format the thief/us...
            if session_id == self.client.session_id:
                user = 'you'
            elif who == self.client.username:
                user = 'another session of you'
            else:
                user = "'{0}'".format(who)

            if force:
                ev = "Port {0} was forcely taken by {1}".format(port_id, user)
            else:
                ev = "Port {0} was taken by {1}".format(port_id, user)

            # call the handler in case its not this session
            if session_id != self.client.session_id:
                self.__async_event_port_acquired(port_id, who)


        # port was released
        elif (event_id == self.EVENT_PORT_RELEASED):
            port_id     = int(data['port_id'])
            who         = data['who']
            session_id  = data['session_id']

            if session_id == self.client.session_id:
                user = 'you'
            elif who == self.client.username:
                user = 'another session of you'
            else:
                user = "'{0}'".format(who)

            ev = "Port {0} was released by {1}".format(port_id, user)

            # call the handler in case its not this session
            if session_id != self.client.session_id:
                self.__async_event_port_released(port_id)

                
        elif (event_id == self.EVENT_PORT_ERROR):
            port_id = int(data['port_id'])
            ev = "port {0} job failed".format(port_id)
            ev_type = 'warning'
            

        # port attr changed
        elif (event_id == self.EVENT_PORT_ATTR_CHG):

            port_id = int(data['port_id'])
            
            diff = self.__async_event_port_attr_changed(port_id, data['attr'])
            if not diff:
                return

            
            ev = "port {0} attributes changed".format(port_id)
            for key, (old_val, new_val) in diff.items():
                ev += '\n  {key}: {old} -> {new}'.format(
                        key = key, 
                        old = old_val.lower() if type(old_val) is str else old_val,
                        new = new_val.lower() if type(new_val) is str else new_val)
                
            ev_type = 'info'
            show_event = False
        
         
    
        # server stopped
        elif (event_id == self.EVENT_SERVER_STOPPED):
            ev = "Server has been shutdown - cause: '{0}'".format(data['cause'])
            self.__async_event_server_stopped(ev)
            ev_type = 'warning'


        else:
            # unknown event - ignore
            return

        # showed events (port job done, 
        self.__add_event_log('server', ev_type, ev, show_event)


    # private functions

    # on rare cases events may come on a non existent prot
    # (server was re-run with different config)
    def __async_event_port_job_done (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_port_job_done()

    def __async_event_port_stopped (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_port_stopped()


    def __async_event_port_started (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_port_started()

    def __async_event_port_paused (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_port_paused()


    def __async_event_port_resumed (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_port_resumed()

    def __async_event_port_acquired (self, port_id, who):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_acquired(who)

    def __async_event_port_released (self, port_id):
        if port_id in self.client.ports:
            self.client.ports[port_id].async_event_released()

    def __async_event_server_stopped (self, cause):
        self.client.conn.mark_for_disconnect(cause)

    def __async_event_port_attr_changed (self, port_id, attr):
        if port_id in self.client.ports:
            return self.client.ports[port_id].async_event_port_attr_changed(attr)

    # add event to log
    def __add_event_log (self, origin, ev_type, msg, show_event = False):

        event = Event(origin, ev_type, msg)
        self.events.append(event)
        
        if ev_type == 'info' and show_event:
            self.logger.async_log("\n\n{0}".format(str(event)))
            
        elif ev_type == 'warning':
            self.logger.urgent_async_log("\n\n{0}".format(str(event)))

     


############################     client     #############################
############################                #############################
############################                #############################

class STLClient(object):
    """TRex Stateless client object - gives operations per TRex/user"""

    # different modes for attaching traffic to ports
    CORE_MASK_SPLIT  = 1
    CORE_MASK_PIN    = 2
    CORE_MASK_SINGLE = 3

    
    def __init__(self,
                 username = common.get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = LoggerApi.VERBOSE_QUIET,
                 logger = None,
                 virtual = False):
        """ 
        Configure the connection settings 

        :parameters:
             username : string 
                the user name, for example imarom

              server  : string 
                the server name or ip 

              sync_port : int 
                the RPC port 

              async_port : int 
                the ASYNC port 

        .. code-block:: python

            # Example

            # connect to local TRex server 
            c = STLClient()

            # connect to remote server trex-remote-server
            c = STLClient(server = "trex-remote-server" )

            c = STLClient(server = "10.0.0.10" )

            # verbose mode 
            c = STLClient(server = "10.0.0.10", verbose_level = LoggerApi.VERBOSE_HIGH )
            
            # change user name 
            c = STLClient(username = "root",server = "10.0.0.10", verbose_level = LoggerApi.VERBOSE_HIGH )

            c.connect()

            c.disconnect()

        """

        self.username   = username
         
        # init objects
        self.ports = {}
        self.server_version = {}
        self.system_info = {}
        self.session_id = random.getrandbits(32)
     
        # logger
        self.logger = DefaultLogger() if not logger else logger

        # initial verbose
        self.logger.set_verbose(verbose_level)

        # async event handler manager
        self.event_handler = EventsHandler(self)

     
        # stats
        self.connection_info = {"username":   username,
                                "server":     server,
                                "sync_port":  sync_port,
                                "async_port": async_port,
                                "virtual":    virtual}

        

        # connection state object
        self.conn = Connection(self.connection_info, self.logger, self)



        self.global_stats = trex_stl_stats.CGlobalStats(self.connection_info,
                                                        self.server_version,
                                                        self.ports,
                                                        self.event_handler)

        self.flow_stats = trex_stl_stats.CRxStats(self.ports)

        self.latency_stats = trex_stl_stats.CLatencyStats(self.ports)

        self.pgid_stats = trex_stl_stats.CPgIdStats()

        self.util_stats = trex_stl_stats.CUtilStats(self)

        self.xstats = trex_stl_stats.CXStats(self)

        self.stats_generator = trex_stl_stats.CTRexInfoGenerator(self.global_stats,
                                                                 self.ports,
                                                                 self.flow_stats,
                                                                 self.latency_stats,
                                                                 self.util_stats,
                                                                 self.xstats,
                                                                 self.conn.async.monitor,
                                                                 self.pgid_stats,
                                                                 self)
        
        self.psv = PortStateValidator(self)
        
        
    ############# private functions - used by the class itself ###########

    
    # some preprocessing for port argument
    def __ports (self, port_id_list):

        # none means all
        if port_id_list == None:
            return range(0, self.get_port_count())

        # always list
        if isinstance(port_id_list, int):
            port_id_list = [port_id_list]

        if not isinstance(port_id_list, list):
             raise ValueError("Bad port id list: {0}".format(port_id_list))

        for port_id in port_id_list:
            if not isinstance(port_id, int) or (port_id < 0) or (port_id > self.get_port_count()):
                raise ValueError("Bad port id {0}".format(port_id))

        return port_id_list


    # sync ports
    def __sync_ports (self, port_id_list = None, force = False):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].sync())

        return rc

    # acquire ports, if port_list is none - get all
    def __acquire (self, port_id_list = None, force = False, sync_streams = True):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].acquire(force, sync_streams))

        return rc

    # release ports
    def __release (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].release())

        return rc

 
    def __add_streams(self, stream_list, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].add_streams(stream_list))

        return rc


    def __remove_streams(self, stream_id_list, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_streams(stream_id_list))

        return rc



    def __remove_all_streams(self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_all_streams())

        self.pgid_stats.reset()

        return rc


    def __get_stream(self, stream_id, port_id, get_pkt = False):

        return self.ports[port_id].get_stream(stream_id)


    def __get_all_streams(self, port_id, get_pkt = False):

        return self.ports[port_id].get_all_streams()


    def __get_stream_id_list(self, port_id):

        return self.ports[port_id].get_stream_id_list()


    def __start (self,
                 multiplier,
                 duration,
                 port_id_list,
                 force,
                 core_mask,
                 start_at_ts = 0):

        port_id_list = self.__ports(port_id_list)

        rc = RC()
        if start_at_ts is None:
            start_at_ts = 0

        for port_id in port_id_list:
            rc.add(self.ports[port_id].start(multiplier,
                                             duration,
                                             force,
                                             core_mask[port_id],
                                             start_at_ts))

        return rc


    def __resume (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].resume())

        return rc

    def __pause (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].pause())

        return rc


    def __stop (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].stop(force))

        return rc


    def __update (self, mult, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].update(mult, force))

        return rc


    def __push_remote (self, pcap_filename, port_id_list, ipg_usec, speedup, count, duration, is_dual, min_ipg_usec):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:

            # for dual, provide the slave handler as well
            slave_handler = self.ports[port_id ^ 0x1].handler if is_dual else ""

            rc.add(self.ports[port_id].push_remote(pcap_filename,
                                                   ipg_usec,
                                                   speedup,
                                                   count,
                                                   duration,
                                                   is_dual,
                                                   slave_handler,
                                                   min_ipg_usec))

        return rc


    def __validate (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].validate())

        return rc


    def __scan6(self, port_id_list = None, timeout = 5):
        port_id_list = self.__ports(port_id_list)

        resp = {}

        for port_id in port_id_list:
            resp[port_id] = self.ports[port_id].scan6(timeout)

        return resp


    def __set_port_attr (self, port_id_list = None, attr_dict = None):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id, port_attr_dict in zip(port_id_list, attr_dict):
            rc.add(self.ports[port_id].set_attr(**port_attr_dict))

        return rc
        
    def __set_rx_queue (self, port_id_list, size):
        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].set_rx_queue(size))

        return rc

    def __remove_rx_queue (self, port_id_list):
        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_rx_queue())

        return rc

    def __get_rx_queue_pkts (self, port_id_list):
        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].get_rx_queue_pkts())

        return rc

    def __set_service_mode (self, port_id_list, enabled):
        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].set_service_mode(enabled))

        return rc
        

    # connect to server
    def __connect(self):

        # before we connect to the server - reject any async updates until fully init
        self.event_handler.disable()
        
        # connect to the server
        rc = self.conn.connect()
        if not rc:
            return rc
 
        # version
        rc = self._transmit("get_version")
        if not rc:
            return rc
            
        self.server_version = rc.data()
        self.global_stats.server_version = rc.data()
        
        # cache system info
        rc = self._transmit("get_system_info")
        if not rc:
            return rc

        self.system_info = rc.data()
        self.global_stats.system_info = rc.data()

        # cache supported commands
        rc = self._transmit("get_supported_cmds")
        if not rc:
            return rc

        self.supported_cmds = sorted(rc.data())

        # create ports
        self.ports.clear()
        for port_id in range(self.system_info["port_count"]):
            info = self.system_info['ports'][port_id]

            self.ports[port_id] = Port(port_id,
                                       self.username,
                                       self.conn.rpc,
                                       self.session_id,
                                       info)

        # sync the ports
        rc = self.__sync_ports()
        if not rc:
            return rc

        
        # mark the event handler as ready to process async updates
        self.event_handler.enable()
        
        # after we are done with configuring the client
        # sync all the data from the server (baseline)
        rc = self.conn.sync()
        if not rc:
            return rc

        self.get_pgid_stats()
        self.pgid_stats.clear_stats()
        
        return RC_OK()


    # disconenct from server
    def __disconnect(self, release_ports = True):
        # release any previous acquired ports
        if self.conn.is_connected() and release_ports:
            self.__release(self.get_acquired_ports())

        # disconnect the link to the server
        self.conn.disconnect()

        return RC_OK()


    # clear stats
    def __clear_stats(self, port_id_list, clear_global, clear_flow_stats, clear_latency_stats, clear_xstats, clear_pgid_stats):

        # we must be sync with the server
        self.conn.barrier()

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        if clear_global:
            self.global_stats.clear_stats()

            # ??? remove
        if clear_flow_stats:
            self.flow_stats.clear_stats()

        if clear_latency_stats:
            self.latency_stats.clear_stats()

        if clear_xstats:
            self.xstats.clear_stats()

        if clear_pgid_stats:
            self.pgid_stats.clear_stats()
            self.stats_generator.clear_stats()

        self.logger.log_cmd("Clearing stats on port(s) {0}:".format(port_id_list))

        return RC


    # get stats
    def __get_stats (self, port_id_list):
        pgid_stats = self.get_pgid_stats()
        if not pgid_stats:
            raise STLError(pgid_stats)

        stats = {}

        stats['global'] = self.global_stats.get_stats()

        total = {}
        for port_id in port_id_list:
            port_stats = self.ports[port_id].get_stats()
            stats[port_id] = port_stats

            for k, v in port_stats.items():
                if not k in total:
                    total[k] = v
                else:
                    total[k] += v

        stats['total'] = total

        if 'flow_stats' in pgid_stats:
            stats['flow_stats'] = pgid_stats['flow_stats']
        else:
            stats['flow_stats'] = {}
        if 'latency' in pgid_stats:
            stats['latency'] = pgid_stats['latency']
        else:
            stats['latency'] = {}

        return stats


    def __decode_core_mask (self, ports, core_mask):
        available_modes = [self.CORE_MASK_PIN, self.CORE_MASK_SPLIT, self.CORE_MASK_SINGLE]

        # predefined modes
        if isinstance(core_mask, int):
            if core_mask not in available_modes:
                raise STLError("'core_mask' can be either %s or a list of masks" % ', '.join(available_modes))

            decoded_mask = {}
            for port in ports:
                # a pin mode was requested and we have
                # the second port from the group in the start list
                if (core_mask == self.CORE_MASK_PIN) and ( (port ^ 0x1) in ports ):
                    decoded_mask[port] = 0x55555555 if( port % 2) == 0 else 0xAAAAAAAA
                elif core_mask == self.CORE_MASK_SINGLE:
                    decoded_mask[port] = 0x1
                else:
                    decoded_mask[port] = None

            return decoded_mask

        # list of masks
        elif isinstance(core_mask, list):
            if len(ports) != len(core_mask):
                raise STLError("'core_mask' list must be the same length as 'ports' list")
            
            decoded_mask = {}
            for i, port in enumerate(ports):
                decoded_mask[port] = core_mask[i]

            return decoded_mask



    ############ functions used by other classes but not users ##############

    # fetch the API Handlers from the connection object
    def _get_api_h (self):
        return self.conn.get_api_h()

        
    def _validate_port_list (self, port_id_list, allow_empty = False):
        # listfiy single int
        if isinstance(port_id_list, int):
            port_id_list = [port_id_list]

        # should be a list
        if not isinstance(port_id_list, list) and not isinstance(port_id_list, tuple):
            raise STLTypeError('port_id_list', type(port_id_list), list)

        if not port_id_list and not allow_empty:
            raise STLError('No ports provided')

        valid_ports = self.get_all_ports()
        for port_id in port_id_list:
            if not port_id in valid_ports:
                raise STLError("Port ID '{0}' is not a valid port ID - valid values: {1}".format(port_id, valid_ports))

        return list_remove_dup(port_id_list)


    # transmit request on the RPC link
    def _transmit(self, method_name, params = None, api_class = 'core'):
        return self.conn.rpc.transmit(method_name, params, api_class)

    # transmit batch request on the RPC link
    def _transmit_batch(self, batch_list):
        return self.conn.rpc.transmit_batch(batch_list)

    # stats
    def _get_formatted_stats(self, port_id_list, stats_mask = trex_stl_stats.COMPACT):

        stats_opts = common.list_intersect(trex_stl_stats.ALL_STATS_OPTS, stats_mask)

        stats_obj = OrderedDict()
        for stats_type in stats_opts:
            stats_obj.update(self.stats_generator.generate_single_statistic(port_id_list, stats_type))

        return stats_obj

    def _get_streams(self, port_id_list, streams_mask=set()):

        streams_obj = self.stats_generator.generate_streams_info(port_id_list, streams_mask)

        return streams_obj


    def _invalidate_stats (self, port_id_list):
        for port_id in port_id_list:
            self.ports[port_id].invalidate_stats()

        self.global_stats.invalidate()
        self.flow_stats.invalidate()

        return RC_OK()


    # remove all RX filters in a safe manner
    def _remove_rx_filters (self, ports, rx_delay_ms):

        # get the enabled RX ports
        rx_ports = [port_id for port_id in ports if self.ports[port_id].has_rx_enabled()]

        if not rx_ports:
            return RC_OK()

        # block while any RX configured port has not yet have it's delay expired
        while any([not self.ports[port_id].has_rx_delay_expired(rx_delay_ms) for port_id in rx_ports]):
            time.sleep(0.01)

        # remove RX filters
        rc = RC()
        for port_id in rx_ports:
            rc.add(self.ports[port_id].remove_rx_filters())

        return rc


    #################################
    # ------ private methods ------ #
    @staticmethod
    def __get_mask_keys(ok_values={True}, **kwargs):
        masked_keys = set()
        for key, val in kwargs.items():
            if val in ok_values:
                masked_keys.add(key)
        return masked_keys

    @staticmethod
    def __filter_namespace_args(namespace, ok_values):
        return {k: v for k, v in namespace.__dict__.items() if k in ok_values}


    # API decorator - double wrap because of argument
    def __api_check(connected = True):

        def wrap (f):
            @wraps(f)
            def wrap2(*args, **kwargs):
                client = args[0]

                func_name = f.__name__

                
                try:
                    # before we enter the API, set the async thread to signal in case of connection lost
                    client.conn.sigint_on_conn_lost_enable()
                    
                    # check connection
                    if connected and not client.is_connected():
                    
                        if client.conn.is_marked_for_disconnect():
                            # connection state is marked for disconnect - something went wrong
                            raise STLError("'{0}' - connection to the server had been lost: '{1}'".format(func_name, client.conn.get_disconnection_cause()))
                        else:
                            # simply was called while disconnected
                            raise STLError("'{0}' - is not valid while disconnected".format(func_name))

                    # call the API
                    ret = f(*args, **kwargs)
                    
                except KeyboardInterrupt as e:
                    # SIGINT can be either from ctrl + c or from the async thread to interrupt the main thread
                    if client.conn.is_marked_for_disconnect():
                        raise STLError("'{0}' - connection to the server had been lost: '{1}'".format(func_name, client.conn.get_disconnection_cause()))
                    else:
                        raise STLError("'{0}' - interrupted by a keyboard signal (probably ctrl + c)".format(func_name))

                finally:
                    # when we exit API context - disable SIGINT from the async thread
                    client.conn.sigint_on_conn_lost_disable()
                    
                    
                return ret
            return wrap2

        return wrap



    ############################     API     #############################
    ############################             #############################
    ############################             #############################
    def __enter__ (self):
        self.connect()
        self.acquire(force = True)
        self.reset()
        return self

    def __exit__ (self, type, value, traceback):
        if self.get_active_ports():
            self.stop(self.get_active_ports())
        self.disconnect()

    ############################   Getters   #############################
    ############################             #############################
    ############################             #############################


    # return verbose level of the logger
    def get_verbose (self):
        """ 
        Get the verbose mode  

        :parameters:
          none

        :return:
            Get the verbose mode as Bool

        :raises:
          None

        """
        return self.logger.get_verbose()

    # is the client on read only mode ?
    def is_all_ports_acquired (self):
        """ 
         is_all_ports_acquired

        :parameters:
          None

        :return:
            Returns True if all ports are acquired

        :raises:
          None

        """

        return (self.get_all_ports() == self.get_acquired_ports())


    # is the client connected ?
    def is_connected (self):
        """ 

        :parameters:
            None

        :return:
            True if the connection is established to the server
            o.w False

        :raises:
          None

        """
        return self.conn.is_connected()


    # get connection info
    def get_connection_info (self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.connection_info


    # get supported commands by the server
    def get_server_supported_cmds(self):
        """ 

        :parameters:
          None

        :return:
            List of commands supported by server RPC

        :raises:
          None

        """

        return self.supported_cmds

    # get server version
    def get_server_version(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.server_version

    # get server system info
    def get_server_system_info(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.system_info

    # get port count
    def get_port_count(self):
        """ 

        :parameters:
          None

        :return:
            Number of ports

        :raises:
          None

        """

        return len(self.ports)


    # returns the port object
    def get_port (self, port_id):
        port = self.ports.get(port_id, None)
        if (port != None):
            return port
        else:
            raise STLArgumentError('port id', port_id, valid_values = self.get_all_ports())


    # get all ports as IDs
    def get_all_ports (self):
        """ 

        :parameters:
          None

        :return:
            List of all ports

        :raises:
          None

        """

        return list(self.ports)

    # get all acquired ports
    def get_acquired_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired()]

    # get all active ports (TX or pause)
    def get_active_ports(self, owned = True):
        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_active() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_active()]


    def get_resolvable_ports (self):
         return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired() and port_obj.is_l3_mode()]
    
    def get_resolved_ports (self):
        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired() and port_obj.is_resolved()]


    def get_service_enabled_ports(self, owned = True):
        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_service_mode_on() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_service_mode_on()]
      
        
    # get paused ports
    def get_paused_ports (self, owned = True):
        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_paused() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_paused()]


    # get all TX ports
    def get_transmitting_ports (self, owned = True):
        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_transmitting() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_transmitting()]


    # get stats
    @__api_check(True)
    def get_stats (self, ports = None, sync_now = True):
        """
        Return dictionary containing statistics information gathered from the server.

        :parameters:

          ports - List of ports to retreive stats on.
                  If None, assume the request is for all acquired ports.

          sync_now - Boolean - If true, create a call to the server to get latest stats, and wait for result to arrive. Otherwise, return last stats saved in client cache.
                            Downside of putting True is a slight delay (few 10th msecs) in getting the result. For practical uses, value should be True.
        :return:
            Statistics dictionary of dictionaries with the below format.

            **Note** that for getting latency and flow_stats statistics there is now a newer and more efficient API
            :ref:`get_pgid_stats<get_pgid_stats>`. In the future, flow stats and latency info might be removed
            from the results of this function, so it is advisable to use the above mentioned API.

            ================================  ===============
            key                               Meaning
            ================================  ===============
            :ref:`numbers (0,1,..<total>`     Statistcs per port number
            :ref:`total <total>`              Sum of port statistics
            :ref:`flow_stats <flow_stats_o>`  Per flow statistics
            :ref:`global <global>`            Global statistics
            :ref:`latency <latency_o>`        Per flow statistics regarding flow latency
            ================================  ===============

            Below is description of each of the inner dictionaries.

            .. _total:

            **total** and per port statistics contain dictionary with following format.

            Most of the bytes counters (unless specified otherwise) are in L2 layer, including the Ethernet FCS. e.g. minimum packet size is 64 bytes

            ===============================  ===============
            key                               Meaning
            ===============================  ===============
            ibytes                           Number of input bytes 
            ierrors                          Number of input errors
            ipackets                         Number of input packets 
            obytes                           Number of output bytes  
            oerrors                          Number of output errors
            opackets                         Number of output packets
            rx_bps                           Receive bits per second rate (L2 layer)
            rx_pps                           Receive packet per second rate
            tx_bps                           Transmit bits per second rate (L2 layer)
            tx_pps                           Transmit packet per second rate
            ===============================  ===============

            .. _flow_stats_o:

            **flow_stats** contains :ref:`global dictionary <flow_stats_global_o>`, and dictionaries per packet group id (pg id). See structures below.

            **per pg_id flow stat** dictionaries have following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            rx_bps              Received bits per second rate
            rx_bps_l1           Received bits per second rate, including layer one
            rx_bytes            Total number of received bytes
            rx_pkts             Total number of received packets
            rx_pps              Received packets per second
            tx_bps              Transmit bits per second rate
            tx_bps_l1           Transmit bits per second rate, including layer one
            tx_bytes            Total number of sent bits
            tx_pkts             Total number of sent packets
            tx_pps              Transmit packets per second rate
            =================   ===============

            .. _flow_stats_global_o:

            **global flow stats** dictionary has the following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            rx_err              Number of flow statistics packets received that we could not associate to any pg_id. This can happen if latency on the used setup is large. See :ref:`wait_on_traffic <wait_on_traffic>` rx_delay_ms parameter for details.
            tx_err              Number of flow statistics packets transmitted that we could not associate to any pg_id. This is never expected. If you see this different than 0, please report.
            =================   ===============

            .. _global:

            **global**

            =================   ===============
            key                 Meaning
            =================   ===============
            bw_per_core         Estimated bit rate Trex can support per core. This is calculated by extrapolation of current rate and load on transmitting cores.
            cpu_util            Estimate of the average utilization percentage of the transimitting cores
            queue_full          Total number of packets transmitted while the NIC TX queue was full. The packets will be transmitted, eventually, but will create high CPU%due to polling the queue.  This usually indicates that the rate we trying to transmit is too high for this port. 
            rx_cpu_util         Estimate of the utilization percentage of the core handling RX traffic. Too high value of this CPU utilization could cause drop of latency streams. 
            rx_drop_bps         Received bits per second drop rate
            rx_bps              Received bits per second rate
            rx_pps              Received packets per second rate
            tx_bps              Transmit bits per second rate
            tx_pps              Transmit packets per second rate
            =================   ===============

            .. _latency_o:

            **latency** contains :ref:`global dictionary <lat_stats_global_o>`, and dictionaries per packet group id (pg id). Each one with the following structure.

            **per pg_id latency stat** dictionaries have following structure:

            =============================          ===============
            key                                    Meaning
            =============================          ===============
            :ref:`err_cntrs<err-cntrs_o>`          Counters describing errors that occured with this pg id
            :ref:`latency<lat_inner_o>`            Information regarding packet latency
            =============================          ===============

            Following are the inner dictionaries of latency

            .. _err-cntrs_o:

            **err-cntrs**

            =================   ===============
            key                 Meaning (see better explanation below the table)
            =================   ===============
            dropped             How many packets were dropped (estimation)
            dup                 How many packets were duplicated.
            out_of_order        How many packets we received out of order.
            seq_too_high        How many events of packet with sequence number too high we saw.
            seq_too_low         How many events of packet with sequence number too low we saw.
            =================   ===============

            For calculating packet error events, we add sequence number to each packet's payload. We decide what went wrong only according to sequence number
            of last packet received and that of the previous packet. 'seq_too_low' and 'seq_too_high' count events we see. 'dup', 'out_of_order' and 'dropped'
            are heuristics we apply to try and understand what happened. They will be accurate in common error scenarios.
            We describe few scenarios below to help understand this.

            Scenario 1: Received packet with seq num 10, and another one with seq num 10. We increment 'dup' and 'seq_too_low' by 1.

            Scenario 2: Received pacekt with seq num 10 and then packet with seq num 15. We assume 4 packets were dropped, and increment 'dropped' by 4, and 'seq_too_high' by 1.
            We expect next packet to arrive with sequence number 16.

            Scenario 2 continue: Received packet with seq num 11. We increment 'seq_too_low' by 1. We increment 'out_of_order' by 1. We *decrement* 'dropped' by 1.
            (We assume here that one of the packets we considered as dropped before, actually arrived out of order).


            .. _lat_inner_o:

            **latency**

            =================   ===============
            key                 Meaning
            =================   ===============
            average             Average latency over the stream lifetime (usec).Low pass filter is applied to the last window average.It is computed each sampling period by following formula: <average> = <prev average>/2 + <last sampling period average>/2
            histogram           Dictionary describing logarithmic distribution histogram of packet latencies. Keys in the dictionary represent range of latencies (in usec). Values are the total number of packets received in this latency range. For example, an entry {100:13} would mean that we saw 13 packets with latency in the range between 100 and 200 usec.
            jitter              Jitter of latency samples, computed as described in :rfc:`3550#appendix-A.8`
            last_max            Maximum latency measured between last two data reads from server (0.5 sec window).
            total_max           Maximum latency measured over the stream lifetime (in usec).
            total_min           Minimum latency measured over the stream lifetime (in usec).
            =================   ===============

            .. _lat_stats_global_o:

            **global latency stats** dictionary has the following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            old_flow            Number of latency statistics packets received that we could not associate to any pg_id. This can happen if latency on the used setup is large. See :ref:`wait_on_traffic <wait_on_traffic>` rx_delay_ms parameter for details.
            bad_hdr             Number of latency packets received with bad latency data. This can happen becuase of garbage packets in the network, or if the DUT causes packet corruption.
            =================   ===============

        :raises:
          None

        """
        # by default use all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # check async barrier
        if not type(sync_now) is bool:
            raise STLArgumentError('sync_now', sync_now)

        # if the user requested a barrier - use it
        if sync_now:
            rc = self.conn.barrier()
            if not rc:
                raise STLError(rc)

        return self.__get_stats(ports)


    def get_events (self, ev_type_filter = None):
        """ 
        returns all the logged events

        :parameters:
          ev_type_filter - 'info', 'warning' or a list of those
                           default: no filter

        :return:
            logged events

        :raises:
          None

        """
        return self.event_handler.get_events(ev_type_filter)


    def get_warnings (self):
        """ 
        returns all the warnings logged events

        :parameters:
          None

        :return:
            warning logged events

        :raises:
          None

        """
        return self.get_events(ev_type_filter = 'warning')


    def get_info (self):
        """ 
        returns all the info logged events

        :parameters:
          None

        :return:
            warning logged events

        :raises:
          None

        """
        return self.get_events(ev_type_filter = 'info')


    # get port(s) info as a list of dicts
    @__api_check(True)
    def get_port_info (self, ports = None):

        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        return [self.ports[port_id].get_formatted_info() for port_id in ports]


    ############################   Commands   #############################
    ############################              #############################
    ############################              #############################


    def set_verbose (self, level):
        """
            Sets verbose level

            :parameters:
                level : str
                    "high"
                    "low"
                    "normal"

            :raises:
                None

        """
        modes = {'low' : LoggerApi.VERBOSE_QUIET, 'normal': LoggerApi.VERBOSE_REGULAR, 'high': LoggerApi.VERBOSE_HIGH}

        if not level in modes.keys():
            raise STLArgumentError('level', level)

        self.logger.set_verbose(modes[level])

    @__api_check(False)
    def connect (self):
        """

            Connects to the TRex server

            :parameters:
                None

            :raises:
                + :exc:`STLError`

        """
        
        # cleanup from previous connection
        self.__disconnect()
        
        rc = self.__connect()
        if not rc:
            self.__disconnect()
            raise STLError(rc)
        

    @__api_check(False)
    def disconnect (self, stop_traffic = True, release_ports = True):
        """
            Disconnects from the server

            :parameters:
                stop_traffic : bool
                    Attempts to stop traffic before disconnecting.
                release_ports : bool
                    Attempts to release all the acquired ports.

        """

        # try to stop ports but do nothing if not possible
        if stop_traffic:
            try:
                self.stop()
            except STLError:
                pass


        self.logger.pre_cmd("Disconnecting from server at '{0}':'{1}'".format(self.connection_info['server'],
                                                                              self.connection_info['sync_port']))
        rc = self.__disconnect(release_ports)
        self.logger.post_cmd(rc)



    @__api_check(True)
    def acquire (self, ports = None, force = False, sync_streams = True):
        """
            Acquires ports for executing commands

            :parameters:
                ports : list
                    Ports on which to execute the command

                force : bool
                    Force acquire the ports.

                sync_streams: bool
                    sync with the server about the configured streams

            :raises:
                + :exc:`STLError`

        """

        # by default use all ports
        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        if force:
            self.logger.pre_cmd("Force acquiring ports {0}:".format(ports))
        else:
            self.logger.pre_cmd("Acquiring ports {0}:".format(ports))

        rc = self.__acquire(ports, force, sync_streams)

        self.logger.post_cmd(rc)

        if not rc:
            # cleanup
            self.__release(ports)
            raise STLError(rc)

        for port_id in ports:
            if not self.ports[port_id].is_resolved():
                self.logger.log(format_text('*** Warning - Port {0} destination is unresolved ***'.format(port_id), 'bold'))

    @__api_check(True)
    def release (self, ports = None):
        """
            Release ports

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`STLError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        self.logger.pre_cmd("Releasing ports {0}:".format(ports))
        rc = self.__release(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

            

    @__api_check(True)
    def ping_rpc_server(self):
        """
            Pings the RPC server

            :parameters:
                 None

            :return:
                 Timestamp of server

            :raises:
                + :exc:`STLError`

        """

        self.logger.pre_cmd("Pinging the server on '{0}' port '{1}': ".format(self.connection_info['server'],
                                                                              self.connection_info['sync_port']))
        rc = self._transmit("ping", api_class = None)
            
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        return rc.data()
        

    @__api_check(True)
    def set_l2_mode (self, port, dst_mac):
        """
            Sets the port mode to L2

            :parameters:
                 port      - the port to set the source address
                 dst_mac   - destination MAC
            :raises:
                + :exc:`STLError`
        """
        
        validate_type('port', port, int)
        if port not in self.get_all_ports():
            raise STLError("port {0} is not a valid port id".format(port))
            
        if not is_valid_mac(dst_mac):
            raise STLError("dest_mac is not a valid MAC address: '{0}'".format(dst_mac))
            
        self.logger.pre_cmd("Setting port {0} in L2 mode: ".format(port))
        rc = self.ports[port].set_l2_mode(dst_mac)
        self.logger.post_cmd(rc)
        
        if not rc:
            raise STLError(rc)
            
            
    @__api_check(True)
    def set_l3_mode (self, port, src_ipv4, dst_ipv4):
        """
            Sets the port mode to L3

            :parameters:
                 port      - the port to set the source address
                 src_ipv4  - IPv4 source address for the port
                 dst_ipv4  - IPv4 destination address
            :raises:
                + :exc:`STLError`
        """
    
        self.psv.validate('Layer 3 Config', port, (PSV_ACQUIRED, PSV_SERVICE))
        
        if not is_valid_ipv4(src_ipv4):
            raise STLError("src_ipv4 is not a valid IPv4 address: '{0}'".format(src_ipv4))
            
        if not is_valid_ipv4(dst_ipv4):
            raise STLError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))
            
        self.logger.pre_cmd("Setting port {0} in L3 mode: ".format(port))
        rc = self.ports[port].set_l3_mode(src_ipv4, dst_ipv4)
        self.logger.post_cmd(rc)
        
        if not rc:
            raise STLError(rc)
    
        # resolve the port
        self.resolve(ports = port)

        
    @__api_check(True)
    def ping_ip (self, src_port, dst_ip, pkt_size = 64, count = 5, interval_sec = 1):
        """
            Pings an IP address through a port

            :parameters:
                 src_port     - on which port_id to send the ICMP PING request
                 dst_ip       - which IP to ping
                 pkt_size     - packet size to use
                 count        - how many times to ping
                 interval_sec - how much time to wait between pings

            :returns:
                List of replies per 'count'

                Each element is dictionary with following items:

                Always available keys:

                * formatted_string - string, human readable output, for example: 'Request timed out.'
                * status - string, one of options: 'success', 'unreachable', 'timeout'

                Available only if status is 'success':

                * src_ip - string, IP replying to request
                * rtt - float, latency of the ping (round trip time)
                * ttl - int, time to live in IPv4 or hop limit in IPv6

            :raises:
                + :exc:`STLError`

        """
        
        if not (is_valid_ipv4(dst_ip) or is_valid_ipv6(dst_ip)):
            raise STLError("PING - dst_ip is not a valid IPv4/6 address: '{0}'".format(dst_ip))
            
        if (pkt_size < 64) or (pkt_size > 9216):
            raise STLError("PING - pkt_size should be a value between 64 and 9216: '{0}'".format(pkt_size))
        
        validate_type('count', count, int)
        validate_type('interval_sec', interval_sec, (int, float))
        
        # validate src port
        if is_valid_ipv4(dst_ip):
            self.psv.validate('PING IPv4', src_port, (PSV_ACQUIRED, PSV_SERVICE, PSV_L3))
        else:
            self.psv.validate('PING IPv6', src_port, (PSV_ACQUIRED, PSV_SERVICE))
        
        
        self.logger.pre_cmd("Pinging {0} from port {1} with {2} bytes of data:".format(dst_ip,
                                                                                       src_port,
                                                                                       pkt_size))
        
        if is_valid_ipv4(dst_ip):
            return self._ping_ipv4(src_port, dst_ip, pkt_size, count, interval_sec)
        else:
            return self._ping_ipv6(src_port, dst_ip, pkt_size, count, interval_sec)
        
            
         
    # IPv4 ping           
    def _ping_ipv4 (self, src_port, dst_ip, pkt_size, count, interval_sec):
        
        ctx = self.create_service_ctx(port = src_port)
        ping = STLServiceICMP(ctx, dst_ip = dst_ip, pkt_size = pkt_size)
        
        records = []
        
        self.logger.log('')
        for i in range(count):
            ctx.run(ping)
            
            records.append(ping.get_record())
            self.logger.log(ping.get_record())
            
            if i != (count - 1):
                time.sleep(interval_sec)
            
        return records
        
        
    # IPv6 ping 
    def _ping_ipv6 (self, src_port, dst_ip, pkt_size, count, interval_sec):
        
        responses_arr = []
        # no async messages
        with self.logger.supress(level = LoggerApi.VERBOSE_REGULAR_SYNC):
            self.logger.log('')
            dst_mac = None
        
            rc = self.ports[src_port].scan6(dst_ip = dst_ip)
            if not rc:
                raise STLError(rc)
            replies = rc.data()
            if len(replies) == 1:
                dst_mac = replies[0]['mac']
                
            for i in range(count):
                rc = self.ports[src_port].ping_ipv6(ping_ip = dst_ip, pkt_size = pkt_size, dst_mac = dst_mac)
                if not rc:
                    raise STLError(rc)
                    
                responses_arr.append(rc.data())
                self.logger.log(rc.data()['formatted_string'])
                
                if i != (count - 1):
                    time.sleep(interval_sec)

        return responses_arr
        
        
        
    @__api_check(True)
    def server_shutdown (self, force = False):
        """
            Sends the server a request for total shutdown

            :parameters:
                force - shutdown server even if some ports are owned by another
                        user

            :raises:
                + :exc:`STLError`

        """

        self.logger.pre_cmd("Sending shutdown request for the server")

        rc = self._transmit("shutdown", params = {'force': force, 'user': self.username})

        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


    @__api_check(True)
    def get_active_pgids(self):
        """
            Get active packet group IDs

            :Parameters:
                None

            :returns:
                Dict with entries 'latency' and 'flow_stats'. Each entry contains list of used packet group IDs
                of the given type.

            :Raises:
                + :exc:`STLError`

        """
        rc = self._transmit("get_active_pgids")

        if not rc:
            raise STLError(rc)

        return rc.data()["ids"]

    @__api_check(True)
    def get_pgid_stats (self, pgid_list = []):
        """
            .. _get_pgid_stats:

            Get flow statistics for give list of pgids

        :parameters:
            pgid_list: list
                pgids to get statistics on. If empty list, get statistics for all pgids.
                Allows to get statistics for 1024 flows in one call (will return error if asking for more).
        :return:
            Return dictionary containing packet group id statistics information gathered from the server.

            ===============================  ===============
            key                               Meaning
            ===============================  ===============
            :ref:`flow_stats <flow_stats>`   Per flow statistics
            :ref:`latency <latency>`         Per flow statistics regarding flow latency
            ===============================  ===============

            Below is description of each of the inner dictionaries.

            .. _flow_stats:

            **flow_stats** contains :ref:`global dictionary <flow_stats_global>`, and dictionaries per packet group id (pg id). See structures below.

            **per pg_id flow stat** dictionaries have following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            rx_bps              Received bits per second rate
            rx_bps_l1           Received bits per second rate, including layer one
            rx_bytes            Total number of received bytes
            rx_pkts             Total number of received packets
            rx_pps              Received packets per second
            tx_bps              Transmit bits per second rate
            tx_bps_l1           Transmit bits per second rate, including layer one
            tx_bytes            Total number of sent bits
            tx_pkts             Total number of sent packets
            tx_pps              Transmit packets per second rate
            =================   ===============

            .. _flow_stats_global:

            **global flow stats** dictionary has the following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            rx_err              Number of flow statistics packets received that we could not associate to any pg_id. This can happen if latency on the used setup is large. See :ref:`wait_on_traffic <wait_on_traffic>` rx_delay_ms parameter for details.
            tx_err              Number of flow statistics packets transmitted that we could not associate to any pg_id. This is never expected. If you see this different than 0, please report.
            =================   ===============

            .. _latency:

            **latency** contains :ref:`global dictionary <lat_stats_global>`, and dictionaries per packet group id (pg id). Each one with the following structure.

            **per pg_id latency stat** dictionaries have following structure:

            ===========================          ===============
            key                                  Meaning
            ===========================          ===============
            :ref:`err_cntrs<err-cntrs>`          Counters describing errors that occured with this pg id
            :ref:`latency<lat_inner>`            Information regarding packet latency
            ===========================          ===============

            Following are the inner dictionaries of latency

            .. _err-cntrs:

            **err-cntrs**

            =================   ===============
            key                 Meaning (see better explanation below the table)
            =================   ===============
            dropped             How many packets were dropped (estimation)
            dup                 How many packets were duplicated.
            out_of_order        How many packets we received out of order.
            seq_too_high        How many events of packet with sequence number too high we saw.
            seq_too_low         How many events of packet with sequence number too low we saw.
            =================   ===============

            For calculating packet error events, we add sequence number to each packet's payload. We decide what went wrong only according to sequence number
            of last packet received and that of the previous packet. 'seq_too_low' and 'seq_too_high' count events we see. 'dup', 'out_of_order' and 'dropped'
            are heuristics we apply to try and understand what happened. They will be accurate in common error scenarios.
            We describe few scenarios below to help understand this.

            Scenario 1: Received packet with seq num 10, and another one with seq num 10. We increment 'dup' and 'seq_too_low' by 1.

            Scenario 2: Received pacekt with seq num 10 and then packet with seq num 15. We assume 4 packets were dropped, and increment 'dropped' by 4, and 'seq_too_high' by 1.
            We expect next packet to arrive with sequence number 16.

            Scenario 2 continue: Received packet with seq num 11. We increment 'seq_too_low' by 1. We increment 'out_of_order' by 1. We *decrement* 'dropped' by 1.
            (We assume here that one of the packets we considered as dropped before, actually arrived out of order).


            .. _lat_inner:

            **latency**

            =================   ===============
            key                 Meaning
            =================   ===============
            average             Average latency over the stream lifetime (usec).Low pass filter is applied to the last window average.It is computed each sampling period by following formula: <average> = <prev average>/2 + <last sampling period average>/2
            histogram           Dictionary describing logarithmic distribution histogram of packet latencies. Keys in the dictionary represent range of latencies (in usec). Values are the total number of packets received in this latency range. For example, an entry {100:13} would mean that we saw 13 packets with latency in the range between 100 and 200 usec.
            jitter              Jitter of latency samples, computed as described in :rfc:`3550#appendix-A.8`
            last_max            Maximum latency measured between last two data reads from server (0.5 sec window).
            total_max           Maximum latency measured over the stream lifetime (in usec).
            total_min           Minimum latency measured over the stream lifetime (in usec).
            =================   ===============

            .. _lat_stats_global:

            **global latency stats** dictionary has the following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            old_flow            Number of latency statistics packets received that we could not associate to any pg_id. This can happen if latency on the used setup is large. See :ref:`wait_on_traffic <wait_on_traffic>` rx_delay_ms parameter for details.
            bad_hdr             Number of latency packets received with bad latency data. This can happen becuase of garbage packets in the network, or if the DUT causes packet corruption.
            =================   ===============

            :raises:
                + :exc:`STLError`

        """

        # transform single stream
        if not isinstance(pgid_list, list):
            pgid_list = [pgid_list]

        if pgid_list == []:
            active_pgids = self.get_active_pgids()
            pgid_list = active_pgids['latency'] + active_pgids['flow_stats']

        # Should not exceed MAX_ALLOWED_PGID_LIST_LEN from flow_stat.cpp
        max_pgid_in_query = 1024 + 128
        pgid_list_len = len(pgid_list)
        index = 0
        ans_dict = {}

        while index <= pgid_list_len:
            curr_pgid_list = pgid_list[index : index + max_pgid_in_query]
            index += max_pgid_in_query
            rc = self._transmit("get_pgid_stats", params = {'pgids': curr_pgid_list})

            if not rc:
                raise STLError(rc)

            for key in rc.data().keys():
                if key in ans_dict:
                    try:
                        ans_dict[key].update(rc.data()[key])
                    except:
                        pass
                else:
                    ans_dict[key] = rc.data()[key]

        # translation from json values to python API names
        j_to_p_lat = {'jit': 'jitter', 'average':'average', 'total_max': 'total_max', 'last_max':'last_max'}
        j_to_p_err = {'drp':'dropped', 'ooo':'out_of_order', 'dup':'dup', 'sth':'seq_too_high', 'stl':'seq_too_low'}
        j_to_p_global = {'old_flow':'old_flow', 'bad_hdr':'bad_hdr'}
        j_to_p_f_stat = {'rp': 'rx_pkts', 'rb': 'rx_bytes', 'tp': 'tx_pkts', 'tb': 'tx_bytes'
                         , 'rbs': 'rx_bps', 'rps': 'rx_pps', 'tbs': 'tx_bps', 'tps': 'tx_pps'}
        j_to_p_g_f_err = {'rx_err': 'rx_err', 'tx_err': 'tx_err'}

        # translate json 'latency' to python API 'latency'
        new = {}
        if 'ver_id' in ans_dict and ans_dict['ver_id'] is not None:
            new['ver_id'] = ans_dict['ver_id']
        else:
            new['ver_id'] = {}

        if 'latency' in ans_dict.keys() and ans_dict['latency'] is not None:
            new['latency'] = {}
            new['latency']['global'] = {}
            for key in j_to_p_global.keys():
                new['latency']['global'][j_to_p_global[key]] = 0
            for pg_id in ans_dict['latency']:
                # 'g' value is not a number
                try:
                    int_pg_id = int(pg_id)
                except:
                    continue
                new['latency'][int_pg_id] = {}
                new['latency'][int_pg_id]['err_cntrs'] = {}
                if 'er' in ans_dict['latency'][pg_id]:
                    for key in j_to_p_err.keys():
                        if key in ans_dict['latency'][pg_id]['er']:
                            new['latency'][int_pg_id]['err_cntrs'][j_to_p_err[key]] = ans_dict['latency'][pg_id]['er'][key]
                        else:
                            new['latency'][int_pg_id]['err_cntrs'][j_to_p_err[key]] = 0
                else:
                    for key in j_to_p_err.keys():
                        new['latency'][int_pg_id]['err_cntrs'][j_to_p_err[key]] = 0

                new['latency'][int_pg_id]['latency'] = {}
                for field in j_to_p_lat.keys():
                    if field in ans_dict['latency'][pg_id]['lat']:
                        new['latency'][int_pg_id]['latency'][j_to_p_lat[field]] = ans_dict['latency'][pg_id]['lat'][field]
                    else:
                        new['latency'][int_pg_id]['latency'][j_to_p_lat[field]] = StatNotAvailable(field)

                if 'histogram' in ans_dict['latency'][pg_id]['lat']:
                    #translate histogram numbers from string to integers
                    new['latency'][int_pg_id]['latency']['histogram'] = {
                                        int(elem): ans_dict['latency'][pg_id]['lat']['histogram'][elem]
                                         for elem in ans_dict['latency'][pg_id]['lat']['histogram']
                    }
                    min_val = min(new['latency'][int_pg_id]['latency']['histogram'])
                    if min_val == 0:
                        min_val = 2
                    new['latency'][int_pg_id]['latency']['total_min'] = min_val
                else:
                    new['latency'][int_pg_id]['latency']['total_min'] = StatNotAvailable('total_min')
                    new['latency'][int_pg_id]['latency']['histogram'] = {}

        # translate json 'flow_stats' to python API 'flow_stats'
        if 'flow_stats' in ans_dict.keys() and ans_dict['flow_stats'] is not None:
            new['flow_stats'] = {}
            new['flow_stats']['global'] = {}

            all_ports = []
            for pg_id in ans_dict['flow_stats']:
                # 'g' value is not a number
                try:
                    int_pg_id = int(pg_id)
                except:
                    continue

                # do this only once
                if all_ports == []:
                    # if field does not exist, we don't know which ports we have. We assume 'tp' will always exist
                    for port in ans_dict['flow_stats'][pg_id]['tp']:
                        all_ports.append(int(port))

                new['flow_stats'][int_pg_id] = {}
                for field in j_to_p_f_stat.keys():
                    new['flow_stats'][int_pg_id][j_to_p_f_stat[field]] = {}
                    #translate ports to integers
                    total = 0
                    if field in ans_dict['flow_stats'][pg_id]:
                        for port in ans_dict['flow_stats'][pg_id][field]:
                            new['flow_stats'][int_pg_id][j_to_p_f_stat[field]][int(port)] = ans_dict['flow_stats'][pg_id][field][port]
                            total += new['flow_stats'][int_pg_id][j_to_p_f_stat[field]][int(port)]
                        new['flow_stats'][int_pg_id][j_to_p_f_stat[field]]['total'] = total
                    else:
                        for port in all_ports:
                            new['flow_stats'][int_pg_id][j_to_p_f_stat[field]][int(port)] = StatNotAvailable(j_to_p_f_stat[field])
                        new['flow_stats'][int_pg_id][j_to_p_f_stat[field]]['total'] = StatNotAvailable('total')
                new['flow_stats'][int_pg_id]['rx_bps_l1'] = {}
                new['flow_stats'][int_pg_id]['tx_bps_l1'] = {}
                for port in new['flow_stats'][int_pg_id]['rx_pkts']:
                    # L1 overhead is 20 bytes per packet
                    new['flow_stats'][int_pg_id]['rx_bps_l1'][port] = float(new['flow_stats'][int_pg_id]['rx_bps'][port]) + float(new['flow_stats'][int_pg_id]['rx_pps'][port]) * 20 * 8
                    new['flow_stats'][int_pg_id]['tx_bps_l1'][port] = float(new['flow_stats'][int_pg_id]['tx_bps'][port]) + float(new['flow_stats'][int_pg_id]['tx_pps'][port]) * 20 * 8

            if 'g' in ans_dict['flow_stats']:
                for field in j_to_p_g_f_err.keys():
                    if field in ans_dict['flow_stats']['g']:
                        new['flow_stats']['global'][j_to_p_g_f_err[field]] = ans_dict['flow_stats']['g'][field]
                    else:
                        new['flow_stats']['global'][j_to_p_g_f_err[field]] = {}
                        for port in all_ports:
                            new['flow_stats']['global'][j_to_p_g_f_err[field]][int(port)] = 0
            else:
                for field in j_to_p_g_f_err.keys():
                    new['flow_stats']['global'][j_to_p_g_f_err[field]] = {}
                    for port in all_ports:
                        new['flow_stats']['global'][j_to_p_g_f_err[field]][int(port)] = 0


        self.pgid_stats.save_stats(new)

        return self.pgid_stats.get_stats()

    @__api_check(True)
    def get_util_stats(self):
        """
            Get utilization stats:
            History of TRex CPU utilization per thread (list of lists)
            MBUFs memory consumption per CPU socket.

            :parameters:
                None

            :raises:
                + :exc:`STLError`

        """
        self.logger.pre_cmd('Getting Utilization stats')
        return self.util_stats.get_stats()

    @__api_check(True)
    def get_xstats(self, port_id):
        """
            Get extended stats of port: all the counters as dict.

            :parameters:
                port_id: int

            :returns:
                Dict with names of counters as keys and values of uint64. Actual keys may vary per NIC.

            :raises:
                + :exc:`STLError`

        """
        self.logger.pre_cmd('Getting xstats')
        return self.xstats.get_stats(port_id)


    @__api_check(True)
    def reset(self, ports = None, restart = False):
        """
            Force acquire ports, stop the traffic, remove all streams and clear stats

            :parameters:
                ports : list
                   Ports on which to execute the command

                restart: bool
                   Restart the NICs (link down / up)
                   
            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        
        if restart:
            self.logger.pre_cmd("Hard resetting ports {0}:".format(ports))
        else:
            self.logger.pre_cmd("Resetting ports {0}:".format(ports))
        
        
        try:
            with self.logger.supress():
            # force take the port and ignore any streams on it
                self.acquire(ports, force = True, sync_streams = False)
                self.stop(ports, rx_delay_ms = 0)
                self.remove_all_streams(ports)
                self.clear_stats(ports)
                self.set_port_attr(ports,
                                   promiscuous = False,
                                   link_up = True if restart else None)
                self.remove_rx_queue(ports)
                self.set_service_mode(ports, False)
                
                
        except STLError as e:
            self.logger.post_cmd(False)
            raise e
                

        self.logger.post_cmd(RC_OK())
        

    @__api_check(True)
    def remove_all_streams (self, ports = None):
        """
            remove all streams from port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command


            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        self.logger.pre_cmd("Removing all streams from port(s) {0}:".format(ports))
        rc = self.__remove_all_streams(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

 
    @__api_check(True)
    def add_streams (self, streams, ports = None):
        """
            Add a list of streams to port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command
                streams: list
                    Streams to attach (or profile)

            :returns:
                List of stream IDs in order of the stream list

            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        if isinstance(streams, STLProfile):
            streams = streams.get_streams()

        # transform single stream
        if not isinstance(streams, list):
            streams = [streams]

        # check streams
        if not all([isinstance(stream, STLStream) for stream in streams]):
            raise STLArgumentError('streams', streams)

        self.logger.pre_cmd("Attaching {0} streams to port(s) {1}:".format(len(streams), ports))
        rc = self.__add_streams(streams, ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        # return the stream IDs
        return rc.data()

    @__api_check(True)
    def add_profile(self, filename, ports = None, **kwargs):
        """ |  Add streams from profile by its type. Supported types are:
            |  .py
            |  .yaml
            |  .pcap file that converted to profile automatically

            :parameters:
                filename : string
                    filename (with path) of the profile
                ports : list
                    list of ports to add the profile (default: all acquired)
                kwargs : dict
                    forward those key-value pairs to the profile (tunables)

            :returns:
                List of stream IDs in order of the stream list

            :raises:
                + :exc:`STLError`

        """

        validate_type('filename', filename, basestring)
        profile = STLProfile.load(filename, **kwargs)
        return self.add_streams(profile.get_streams(), ports)


    @__api_check(True)
    def remove_streams (self, stream_id_list, ports = None):
        """
            Remove a list of streams from ports

            :parameters:
                ports : list
                    Ports on which to execute the command
                stream_id_list: list
                    Stream id list to remove


            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # transform single stream
        if not isinstance(stream_id_list, list):
            stream_id_list = [stream_id_list]

        # check streams
        for stream_id in stream_id_list:
            validate_type('stream_id', stream_id, int)

        # remove streams
        self.logger.pre_cmd("Removing {0} streams from port(s) {1}:".format(len(stream_id_list), ports))
        rc = self.__remove_streams(stream_id_list, ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


    # common checks for start API
    def __pre_start_check (self, cmd_name, ports, force):
        if force:
            return self.psv.validate(cmd_name, ports)
            
        states = {PSV_UP:           "check the connection or specify 'force'",
                  PSV_IDLE:         "please stop them or specify 'force'",
                  PSV_RESOLVED:     "please resolve them or specify 'force'",
                  PSV_NON_SERVICE:  "please disable service mode or specify 'force'"}
        
        return self.psv.validate(cmd_name, ports, states)     
            
        
    @__api_check(True)
    def start (self,
               ports = None,
               mult = "1",
               force = False,
               duration = -1,
               total = False,
               core_mask = None,
               synchronized = False):
        """
            Start traffic on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

                mult : str
                    Multiplier in a form of pps, bps, or line util in %
                    Examples: "5kpps", "10gbps", "85%", "32mbps"

                force : bool
                    If the ports are not in stopped mode or do not have sufficient bandwidth for the traffic, determines whether to stop the current traffic and force start.
                    True: Force start
                    False: Do not force start

                duration : int
                    Limit the run time (seconds)
                    -1 = unlimited

                total : bool
                    Determines whether to divide the configured bandwidth among the ports, or to duplicate the bandwidth for each port.
                    True: Divide bandwidth among the ports
                    False: Duplicate

                core_mask: CORE_MASK_SPLIT, CORE_MASK_PIN, CORE_MASK_SINGLE or a list of masks (one per port)
                    Determine the allocation of cores per port
                    In CORE_MASK_SPLIT all the traffic will be divided equally between all the cores
                    associated with each port
                    In CORE_MASK_PIN, for each dual ports (a group that shares the same cores)
                    the cores will be divided half pinned for each port

                synchronized: bool
                    In case of several ports, ensure their transmitting time is syncronized.
                    Must use adjacent ports (belong to same set of cores).
                    Will set default core_mask to 0x1.
                    Recommended ipg 1ms and more.

            :raises:
                + :exc:`STLError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.__pre_start_check('START', ports, force)

        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('duration', duration, (int, float))
        validate_type('total', total, bool)
        validate_type('core_mask', core_mask, (type(None), int, list))

        
        #########################
        # decode core mask argument
        if core_mask is None:
            core_mask = self.CORE_MASK_SINGLE if synchronized else self.CORE_MASK_SPLIT
        decoded_mask = self.__decode_core_mask(ports, core_mask)
        #######################

        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = False,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)


        # stop active ports if needed
        active_ports = list(set(self.get_active_ports()).intersection(ports))
        if active_ports and force:
            self.stop(active_ports)

        if synchronized:
            # start synchronized (per pair of ports) traffic
            if len(ports) % 2:
                raise STLError('Must use even number of ports in synchronized mode')
            for port in ports:
                if port ^ 1 not in ports:
                    raise STLError('Must use adjacent ports in synchronized mode. Port "%s" has not pair.' % port)

            start_time = time.time()
            with self.logger.supress():
                ping_data = self.ping_rpc_server()
            start_at_ts = ping_data['ts'] + max((time.time() - start_time), 0.5) * len(ports)
            synchronized_str = 'synchronized '
        else:
            start_at_ts = None
            synchronized_str = ''

        # start traffic
        self.logger.pre_cmd("Starting {}traffic on port(s) {}:".format(synchronized_str, ports))
        rc = self.__start(mult_obj, duration, ports, force, decoded_mask, start_at_ts)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        return rc
        

    @__api_check(True)
    def stop (self, ports = None, rx_delay_ms = None):
        """
            Stop port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

                rx_delay_ms : int
                    time to wait until RX filters are removed
                    this value should reflect the time it takes
                    packets which were transmitted to arrive
                    to the destination.
                    after this time the RX filters will be removed

            :raises:
                + :exc:`STLError`

        """

        if ports is None:
            ports = self.get_active_ports()
            if not ports:
                return

        ports = self.psv.validate('STOP', ports, PSV_ACQUIRED)
        
        self.logger.pre_cmd("Stopping traffic on port(s) {0}:".format(ports))
        rc = self.__stop(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        if rx_delay_ms is None:
            if self.ports[ports[0]].is_virtual(): # assume all ports have same type
                rx_delay_ms = 100
            else:
                rx_delay_ms = 10

        # remove any RX filters
        rc = self._remove_rx_filters(ports, rx_delay_ms = rx_delay_ms)
        if not rc:
            raise STLError(rc)

        
    @__api_check(True)
    def update (self, ports = None, mult = "1", total = False, force = False):
        """
            Update traffic on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

                mult : str
                    Multiplier in a form of pps, bps, or line util in %
                    Can also specify +/-
                    Examples: "5kpps+", "10gbps-", "85%", "32mbps", "20%+"

                force : bool
                    If the ports are not in stopped mode or do not have sufficient bandwidth for the traffic, determines whether to stop the current traffic and force start.
                    True: Force start
                    False: Do not force start

                total : bool
                    Determines whether to divide the configured bandwidth among the ports, or to duplicate the bandwidth for each port.
                    True: Divide bandwidth among the ports
                    False: Duplicate


            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_active_ports()
        ports = self._validate_port_list(ports)

        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('total', total, bool)

        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = True,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)


        # call low level functions
        self.logger.pre_cmd("Updating traffic on port(s) {0}:".format(ports))
        rc = self.__update(mult_obj, ports, force)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)



    @__api_check(True)
    def pause (self, ports = None):
        """
            Pause traffic on port(s). Works only for ports that are active, and only if all streams are in Continuous mode.

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_transmitting_ports()
        ports = self._validate_port_list(ports)

        self.logger.pre_cmd("Pausing traffic on port(s) {0}:".format(ports))
        rc = self.__pause(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

    @__api_check(True)
    def resume (self, ports = None):
        """
            Resume traffic on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_paused_ports()
        ports = self._validate_port_list(ports)


        self.logger.pre_cmd("Resume traffic on port(s) {0}:".format(ports))
        rc = self.__resume(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


    @__api_check(True)
    def push_remote (self,
                     pcap_filename,
                     ports = None,
                     ipg_usec = None,
                     speedup = 1.0,
                     count = 1,
                     duration = -1,
                     is_dual = False,
                     min_ipg_usec = None,
                     force  = False):
        """
            Push a remote server-reachable PCAP file
            the path must be fullpath accessible to the server

            :parameters:
                pcap_filename : str
                    PCAP file name in full path and accessible to the server

                ports : list
                    Ports on which to execute the command

                ipg_usec : float
                    Inter-packet gap in microseconds.
                    Exclusive with min_ipg_usec

                speedup : float
                    A factor to adjust IPG. effectively IPG = IPG / speedup

                count: int
                    How many times to transmit the cap

                duration: float
                    Limit runtime by duration in seconds
                    
                is_dual: bool
                    Inject from both directions.
                    requires ERF file with meta data for direction.
                    also requires that all the ports will be in master mode
                    with their adjacent ports as slaves

                min_ipg_usec : float
                    Minimum inter-packet gap in microseconds to guard from too small ipg.
                    Exclusive with ipg_usec

                force : bool
                    Ignore if port is active
 
                
            :raises:
                + :exc:`STLError`

        """
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.__pre_start_check('PUSH', ports, force)

        validate_type('pcap_filename', pcap_filename, basestring)
        validate_type('ipg_usec', ipg_usec, (float, int, type(None)))
        validate_type('speedup',  speedup, (float, int))
        validate_type('count',  count, int)
        validate_type('duration', duration, (float, int))
        validate_type('is_dual', is_dual, bool)
        validate_type('min_ipg_usec', min_ipg_usec, (float, int, type(None)))

        # if force - stop any active ports
        if force:
            active_ports = list(set(self.get_active_ports()).intersection(ports))
            if active_ports:
                self.stop(active_ports)
                
        # for dual mode check that all are masters
        if is_dual:
            if not pcap_filename.endswith('erf'):
                raise STLError("dual mode: only ERF format is supported for dual mode")

            for port in ports:
                master = port
                slave = port ^ 0x1

                if slave in ports:
                    raise STLError("dual mode: cannot provide adjacent ports ({0}, {1}) in a batch".format(master, slave))

                if not slave in self.get_acquired_ports():
                    raise STLError("dual mode: adjacent port {0} must be owned during dual mode".format(slave))


        self.logger.pre_cmd("Pushing remote PCAP on port(s) {0}:".format(ports))
        rc = self.__push_remote(pcap_filename, ports, ipg_usec, speedup, count, duration, is_dual, min_ipg_usec)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


    @__api_check(True)
    def push_pcap (self,
                   pcap_filename,
                   ports = None,
                   ipg_usec = None,
                   speedup = 1.0,
                   count = 1,
                   duration = -1,
                   force = False,
                   vm = None,
                   packet_hook = None,
                   is_dual = False,
                   min_ipg_usec = None):
        """
            Push a local PCAP to the server
            This is equivalent to loading a PCAP file to a profile
            and attaching the profile to port(s)
            
            file size is limited to 1MB

            :parameters:
                pcap_filename : str
                    PCAP filename (accessible locally)

                ports : list
                    Ports on which to execute the command

                ipg_usec : float
                    Inter-packet gap in microseconds.
                    Exclusive with min_ipg_usec

                speedup : float
                    A factor to adjust IPG. effectively IPG = IPG / speedup

                count: int
                    How many times to transmit the cap

                duration: float
                    Limit runtime by duration in seconds

                force: bool
                    Ignore file size limit - push any file size to the server
                    also ignore if port is active
                    
                vm: list of VM instructions
                    VM instructions to apply for every packet

                packet_hook : Callable or function
                    Will be applied to every packet

                is_dual: bool
                    Inject from both directions.
                    requires ERF file with meta data for direction.
                    also requires that all the ports will be in master mode
                    with their adjacent ports as slaves

                min_ipg_usec : float
                    Minimum inter-packet gap in microseconds to guard from too small ipg.
                    Exclusive with ipg_usec

            :raises:
                + :exc:`STLError`

        """
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.__pre_start_check('PUSH', ports, force)
        
        validate_type('pcap_filename', pcap_filename, basestring)
        validate_type('ipg_usec', ipg_usec, (float, int, type(None)))
        validate_type('speedup',  speedup, (float, int))
        validate_type('count',  count, int)
        validate_type('duration', duration, (float, int))
        validate_type('vm', vm, (list, type(None)))
        validate_type('is_dual', is_dual, bool)
        validate_type('min_ipg_usec', min_ipg_usec, (float, int, type(None)))
        if all([ipg_usec, min_ipg_usec]):
            raise STLError('Please specify either ipg or minimal ipg, not both.')

        # if force - stop any active ports
        if force:
            active_ports = list(set(self.get_active_ports()).intersection(ports))
            if active_ports:
                self.stop(active_ports)


        # no support for > 1MB PCAP - use push remote
        if not force and os.path.getsize(pcap_filename) > (1024 * 1024):
            raise STLError("PCAP size of {:} is too big for local push - consider using remote push or provide 'force'".format(format_num(os.path.getsize(pcap_filename), suffix = 'B')))

        if is_dual:
            for port in ports:
                master = port
                slave = port ^ 0x1

                if slave in ports:
                    raise STLError("dual mode: please specify only one of adjacent ports ({0}, {1}) in a batch".format(master, slave))

                if not slave in self.get_acquired_ports():
                    raise STLError("dual mode: adjacent port {0} must be owned during dual mode".format(slave))

        # regular push
        if not is_dual:

            # create the profile from the PCAP
            try:
                self.logger.pre_cmd("Converting '{0}' to streams:".format(pcap_filename))
                profile = STLProfile.load_pcap(pcap_filename,
                                               ipg_usec,
                                               speedup,
                                               count,
                                               vm = vm,
                                               packet_hook = packet_hook,
                                               min_ipg_usec = min_ipg_usec)
                self.logger.post_cmd(RC_OK())
            except STLError as e:
                self.logger.post_cmd(RC_ERR(e))
                raise


            self.remove_all_streams(ports = ports)
            id_list = self.add_streams(profile.get_streams(), ports)
            
            return self.start(ports = ports, duration = duration, force = force)

        else:

            # create a dual profile
            split_mode = 'MAC'
            if (ipg_usec and ipg_usec < 1000 * speedup) or (min_ipg_usec and min_ipg_usec < 1000):
                self.event_handler.log_warning('In order to get synchronized traffic, ensure that effective ipg is at least 1000 usec ')
            
            try:
                self.logger.pre_cmd("Analyzing '{0}' for dual ports based on {1}:".format(pcap_filename, split_mode))
                profile_a, profile_b = STLProfile.load_pcap(pcap_filename,
                                                            ipg_usec,
                                                            speedup,
                                                            count,
                                                            vm = vm,
                                                            packet_hook = packet_hook,
                                                            split_mode = split_mode,
                                                            min_ipg_usec = min_ipg_usec)

                self.logger.post_cmd(RC_OK())

            except STLError as e:
                self.logger.post_cmd(RC_ERR(e))
                raise

            all_ports = ports + [p ^ 0x1 for p in ports if profile_b]

            self.remove_all_streams(ports = all_ports)

            for port in ports:
                master = port
                slave = port ^ 0x1

                self.add_streams(profile_a.get_streams(), master)
                if profile_b:
                    self.add_streams(profile_b.get_streams(), slave)

            return self.start(ports = all_ports, duration = duration, force = force, synchronized = True)




    @__api_check(True)
    def push_packets (self,
                      pkts,
                      ports = None,
                      ipg_usec = 100,
                      count = 1,
                      duration = -1,
                      force = False,
                      vm = None):
        
        """
            Pushes a list of packets to the server
            a 'packet' can be anything with a bytes representation
            such as Scapy object, a simple string, a byte array and etc.
            
            Total size, as for PCAP pushing is limited to 1MB
            unless 'force' is specified
            
            the list of packets will be saved to a temporary file
            which will be deleted when the function exists
            
            :parameters:
                pkts : Scapy pkt or a list of scapy pkts

                ports : list
                    Ports on which to execute the command

                ipg_usec : float
                    Inter-packet gap in microseconds.

                count: int
                    How many times to transmit the list

                duration: float
                    Limit runtime by duration in seconds

                force: bool
                    Ignore size limit - push any size to the server

                vm: list of VM instructions
                    VM instructions to apply for every packet

            :raises:
                + :exc:`STLError`
        """
        
        # validate ports
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.__pre_start_check('PUSH', ports, force)
        
        validate_type('count',  count, int)
        validate_type('duration', duration, (float, int))
        validate_type('vm', vm, (list, type(None)))
        
        # if force - stop any active ports
        if force:
            active_ports = list(set(self.get_active_ports()).intersection(ports))
            if active_ports:
                self.stop(active_ports)
                
        # pkts should be scapy, bytes, str or a list of them
        pkts = listify(pkts)
        for pkt in pkts:
            if not isinstance(pkt, (Ether, bytes)):
                raise STLTypeError('pkts', type(pkt), (Ether, bytes))
        
        # IPG
        validate_type('ipg_usec', ipg_usec, (float, int, type(None)))
        if ipg_usec < 0:
            raise STLError("'ipg_usec' should not be negative")
        
        # init the stream list
        streams = []
        
        for i, pkt in enumerate(pkts, start = 1):
            
            # handle last packet
            
            if i == len(pkts):
                next = 1
                action_count = count
            else:
                next = i + 1
                action_count = 0

            # is the packet Scapy or a simple buffer ?
            packet = STLPktBuilder(pkt = pkt, vm = vm) if isinstance(pkt, Ether) else STLPktBuilder(pkt_buffer = pkt, vm = vm)
            
            # add the stream
            streams.append(STLStream(name = i,
                                     packet = packet,
                                     mode = STLTXSingleBurst(total_pkts = 1, percentage = 100),
                                     self_start = True if (i == 1) else False,
                                     isg = ipg_usec,  # usec
                                     action_count = action_count,
                                     next = next))
        
        
        # remove all streams, attach the created list and start
        self.remove_all_streams(ports = ports)
        id_list = self.add_streams(streams, ports)
            
        return self.start(ports = ports, duration = duration, force = force)

    

    @__api_check(True)
    def validate (self, ports = None, mult = "1", duration = -1, total = False):
        """
            Validate port(s) configuration

            :parameters:
                ports : list
                    Ports on which to execute the command

             mult : str
                    Multiplier in a form of pps, bps, or line util in %
                    Examples: "5kpps", "10gbps", "85%", "32mbps"

            duration : int
                    Limit the run time (seconds)
                    -1 = unlimited

            total : bool
                    Determines whether to divide the configured bandwidth among the ports, or to duplicate the bandwidth for each port.
                    True: Divide bandwidth among the ports
                    False: Duplicate

            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        validate_type('mult', mult, basestring)
        validate_type('duration', duration, (int, float))
        validate_type('total', total, bool)


        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = True,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)

        self.logger.pre_cmd("Validating streams on port(s) {0}:".format(ports))
        rc = self.__validate(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        for port in ports:
            self.ports[port].print_profile(mult_obj, duration)


    @__api_check(False)
    def clear_stats (self, ports = None, clear_global = True, clear_flow_stats = True, clear_latency_stats = True, clear_xstats = True, clear_pgid_stats = True):
        """
            Clear stats on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

                clear_global : bool
                    Clear the global stats

                clear_flow_stats : bool 
                    Clear the flow stats

                clear_latency_stats : bool 
                    Clear the latency stats

                clear_xstats : bool 
                    Clear the extended stats

            :raises:
                + :exc:`STLError`

        """

        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        # verify clear global
        if not type(clear_global) is bool:
            raise STLArgumentError('clear_global', clear_global)

        rc = self.__clear_stats(ports, clear_global, clear_flow_stats, clear_latency_stats, clear_xstats, clear_pgid_stats)
        if not rc:
            raise STLError(rc)


  
    @__api_check(True)
    def is_traffic_active (self, ports = None):
        """
            Return if specified port(s) have traffic 

            :parameters:
                ports : list
                    Ports on which to execute the command


            :raises:
                + :exc:`STLTimeoutError` - in case timeout has expired
                + :exe:'STLError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        return set(self.get_active_ports()).intersection(ports)



    @__api_check(True)
    def wait_on_traffic (self, ports = None, timeout = None, rx_delay_ms = None):
        """
             .. _wait_on_traffic:

            Block until traffic on specified port(s) has ended

            :parameters:
                ports : list
                    Ports on which to execute the command

                timeout : int
                    timeout in seconds
                    default will be blocking

                rx_delay_ms : int
                    Time to wait (in milliseconds) after last packet was sent, until RX filters used for
                    measuring flow statistics and latency are removed.
                    This value should reflect the time it takes packets which were transmitted to arrive
                    to the destination.
                    After this time, RX filters will be removed, and packets arriving for per flow statistics feature and latency flows will be counted as errors.

            :raises:
                + :exc:`STLTimeoutError` - in case timeout has expired
                + :exe:'STLError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)


        timer = PassiveTimer(timeout)

        # wait while any of the required ports are active
        while set(self.get_active_ports()).intersection(ports):

            time.sleep(0.01)
            if timer.has_expired():
                raise STLTimeoutError(timeout)

        if rx_delay_ms is None:
            if self.ports[ports[0]].is_virtual(): # assume all ports have same type
                rx_delay_ms = 100
            else:
                rx_delay_ms = 10

        # remove any RX filters
        rc = self._remove_rx_filters(ports, rx_delay_ms = rx_delay_ms)
        if not rc:
            raise STLError(rc)


    @__api_check(True)
    def set_port_attr (self, 
                       ports = None,
                       promiscuous = None,
                       link_up = None,
                       led_on = None,
                       flow_ctrl = None,
                       resolve = True,
                       multicast = None):
        """
            Set port attributes

            :parameters:
                promiscuous      - True or False
                link_up          - True or False
                led_on           - True or False
                flow_ctrl        - 0: disable all, 1: enable tx side, 2: enable rx side, 3: full enable
                resolve          - if true, in case a destination address is configured as IPv4 try to resolve it
                multicast        - enable receiving multicast, True or False
            :raises:
                + :exe:'STLError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # check arguments
        validate_type('promiscuous', promiscuous, (bool, type(None)))
        validate_type('link_up', link_up, (bool, type(None)))
        validate_type('led_on', led_on, (bool, type(None)))
        validate_type('flow_ctrl', flow_ctrl, (int, type(None)))
        validate_type('multicast', multicast, (bool, type(None)))
    
        # common attributes for all ports
        cmn_attr_dict = {}

        cmn_attr_dict['promiscuous']     = promiscuous
        cmn_attr_dict['link_status']     = link_up
        cmn_attr_dict['led_status']      = led_on
        cmn_attr_dict['flow_ctrl_mode']  = flow_ctrl
        cmn_attr_dict['multicast']       = multicast
        
        # each port starts with a set of the common attributes
        attr_dict = [dict(cmn_attr_dict) for _ in ports]
    
        self.logger.pre_cmd("Applying attributes on port(s) {0}:".format(ports))
        rc = self.__set_port_attr(ports, attr_dict)
        self.logger.post_cmd(rc)
            
        if not rc:
            raise STLError(rc)

      
            
    @__api_check(True)
    def get_port_attr (self, port):
        """
            get the port attributes currently set
            
            :parameters:
                port - for which port to return port attributes
           
                     
            :raises:
                + :exe:'STLError'
                
        """
        validate_type('port', port, int)
        if port not in self.get_all_ports():
            raise STLError("'{0}' is not a valid port id".format(port))
            
        return self.ports[port].get_formatted_info()
            
    
        
    @__api_check(True)
    def set_service_mode (self, ports = None, enabled = True):
        """
            Set service mode for port(s)
            In service mode ports will respond to ARP, PING and etc.

            :parameters:
                ports          - for which ports to configure service mode on/off
                enabled        - True for activating service mode, False for disabling
            :raises:
                + :exe:'STLError'

        """
        # by default take all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)
        
        if enabled:
            self.logger.pre_cmd('Enabling service mode on port(s) {0}:'.format(ports))
        else:
            self.logger.pre_cmd('Disabling service mode on port(s) {0}:'.format(ports))
            
        rc = self.__set_service_mode(ports, enabled)
        self.logger.post_cmd(rc)
        
        if not rc:
            raise STLError(rc)
          
              
    @contextmanager
    def service_mode (self, ports):
        self.set_service_mode(ports = ports)
        try:
            yield
        finally:
            self.set_service_mode(ports = ports, enabled = False)
        
        
    @__api_check(True)
    def resolve (self, ports = None, retries = 0, verbose = True):
        """
            Resolves ports (ARP resolution)

            :parameters:
                ports          - which ports to resolve
                retries        - how many times to retry on each port (intervals of 100 milliseconds)
                verbose        - log for each request the response
            :raises:
                + :exe:'STLError'

        """
        # by default - resolve all the ports that are configured with IPv4 dest
        ports = ports if ports is not None else self.get_resolvable_ports()
        ports = self.psv.validate('ARP', ports, (PSV_ACQUIRED, PSV_SERVICE, PSV_L3))
        
        self.logger.pre_cmd('Resolving destination on port(s) {0}:'.format(ports))
        
        # generate the context
        arps = []
        for port in ports:

            self.ports[port].invalidate_arp()
            
            src_ipv4 = self.ports[port].get_layer_cfg()['ipv4']['src']
            dst_ipv4 = self.ports[port].get_layer_cfg()['ipv4']['dst']
            
            ctx = self.create_service_ctx(port)
            
            # retries
            for i in range(retries + 1):
                arp = STLServiceARP(ctx, dst_ip = dst_ipv4, src_ip = src_ipv4)
                ctx.run(arp)
                if arp.get_record():
                    self.ports[port].set_l3_mode(src_ipv4, dst_ipv4, arp.get_record().dst_mac)
                    break
            
            arps.append(arp)
            
        
        self.logger.post_cmd(all([arp.get_record() for arp in arps]))
        
        for port, arp in zip(ports, arps):
            if arp.get_record():
                self.logger.log(format_text("Port {0} - {1}".format(port, arp.get_record()), 'bold'))
            else:
                self.logger.log(format_text("Port {0} - *** {1}".format(port, arp.get_record()), 'bold'))
        
     

    # alias
    arp = resolve


    @__api_check(True)
    def scan6(self, ports = None, timeout = 5, verbose = False):
        """
            Search for IPv6 devices on ports

            :parameters:
                ports          - for which ports to apply a unique sniffer (each port gets a unique file)
                timeout        - how much time to wait for responses
                verbose        - log for each request the response
            :return:
                list of dictionaries per neighbor:

                    * type   - type of device: 'Router' or 'Host'
                    * mac    - MAC address of device
                    * ipv6   - IPv6 address of device
            :raises:
                + :exe:'STLError'

        """

        if ports is None:
            ports = self.get_acquired_ports()
        else:
            ports = self._validate_port_list(ports)
            not_acquired = list_difference(ports, self.get_acquired_ports())
            if not_acquired:
                raise STLError('Following ports are not acquired: %s' % ', '.join(map(str, not_acquired)))

        not_in_service = list_difference(ports, self.get_service_enabled_ports())
        if not_in_service:
            raise STLError('Following ports are not in service mode: %s' % ', '.join(map(str, not_in_service)))

        self.logger.pre_cmd('Scanning network for IPv6 nodes on port(s) {0}:'.format(ports))

        with self.logger.supress(level = LoggerApi.VERBOSE_REGULAR_SYNC):
            rc_per_port = self.__scan6(ports, timeout)

        self.logger.post_cmd(rc_per_port)

        err = RC()
        replies_per_port = {}
        for port, rc in rc_per_port.items():
            if not rc:
                err.add(rc)
            else:
                replies_per_port[port] = rc.data()

        if err.rc_list:
            raise STLError(err)

        if verbose:
            for port, replies in replies_per_port.items():
                if replies:
                    max_ip_len = 0
                    for resp in replies:
                        max_ip_len = max(max_ip_len, len(resp['ipv6']))
                    scan_table = TRexTextTable()
                    scan_table.set_cols_align(['c', 'c', 'l'])
                    scan_table.header(['Device', 'MAC', 'IPv6 address'])
                    scan_table.set_cols_width([9, 19, max_ip_len + 2])

                    resp = 'Port %s - IPv6 search result:' % port
                    self.logger.log(format_text(resp, 'bold'))
                    node_types = defaultdict(list)
                    for reply in replies:
                        node_types[reply['type']].append(reply)
                    for key in sorted(node_types.keys()):
                        for reply in node_types[key]:
                            scan_table.add_row([key, reply['mac'], reply['ipv6']])
                    self.logger.log(scan_table.draw())
                    self.logger.log('')
                else:
                    self.logger.log(format_text('Port %s: no replies! Try to ping with explicit address.' % port, 'bold'))

        return replies_per_port


    @__api_check(True)
    def start_capture (self, tx_ports = None, rx_ports = None, limit = 1000, mode = 'fixed'):
        """
            Starts a low rate packet capturing on the server

            :parameters:
                tx_ports: list
                    on which ports to capture TX
                    
                rx_ports: list
                    on which ports to capture RX
                    
                limit: int
                    limit how many packets will be written memory requierment is O(9K * limit)
                    
                mode: str
                    'fixed'  - when full, newer packets will be dropped
                    'cyclic' - when full, older packets will be dropped
                                  
            :returns:
                returns a dictionary:
                {'id: <new_id>, 'ts': <starting timestamp>}
                
                where 'id' is the new capture ID for future commands
                and 'ts' is that server monotonic timestamp when
                the capture was created
                
            :raises:
                + :exe:'STLError'

        """

        # default values for TX / RX ports
        tx_ports = tx_ports if tx_ports is not None else []
        rx_ports = rx_ports if rx_ports is not None else []
        
        # check arguments
        tx_ports = self._validate_port_list(tx_ports, allow_empty = True)
        rx_ports = self._validate_port_list(rx_ports, allow_empty = True)
        merge_ports = set(tx_ports + rx_ports)
        
        # make sure at least one port to capture
        if not merge_ports:
            raise STLError("start_capture - must get at least one port to capture")
            
        validate_type('limit', limit, (int))
        if limit <= 0:
            raise STLError("'limit' must be a positive value")

        if mode not in ('fixed', 'cyclic'):
            raise STLError("'mode' must be either 'fixed' or 'cyclic'")
        
        # verify service mode
        non_service_ports =  list_difference(merge_ports, self.get_service_enabled_ports())
        if non_service_ports:
            raise STLError("Port(s) {0} are not under service mode. packet capturing requires all ports to be in service mode".format(non_service_ports))
        
            
        # actual job
        self.logger.pre_cmd("Starting packet capturing up to {0} packets".format(limit))
        rc = self._transmit("capture", params = {'command': 'start', 'limit': limit, 'mode': mode, 'tx': tx_ports, 'rx': rx_ports})
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        return {'id': rc.data()['capture_id'], 'ts': rc.data()['start_ts']}


        
    @__api_check(True)
    def stop_capture (self, capture_id, output = None):
        """
            Stops an active capture and optionally save it to a PCAP file

            :parameters:
                capture_id: int
                    an active capture ID to stop
                    
                output: None / str / list
                    if output is None - all the packets will be discarded
                    if output is a 'str' - it will be interpeted as output filename
                    if it is a list, the API will populate the list with packet objects

                    in case 'output' is a list, each element in the list is an object
                    containing:
                    'binary' - binary bytes of the packet
                    'origin' - RX or TX origin
                    'ts'     - timestamp relative to the start of the capture
                    'index'  - order index in the capture
                    'port'   - on which port did the packet arrive or was transmitted from
                    
            :raises:
                + :exe:'STLError'

        """
        
        # stopping a capture requires:
        # 1. stopping
        # 2. fetching
        # 3. saving to file
        
        
        validate_type('capture_id', capture_id, (int))
        validate_type('output', output, (type(None), str, list))
        
        # stop
        
        self.logger.pre_cmd("Stopping packet capture {0}".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'stop', 'capture_id': capture_id})
        self.logger.post_cmd(rc)
        if not rc:
            raise STLError(rc)
        
        # pkt count
        pkt_count = rc.data()['pkt_count']
        
        # fetch packets
        if output is not None:
            self.fetch_capture_packets(capture_id, output, pkt_count)
        
        # remove
        self.logger.pre_cmd("Removing PCAP capture {0} from server".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': capture_id})
        self.logger.post_cmd(rc)
        if not rc:
            raise STLError(rc)
        

    @__api_check(True)
    def fetch_capture_packets (self, capture_id, output, pkt_count = 1000):
        """
            Fetch packets from existing active capture

            :parameters:
                capture_id: int
                    an active capture ID

                pkt_count: int
                    maximum packets to fetch
                    
                output: str / list
                    if output is a 'str' - it will be interpeted as output filename
                    if it is a list, the API will populate the list with packet objects

                    in case 'output' is a list, each element in the list is an object
                    containing:
                    'binary' - binary bytes of the packet
                    'origin' - RX or TX origin
                    'ts'     - timestamp relative to the start of the capture
                    'index'  - order index in the capture
                    'port'   - on which port did the packet arrive or was transmitted from

            :raises:
                + :exe:'STLError'

        """

        write_to_file = isinstance(output, basestring)
        
        self.logger.pre_cmd("Writing {0} packets to '{1}'".format(pkt_count, output if write_to_file else 'list'))

        # create a PCAP file
        if write_to_file:
            writer = RawPcapWriter(output, linktype = 1)
            writer._write_header(None)
        else:
            # clear the list
            del output[:]

        # assumes the server has 'count' packets
        pending = pkt_count
        rc = RC_OK()
        
        # fetch with iteratios - each iteration up to 50 packets
        while pending > 0:
            rc = self._transmit("capture", params = {'command': 'fetch', 'capture_id': capture_id, 'pkt_limit': min(50, pending)})
            if not rc:
                self.logger.post_cmd(rc)
                raise STLError(rc)

            pkts      = rc.data()['pkts']
            pending   = rc.data()['pending']
            start_ts  = rc.data()['start_ts']
            
            # write packets
            for pkt in pkts:
                ts = pkt['ts'] - start_ts
                
                pkt['binary'] = base64.b64decode(pkt['binary'])
                
                if write_to_file:
                    ts_sec, ts_usec = sec_split_usec(ts)
                    writer._write_packet(pkt['binary'], sec = ts_sec, usec = ts_usec)
                else:
                    output.append(pkt)


        self.logger.post_cmd(rc)

            
            
    @__api_check(True)
    def get_capture_status (self):
        """
            Returns a dictionary where each key is an capture ID
            Each value is an object describing the capture

        """
        rc = self._transmit("capture", params = {'command': 'status'})
        if not rc:
            raise STLError(rc)

        # reformat as dictionary
        output = {}
        for c in rc.data():
            output[c['id']] = c
            
        return output

        
    @__api_check(True)
    def remove_all_captures (self):
        """
            Removes any existing captures
        """
        captures = self.get_capture_status()
        
        self.logger.pre_cmd("Removing all packet captures from server")
        
        for capture_id in captures.keys():
            # remove
            rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': capture_id})
            if not rc:
                raise STLError(rc)

        self.logger.post_cmd(RC_OK())
                

        
    @__api_check(True)
    def set_rx_queue (self, ports = None, size = 1000):
        """
            Sets RX queue for port(s)
            The queue is cyclic and will hold last 'size' packets

            :parameters:
                ports          - for which ports to apply a queue
                size           - size of the queue
            :raises:
                + :exe:'STLError'

        """
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # check arguments
        validate_type('size', size, (int))
        if size <= 0:
            raise STLError("'size' must be a positive value")

        self.logger.pre_cmd("Setting RX queue on port(s) {0}:".format(ports))
        rc = self.__set_rx_queue(ports, size)
        self.logger.post_cmd(rc)


        if not rc:
            raise STLError(rc)



    @__api_check(True)
    def remove_rx_queue (self, ports = None):
        """
            Removes RX queue from port(s)

            :parameters:
                ports          - for which ports to remove the RX queue
            :raises:
                + :exe:'STLError'

        """
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        self.logger.pre_cmd("Removing RX queue on port(s) {0}:".format(ports))
        rc = self.__remove_rx_queue(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)



    @__api_check(True)
    def get_rx_queue_pkts (self, ports = None):
        """
            Returns any packets queued on the RX side by the server
            return value is a dictonary per port
            
            :parameters:
                ports          - for which ports to fetch
        """
        
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)
        
        rc = self.__get_rx_queue_pkts(ports)
        if not rc:
            raise STLError(rc)
            
        # decode the data back to the user
        result = {}
        for port, r in zip(ports, rc.data()):
            result[port] = r
        
        return result
            
        
    def clear_events (self):
        """
            Clear all events

            :parameters:
                None

            :raises:
                None

        """
        self.event_handler.clear_events()


    def create_service_ctx (self, port):
        """
            Generates a service context.
            Services can be added to the context,
            and then executed
        """
        
        return STLServiceCtx(self, port)
        
    ############################   Line       #############################
    ############################   Commands   #############################
    ############################              #############################

    # console decorator
    def __console(f):
        @wraps(f)
        def wrap(*args):
            client = args[0]

            time1 = time.time()

            try:
                rc = f(*args)
            except STLError as e:
                client.logger.log("\nAction has failed with the following error:\n\n" + format_text(e.brief() + "\n", 'bold'))
                return RC_ERR(e.brief())

            # if got true - print time
            if rc:
                delta = time.time() - time1
                client.logger.log(format_time(delta) + "\n")

            return rc

        return wrap


    @__console
    def ping_line (self, line):
        '''pings the server / specific IP'''
        
        # no parameters - so ping server
        if not line:
            self.ping_rpc_server()
            return True
            
        parser = parsing_opts.gen_parser(self,
                                         "ping",
                                         self.ping_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.PING_IP,
                                         parsing_opts.PKT_SIZE,
                                         parsing_opts.PING_COUNT)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts
            
        # IP ping
        # source ports maps to ports as a single port
        self.ping_ip(opts.ports[0], opts.ping_ip, opts.pkt_size, opts.count)
        
        
    @__console
    def shutdown_line (self, line):
        '''shutdown the server'''
        parser = parsing_opts.gen_parser(self,
                                         "shutdown",
                                         self.shutdown_line.__doc__,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        self.server_shutdown(force = opts.force)    
        return RC_OK()

    @__console
    def connect_line (self, line):
        '''Connects to the TRex server and acquire ports'''
        parser = parsing_opts.gen_parser(self,
                                         "connect",
                                         self.connect_line.__doc__,
                                         parsing_opts.FORCE,
                                         parsing_opts.READONLY)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        self.connect()
        if not opts.readonly:
            self.acquire(force = opts.force)

        return RC_OK()


    @__console
    def acquire_line (self, line):
        '''Acquire ports\n'''

        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "acquire",
                                         self.acquire_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split(), default_ports = self.get_all_ports())
        if not opts:
            return opts

        # filter out all the already owned ports
        ports = list_difference(opts.ports, self.get_acquired_ports())
        if not ports:
            msg = "acquire - all of port(s) {0} are already acquired".format(opts.ports)
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)

        self.acquire(ports = ports, force = opts.force)

        return RC_OK()


    @__console
    def release_line (self, line):
        '''Release ports\n'''

        parser = parsing_opts.gen_parser(self,
                                         "release",
                                         self.release_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports())
        if not opts:
            return opts

        ports = list_intersect(opts.ports, self.get_acquired_ports())
        if not ports:
            if not opts.ports:
                msg = "release - no acquired ports"
                self.logger.log(format_text(msg, 'bold'))
                return RC_ERR(msg)
            else:
                msg = "release - none of port(s) {0} are acquired".format(opts.ports)
                self.logger.log(format_text(msg, 'bold'))
                return RC_ERR(msg)

        
        self.release(ports = ports)

        return RC_OK()


    @__console
    def reacquire_line (self, line):
        '''reacquire all the ports under your username which are not acquired by your session'''

        parser = parsing_opts.gen_parser(self,
                                         "reacquire",
                                         self.reacquire_line.__doc__)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        # find all the on-owned ports under your name
        my_unowned_ports = list_difference([k for k, v in self.ports.items() if v.get_owner() == self.username], self.get_acquired_ports())
        if not my_unowned_ports:
            msg = "reacquire - no unowned ports under '{0}'".format(self.username)
            self.logger.log(msg)
            return RC_ERR(msg)

        self.acquire(ports = my_unowned_ports, force = True)
        return RC_OK()


    @__console
    def disconnect_line (self, line):
        self.disconnect()


    @__console
    def reset_line (self, line):
        '''Reset ports - if no ports are provided all acquired ports will be reset'''

        parser = parsing_opts.gen_parser(self,
                                         "reset",
                                         self.reset_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.PORT_RESTART)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if not opts:
            return opts

        self.reset(ports = opts.ports, restart = opts.restart)

        return RC_OK()



    @__console
    def start_line (self, line):
        '''Start selected traffic on specified ports on TRex\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "start",
                                         self.start_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.TOTAL,
                                         parsing_opts.FORCE,
                                         parsing_opts.FILE_PATH,
                                         parsing_opts.DURATION,
                                         parsing_opts.TUNABLES,
                                         parsing_opts.MULTIPLIER_STRICT,
                                         parsing_opts.DRY_RUN,
                                         parsing_opts.CORE_MASK_GROUP,
                                         parsing_opts.SYNCHRONIZED)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if not opts:
            return opts

        # core mask
        if opts.core_mask is not None:
            core_mask =  opts.core_mask
        else:
            core_mask = self.CORE_MASK_PIN if opts.pin_cores else self.CORE_MASK_SPLIT

        # just for sanity - will be checked on the API as well
        self.__decode_core_mask(opts.ports, core_mask)

        # for better use experience - check this first
        self.__pre_start_check('START', opts.ports, opts.force)
                 
        # stop ports if needed
        active_ports = list_intersect(self.get_active_ports(), opts.ports)
        if active_ports and opts.force:
            self.stop(active_ports)


        # process tunables
        if type(opts.tunables) is dict:
            tunables = opts.tunables
        else:
            tunables = {}


        # remove all streams
        self.remove_all_streams(opts.ports)

        # pack the profile
        try:
            for port in opts.ports:

                profile = STLProfile.load(opts.file[0],
                                          direction = tunables.get('direction', port % 2),
                                          port_id = port,
                                          **tunables)

                self.add_streams(profile.get_streams(), ports = port)

        except STLError as e:
            for line in e.brief().splitlines():
                if ansi_len(line.strip()):
                    error = line
            msg = format_text("\nError loading profile '{0}'".format(opts.file[0]), 'bold')
            self.logger.log(msg + '\n')
            self.logger.log(e.brief() + "\n")
            return RC_ERR("%s: %s" % (msg, error))


        if opts.dry:
            self.validate(opts.ports, opts.mult, opts.duration, opts.total)
        else:

            self.start(opts.ports,
                       opts.mult,
                       opts.force,
                       opts.duration,
                       opts.total,
                       core_mask,
                       opts.sync)

        return RC_OK()



    @__console
    def stop_line (self, line):
        '''Stop active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "stop",
                                         self.stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_active_ports(), verify_acquired = True)
        if not opts:
            return opts


        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_active_ports())
        if not ports:
            if not opts.ports:
                msg = 'stop - no active ports'
            else:
                msg = 'stop - no active traffic on ports {0}'.format(opts.ports)

            self.logger.log(msg)
            return RC_ERR(msg)

        # call API
        self.stop(ports)

        return RC_OK()


    @__console
    def update_line (self, line):
        '''Update port(s) speed currently active\n'''
        parser = parsing_opts.gen_parser(self,
                                         "update",
                                         self.update_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.MULTIPLIER,
                                         parsing_opts.TOTAL,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split(), default_ports = self.get_active_ports(), verify_acquired = True)
        if not opts:
            return opts


        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_active_ports())
        if not ports:
            if not opts.ports:
                msg = 'update - no active ports'
            else:
                msg = 'update - no active traffic on ports {0}'.format(opts.ports)

            self.logger.log(msg)
            return RC_ERR(msg)

        self.update(ports, opts.mult, opts.total, opts.force)

        return RC_OK()


    @__console
    def pause_line (self, line):
        '''Pause active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "pause",
                                         self.pause_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_transmitting_ports(), verify_acquired = True)
        if not opts:
            return opts

        # check for already paused case
        if opts.ports and is_sub_list(opts.ports, self.get_paused_ports()):
            msg = 'pause - all of port(s) {0} are already paused'.format(opts.ports)
            self.logger.log(msg)
            return RC_ERR(msg)

        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_transmitting_ports())
        if not ports:
            if not opts.ports:
                msg = 'pause - no transmitting ports'
            else:
                msg = 'pause - none of ports {0} are transmitting'.format(opts.ports)

            self.logger.log(msg)
            return RC_ERR(msg)

        self.pause(ports)

        return RC_OK()


    @__console
    def resume_line (self, line):
        '''Resume active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "resume",
                                         self.resume_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_paused_ports(), verify_acquired = True)
        if not opts:
            return opts

        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_paused_ports())
        if not ports:
            if not opts.ports:
                msg = 'resume - no paused ports'
            else:
                msg = 'resume - none of ports {0} are paused'.format(opts.ports)
                
            self.logger.log(msg)
            return RC_ERR(msg)


        self.resume(ports)

        # true means print time
        return RC_OK()

   
    @__console
    def clear_stats_line (self, line):
        '''Clear cached local statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "clear",
                                         self.clear_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())

        if not opts:
            return opts

        self.clear_stats(opts.ports)

        return RC_OK()


    @__console
    def show_stats_line (self, line):
        '''Get statistics from TRex server by port\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.show_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STATS_MASK)

        opts = parser.parse_args(line.split())

        if not opts:
            return opts

        # determine stats mask
        mask = self.__get_mask_keys(**self.__filter_namespace_args(opts, trex_stl_stats.ALL_STATS_OPTS))
        if not mask:
            # set to show all stats if no filter was given
            mask = trex_stl_stats.COMPACT

        stats_opts = common.list_intersect(trex_stl_stats.ALL_STATS_OPTS, mask)

        stats = self._get_formatted_stats(opts.ports, mask)


        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


    @__console
    def show_streams_line(self, line):
        '''Get stream statistics from TRex server by port\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "streams",
                                         self.show_streams_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STREAMS_MASK)

        opts = parser.parse_args(line.split())

        if not opts:
            return opts

        streams = self._get_streams(opts.ports, set(opts.streams))
        if not streams:
            self.logger.log(format_text("No streams found with desired filter.\n", "bold", "magenta"))

        else:
            # print stats to screen
            for stream_hdr, port_streams_data in streams.items():
                text_tables.print_table_with_header(port_streams_data.text_table,
                                                    header= stream_hdr.split(":")[0] + ":",
                                                    untouched_header= stream_hdr.split(":")[1])




    @__console
    def validate_line (self, line):
        '''Validates port(s) stream configuration\n'''

        parser = parsing_opts.gen_parser(self,
                                         "validate",
                                         self.validate_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        self.validate(opts.ports)



 
    @__console
    def push_line (self, line):
        '''Push a pcap file '''
        args = [self,
                "push",
                self.push_line.__doc__,
                parsing_opts.REMOTE_FILE,
                parsing_opts.PORT_LIST_WITH_ALL,
                parsing_opts.COUNT,
                parsing_opts.DURATION,
                parsing_opts.IPG,
                parsing_opts.MIN_IPG,
                parsing_opts.SPEEDUP,
                parsing_opts.FORCE,
                parsing_opts.DUAL]

        parser = parsing_opts.gen_parser(*(args + [parsing_opts.FILE_PATH_NO_CHECK]))
        opts = parser.parse_args(line.split(), verify_acquired = True)

        if not opts:
            return opts

        if not opts.remote:
            parser = parsing_opts.gen_parser(*(args + [parsing_opts.FILE_PATH]))
            opts = parser.parse_args(line.split(), verify_acquired = True)

        if not opts:
            return opts

            
        if opts.remote:
            self.push_remote(opts.file[0],
                             ports          = opts.ports,
                             ipg_usec       = opts.ipg_usec,
                             min_ipg_usec   = opts.min_ipg_usec,
                             speedup        = opts.speedup,
                             count          = opts.count,
                             duration       = opts.duration,
                             force          = opts.force,
                             is_dual        = opts.dual)

        else:
            self.push_pcap(opts.file[0],
                           ports          = opts.ports,
                           ipg_usec       = opts.ipg_usec,
                           min_ipg_usec   = opts.min_ipg_usec,
                           speedup        = opts.speedup,
                           count          = opts.count,
                           duration       = opts.duration,
                           force          = opts.force,
                           is_dual        = opts.dual)

        

        return RC_OK()



    @__console
    def set_port_attr_line (self, line):
        '''Sets port attributes '''

        parser = parsing_opts.gen_parser(self,
                                         "portattr",
                                         self.set_port_attr_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.PROMISCUOUS,
                                         parsing_opts.LINK_STATUS,
                                         parsing_opts.LED_STATUS,
                                         parsing_opts.FLOW_CTRL,
                                         parsing_opts.SUPPORTED,
                                         parsing_opts.MULTICAST,
                                         )

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports())
        if not opts:
            return opts

        opts.prom            = parsing_opts.ON_OFF_DICT.get(opts.prom)
        opts.mult            = parsing_opts.ON_OFF_DICT.get(opts.mult)
        opts.link            = parsing_opts.UP_DOWN_DICT.get(opts.link)
        opts.led             = parsing_opts.ON_OFF_DICT.get(opts.led)
        opts.flow_ctrl       = parsing_opts.FLOW_CTRL_DICT.get(opts.flow_ctrl)

        # if no attributes - fall back to printing the status
        if not list(filter(lambda opt:opt[0] not in ('all_ports', 'ports') and opt[1] is not None, opts._get_kwargs())):
            ports = opts.ports if opts.ports else self.get_all_ports()
            self.show_stats_line("--ps --port {0}".format(' '.join(str(port) for port in ports)))
            return

        if opts.supp:
            info = self.ports[0].get_formatted_info() # assume for now all ports are same
            print('')
            print('Supported attributes for current NICs:')
            print('  Promiscuous:   yes')
            print('  Multicast:     yes')
            print('  Link status:   %s' % info['link_change_supported'])
            print('  LED status:    %s' % info['led_change_supported'])
            print('  Flow control:  %s' % info['fc_supported'])
            print('')
        else:
            if not opts.ports:
                raise STLError('No acquired ports!')
            self.set_port_attr(opts.ports,
                               opts.prom,
                               opts.link,
                               opts.led,
                               opts.flow_ctrl,
                               multicast = opts.mult)


    @__console
    def set_rx_sniffer_line (self, line):
        '''Sets a port sniffer on RX channel in form of a PCAP file'''

        parser = parsing_opts.gen_parser(self,
                                         "set_rx_sniffer",
                                         self.set_rx_sniffer_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.OUTPUT_FILENAME,
                                         parsing_opts.LIMIT)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if not opts:
            return opts

        self.set_rx_sniffer(opts.ports, opts.output_filename, opts.limit)

        return RC_OK()
        

    @__console
    def resolve_line (self, line):
        '''Performs a port ARP resolution'''

        parser = parsing_opts.gen_parser(self,
                                         "resolve",
                                         self.resolve_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.RETRIES)

        opts = parser.parse_args(line.split(), default_ports = self.get_resolvable_ports(), verify_acquired = True)
        if not opts:
            return opts

        
        self.resolve(ports = opts.ports, retries = opts.retries)

        return RC_OK()
        
    
    @__console
    def scan6_line(self, line):
        '''Search for IPv6 neighbors'''

        parser = parsing_opts.gen_parser(self,
                                         "scan6",
                                         self.scan6_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.TIMEOUT)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if not opts:
            return opts

        rc_per_port = self.scan6(ports = opts.ports, timeout = opts.timeout, verbose = True)
        #for port, rc in rc_per_port.items():
        #    if rc and len(rc.data()) == 1 and not self.ports[port].is_resolved():
        #        self.ports[port].set_l2_mode(rc.data()[0][1])
        return RC_OK()


    @__console
    def set_l2_mode_line (self, line):
        '''Configures a port in L2 mode'''

        parser = parsing_opts.gen_parser(self,
                                         "port",
                                         self.set_l2_mode_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.DST_MAC,
                                         )

        opts = parser.parse_args(line.split())
        if not opts:
            return opts


        # source ports maps to ports as a single port
        self.set_l2_mode(opts.ports[0], dst_mac = opts.dst_mac)

        return RC_OK()
        
        
    @__console
    def set_l3_mode_line (self, line):
        '''Configures a port in L3 mode'''

        parser = parsing_opts.gen_parser(self,
                                         "port",
                                         self.set_l3_mode_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.SRC_IPV4,
                                         parsing_opts.DST_IPV4,
                                         )

        opts = parser.parse_args(line.split())
        if not opts:
            return opts


        # source ports maps to ports as a single port
        self.set_l3_mode(opts.ports[0], src_ipv4 = opts.src_ipv4, dst_ipv4 = opts.dst_ipv4)

        return RC_OK()
        
        
    @__console
    def show_profile_line (self, line):
        '''Shows profile information'''

        parser = parsing_opts.gen_parser(self,
                                         "profile",
                                         self.show_profile_line.__doc__,
                                         parsing_opts.FILE_PATH)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        info = STLProfile.get_info(opts.file[0])

        self.logger.log(format_text('\nProfile Information:\n', 'bold'))

        # general info
        self.logger.log(format_text('\nGeneral Information:', 'underline'))
        self.logger.log('Filename:         {:^12}'.format(opts.file[0]))
        self.logger.log('Stream count:     {:^12}'.format(info['stream_count']))

        # specific info
        profile_type = info['type']
        self.logger.log(format_text('\nSpecific Information:', 'underline'))

        if profile_type == 'python':
            self.logger.log('Type:             {:^12}'.format('Python Module'))
            self.logger.log('Tunables:         {:^12}'.format(str(['{0} = {1}'.format(k ,v) for k, v in info['tunables'].items()])))

        elif profile_type == 'yaml':
            self.logger.log('Type:             {:^12}'.format('YAML'))

        elif profile_type == 'pcap':
            self.logger.log('Type:             {:^12}'.format('PCAP file'))

        self.logger.log("")


    @__console
    def get_events_line (self, line):
        '''shows events recieved from server\n'''

        x = [parsing_opts.ArgumentPack(['-c','--clear'],
                                      {'action' : "store_true",
                                       'default': False,
                                       'help': "clear the events log"}),

             parsing_opts.ArgumentPack(['-i','--info'],
                                      {'action' : "store_true",
                                       'default': False,
                                       'help': "show info events"}),

             parsing_opts.ArgumentPack(['-w','--warn'],
                                      {'action' : "store_true",
                                       'default': False,
                                       'help': "show warning events"}),

             ]


        parser = parsing_opts.gen_parser(self,
                                         "events",
                                         self.get_events_line.__doc__,
                                         *x)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts


        ev_type_filter = []

        if opts.info:
            ev_type_filter.append('info')

        if opts.warn:
            ev_type_filter.append('warning')

        if not ev_type_filter:
            ev_type_filter = None

        events = self.get_events(ev_type_filter)
        for ev in events:
            self.logger.log(ev)

        if opts.clear:
            self.clear_events()
         
    def generate_prompt (self, prefix = 'trex'):
        if not self.is_connected():
            return "{0}(offline)>".format(prefix)

        elif not self.get_acquired_ports():
            return "{0}(read-only)>".format(prefix)

        elif self.is_all_ports_acquired():
            
            p = prefix
            
            if self.get_service_enabled_ports():
                if self.get_service_enabled_ports() == self.get_acquired_ports():
                    p += '(service)'
                else:
                    p += '(service: {0})'.format(', '.join(map(str, self.get_service_enabled_ports())))
                
            return "{0}>".format(p)

        else:
            return "{0} (ports: {1})>".format(prefix, ', '.join(map(str, self.get_acquired_ports())))
            
            

    @__console
    def service_line (self, line):
        '''Configures port for service mode.
           In service mode ports will reply to ARP, PING
           and etc.
        '''

        parser = parsing_opts.gen_parser(self,
                                         "service",
                                         self.service_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.SERVICE_OFF)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts
            
        self.set_service_mode(ports = opts.ports, enabled = opts.enabled)
        

    @__console
    def pkt_line (self, line):
        '''
            Sends a Scapy format packet
        '''
        
        parser = parsing_opts.gen_parser(self,
                                         "pkt",
                                         self.pkt_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.COUNT,
                                         parsing_opts.DRY_RUN,
                                         parsing_opts.SCAPY_PKT_CMD,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts
            
        # show layers option
        if opts.layers:
            self.logger.log(format_text('\nRegistered Layers:\n', 'underline'))
            self.logger.log(parsing_opts.ScapyDecoder.formatted_layers())
            return

        # dry run option
        if opts.dry:
            self.logger.log(format_text('\nPacket (Size: {0}):\n'.format(format_num(len(opts.scapy_pkt), suffix = 'B')), 'bold', 'underline'))
            opts.scapy_pkt.show2()
            self.logger.log(format_text('\n*** DRY RUN - no traffic was injected ***\n', 'bold'))
            return
   
            
        self.logger.pre_cmd("Pushing {0} packet(s) (size: {1}) on port(s) {2}:".format(opts.count if opts.count else 'infinite',
                                                                                       len(opts.scapy_pkt), opts.ports))
        try:
            with self.logger.supress():
                self.push_packets(pkts = opts.scapy_pkt, ports = opts.ports, force = opts.force, count = opts.count)
                
        except STLError as e:
            self.logger.post_cmd(False)
            raise
        else:
            self.logger.post_cmd(RC_OK())
        
            
    # save current history to a temp file
    def __push_history (self):
        tmp_file = tempfile.mktemp()
        readline.write_history_file(tmp_file)
        readline.clear_history()
        return tmp_file
        
    # restore history from a temp file        
    def __pop_history (self, filename):
        readline.clear_history()
        readline.read_history_file(filename)
        
 
        
    @__console
    def debug_line (self, line):
        '''
            Internal debugger for development.
            Requires IPython and readline modules installed
        '''
     
        parser = parsing_opts.gen_parser(self,
                                         "debug",
                                         self.debug_line.__doc__)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts
            
        
        try:
            from IPython.terminal.ipapp import load_default_config
            from IPython.terminal.embed import InteractiveShellEmbed
            from IPython import embed
            
        except ImportError:
            self.logger.log(format_text("\n*** 'IPython' is required for interactive debugging ***\n", 'bold'))
            return
            
        try:
            import readline
        except ImportError:
            self.logger.log(format_text("\n*** 'readline' is required for interactive debugging ***\n", 'bold'))
            return
            
        self.logger.log(format_text("\n*** Starting IPython... use 'client' as client object, Ctrl + D to exit ***\n", 'bold'))
        
        
        client = self
        auto_completer = readline.get_completer()
        
        console_h = self.__push_history()
        
        try:
            
            cfg = load_default_config()
            cfg['TerminalInteractiveShell']['confirm_exit'] = False
            
            embed(config = cfg, display_banner = False)
            #InteractiveShellEmbed.clear_instance()
            
        finally:
            readline.set_completer(auto_completer)
            self.__pop_history(console_h)
            try:
                os.unlink(console_h)
            except OSError:
                pass
        
        self.logger.log(format_text("\n*** Leaving IPython ***\n"))
        
        return


