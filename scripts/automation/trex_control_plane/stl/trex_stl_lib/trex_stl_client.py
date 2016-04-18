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
from .utils.common import list_intersect, list_difference, is_sub_list
from .utils.text_opts import *
from functools import wraps

from collections import namedtuple
from yaml import YAMLError
import time
import datetime
import re
import random
import json
import traceback

############################     logger     #############################
############################                #############################
############################                #############################

# logger API for the client
class LoggerApi(object):
    # verbose levels
    VERBOSE_QUIET   = 0
    VERBOSE_REGULAR = 1
    VERBOSE_HIGH    = 2

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
    def log (self, msg, level = VERBOSE_REGULAR, newline = True):
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
    def supress (self):
        class Supress(object):
            def __init__ (self, logger):
                self.logger = logger

            def __enter__ (self):
                self.saved_level = self.logger.get_verbose()
                self.logger.set_verbose(LoggerApi.VERBOSE_QUIET)

            def __exit__ (self, type, value, traceback):
                self.logger.set_verbose(self.saved_level)

        return Supress(self)



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
            self.__add_event_log('local', 'info', msg, True)
            self.client.connected = False


    def on_async_alive (self):
        pass

    

    def on_async_rx_stats_event (self, data, baseline):
        self.client.flow_stats.update(data, baseline)


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
    def on_async_event (self, type, data):
        # DP stopped
        show_event = False

        # port started
        if (type == 0):
            port_id = int(data['port_id'])
            ev = "Port {0} has started".format(port_id)
            self.__async_event_port_started(port_id)

        # port stopped
        elif (type == 1):
            port_id = int(data['port_id'])
            ev = "Port {0} has stopped".format(port_id)

            # call the handler
            self.__async_event_port_stopped(port_id)


        # port paused
        elif (type == 2):
            port_id = int(data['port_id'])
            ev = "Port {0} has paused".format(port_id)

            # call the handler
            self.__async_event_port_paused(port_id)

        # port resumed
        elif (type == 3):
            port_id = int(data['port_id'])
            ev = "Port {0} has resumed".format(port_id)

            # call the handler
            self.__async_event_port_resumed(port_id)

        # port finished traffic
        elif (type == 4):
            port_id = int(data['port_id'])
            ev = "Port {0} job done".format(port_id)

            # call the handler
            self.__async_event_port_job_done(port_id)
            show_event = True

        # port was acquired - maybe stolen...
        elif (type == 5):
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
        elif (type == 6):
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


        # server stopped
        elif (type == 100):
            ev = "Server has stopped"
            self.__async_event_server_stopped()
            show_event = True


        else:
            # unknown event - ignore
            return


        self.__add_event_log('server', 'info', ev, show_event)


    # private functions

    def __async_event_port_job_done (self, port_id):
        self.client.ports[port_id].async_event_port_job_done()

    def __async_event_port_stopped (self, port_id):
        self.client.ports[port_id].async_event_port_stopped()


    def __async_event_port_started (self, port_id):
        self.client.ports[port_id].async_event_port_started()


    def __async_event_port_paused (self, port_id):
        self.client.ports[port_id].async_event_port_paused()


    def __async_event_port_resumed (self, port_id):
        self.client.ports[port_id].async_event_port_resumed()


    def __async_event_port_acquired (self, port_id, who):
        self.client.ports[port_id].async_event_acquired(who)

    def __async_event_port_released (self, port_id):
        self.client.ports[port_id].async_event_released()

    def __async_event_server_stopped (self):
        self.client.connected = False


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

    def transmit(self, method_name, params = None, api_class = 'core'):
        if self.virtual:
            self._prompt_virtual_tx_msg()
            _, msg = self.rpc_link.create_jsonrpc_v2(method_name, params, api_class)
            print(msg)
            return
        else:
            return self.rpc_link.invoke_rpc_method(method_name, params, api_class)

    def transmit_batch(self, batch_list):
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
            return batch.invoke()

    def _prompt_virtual_tx_msg(self):
        print("Transmitting virtually over tcp://{server}:{port}".format(server=self.server,
                                                                         port=self.port))



############################     client     #############################
############################                #############################
############################                #############################

class STLClient(object):
    """TRex Stateless client object - gives operations per TRex/user"""

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
            :caption: Example


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

        self.stats_generator = trex_stl_stats.CTRexInfoGenerator(self.global_stats,
                                                                 self.ports,
                                                                 self.flow_stats)


        # API classes
        self.api_vers = [ {'type': 'core', 'major': 1, 'minor':2 }
                        ]
        self.api_h = {'core': None}

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
    def __acquire (self, port_id_list = None, force = False):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].acquire(force))

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


    def __start (self, multiplier, duration, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].start(multiplier, duration, force))

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


    def __validate (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].validate())

        return rc


    def __set_port_attr (self, port_id_list = None, attr_dict = None):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].set_attr(attr_dict))

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

        # cache supported commands
        rc = self._transmit("get_supported_cmds")
        if not rc:
            return rc

        self.supported_cmds = rc.data()

        # create ports
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
    def __clear_stats(self, port_id_list, clear_global, clear_flow_stats):

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        if clear_global:
            self.global_stats.clear_stats()

        if clear_flow_stats:
            self.flow_stats.clear_stats()

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

        return stats


    ############ functions used by other classes but not users ##############

    def _validate_port_list (self, port_id_list):
        # listfiy single int
        if isinstance(port_id_list, int):
            port_id_list = [port_id_list]

        # should be a list
        if not isinstance(port_id_list, list):
            raise STLTypeError('port_id_list', type(port_id_list), list)

        if not port_id_list:
            raise STLError('No ports provided')

        valid_ports = self.get_all_ports()
        for port_id in port_id_list:
            if not port_id in valid_ports:
                raise STLError("Port ID '{0}' is not a valid port ID - valid values: {1}".format(port_id, valid_ports))

        return port_id_list


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

                ret = f(*args, **kwargs)
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
    def get_stats (self, ports = None, async_barrier = True):
        # by default use all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # check async barrier
        if not type(async_barrier) is bool:
            raise STLArgumentError('async_barrier', async_barrier)


        # if the user requested a barrier - use it
        if async_barrier:
            rc = self.async_client.barrier()
            if not rc:
                raise STLError(rc)

        return self.__get_stats(ports)


    def get_events (self, ev_type_filter = None):
        """ 
        returns all the logged events

        :parameters:
          ev_type_filter - 'info', 'warning' or a list of those
                           default is no filter

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

        return [self.ports[port_id].get_info() for port_id in ports]


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
        def connect(self):

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
    def acquire (self, ports = None, force = False):
        """
            Acquires ports for executing commands

            :parameters:
                ports : list
                    Ports on which to execute the command
                force : bool
                    Force acquire the ports.

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

        rc = self.__acquire(ports, force)

        self.logger.post_cmd(rc)

        if not rc:
            # cleanup
            self.__release(ports)
            raise STLError(rc)


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
    def ping(self):
        """
            Pings the server

            :parameters:
                None


            :raises:
                + :exc:`STLError`

        """

        self.logger.pre_cmd( "Pinging the server on '{0}' port '{1}': ".format(self.connection_info['server'],
                                                                               self.connection_info['sync_port']))
        rc = self._transmit("ping", api_class = None)
        
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


    @__api_check(True)
    def reset(self, ports = None):
        """
            Force acquire ports, stop the traffic, remove all streams and clear stats

            :parameters:
                ports : list
                   Ports on which to execute the command


            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        self.acquire(ports, force = True)
        self.stop(ports, rx_delay_ms = 0)
        self.remove_all_streams(ports)
        self.clear_stats(ports)


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


   
    @__api_check(True)
    def start (self,
               ports = None,
               mult = "1",
               force = False,
               duration = -1,
               total = False):
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


            :raises:
                + :exc:`STLError`

        """


        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)


        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = False,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)

        # some type checkings

        if not type(force) is bool:
            raise STLArgumentError('force', force)

        if not isinstance(duration, (int, float)):
            raise STLArgumentError('duration', duration)

        if not type(total) is bool:
            raise STLArgumentError('total', total)


        # verify ports are stopped or force stop them
        active_ports = list(set(self.get_active_ports()).intersection(ports))
        if active_ports:
            if not force:
                raise STLError("Port(s) {0} are active - please stop them or specify 'force'".format(active_ports))
            else:
                rc = self.stop(active_ports)
                if not rc:
                    raise STLError(rc)


        # start traffic
        self.logger.pre_cmd("Starting traffic on port(s) {0}:".format(ports))
        rc = self.__start(mult_obj, duration, ports, force)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)



    
    @__api_check(True)
    def stop (self, ports = None, rx_delay_ms = 10):
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

        ports = ports if ports is not None else self.get_active_ports()
        ports = self._validate_port_list(ports)

        if not ports:
            return

        self.logger.pre_cmd("Stopping traffic on port(s) {0}:".format(ports))
        rc = self.__stop(ports)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

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


        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = True,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)

        # verify total
        if not type(total) is bool:
            raise STLArgumentError('total', total)


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
    def validate (self, ports = None, mult = "1", duration = "-1", total = False):
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


        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = True,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise STLArgumentError('mult', mult)


        if not isinstance(duration, (int, float)):
            raise STLArgumentError('duration', duration)


        self.logger.pre_cmd("Validating streams on port(s) {0}:".format(ports))
        rc = self.__validate(ports)
        self.logger.post_cmd(rc)


        for port in ports:
            self.ports[port].print_profile(mult_obj, duration)


    @__api_check(False)
    def clear_stats (self, ports = None, clear_global = True, clear_flow_stats = True):
        """
            Clear stats on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

                clear_global : bool
                    Clear the global stats

                clear_flow_stats : bool 
                    Clear the flow stats

            :raises:
                + :exc:`STLError`

        """

        ports = ports if ports is not None else self.get_all_ports()
        ports = self._validate_port_list(ports)

        # verify clear global
        if not type(clear_global) is bool:
            raise STLArgumentError('clear_global', clear_global)


        rc = self.__clear_stats(ports, clear_global, clear_flow_stats)
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
    def wait_on_traffic (self, ports = None, timeout = 60, rx_delay_ms = 10):
        """
            Block until traffic on specified port(s) has ended

            :parameters:
                ports : list
                    Ports on which to execute the command

                timeout : int
                    timeout in seconds

                rx_delay_ms : int
                    time to wait until RX filters are removed
                    this value should reflect the time it takes
                    packets which were transmitted to arrive
                    to the destination.
                    after this time the RX filters will be removed


            :raises:
                + :exc:`STLTimeoutError` - in case timeout has expired
                + :exe:'STLError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)


        expr = time.time() + timeout

        # wait while any of the required ports are active
        while set(self.get_active_ports()).intersection(ports):
            time.sleep(0.01)
            if time.time() > expr:
                raise STLTimeoutError(timeout)

        # remove any RX filters
        rc = self._remove_rx_filters(ports, rx_delay_ms = rx_delay_ms)
        if not rc:
            raise STLError(rc)


    @__api_check(True)
    def set_port_attr (self, ports = None, promiscuous = None):
        """
            Set port attributes

            :parameters:
                promiscuous - True or False

            :raises:
                None

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self._validate_port_list(ports)

        # check arguments
        validate_type('promiscuous', promiscuous, (bool, type(None)))

        # build attributes
        attr_dict = {}
        if promiscuous is not None:
            attr_dict['promiscuous'] = {'enabled': bool(promiscuous)}
        
        # no attributes to set
        if not attr_dict:
            return

        self.logger.pre_cmd("Applying attributes on port(s) {0}:".format(ports))
        rc = self.__set_port_attr(ports, attr_dict)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

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
                client.logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
                return

            # if got true - print time
            if rc:
                delta = time.time() - time1
                client.logger.log(format_time(delta) + "\n")


        return wrap

    @__console
    def ping_line (self, line):
        '''pings the server'''
        self.ping()
        return True

    @__console
    def connect_line (self, line):
        '''Connects to the TRex server and acquire ports'''
        parser = parsing_opts.gen_parser(self,
                                         "connect",
                                         self.connect_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split(), default_ports = self.get_all_ports())
        if opts is None:
            return

        self.connect()
        self.acquire(ports = opts.ports, force = opts.force)

        # true means print time
        return True


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
        if opts is None:
            return

        # filter out all the already owned ports
        ports = list_difference(opts.ports, self.get_acquired_ports())
        if not ports:
            self.logger.log("acquire - all port(s) {0} are already acquired".format(opts.ports))
            return

        self.acquire(ports = ports, force = opts.force)

        # true means print time
        return True


    #
    @__console
    def release_line (self, line):
        '''Release ports\n'''

        parser = parsing_opts.gen_parser(self,
                                         "release",
                                         self.release_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports())
        if opts is None:
            return

        ports = list_intersect(opts.ports, self.get_acquired_ports())
        if not ports:
            if not opts.ports:
                self.logger.log("release - no acquired ports")
                return
            else:
                self.logger.log("release - none of port(s) {0} are acquired".format(opts.ports))
                return

        
        self.release(ports = ports)

        # true means print time
        return True


    @__console
    def reacquire_line (self, line):
        '''reacquire all the ports under your username'''
        my_unowned_ports = list_difference([k for k, v in self.ports.items() if v.get_owner() == self.username], self.get_acquired_ports())
        if not my_unowned_ports:
            self.logger.log("reacquire - no unowned ports under '{0}'".format(self.username))
            return

        self.acquire(ports = my_unowned_ports, force = True)
        return True

    @__console
    def disconnect_line (self, line):
        self.disconnect()
        


    @__console
    def reset_line (self, line):
        '''Reset ports - if no ports are provided all acquired ports will be reset'''

        parser = parsing_opts.gen_parser(self,
                                         "reset",
                                         self.reset_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if opts is None:
            return

        self.reset(ports = opts.ports)

        # true means print time
        return True



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
                                         parsing_opts.DRY_RUN)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        if opts is None:
            return

        active_ports = list_intersect(self.get_active_ports(), opts.ports)
        if active_ports:
            if not opts.force:
                msg = "Port(s) {0} are active - please stop them or add '--force'\n".format(active_ports)
                self.logger.log(format_text(msg, 'bold'))
                return
            else:
                self.stop(active_ports)

        
        # default value for tunables (empty)
        tunables = [{}] * len(opts.ports)

        # process tunables
        if opts.tunables:

            # for one tunable - duplicate for all ports
            if len(opts.tunables) == 1:
                tunables = opts.tunables * len(opts.ports)

            else:
                # must be exact
                if len(opts.ports) != len(opts.tunables):
                    self.logger.log('tunables section count must be 1 or exactly as the number of ports: got {0}'.format(len(opts.tunables)))
                    return
                tunables = opts.tunables
            


        # remove all streams
        self.remove_all_streams(opts.ports)

        # pack the profile
        try:
            for port, t in zip(opts.ports, tunables):

                profile = STLProfile.load(opts.file[0],
                                          direction = t.get('direction', port % 2),
                                          port_id = port,
                                          **t)

                self.add_streams(profile.get_streams(), ports = port)

        except STLError as e:
            self.logger.log(format_text("\nError while loading profile '{0}'\n".format(opts.file[0]), 'bold'))
            self.logger.log(e.brief() + "\n")
            return


        if opts.dry:
            self.validate(opts.ports, opts.mult, opts.duration, opts.total)
        else:
            self.start(opts.ports,
                       opts.mult,
                       opts.force,
                       opts.duration,
                       opts.total)

        # true means print time
        return True



    @__console
    def stop_line (self, line):
        '''Stop active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "stop",
                                         self.stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_active_ports(), verify_acquired = True)
        if opts is None:
            return


        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_active_ports())
        if not ports:
            if not opts.ports:
                self.logger.log('stop - no active ports')
            else:
                self.logger.log('stop - no active traffic on ports {0}'.format(opts.ports))
            return

        # call API
        self.stop(ports)

        # true means print time
        return True


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
        if opts is None:
            return


        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_active_ports())
        if not ports:
            if not opts.ports:
                self.logger.log('update - no active ports')
            else:
                self.logger.log('update - no active traffic on ports {0}'.format(opts.ports))
            return

        self.update(ports, opts.mult, opts.total, opts.force)

        # true means print time
        return True


    @__console
    def pause_line (self, line):
        '''Pause active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "pause",
                                         self.pause_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_transmitting_ports(), verify_acquired = True)
        if opts is None:
            return

        # check for already paused case
        if opts.ports and is_sub_list(opts.ports, self.get_paused_ports()):
            self.logger.log('pause - all of port(s) {0} are already paused'.format(opts.ports))
            return

        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_transmitting_ports())
        if not ports:
            if not opts.ports:
                self.logger.log('pause - no transmitting ports')
            else:
                self.logger.log('pause - none of ports {0} are transmitting'.format(opts.ports))
            return

        self.pause(ports)

        # true means print time
        return True


    @__console
    def resume_line (self, line):
        '''Resume active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "resume",
                                         self.resume_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_paused_ports(), verify_acquired = True)
        if opts is None:
            return

        # find the relevant ports
        ports = list_intersect(opts.ports, self.get_paused_ports())
        if not ports:
            if not opts.ports:
                self.logger.log('resume - no paused ports')
            else:
                self.logger.log('resume - none of ports {0} are paused'.format(opts.ports))
            return


        self.resume(ports)

        # true means print time
        return True

   
    @__console
    def clear_stats_line (self, line):
        '''Clear cached local statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "clear",
                                         self.clear_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())

        if opts is None:
            return

        self.clear_stats(opts.ports)




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

        if opts is None:
            return

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

        if opts is None:
            return

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
        if opts is None:
            return

        self.validate(opts.ports)



 
    @__console
    def push_line (self, line):
        '''Push a pcap file '''

        parser = parsing_opts.gen_parser(self,
                                         "push",
                                         self.push_line.__doc__,
                                         parsing_opts.FILE_PATH,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.COUNT,
                                         parsing_opts.DURATION,
                                         parsing_opts.IPG,
                                         parsing_opts.SPEEDUP,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        active_ports = list(set(self.get_active_ports()).intersection(opts.ports))

        if active_ports:
            if not opts.force:
                msg = "Port(s) {0} are active - please stop them or add '--force'\n".format(active_ports)
                self.logger.log(format_text(msg, 'bold'))
                return
            else:
                self.stop(active_ports)

        # pcap injection removes all previous streams from the ports
        self.remove_all_streams(ports = opts.ports)
            
        profile = STLProfile.load_pcap(opts.file[0],
                                       opts.ipg_usec,
                                       opts.speedup,
                                       opts.count)

        id_list = self.add_streams(profile.get_streams(), opts.ports)
        self.start(ports = opts.ports, duration = opts.duration, force = opts.force)

        return True



    @__console
    def set_port_attr_line (self, line):
        '''Sets port attributes '''

        parser = parsing_opts.gen_parser(self,
                                         "port",
                                         self.set_port_attr_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.PROMISCUOUS_SWITCH)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        self.set_port_attr(opts.ports, opts.prom)

    

    @__console
    def show_profile_line (self, line):
        '''Shows profile information'''

        parser = parsing_opts.gen_parser(self,
                                         "port",
                                         self.show_profile_line.__doc__,
                                         parsing_opts.FILE_PATH)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

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
            self.logger.log('Tunables:         {:^12}'.format(['{0} = {1}'.format(k ,v) for k, v in info['tunables'].items()]))

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
        if opts is None:
            return


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
            self.logger.log(format_text("\nEvent log was cleared\n"))

