#!/router/bin/python

# for API usage the path name must be full
from .trex_stl_exceptions import *
from .trex_stl_streams import *

from .trex_stl_jsonrpc_client import JsonRpcClient, BatchMessage
from . import trex_stl_stats

from .trex_stl_port import Port
from .trex_stl_types import *
from .trex_stl_async_client import CTRexAsyncClient

from .utils import parsing_opts, text_tables, common
from .utils.common import *
from .utils.text_opts import *
from functools import wraps
from texttable import ansi_len


from collections import namedtuple
from yaml import YAMLError
import time
import datetime
import threading
import re
import random
import json
import traceback
import os.path
import argparse

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


    def __init__ (self, client):
        self.client = client
        self.logger = self.client.logger

        self.events = []

    # public functions

    def get_events (self, ev_type_filter = None):
        if ev_type_filter:
            return [ev for ev in self.events if ev.ev_type in listify(ev_type_filter)]
        else:
            return [ev for ev in self.events]


    def clear_events (self):
        self.events = []


    def log_warning (self, msg, show = True):
        self.__add_event_log('local', 'warning', msg, show)


    # events called internally

    def on_async_dead (self):
        if self.client.connected:
            msg = 'Lost connection to server'
            self.client.connected = False
            self.__add_event_log('local', 'info', msg, True)


    def on_async_alive (self):
        pass

    

    def on_async_rx_stats_event (self, data, baseline):
        self.client.flow_stats.update(data, baseline)

    def on_async_latency_stats_event (self, data, baseline):
        self.client.latency_stats.update(data, baseline)

    # handles an async stats update from the subscriber
    def on_async_stats_update(self, dump_data, baseline):
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
    def on_async_event (self, event_type, data):
        # DP stopped
        show_event = False

        # port started
        if (event_type == 0):
            port_id = int(data['port_id'])
            ev = "Port {0} has started".format(port_id)
            self.__async_event_port_started(port_id)

        # port stopped
        elif (event_type == 1):
            port_id = int(data['port_id'])
            ev = "Port {0} has stopped".format(port_id)

            # call the handler
            self.__async_event_port_stopped(port_id)


        # port paused
        elif (event_type == 2):
            port_id = int(data['port_id'])
            ev = "Port {0} has paused".format(port_id)

            # call the handler
            self.__async_event_port_paused(port_id)

        # port resumed
        elif (event_type == 3):
            port_id = int(data['port_id'])
            ev = "Port {0} has resumed".format(port_id)

            # call the handler
            self.__async_event_port_resumed(port_id)

        # port finished traffic
        elif (event_type == 4):
            port_id = int(data['port_id'])
            ev = "Port {0} job done".format(port_id)

            # call the handler
            self.__async_event_port_job_done(port_id)
            show_event = True

        # port was acquired - maybe stolen...
        elif (event_type == 5):
            session_id = data['session_id']

            port_id = int(data['port_id'])
            who     = data['who']
            force   = data['force']

            # if we hold the port and it was not taken by this session - show it
            if port_id in self.client.get_acquired_ports() and session_id != self.client.session_id:
                show_event = True

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
        elif (event_type == 6):
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

        elif (event_type == 7):
            port_id = int(data['port_id'])
            ev = "port {0} job failed".format(port_id)
            show_event = True

        # port attr changed
        elif (event_type == 8):

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
                
            show_event = True
            
         
    
        # server stopped
        elif (event_type == 100):
            ev = "Server has stopped"
            # to avoid any new messages on async
            self.client.async_client.set_as_zombie()
            self.__async_event_server_stopped()
            show_event = True


        else:
            # unknown event - ignore
            return


        self.__add_event_log('server', 'info', ev, show_event)


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

    def __async_event_server_stopped (self):
        self.client.connected = False

    def __async_event_port_attr_changed (self, port_id, attr):
        if port_id in self.client.ports:
            return self.client.ports[port_id].async_event_port_attr_changed(attr)

    # add event to log
    def __add_event_log (self, origin, ev_type, msg, show = False):

        event = Event(origin, ev_type, msg)
        self.events.append(event)
        if show:
            self.logger.async_log("\n\n{0}".format(str(event)))


  


############################     RPC layer     #############################
############################                   #############################
############################                   #############################

class CCommLink(object):
    """Describes the connectivity of the stateless client method"""
    def __init__(self, server="localhost", port=5050, virtual=False, client = None):
        self.virtual = virtual
        self.server = server
        self.port = port
        self.rpc_link = JsonRpcClient(self.server, self.port, client)

    @property
    def is_connected(self):
        if not self.virtual:
            return self.rpc_link.connected
        else:
            return True

    def get_server (self):
        return self.server

    def get_port (self):
        return self.port

    def connect(self):
        if not self.virtual:
            return self.rpc_link.connect()

    def disconnect(self):
        if not self.virtual:
            return self.rpc_link.disconnect()

    def transmit(self, method_name, params = None, api_class = 'core', retry = 0):
        if self.virtual:
            self._prompt_virtual_tx_msg()
            _, msg = self.rpc_link.create_jsonrpc_v2(method_name, params, api_class)
            print(msg)
            return
        else:
            return self.rpc_link.invoke_rpc_method(method_name, params, api_class, retry = retry)

    def transmit_batch(self, batch_list, retry = 0):
        if self.virtual:
            self._prompt_virtual_tx_msg()
            print([msg
                   for _, msg in [self.rpc_link.create_jsonrpc_v2(command.method, command.params, command.api_class)
                                  for command in batch_list]])
        else:
            batch = self.rpc_link.create_batch()
            for command in batch_list:
                batch.add(command.method, command.params, command.api_class)
            # invoke the batch
            return batch.invoke(retry = retry)

    def _prompt_virtual_tx_msg(self):
        print("Transmitting virtually over tcp://{server}:{port}".format(server=self.server,
                                                                         port=self.port))



############################     client     #############################
############################                #############################
############################                #############################

class STLClient(object):
    """TRex Stateless client object - gives operations per TRex/user"""

    # different modes for attaching traffic to ports
    CORE_MASK_SPLIT = 1
    CORE_MASK_PIN   = 2

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
        self.connected = False

        # API classes
        self.api_vers = [ {'type': 'core', 'major': 3, 'minor': 0 } ]
        self.api_h = {'core': None}

        # logger
        self.logger = DefaultLogger() if not logger else logger

        # initial verbose
        self.logger.set_verbose(verbose_level)

        # low level RPC layer
        self.comm_link = CCommLink(server,
                                   sync_port,
                                   virtual,
                                   self)

        # async event handler manager
        self.event_handler = EventsHandler(self)

        # async subscriber level
        self.async_client = CTRexAsyncClient(server,
                                             async_port,
                                             self)

        
      

        # stats
        self.connection_info = {"username":   username,
                                "server":     server,
                                "sync_port":  sync_port,
                                "async_port": async_port,
                                "virtual":    virtual}

        
        self.global_stats = trex_stl_stats.CGlobalStats(self.connection_info,
                                                        self.server_version,
                                                        self.ports,
                                                        self.event_handler)

        self.flow_stats = trex_stl_stats.CRxStats(self.ports)

        self.latency_stats = trex_stl_stats.CLatencyStats(self.ports)

        self.util_stats = trex_stl_stats.CUtilStats(self)

        self.xstats = trex_stl_stats.CXStats(self)

        self.stats_generator = trex_stl_stats.CTRexInfoGenerator(self.global_stats,
                                                                 self.ports,
                                                                 self.flow_stats,
                                                                 self.latency_stats,
                                                                 self.util_stats,
                                                                 self.xstats,
                                                                 self.async_client.monitor)
        
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
                 core_mask):

        port_id_list = self.__ports(port_id_list)

        rc = RC()


        for port_id in port_id_list:
            rc.add(self.ports[port_id].start(multiplier,
                                             duration,
                                             force,
                                             core_mask[port_id]))

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


    def __resolve (self, port_id_list = None, retries = 0):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].arp_resolve(retries))

        return rc


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

        # first disconnect if already connected
        if self.is_connected():
            self.__disconnect()

        # clear this flag
        self.connected = False

        # connect sync channel
        self.logger.pre_cmd("Connecting to RPC server on {0}:{1}".format(self.connection_info['server'], self.connection_info['sync_port']))
        rc = self.comm_link.connect()
        self.logger.post_cmd(rc)

        if not rc:
            return rc

        
        # API sync
        rc = self._transmit("api_sync", params = {'api_vers': self.api_vers}, api_class = None)
        if not rc:
            return rc

        # decode
        for api in rc.data()['api_vers']:
            self.api_h[ api['type'] ] = api['api_h']


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
                                       self.comm_link,
                                       self.session_id,
                                       info)


        # sync the ports
        rc = self.__sync_ports()
        if not rc:
            return rc

        
        # connect async channel
        self.logger.pre_cmd("Connecting to publisher server on {0}:{1}".format(self.connection_info['server'], self.connection_info['async_port']))
        rc = self.async_client.connect()
        self.logger.post_cmd(rc)

        if not rc:
            return rc

        self.connected = True

        return RC_OK()


    # disconenct from server
    def __disconnect(self, release_ports = True):
        # release any previous acquired ports
        if self.is_connected() and release_ports:
            self.__release(self.get_acquired_ports())

        self.comm_link.disconnect()
        self.async_client.disconnect()

        self.connected = False

        return RC_OK()


    # clear stats
    def __clear_stats(self, port_id_list, clear_global, clear_flow_stats, clear_latency_stats, clear_xstats):

        # we must be sync with the server
        self.async_client.barrier()

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        if clear_global:
            self.global_stats.clear_stats()

        if clear_flow_stats:
            self.flow_stats.clear_stats()

        if clear_latency_stats:
            self.latency_stats.clear_stats()

        if clear_xstats:
            self.xstats.clear_stats()

        self.logger.log_cmd("Clearing stats on port(s) {0}:".format(port_id_list))

        return RC


    # get stats
    def __get_stats (self, port_id_list):
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

        stats['flow_stats'] = self.flow_stats.get_stats()
        stats['latency'] = self.latency_stats.get_stats()

        return stats


    def __decode_core_mask (self, ports, core_mask):

        # predefined modes
        if isinstance(core_mask, int):
            if core_mask not in [self.CORE_MASK_PIN, self.CORE_MASK_SPLIT]:
                raise STLError("'core_mask' can be either CORE_MASK_PIN, CORE_MASK_SPLIT or a list of masks")

            decoded_mask = {}
            for port in ports:
                # a pin mode was requested and we have
                # the second port from the group in the start list
                if (core_mask == self.CORE_MASK_PIN) and ( (port ^ 0x1) in ports ):
                    decoded_mask[port] = 0x55555555 if( port % 2) == 0 else 0xAAAAAAAA
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

    def _validate_port_list (self, port_id_list, allow_empty = False):
        # listfiy single int
        if isinstance(port_id_list, int):
            port_id_list = [port_id_list]

        # should be a list
        if not isinstance(port_id_list, list):
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
        return self.comm_link.transmit(method_name, params, api_class)

    # transmit batch request on the RPC link
    def _transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)

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

                # check connection
                if connected and not client.is_connected():
                    raise STLStateError(func_name, 'disconnected')

                try:
                    ret = f(*args, **kwargs)
                except KeyboardInterrupt as e:
                    raise STLError("Interrupted by a keyboard signal (probably ctrl + c)")

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
            is_connected

        :raises:
          None

        """

        return self.connected and self.comm_link.is_connected


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
            Connection dict 

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
            Connection dict 

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
            Connection dict 

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

         
    def get_service_enabled_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired() and port_obj.is_service_mode_on()]

        
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
            Statistics dictionary of dictionaries with the following format:

            ===============================  ===============
            key                               Meaning
            ===============================  ===============
            :ref:`numbers (0,1,..<total>`    Statistcs per port number
            :ref:`total <total>`             Sum of port statistics
            :ref:`flow_stats <flow_stats>`   Per flow statistics
            :ref:`global <global>`           Global statistics
            :ref:`latency <latency>`         Per flow statistics regarding flow latency
            ===============================  ===============

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
            rx_bps                           Receive bytes per second rate (L2 layer)
            rx_pps                           Receive packet per second rate
            tx_bps                           Transmit bytes per second rate (L2 layer)
            tx_pps                           Transmit packet per second rate
            ===============================  ===============

            .. _flow_stats:

            **flow_stats** contains :ref:`global dictionary <flow_stats_global>`, and dictionaries per packet group id (pg id). See structures below.

            **per pg_id flow stat** dictionaries have following structure:

            =================   ===============
            key                 Meaning
            =================   ===============
            rx_bps              Received bytes per second rate
            rx_bps_l1           Received bytes per second rate, including layer one
            rx_bytes            Total number of received bytes
            rx_pkts             Total number of received packets
            rx_pps              Received packets per second
            tx_bps              Transmit bytes per second rate
            tx_bps_l1           Transmit bytes per second rate, including layer one
            tx_bytes            Total number of sent bytes
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

            .. _global:

            **global**

            =================   ===============
            key                 Meaning
            =================   ===============
            bw_per_core         Estimated byte rate Trex can support per core. This is calculated by extrapolation of current rate and load on transmitting cores.
            cpu_util            Estimate of the average utilization percentage of the transimitting cores
            queue_full          Total number of packets transmitted while the NIC TX queue was full. The packets will be transmitted, eventually, but will create high CPU%due to polling the queue.  This usually indicates that the rate we trying to transmit is too high for this port. 
            rx_cpu_util         Estimate of the utilization percentage of the core handling RX traffic. Too high value of this CPU utilization could cause drop of latency streams. 
            rx_drop_bps         Received bytes per second drop rate
            rx_bps              Received bytes per second rate
            rx_pps              Received packets per second rate
            tx_bps              Transmit bytes per second rate
            tx_pps              Transmit packets per second rate
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
            rc = self.async_client.barrier()
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
        rc = self.__connect()
        if not rc:
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

            :raises:
                + :exc:`STLError`

        """
        
        self.logger.pre_cmd("Pinging the server on '{0}' port '{1}': ".format(self.connection_info['server'],
                                                                              self.connection_info['sync_port']))
        rc = self._transmit("ping", api_class = None)
            
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)
        

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
        
        validate_type('port', port, int)
        if port not in self.get_all_ports():
            raise STLError("port {0} is not a valid port id".format(port))
            
        if not is_valid_ipv4(src_ipv4):
            raise STLError("src_ipv4 is not a valid IPv4 address: '{0}'".format(src_ipv4))
            
        if not is_valid_ipv4(dst_ipv4):
            raise STLError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))
            
        self.logger.pre_cmd("Setting port {0} in L3 mode: ".format(port))
        rc = self.ports[port].set_l3_mode(src_ipv4, dst_ipv4)
        self.logger.post_cmd(rc)
        
        if not rc:
            raise STLError(rc)
    
        # try to resolve
        with self.logger.supress(level = LoggerApi.VERBOSE_REGULAR_SYNC):
            self.logger.pre_cmd("ARP resolving address '{0}': ".format(dst_ipv4))
            rc = self.ports[port].arp_resolve(0)
            self.logger.post_cmd(rc)
            if not rc:
                raise STLError(rc)
            

    @__api_check(True)
    def ping_ip (self, src_port, dst_ipv4, pkt_size = 64, count = 5):
        """
            Pings an IP address through a port

            :parameters:
                 src_port - on which port_id to send the ICMP PING request
                 dst_ipv4 - which IP to ping
                 pkt_size - packet size to use
                 count    - how many times to ping
            :raises:
                + :exc:`STLError`

        """
        # validate src port
        validate_type('src_port', src_port, int)
        if src_port not in self.get_all_ports():
            raise STLError("src port is not a valid port id")
        
        if not is_valid_ipv4(dst_ipv4):
            raise STLError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))
            
        if (pkt_size < 64) or (pkt_size > 9216):
            raise STLError("pkt_size should be a value between 64 and 9216: '{0}'".format(pkt_size))
            
        validate_type('count', count, int)
 
        self.logger.pre_cmd("Pinging {0} from port {1} with {2} bytes of data:".format(dst_ipv4,
                                                                                       src_port,
                                                                                       pkt_size))
        
        # no async messages
        with self.logger.supress(level = LoggerApi.VERBOSE_REGULAR_SYNC):
            self.logger.log('')
            for i in range(count):
                rc = self.ports[src_port].ping(ping_ipv4 = dst_ipv4, pkt_size = pkt_size)
                if not rc:
                    raise STLError(rc)
                    
                self.logger.log(rc.data())
                
                if i != (count - 1):
                    time.sleep(1)
        
        
        
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
            Get active group IDs

            :parameters:
                None


            :raises:
                + :exc:`STLError`

        """

        self.logger.pre_cmd( "Getting active packet group ids")

        rc = self._transmit("get_active_pgids")

        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        return rc.data()

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
    def __pre_start_check (self, ports, force):
        
        # verify link status
        ports_link_down = [port_id for port_id in ports if not self.ports[port_id].is_up()]
        if ports_link_down and not force:
            raise STLError("Port(s) %s - link DOWN - check the connection or specify 'force'" % ports_link_down)
        
        # verify ports are stopped or force stop them
        active_ports = [port_id for port_id in ports if self.ports[port_id].is_active()]
        if active_ports and not force:
            raise STLError("Port(s) {0} are active - please stop them or specify 'force'".format(active_ports))
            
        # warn if ports are not resolved
        unresolved_ports = [port_id for port_id in ports if not self.ports[port_id].is_resolved()]
        if unresolved_ports and not force:
            raise STLError("Port(s) {0} have unresolved destination addresses - please resolve them or specify 'force'".format(unresolved_ports))
     
        if self.get_service_enabled_ports() and not force:
            raise STLError("Port(s) {0} are under service mode - please disable service mode or specify 'force'".format(self.get_service_enabled_ports()))
            
        
    @__api_check(True)
    def start (self,
               ports = None,
               mult = "1",
               force = False,
               duration = -1,
               total = False,
               core_mask = CORE_MASK_SPLIT):
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

                core_mask: CORE_MASK_SPLIT, CORE_MASK_PIN or a list of masks (one per port)
                    Determine the allocation of cores per port
                    In CORE_MASK_SPLIT all the traffic will be divided equally between all the cores
                    associated with each port
                    In CORE_MASK_PIN, for each dual ports (a group that shares the same cores)
                    the cores will be divided half pinned for each port

            :raises:
                + :exc:`STLError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('duration', duration, (int, float))
        validate_type('total', total, bool)
        validate_type('core_mask', core_mask, (int, list))

      
        # some sanity checks before attempting start
        self.__pre_start_check(ports, force)
        
        #########################
        # decode core mask argument
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

        
        # start traffic
        self.logger.pre_cmd("Starting traffic on port(s) {0}:".format(ports))
        rc = self.__start(mult_obj, duration, ports, force, decoded_mask)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


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

        ports = self._validate_port_list(ports)
        
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
                     min_ipg_usec = None):
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

            :raises:
                + :exc:`STLError`

        """
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        validate_type('pcap_filename', pcap_filename, basestring)
        validate_type('ipg_usec', ipg_usec, (float, int, type(None)))
        validate_type('speedup',  speedup, (float, int))
        validate_type('count',  count, int)
        validate_type('duration', duration, (float, int))
        validate_type('is_dual', is_dual, bool)
        validate_type('min_ipg_usec', min_ipg_usec, (float, int, type(None)))

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
        ports = self._validate_port_list(ports)

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
                self.logger.post_cmd(RC_OK)
            except STLError as e:
                self.logger.post_cmd(RC_ERR(e))
                raise


            self.remove_all_streams(ports = ports)
            id_list = self.add_streams(profile.get_streams(), ports)

            return self.start(ports = ports, duration = duration)

        else:

            # create a dual profile
            split_mode = 'MAC'
            
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

            return self.start(ports = all_ports, duration = duration)





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
    def clear_stats (self, ports = None, clear_global = True, clear_flow_stats = True, clear_latency_stats = True, clear_xstats = True):
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

        rc = self.__clear_stats(ports, clear_global, clear_flow_stats, clear_latency_stats, clear_xstats)
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

            # make sure ASYNC thread is still alive - otherwise we will be stuck forever
            if not self.async_client.is_active():
                raise STLError("subscriber thread is dead")

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
                       resolve = True):
        """
            Set port attributes

            :parameters:
                promiscuous      - True or False
                link_up          - True or False
                led_on           - True or False
                flow_ctrl        - 0: disable all, 1: enable tx side, 2: enable rx side, 3: full enable
                resolve          - if true, in case a destination address is configured as IPv4 try to resolve it
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
    
        # common attributes for all ports
        cmn_attr_dict = {}

        cmn_attr_dict['promiscuous']     = promiscuous
        cmn_attr_dict['link_status']     = link_up
        cmn_attr_dict['led_status']      = led_on
        cmn_attr_dict['flow_ctrl_mode']  = flow_ctrl
        
        # each port starts with a set of the common attributes
        attr_dict = [dict(cmn_attr_dict) for _ in ports]
    
        self.logger.pre_cmd("Applying attributes on port(s) {0}:".format(ports))
        rc = self.__set_port_attr(ports, attr_dict)
        self.logger.post_cmd(rc)
            
        if not rc:
            raise STLError(rc)

      
            
    
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
        ports = self._validate_port_list(ports)
            
        self.logger.pre_cmd('Resolving destination on port(s) {0}:'.format(ports))
        
        with self.logger.supress(level = LoggerApi.VERBOSE_REGULAR_SYNC):
            rc = self.__resolve(ports, retries)
            
        self.logger.post_cmd(rc)

        if verbose:
            for x in filter(bool, listify(rc.data())):
                self.logger.log(format_text("{0}".format(x), 'bold'))
                
        if not rc:
            raise STLError(rc)


            
        
    @__api_check(True)
    def start_capture (self, tx_ports, rx_ports, limit = 1000):
        """
            Starts a capture to PCAP on port(s)

            :parameters:
                tx_ports       - on which ports to capture TX
                rx_ports       - on which ports to capture RX
                limit          - limit how many packets will be written
                
            :returns:
                returns a dictionary containing
                {'id: <new_id>, 'ts': <starting timestamp>}
                
            :raises:
                + :exe:'STLError'

        """
        
        tx_ports = self._validate_port_list(tx_ports, allow_empty = True)
        rx_ports = self._validate_port_list(rx_ports, allow_empty = True)
        merge_ports = set(tx_ports + rx_ports)
        
        if not merge_ports:
            raise STLError("start_capture - must get at least one port to capture")
            
        # check arguments
        validate_type('limit', limit, (int))
        if limit <= 0:
            raise STLError("'limit' must be a positive value")

        non_service_ports =  list_difference(set(tx_ports + rx_ports), self.get_service_enabled_ports())
        if non_service_ports:
            raise STLError("Port(s) {0} are not under service mode. PCAP capturing requires all ports to be in service mode".format(non_service_ports))
        
            
        self.logger.pre_cmd("Starting PCAP capturing up to {0} packets".format(limit))
        
        rc = self._transmit("capture", params = {'command': 'start', 'limit': limit, 'tx': tx_ports, 'rx': rx_ports})
        self.logger.post_cmd(rc)


        if not rc:
            raise STLError(rc)

        return {'id': rc.data()['capture_id'], 'ts': rc.data()['start_ts']}


        
                
    def __fetch_capture_packets (self, capture_id, output_filename, pkt_count):
        self.logger.pre_cmd("Writing {0} packets to '{1}'".format(pkt_count, output_filename))

        # create a PCAP file
        writer = RawPcapWriter(output_filename, linktype = 1)
        writer._write_header(None)
        
        # fetch
        pending = pkt_count
        rc = RC_OK()
        while pending > 0:
            rc = self._transmit("capture", params = {'command': 'fetch', 'capture_id': capture_id, 'pkt_limit': 50})
            if not rc:
                self.logger.post_cmd(rc)
                raise STLError(rc)
        
            pkts      = rc.data()['pkts']
            pending   = rc.data()['pending']
            start_ts  = rc.data()['start_ts']
            
            for pkt in pkts:
                ts = pkt['ts'] - start_ts
                ts_sec  = int(ts)
                ts_usec = int( (ts - ts_sec) * 1e6 )
                
                pkt_bin = base64.b64decode(pkt['binary'])
                writer._write_packet(pkt_bin, sec = ts_sec, usec = ts_usec)
                
            
            
        
        self.logger.post_cmd(rc)
        
        
    @__api_check(True)
    def stop_capture (self, capture_id, output_filename = None):
        """
            Stops an active capture

            :parameters:
                capture_id        - an active capture ID to stop
                output_filename   - output filename to save capture

            :raises:
                + :exe:'STLError'

        """
        
        # stopping a capture requires:
        # 1. stopping
        # 2. fetching
        # 3. saving to file
        
        # stop
        
        self.logger.pre_cmd("Stopping PCAP capture {0}".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'stop', 'capture_id': capture_id})
        self.logger.post_cmd(rc)
        if not rc:
            raise STLError(rc)
        
        # pkt count
        pkt_count = rc.data()['pkt_count']
        
        # fetch packets    
        if output_filename:
            self.__fetch_capture_packets(capture_id, output_filename, pkt_count)
        
        # remove
        self.logger.pre_cmd("Removing PCAP capture {0} from server".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': capture_id})
        self.logger.post_cmd(rc)
        if not rc:
            raise STLError(rc)
        

    @__api_check(True)
    def get_capture_status (self):
        """
            returns a list of all active captures
            each element in the list is an object containing
            info about the capture

        """

        rc = self._transmit("capture", params = {'command': 'status'})

        if not rc:
            raise STLError(rc)

        return rc.data()

    @__api_check(True)
    def remove_all_captures (self):
        """
            Removes any existing captures
        """
        captures = self.get_capture_status()
        
        self.logger.pre_cmd("Removing all PCAP captures from server")
        
        for c in captures:
            # remove
            rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': c['id']})
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
                client.logger.log("\nAction has failed with the following error:\n" + format_text(e.brief() + "\n", 'bold'))
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
                                         parsing_opts.PING_IPV4,
                                         parsing_opts.PKT_SIZE,
                                         parsing_opts.PING_COUNT)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts
            
        # IP ping
        # source ports maps to ports as a single port
        self.ping_ip(opts.ports[0], opts.ping_ipv4, opts.pkt_size, opts.count)

        
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
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        self.connect()
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


    #
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
                                         parsing_opts.CORE_MASK_GROUP)

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
        try:
            self.__pre_start_check(opts.ports, opts.force)
        except STLError as e:
            msg = e.brief()
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)
            
                
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
                       core_mask)

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

        active_ports = list(set(self.get_active_ports()).intersection(opts.ports))

        if active_ports:
            if not opts.force:
                msg = "Port(s) {0} are active - please stop them or add '--force'\n".format(active_ports)
                self.logger.log(format_text(msg, 'bold'))
                return RC_ERR(msg)
            else:
                self.stop(active_ports)


        if opts.remote:
            self.push_remote(opts.file[0],
                             ports          = opts.ports,
                             ipg_usec       = opts.ipg_usec,
                             min_ipg_usec   = opts.min_ipg_usec,
                             speedup        = opts.speedup,
                             count          = opts.count,
                             duration       = opts.duration,
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
                                         )

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports())
        if not opts:
            return opts

        opts.prom            = parsing_opts.ON_OFF_DICT.get(opts.prom)
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
                               opts.flow_ctrl)
                         
               
             

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
        

