#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages

from client_utils.jsonrpc_client import JsonRpcClient, BatchMessage
from client_utils import general_utils
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
import random
from trex_port import Port
from common.trex_types import *
from trex_async_client import CTRexAsyncClient

# basic error for API
class STLError(Exception):
    def __init__ (self, msg):
        self.msg = str(msg)

    def __str__ (self):
        exc_type, exc_obj, exc_tb = sys.exc_info()
        fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]


        s = "\n******\n"
        s += "Error at {0}:{1}\n\n".format(format_text(fname, 'bold'), format_text(exc_tb.tb_lineno), 'bold')
        s += "specific error:\n\n{0}\n".format(format_text(self.msg, 'bold'))
        
        return s

    def brief (self):
        return self.msg


# raised when the client state is invalid for operation
class STLStateError(STLError):
    def __init__ (self, op, state):
        self.msg = "Operation '{0}' is not valid while '{1}'".format(op, state)


# port state error
class STLPortStateError(STLError):
    def __init__ (self, port, op, state):
        self.msg = "Operation '{0}' on port '{1}' is not valid for state '{2}'".format(op, port, state)


# raised when argument is not valid for operation
class STLArgumentError(STLError):
    def __init__ (self, name, got, valid_values = None, extended = None):
        self.msg = "Argument: '{0}' invalid value: '{1}'".format(name, got)
        if valid_values:
            self.msg += " - valid values are '{0}'".format(valid_values)

        if extended:
            self.msg += "\n{0}".format(extended)


class STLTimeoutError(STLError):
    def __init__ (self, timeout):
        self.msg = "Timeout: operation took more than '{0}' seconds".format(timeout)



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
        raise Exception("implement this")

    # implemented by specific logger
    def flush(self):
        raise Exception("implement this")

    def set_verbose (self, level):
        if not level in xrange(self.VERBOSE_QUIET, self.VERBOSE_HIGH + 1):
            raise ValueError("bad value provided for logger")

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
    def write (self, msg, newline = True):
        if newline:
            print msg
        else:
            print msg,

    def flush (self):
        sys.stdout.flush()


############################     async event hander     #############################
############################                            #############################
############################                            #############################

# handles different async events given to the client
class AsyncEventHandler(object):

    def __init__ (self, client):
        self.client = client
        self.logger = self.client.logger

        self.events = []

    # public functions

    def get_events (self):
        return self.events


    def clear_events (self):
        self.events = []


    def on_async_dead (self):
        if self.client.connected:
            msg = 'lost connection to server'
            self.__add_event_log(msg, 'local', True)
            self.client.connected = False


    def on_async_alive (self):
        pass


    # handles an async stats update from the subscriber
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
                if self.client.ports.has_key(port_id):
                    if not port_id in port_stats:
                        port_stats[port_id] = {}
                    port_stats[port_id][field_name] = value
                else:
                    continue
            else:
                # no port match - general stats
                global_stats[key] = value

        # update the general object with the snapshot
        self.client.global_stats.update(global_stats)

        # update all ports
        for port_id, data in port_stats.iteritems():
            self.client.ports[port_id].port_stats.update(data)


    # dispatcher for server async events (port started, port stopped and etc.)
    def handle_async_event (self, type, data):
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
            self.__async_event_port_stopped(port_id)
            show_event = True

        # port was stolen...
        elif (type == 5):
            session_id = data['session_id']

            # false alarm, its us
            if session_id == self.client.session_id:
                return

            port_id = int(data['port_id'])
            who = data['who']

            ev = "Port {0} was forcely taken by '{1}'".format(port_id, who)

            # call the handler
            self.__async_event_port_forced_acquired(port_id)
            show_event = True

        # server stopped
        elif (type == 100):
            ev = "Server has stopped"
            self.__async_event_server_stopped()
            show_event = True


        else:
            # unknown event - ignore
            return


        self.__add_event_log(ev, 'server', show_event)


    # private functions

    def __async_event_port_stopped (self, port_id):
        self.client.ports[port_id].async_event_port_stopped()


    def __async_event_port_started (self, port_id):
        self.client.ports[port_id].async_event_port_started()


    def __async_event_port_paused (self, port_id):
        self.client.ports[port_id].async_event_port_paused()


    def __async_event_port_resumed (self, port_id):
        self.client.ports[port_id].async_event_port_resumed()


    def __async_event_port_forced_acquired (self, port_id):
        self.client.ports[port_id].async_event_forced_acquired()


    def __async_event_server_stopped (self):
        self.client.connected = False


    # add event to log
    def __add_event_log (self, msg, ev_type, show = False):

        if ev_type == "server":
            prefix = "[server]"
        elif ev_type == "local":
            prefix = "[local]"

        ts = time.time()
        st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
        self.events.append("{:<10} - {:^8} - {:}".format(st, prefix, format_text(msg, 'bold')))

        if show:
            self.logger.async_log(format_text("\n\n{:^8} - {:}".format(prefix, format_text(msg, 'bold'))))


  


############################     RPC layer     #############################
############################                   #############################
############################                   #############################

class CCommLink(object):
    """describes the connectivity of the stateless client method"""
    def __init__(self, server="localhost", port=5050, virtual=False, prn_func = None):
        self.virtual = virtual
        self.server = server
        self.port = port
        self.rpc_link = JsonRpcClient(self.server, self.port, prn_func)

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



############################     client     #############################
############################                #############################
############################                #############################

class CTRexStatelessClient(object):
    """docstring for CTRexStatelessClient"""

    def __init__(self,
                 username = general_utils.get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = LoggerApi.VERBOSE_QUIET,
                 logger = None,
                 virtual = False):


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
                                   self.logger)

        # async event handler manager
        self.event_handler = AsyncEventHandler(self)

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

        
        self.global_stats = trex_stats.CGlobalStats(self.connection_info,
                                                    self.server_version,
                                                    self.ports)

        self.stats_generator = trex_stats.CTRexInfoGenerator(self.global_stats,
                                                              self.ports)

        # stream DB
        self.streams_db = CStreamsDB()

 
 
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
             raise ValueError("bad port id list: {0}".format(port_id_list))

        for port_id in port_id_list:
            if not isinstance(port_id, int) or (port_id < 0) or (port_id > self.get_port_count()):
                raise ValueError("bad port id {0}".format(port_id))

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


    def __add_stream(self, stream_id, stream_obj, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].add_stream(stream_id, stream_obj))

        return rc



    def __add_stream_pack(self, stream_pack, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].add_streams(stream_pack))

        return rc



    def __remove_stream(self, stream_id, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].remove_stream(stream_id))

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


    def __start_traffic (self, multiplier, duration, port_id_list = None):

        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].start(multiplier, duration))

        return rc


    def __resume_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].resume())

        return rc

    def __pause_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].pause())

        return rc


    def __stop_traffic (self, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].stop(force))

        return rc


    def __update_traffic (self, mult, port_id_list = None, force = False):

        port_id_list = self.__ports(port_id_list)
        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].update(mult))

        return rc


    def __validate (self, port_id_list = None):
        port_id_list = self.__ports(port_id_list)

        rc = RC()

        for port_id in port_id_list:
            rc.add(self.ports[port_id].validate())

        return rc




    # connect to server
    def __connect(self):

        # first disconnect if already connected
        if self.is_connected():
            self.__disconnect()

        # clear this flag
        self.connected = False

        # connect sync channel
        self.logger.pre_cmd("connecting to RPC server on {0}:{1}".format(self.connection_info['server'], self.connection_info['sync_port']))
        rc = self.comm_link.connect()
        self.logger.post_cmd(rc)

        if not rc:
            return rc

        # connect async channel
        self.logger.pre_cmd("connecting to publisher server on {0}:{1}".format(self.connection_info['server'], self.connection_info['async_port']))
        rc = self.async_client.connect()
        self.logger.post_cmd(rc)

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

        # cache supported commands
        rc = self._transmit("get_supported_cmds")
        if not rc:
            return rc

        self.supported_cmds = rc.data()

        # create ports
        for port_id in xrange(self.system_info["port_count"]):
            speed = self.system_info['ports'][port_id]['speed']
            driver = self.system_info['ports'][port_id]['driver']

            self.ports[port_id] = Port(port_id,
                                       speed,
                                       driver,
                                       self.username,
                                       self.comm_link,
                                       self.session_id)


        # sync the ports
        rc = self.__sync_ports()
        if not rc:
            return rc

        
        self.connected = True
        return RC_OK()


    # disconenct from server
    def __disconnect(self):
        # release any previous acquired ports
        if self.is_connected():
            self.__release(self.get_acquired_ports())

        self.comm_link.disconnect()
        self.async_client.disconnect()

        self.connected = False

        return RC_OK()


    # ping server
    def __ping (self):
        return self._transmit("ping")


    # start command
    def __start (self, port_id_list, stream_list, mult, force, duration, dry):

        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if active_ports:
            if not force:
                msg = "Port(s) {0} are active - please stop them or add '--force'".format(active_ports)
                self.logger.log(format_text(msg, 'bold'))
                return RC_ERR(msg)
            else:
                rc = self.__stop(active_ports)
                if not rc:
                    return rc


        self.logger.pre_cmd("Removing all streams from port(s) {0}:".format(port_id_list))
        rc = self.__remove_all_streams(port_id_list)
        self.logger.post_cmd(rc)

        if not rc:
            return rc
 

        self.logger.pre_cmd("Attaching {0} streams to port(s) {1}:".format(len(stream_list.compiled), port_id_list))
        rc = self.__add_stream_pack(stream_list, port_id_list)
        self.logger.post_cmd(rc)

        if not rc:
            return rc
 
        # when not on dry - start the traffic , otherwise validate only
        if not dry:

            self.logger.pre_cmd("Starting traffic on port(s) {0}:".format(port_id_list))
            rc = self.__start_traffic(mult, duration, port_id_list)
            self.logger.post_cmd(rc)

            return rc
        else:

            self.logger.pre_cmd("Validating traffic profile on port(s) {0}:".format(port_id_list))
            rc = self.__validate(port_id_list)
            self.logger.post_cmd(rc)
            

            if rc.bad():
                return rc
 
            # show a profile on one port for illustration
            self.ports[port_id_list[0]].print_profile(mult, duration)
 
            return rc


    # stop cmd
    def __stop (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_WARN(msg)

        
        self.logger.pre_cmd("Stopping traffic on port(s) {0}:".format(port_id_list))
        rc = self.__stop_traffic(active_ports)
        self.logger.post_cmd(rc)

        if not rc:
            return rc

        return RC_OK()

    #update cmd
    def __update (self, port_id_list, mult):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_WARN(msg)

        self.logger.pre_cmd("Updating traffic on port(s) {0}:".format(port_id_list))
        rc = self.__update_traffic(mult, active_ports)
        self.logger.post_cmd(rc)

        return rc


    # pause cmd
    def __pause (self, port_id_list):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_WARN(msg)

        self.logger.pre_cmd("Pausing traffic on port(s) {0}:".format(port_id_list))
        rc = self.__pause_traffic(active_ports)
        self.logger.post_cmd(rc)
        
        return rc


    # resume cmd
    def __resume (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on porvided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_WARN(msg)

        self.logger.pre_cmd("Resume traffic on port(s) {0}:".format(port_id_list))
        rc = self.__resume_traffic(active_ports)
        self.logger.post_cmd(rc)

        return rc


    # clear stats
    def __clear_stats(self, port_id_list, clear_global):

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        if clear_global:
            self.global_stats.clear_stats()

        self.logger.pre_cmd("clearing stats on port(s) {0}:".format(port_id_list))
        rc = RC_OK()
        self.logger.post_cmd(rc)

        return RC


    # get stats
    def __get_stats (self, port_id_list):
        stats = {}

        stats['global'] = self.global_stats.get_stats()

        total = {}
        for port_id in port_id_list:
            port_stats = self.ports[port_id].get_stats()
            stats["port {0}".format(port_id)] = port_stats

            for k, v in port_stats.iteritems():
                if not k in total:
                    total[k] = v
                else:
                    total[k] += v

        stats['total'] = total

        return stats


    def __process_profiles (self, profiles, out):

        for profile in (profiles if isinstance(profiles, list) else [profiles]):
            # filename
            if isinstance(profile, str):

                if not os.path.isfile(profile):
                    return RC_ERR("file '{0}' does not exists".format(profile))

                try:
                    stream_list = self.streams_db.load_yaml_file(profile)
                except Exception as e:
                    rc = RC_ERR(str(e))
                    return rc

                out.append(stream_list)

            else:
                return RC_ERR("unknown profile '{0}'".format(profile))


        return RC_OK()


    ############ functions used by other classes but not users ##############

    def _verify_port_id_list (self, port_id_list):
        # check arguments
        if not isinstance(port_id_list, list):
            return RC_ERR("ports should be an instance of 'list' not {0}".format(type(port_id_list)))

        # all ports are valid ports
        if not port_id_list or not all([port_id in self.get_all_ports() for port_id in port_id_list]):
            return RC_ERR("")

        return RC_OK()

    def _validate_port_list(self, port_id_list):
        if not isinstance(port_id_list, list):
            return False

        # check each item of the sequence
        return (port_id_list and all([port_id in self.get_all_ports() for port_id in port_id_list]))



    # transmit request on the RPC link
    def _transmit(self, method_name, params={}):
        return self.comm_link.transmit(method_name, params)

    # transmit batch request on the RPC link
    def _transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)

    ############# helper functions section ##############

   
    ########## port commands ##############
 
    ######################### Console (high level) API #########################

 

    # clear stats
    def cmd_clear(self, port_id_list):

        for port_id in port_id_list:
            self.ports[port_id].clear_stats()

        self.global_stats.clear_stats()

        return RC_OK()


    def cmd_invalidate (self, port_id_list):
        for port_id in port_id_list:
            self.ports[port_id].invalidate_stats()

        self.global_stats.invalidate()

        return RC_OK()

  



 


    # validate port(s) profile
    def cmd_validate (self, port_id_list):
        self.logger.pre_cmd("Validating streams on port(s) {0}:".format(port_id_list))
        rc = self.__validate(port_id_list)
        self.logger.post_cmd(rc)

        return rc


    # stats
    def cmd_stats(self, port_id_list, stats_mask=set()):
        stats_opts = trex_stats.ALL_STATS_OPTS.intersection(stats_mask)

        stats_obj = {}
        for stats_type in stats_opts:
            stats_obj.update(self.stats_generator.generate_single_statistic(port_id_list, stats_type))
        return stats_obj

    def cmd_streams(self, port_id_list, streams_mask=set()):

        streams_obj = self.stats_generator.generate_streams_info(port_id_list, streams_mask)

        return streams_obj


    ############## High Level API With Parser ################


    #################################
    # ------ private methods ------ #
    @staticmethod
    def __get_mask_keys(ok_values={True}, **kwargs):
        masked_keys = set()
        for key, val in kwargs.iteritems():
            if val in ok_values:
                masked_keys.add(key)
        return masked_keys

    @staticmethod
    def __filter_namespace_args(namespace, ok_values):
        return {k: v for k, v in namespace.__dict__.items() if k in ok_values}


    # API decorator - double wrap because of argument
    def __api_check(connected = True):

        def wrap (f):
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
        self.connect(mode = "RWF")
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
        return self.logger.get_verbose()

    # is the client on read only mode ?
    def is_all_ports_acquired (self):
        return not (self.get_all_ports() == self.get_acquired_ports())

    # is the client connected ?
    def is_connected (self):
        return self.connected and self.comm_link.is_connected


    # get connection info
    def get_connection_info (self):
        return self.connection_info


    # get supported commands by the server
    def get_server_supported_cmds(self):
        return self.supported_cmds

    # get server version
    def get_server_version(self):
        return self.server_version

    # get server system info
    def get_server_system_info(self):
        return self.system_info

    # get port count
    def get_port_count(self):
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
        return self.ports.keys()

    # get all acquired ports
    def get_acquired_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_acquired()]

    # get all active ports (TX or pause)
    def get_active_ports(self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_active()]

    # get paused ports
    def get_paused_ports (self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_paused()]

    # get all TX ports
    def get_transmitting_ports (self):
        return [port_id
                for port_id, port_obj in self.ports.iteritems()
                if port_obj.is_transmitting()]


    ############################   Commands   #############################
    ############################              #############################
    ############################              #############################


    # set the log on verbose level
    def set_verbose (self, level):
        self.logger.set_verbose(level)


    # connects to the server
    # mode can be:
    # 'RO' - read only
    # 'RW' - read/write
    # 'RWF' - read write forced (take ownership)
    @__api_check(False)
    def connect (self, mode = "RW"):
        modes = ['RO', 'RW', 'RWF']
        if not mode in modes:
            raise STLArgumentError('mode', mode, modes)
        
        rc = self.__connect()
        if not rc:
            raise STLError(rc)
        
        # acquire all ports for 'RW' or 'RWF'
        if (mode == "RW") or (mode == "RWF"):
            self.acquire(ports = self.get_all_ports(), force = True if mode == "RWF" else False)




    # acquire ports
    # this is not needed if connect was called with "RW" or "RWF"
    # but for "RO" this might be needed
    @__api_check(True)
    def acquire (self, ports = None, force = False):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify ports
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        # verify valid port id list
        if force:
            self.logger.pre_cmd("Force acquiring ports {0}:".format(ports))
        else:
            self.logger.pre_cmd("Acquiring ports {0}:".format(ports))

        rc = self.__acquire(ports, force)

        self.logger.post_cmd(rc)

        if not rc:
            self.__release(ports)
            raise STLError(rc)



    # force connect syntatic sugar
    @__api_check(False)
    def fconnect (self):
        self.connect(mode = "RWF")


    # disconnects from the server
    @__api_check(False)
    def disconnect (self, log = True):
        rc = self.__disconnect()
        if log:
            self.logger.log_cmd("Disconnecting from server at '{0}':'{1}'".format(self.connection_info['server'],
                                                                                  self.connection_info['sync_port']))
        if not rc:
            raise STLError(rc)

        return rc


    # teardown - call after test is done
    # NEVER throws an exception
    @__api_check(False)
    def teardown (self, stop_traffic = True):
        
        # try to stop traffic
        if stop_traffic and self.get_active_ports():
            try:
                self.stop()
            except STLError:
                pass

        # disconnect
        self.__disconnect()



    # pings the server on the RPC channel
    @__api_check(True)
    def ping(self):
        rc = self.__ping()
        self.logger.pre_cmd( "Pinging the server on '{0}' port '{1}': ".format(self.connection_info['server'],
                                                                                   self.connection_info['sync_port']))

        if not rc:
            raise STLError(rc)

        return rc


    # reset the server by performing
    # force acquire, stop, and remove all streams
    @__api_check(True)
    def reset(self):

        self.logger.pre_cmd("Force acquiring all ports:")
        rc = self.__acquire(force = True)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


        # force stop all ports
        self.logger.pre_cmd("Stop traffic on all ports:")
        rc = self.__stop_traffic(self.get_all_ports(), True)
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)


        # remove all streams
        self.logger.pre_cmd("Removing all streams from all ports:")
        rc = self.__remove_all_streams(self.get_all_ports())
        self.logger.post_cmd(rc)

        if not rc:
            raise STLError(rc)

        # TODO: clear stats
        return RC_OK()


    # start cmd
    @__api_check(True)
    def start (self,
               profiles,
               ports = None,
               mult = "1",
               force = False,
               duration = -1,
               dry = False,
               total = False):


        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

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


        # process profiles
        stream_list = []
        rc = self.__process_profiles(profiles, stream_list)
        if not rc:
            raise STLError(rc)

        # dry run
        if dry:
            self.logger.log(format_text("\n*** DRY RUN ***", 'bold'))

        # call private method to start

        rc = self.__start(ports, stream_list[0], mult_obj, force, duration, dry)
        if not rc:
            raise STLError(rc)



    # stop traffic on ports
    @__api_check(True)
    def stop (self, ports = None):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        rc = self.__stop(ports)
        if not rc:
            raise STLError(rc)


        
    # update traffic
    @__api_check(True)
    def update (self, ports = None, mult = "1", total = False):

         # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())


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
        rc = self.__update(ports, mult_obj)
        if not rc:
            raise STLError(rc)

        return rc


    # pause traffic on ports
    @__api_check(True)
    def pause (self, ports = None):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        rc = self.__pause(ports)
        if not rc:
            raise STLError(rc)

        return rc
              
    
    # resume traffic on ports
    @__api_check(True)
    def resume (self, ports = None):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        rc = self.__resume(ports)
        if not rc:
            raise STLError(rc)

        return rc  


    # clear stats
    @__api_check(False)
    def clear_stats (self, ports = None, clear_global = True):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        # verify clear global
        if not type(clear_global) is bool:
            raise STLArgumentError('clear_global', clear_global)


        rc = self.__clear_stats(ports, clear_global)
        if not rc:
            raise STLError(rc)


    # get stats
    @__api_check(False)
    def get_stats (self, ports = None, async_barrier = True):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        # check async barrier
        if not type(async_barrier) is bool:
            raise STLArgumentError('async_barrier', async_barrier)


        # if the user requested a barrier - use it
        if async_barrier:
            rc = self.async_client.barrier()
            if not rc:
                raise STLError(rc)

        return self.__get_stats(ports)


    # wait while traffic is on, on timeout throw STLTimeoutError
    @__api_check(True)
    def wait_on_traffic (self, ports = None, timeout = 60):
        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self._validate_port_list(ports)
        if not rc:
            raise STLArgumentError('ports', ports, valid_values = self.get_all_ports())

        expr = time.time() + timeout

        # wait while any of the required ports are active
        while set(self.get_active_ports()).intersection(ports):
            time.sleep(0.01)
            if time.time() > expr:
                raise STLTimeoutError(timeout)




    ############################   Line       #############################
    ############################   Commands   #############################
    ############################              #############################
    # console decorator
    def __console(f):
        def wrap(*args):
            client = args[0]

            time1 = time.time()

            try:
                rc = f(*args)
            except STLError as e:
                client.logger.log("Log:\n" + format_text(e.brief() + "\n", 'bold'))
                return

            # don't want to print on error
            if not rc or rc.warn():
                return rc

            delta = time.time() - time1


            client.logger.log(format_time(delta) + "\n")

            return rc

        return wrap


    @__console
    def connect_line (self, line):
        '''Connects to the TRex server'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "connect",
                                         self.connect_line.__doc__,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())

        if opts is None:
            return

        # call the API
        mode = "RWF" if opts.force else "RW"
        self.connect(mode)


    @__console
    def disconnect_line (self, line):
        self.disconnect()


    @__console
    def reset_line (self, line):
        self.reset()


    @__console
    def start_line (self, line):
        '''Start selected traffic in specified ports on TRex\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "start",
                                         self.start_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.TOTAL,
                                         parsing_opts.FORCE,
                                         parsing_opts.STREAM_FROM_PATH_OR_FILE,
                                         parsing_opts.DURATION,
                                         parsing_opts.MULTIPLIER_STRICT,
                                         parsing_opts.DRY_RUN)

        opts = parser.parse_args(line.split())


        if opts is None:
            return

        # pack the profile
        profiles = [opts.file[0]]

        self.start(profiles,
                   opts.ports,
                   opts.mult,
                   opts.force,
                   opts.duration,
                   opts.dry,
                   opts.total)



    @__console
    def stop_line (self, line):
        '''Stop active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "stop",
                                         self.stop_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        self.stop(opts.ports)


    @__console
    def update_line (self, line):
        '''Update port(s) speed currently active\n'''
        parser = parsing_opts.gen_parser(self,
                                         "update",
                                         self.update_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.MULTIPLIER,
                                         parsing_opts.TOTAL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        self.update(opts.ports, opts.mult, opts.total)


    @__console
    def pause_line (self, line):
        '''Pause active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "pause",
                                         self.pause_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        self.pause(opts.ports)


    @__console
    def resume_line (self, line):
        '''Resume active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "resume",
                                         self.resume_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if opts is None:
            return

        return self.resume(opts.ports)

   
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
    def print_formatted_stats_line (self, line):
        '''Fetch statistics from TRex server by port\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.show_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STATS_MASK)

        opts = parser.parse_args(line.split())

        if opts is None:
            return None

        # determine stats mask
        mask = self.__get_mask_keys(**self.__filter_namespace_args(opts, trex_stats.ALL_STATS_OPTS))
        if not mask:
            # set to show all stats if no filter was given
            mask = trex_stats.ALL_STATS_OPTS

      
        self.print_formatted_stats()
        stats = self.get_stats(opts.ports, mask)

        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)

        return RC_OK()

    def cmd_streams_line(self, line):
        '''Fetch streams statistics from TRex server by port\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "streams",
                                         self.cmd_streams_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STREAMS_MASK)#,
                                         #parsing_opts.FULL_OUTPUT)

        opts = parser.parse_args(line.split())

        if opts is None:
            return RC_ERR("bad command line parameters")

        streams = self.cmd_streams(opts.ports, set(opts.streams))
        if not streams:
            # we got no streams running

            self.logger.log(format_text("No streams found with desired filter.\n", "bold", "magenta"))
            return RC_ERR("No streams found with desired filter.")
        else:
            # print stats to screen
            for stream_hdr, port_streams_data in streams.iteritems():
                text_tables.print_table_with_header(port_streams_data.text_table,
                                                    header= stream_hdr.split(":")[0] + ":",
                                                    untouched_header= stream_hdr.split(":")[1])
            return RC_OK()




    @__console
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
        self.logger.log(format_text("Exiting\n", 'bold'))
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

        self.logger.log(format_text("Waiting for {0} seconds...\n".format(delay_sec), 'bold'))
        time.sleep(delay_sec)

        return RC_OK()

    # run a script of commands
    def run_script_file (self, filename):

        self.logger.log(format_text("\nRunning script file '{0}'...".format(filename), 'bold'))

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

            self.logger.log(format_text("Executing line {0} : '{1}'\n".format(index, line)))

            if not cmd in cmd_table:
                print "\n*** Error at line {0} : '{1}'\n".format(index, line)
                self.logger.log(format_text("unknown command '{0}'\n".format(cmd), 'bold'))
                return False

            rc = cmd_table[cmd](args)
            if rc.bad():
                return False

        self.logger.log(format_text("\n[Done]", 'bold'))

        return True

