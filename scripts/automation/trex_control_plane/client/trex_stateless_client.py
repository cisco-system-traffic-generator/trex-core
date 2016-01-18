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

############################     logger     #############################
############################                #############################
############################                #############################

class STLFailure(Exception):
    def __init__ (self, rc_or_str):
        self.msg = str(rc_or_str)

    def __str__ (self):
        exc_type, exc_obj, exc_tb = sys.exc_info()
        fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]


        s = "\n******\n"
        s += "Error reported at {0}:{1}\n\n".format(format_text(fname, 'bold'), format_text(exc_tb.tb_lineno), 'bold')
        s += "specific error:\n\n'{0}'\n".format(format_text(self.msg, 'bold'))
        
        return s


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

    # annotates an action with a RC - writes to log the result
    def annotate (self, rc, desc = None, show_status = True):
        rc.annotate(self.log, desc, show_status)


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
        self.client.read_only = True

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
                 verbose_level = LoggerApi.VERBOSE_REGULAR,
                 logger = None,
                 virtual = False):


        self.username   = username
         
        # init objects
        self.ports = {}
        self.server_version = {}
        self.system_info = {}
        self.session_id = random.getrandbits(32)
        self.read_only = False
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
    # mode can be RW - read / write, RWF - read write with force , RO - read only
    def __connect(self, mode = "RW"):

        # first disconnect if already connected
        if self.is_connected():
            self.__disconnect()

        # clear this flag
        self.connected = False

        # connect sync channel
        rc = self.comm_link.connect()
        if not rc:
            return rc

        # connect async channel
        rc = self.async_client.connect()
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

        # acquire all ports
        if mode == "RW":
            rc = self.__acquire(force = False)

            # fallback to read only if failed
            if not rc:
                self.logger.annotate(rc, show_status = False)
                self.logger.log(format_text("Switching to read only mode - only few commands will be available", 'bold'))

                self.__release(self.get_acquired_ports())
                self.read_only = True
            else:
                self.read_only = False

        elif mode == "RWF":
            rc = self.__acquire(force = True)
            if not rc:
                return rc
            self.read_only = False

        elif mode == "RO":
            # no acquire on read only
            rc = RC_OK()
            self.read_only = True


        
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
                rc = self.cmd_stop(active_ports)
                if not rc:
                    return rc


        rc = self.__remove_all_streams(port_id_list)
        self.logger.annotate(rc,"Removing all streams from port(s) {0}:".format(port_id_list))
        if rc.bad():
            return rc
 

        rc = self.__add_stream_pack(stream_list, port_id_list)
        self.logger.annotate(rc,"Attaching {0} streams to port(s) {1}:".format(len(stream_list.compiled), port_id_list))
        if rc.bad():
            return rc
 
        # when not on dry - start the traffic , otherwise validate only
        if not dry:
            rc = self.__start_traffic(mult, duration, port_id_list)
            self.logger.annotate(rc,"Starting traffic on port(s) {0}:".format(port_id_list))
 
            return rc
        else:
            rc = self.__validate(port_id_list)
            self.logger.annotate(rc,"Validating traffic profile on port(s) {0}:".format(port_id_list))
 
            if rc.bad():
                return rc
 
            # show a profile on one port for illustration
            self.ports[port_id_list[0]].print_profile(mult, duration)
 
            return rc



    def __verify_port_id_list (self, port_id_list):
        # check arguments
        if not isinstance(port_id_list, list):
            return RC_ERR("ports should be an instance of 'list'")

        # all ports are valid ports
        if not all([port_id in self.get_all_ports() for port_id in port_id_list]):
            return RC_ERR("Port IDs valid values are '{0}' but provided '{1}'".format(self.get_all_ports(), port_id_list))

        return RC_OK()


    def __verify_mult (self, mult, strict):
        if not isinstance(mult, dict):
            return RC_ERR("mult should be an instance of dict")

        types = ["raw", "bps", "pps", "percentage"]
        if not mult.get('type', None) in types:
            return RC_ERR("mult should contain 'type' field of one of '{0}'".format(types))

        if strict:
            ops = ["abs"]
        else:
            ops = ["abs", "add", "sub"]
        if not mult.get('op', None) in ops:
            return RC_ERR("mult should contain 'op' field of one of '{0}'".format(ops))

        return RC_OK()

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
                    self.logger.annotate(rc)
                    return rc

                out += stream_list
            else:
                return RC_ERR("unknown profile '{0}'".format(profile))


        return RC_OK()



        # stream list
        if opts.db:
            stream_list = self.streams_db.get_stream_pack(opts.db)
            rc = RC(stream_list != None)
            self.logger.annotate(rc,"Load stream pack (from DB):")
            if rc.bad():
                return RC_ERR("Failed to load stream pack")

        else:
            # load streams from file
            stream_list = None
            try:
              stream_list = self.streams_db.load_yaml_file(opts.file[0])
            except Exception as e:
                s = str(e)
                rc=RC_ERR(s)
                self.logger.annotate(rc)
                return rc

            rc = RC(stream_list != None)
            self.logger.annotate(rc,"Load stream pack (from file):")
            if stream_list == None:
                return RC_ERR("Failed to load stream pack")

    ############ functions used by other classes but not users ##############

    def _validate_port_list(self, port_id_list):
        if not isinstance(port_id_list, list):
            print type(port_id_list)
            return False

        # check each item of the sequence
        return all([ (port_id >= 0) and (port_id < self.get_port_count())
                      for port_id in port_id_list ])


    # transmit request on the RPC link
    def _transmit(self, method_name, params={}):
        return self.comm_link.transmit(method_name, params)

    # transmit batch request on the RPC link
    def _transmit_batch(self, batch_list):
        return self.comm_link.transmit_batch(batch_list)

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

            client = args[0]
            client.logger.log(format_time(delta) + "\n")

            return ret

        return wrap



    ########## port commands ##############
 
    ######################### Console (high level) API #########################

    # stop cmd
    def cmd_stop (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)

        rc = self.__stop_traffic(active_ports)
        self.logger.annotate(rc,"Stopping traffic on port(s) {0}:".format(port_id_list))
        if rc.bad():
            return rc

        return RC_OK()

    # update cmd
    def cmd_update (self, port_id_list, mult):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)

        rc = self.__update_traffic(mult, active_ports)
        self.logger.annotate(rc,"Updating traffic on port(s) {0}:".format(port_id_list))

        return rc

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

    # pause cmd
    def cmd_pause (self, port_id_list):

        # find the relevant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on provided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)

        rc = self.__pause_traffic(active_ports)
        self.logger.annotate(rc,"Pausing traffic on port(s) {0}:".format(port_id_list))
        return rc



    # resume cmd
    def cmd_resume (self, port_id_list):

        # find the relveant ports
        active_ports = list(set(self.get_active_ports()).intersection(port_id_list))

        if not active_ports:
            msg = "No active traffic on porvided ports"
            self.logger.log(format_text(msg, 'bold'))
            return RC_ERR(msg)

        rc = self.__resume_traffic(active_ports)
        self.logger.annotate(rc,"Resume traffic on port(s) {0}:".format(port_id_list))
        return rc


   


    # validate port(s) profile
    def cmd_validate (self, port_id_list):
        rc = self.__validate(port_id_list)
        self.logger.annotate(rc,"Validating streams on port(s) {0}:".format(port_id_list))
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
            self.logger.log(format_text("\n*** DRY RUN ***", 'bold'))

        if opts.db:
            stream_list = self.streams_db.get_stream_pack(opts.db)
            rc = RC(stream_list != None)
            self.logger.annotate(rc,"Load stream pack (from DB):")
            if rc.bad():
                return RC_ERR("Failed to load stream pack")

        else:
            # load streams from file
            stream_list = None
            try:
              stream_list = self.streams_db.load_yaml_file(opts.file[0])
            except Exception as e:
                s = str(e)
                rc=RC_ERR(s)
                self.logger.annotate(rc)
                return rc

            rc = RC(stream_list != None)
            self.logger.annotate(rc,"Load stream pack (from file):")
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

    def __verify_connected(f):
        #@wraps(f)
        def wrap(*args):
            inst = args[0]
            func_name = f.__name__

            if not inst.stateless_client.is_connected():
                return RC_ERR("cannot execute '{0}' while client is disconnected".format(func_name))

            ret = f(*args)
            return ret

        return wrap



    ############################     API     #############################
    ############################             #############################
    ############################             #############################


    ############################   Getters   #############################
    ############################             #############################
    ############################             #############################


    # return verbose level of the logger
    def get_verbose (self):
        return self.logger.get_verbose()

    # is the client on read only mode ?
    def is_read_only (self):
        return self.read_only

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
        return self.ports.get(port_id, RC_ERR("invalid port id"))

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
    def connect (self, mode = "RW"):
        modes = ['RO', 'RW', 'RWF']
        if not mode in modes:
            return RC_ERR("invalid mode '{0}'".format(mode))
        
        rc = self.__connect(mode)
        self.logger.annotate(rc)

        if not rc:
            raise STLFailure(rc)

        return rc


    # disconnects from the server
    def disconnect (self, annotate = True):
        rc = self.__disconnect()
        if annotate:
            self.logger.annotate(rc, "Disconnecting from server at '{0}':'{1}'".format(self.connection_info['server'],
                                                                                       self.connection_info['sync_port']))
        if not rc:
            raise STLFailure(rc)

        return rc


    # teardown - call after test is done
    def teardown (self):
        # for now, its only disconnect
        rc = self.__disconnect()
        if not rc:
            raise STLFailure(rc)

        return rc


    # pings the server on the RPC channel
    def ping(self):
        rc = self.__ping()
        self.logger.annotate(rc, "Pinging the server on '{0}' port '{1}': ".format(self.connection_info['server'],
                                                                                   self.connection_info['sync_port']))

        if not rc:
            raise STLFailure(rc)

        return rc


    # reset the server by performing
    # force acquire, stop, and remove all streams
    def reset(self):

        rc = self.__acquire(force = True)
        self.logger.annotate(rc, "Force acquiring all ports:")
        if not rc:
            raise STLFailure(rc)


        # force stop all ports
        rc = self.__stop_traffic(self.get_all_ports(), True)
        self.logger.annotate(rc,"Stop traffic on all ports:")
        if not rc:
            raise STLFailure(rc)


        # remove all streams
        rc = self.__remove_all_streams(self.get_all_ports())
        self.logger.annotate(rc,"Removing all streams from all ports:")
        if not rc:
            raise STLFailure(rc)

        # TODO: clear stats
        return RC_OK()

    # start cmd
    def start (self,
               profiles,
               ports = None,
               mult = "1",
               force = False,
               duration = -1,
               dry = False):


        # by default use all ports
        if ports == None:
            ports = self.get_all_ports()

        # verify valid port id list
        rc = self.__verify_port_id_list(ports)
        if not rc:
            raise STLFailure(rc)


        # verify multiplier
        try:
            result = parsing_opts.match_multiplier_common(mult)
        except argparse.ArgumentTypeError:
            raise STLFailure("bad format for multiplier: {0}".format(mult))

        # process profiles
        stream_list = []
        rc = self.__process_profiles(profiles, stream_list)
        if not rc:
            raise STLFailure(rc)

     


    ############################   Line       #############################
    ############################   Commands   #############################
    ############################              #############################

    def connect_line (self, line):
        '''Connects to the TRex server'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "connect",
                                         self.connect_line.__doc__,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())

        if opts is None:
            return RC_ERR("bad command line parameters")

        # call the API
        if opts.force:
            rc = self.connect(mode = "RWF")
        else:
            rc = self.connect(mode = "RW")


    def disconnect_line (self, line):
        return self.disconnect()

    def reset_line (self, line):
        return self.reset()
