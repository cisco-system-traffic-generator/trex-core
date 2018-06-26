from contextlib import contextmanager
import traceback
import re
import os
import signal
import inspect
import sys
import time
import base64
from collections import OrderedDict, defaultdict

from ..utils.common import get_current_user, list_remove_dup, is_valid_ipv4, is_valid_ipv6, is_valid_mac, list_difference, list_intersect, PassiveTimer
from ..utils import parsing_opts, text_tables
from ..utils.text_opts import format_text, format_num
from ..utils.text_tables import TRexTextTable

from .trex_events import Event
from .trex_ctx import TRexCtx
from .trex_conn import Connection
from .trex_logger import ScreenLogger
from .trex_types import *
from .trex_exceptions import *
from .trex_psv import *
from .trex_vlan import VLAN
from .trex_api_annotators import client_api, console_api

from .stats.trex_stats import StatsBatch
from .stats.trex_global_stats import GlobalStats, UtilStats
from .stats.trex_port_stats import PortStatsSum

from .services.trex_service_int import ServiceCtx
from .services.trex_service_icmp import ServiceICMP
from .services.trex_service_arp import ServiceARP
from .services.trex_service_ipv6 import ServiceICMPv6, ServiceIPv6Scan


from scapy.layers.l2 import Ether, Packet
from scapy.utils import RawPcapWriter


# imarom: move me to someplace apropriate
class PacketBuffer:
    '''
    Class to be used when sending packets via push_packets.

    :parameters:
        buffer : bytes or Scapy packet
            Packet to send

        port_src : bool
            Override src MAC with TRex port src

        port_dst : bool
            Override dst MAC with TRex port dst
    '''
    def __init__(self, buffer, port_src = False, port_dst = False):
        if isinstance(buffer, Packet):
            self.buffer = bytes(buffer)
        else:
            validate_type('buffer', buffer, bytes)
            self.buffer = buffer
        self.port_src = port_src
        self.port_dst = port_dst




class TRexClient(object):
    """
        TRex abstract client

        contains common code between STLClient, ASTFClient and etc.
    """
    def __init__(self,
                 api_ver = None,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = "error",
                 logger = None):

        # logger
        logger = logger if logger is not None else ScreenLogger()
        logger.set_verbose(verbose_level)

        # first create a TRex context
        self.ctx = TRexCtx(api_ver,
                           username,
                           server,
                           sync_port,
                           async_port,
                           logger)

        # init objects
        self.ports           = {}

        # event handler
        self._register_events()

        # common stats (for STL, ASTF)

        self.global_stats = GlobalStats(self)
        self.util_stats   = UtilStats()

        # connection state object
        self.conn = Connection(self.ctx)

        # port state checker
        self.psv = PortStateValidator(self)


    def get_mode (self):
        """
            return the mode/type for the client
        """
        raise NotImplementedError()

############################    abstract   #############################
############################    functions  #############################
############################               #############################

    def _on_connect (self):
        """
            for deriving objects

            actions performed when connecting to the server

        """
        raise NotImplementedError


############################     events     #############################
############################                #############################
############################                #############################

    # register all common events
    def _register_events (self):

        # server stopped event
        self.ctx.event_handler.register_event_handler("server stopped", self._on_server_stopped)

        # subscriber thread events
        self.ctx.event_handler.register_event_handler("subscriber timeout", self._on_subscriber_timeout)
        self.ctx.event_handler.register_event_handler("subscriber resumed", self._on_subscriber_resumed)
        self.ctx.event_handler.register_event_handler("subscriber crashed", self._on_subscriber_crashed)

        # port related events
        self.ctx.event_handler.register_event_handler("port started",  self._on_port_started)
        self.ctx.event_handler.register_event_handler("port stopped",  self._on_port_stopped)
        self.ctx.event_handler.register_event_handler("port paused",   self._on_port_paused)
        self.ctx.event_handler.register_event_handler("port resumed",  self._on_port_resumed)
        self.ctx.event_handler.register_event_handler("port job done", self._on_port_job_done)

        self.ctx.event_handler.register_event_handler("port acquired", self._on_port_acquired)
        self.ctx.event_handler.register_event_handler("port released", self._on_port_released)

        self.ctx.event_handler.register_event_handler("port error",    self._on_port_error)
        self.ctx.event_handler.register_event_handler("port attr chg", self._on_port_attr_chg)

        self.ctx.event_handler.register_event_handler("global stats update", lambda *args, **kwargs: None)


    # executed when server has stopped
    def _on_server_stopped (self, cause):
        msg = "Server has been shutdown - cause: '{0}'".format(cause)
        self.conn.mark_for_disconnect(cause)

        ev = Event('server', 'warning', msg)
        self.ctx.logger.critical(ev)

        return ev


    # executed when the subscriber thread has timed-out
    def _on_subscriber_timeout (self, timeout_sec):
        if self.conn.is_connected():
            msg = 'Connection lost - Subscriber timeout: no data from TRex server for more than {0} seconds'.format(timeout_sec)
            
            # we cannot simply disconnect the connection - we mark it for disconnection
            # later on, the main thread will execute an ordered disconnection
            
            self.conn.mark_for_disconnect(msg)

            ev = Event('local', 'warning', msg)
            self.ctx.logger.critical(ev)

            return ev
            

         
    # executed when the subscriber thread has resumed (after timeout)
    def _on_subscriber_resumed (self):
        pass


    # executed when the subscriber theard has crashed
    def _on_subscriber_crashed (self, e):
        msg = 'subscriber thread has crashed:\n\n{0}'.format(traceback.format_exc())
        
        # if connected, mark as disconnected
        if self.conn.is_connected():
            self.conn.mark_for_disconnect(msg)

        ev = Event('local', 'warning', msg)
        self.ctx.logger.critical(ev)

        # kill the process
        self.ctx.logger.critical('Terminating due to subscriber crash')
        os.kill(os.getpid(), signal.SIGTERM)

        return ev
   

    def _on_port_started (self, port_id):
        msg = "Port {0} has started".format(port_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_port_started()

        return Event('server', 'info', msg)


    def _on_port_stopped (self, port_id):
        msg = "Port {0} has stopped".format(port_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_port_stopped()

        return Event('server', 'info', msg)


    def _on_port_paused (self, port_id):
        msg = "Port {0} has paused".format(port_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_port_paused()

        return Event('server', 'info', msg)


    def _on_port_resumed (self, port_id):
        msg = "Port {0} has resumed".format(port_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_port_resumed()

        return Event('server', 'info', msg)


    def _on_port_job_done (self, port_id):
        msg = "Port {0} job done".format(port_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_port_job_done()

        ev = Event('server', 'info', msg)
        self.ctx.logger.info(ev)

        return ev



    # on port acquired event
    def _on_port_acquired (self, port_id, who, session_id, force):
        # if we hold the port and it was not taken by this session - show it
        if port_id in self.get_acquired_ports() and session_id != self.ctx.session_id:
            ev_type = 'warning'
        else:
            ev_type = 'info'

        # format the thief/us...
        if session_id == self.ctx.session_id:
            user = 'you'
        elif who == self.ctx.username:
            user = 'another session of you'
        else:
            user = "'{0}'".format(who)

        if force:
            msg = "Port {0} was forcely taken by {1}".format(port_id, user)
        else:
            msg = "Port {0} was taken by {1}".format(port_id, user)

        # call the handler in case its not this session
        if (session_id != self.ctx.session_id) and (port_id in self.ports):
            self.ports[port_id].async_event_port_acquired(who)

        ev = Event('server', ev_type, msg)

        if ev_type == 'warning':
            self.ctx.logger.warning(ev)

        return ev



    # on port release event
    def _on_port_released (self, port_id, who, session_id):
        if session_id == self.ctx.session_id:
            user = 'you'
        elif who == self.ctx.username:
            user = 'another session of you'
        else:
            user = "'{0}'".format(who)

        msg = "Port {0} was released by {1}".format(port_id, user)

        # call the handler in case its not this session
        if (session_id != self.ctx.session_id) and (port_id in self.ports):
            self.ports[port_id].async_event_port_released()

        return Event('server', 'info', msg)


    def _on_port_error (self, port_id):
        msg = "port {0} job failed".format(port_id)

        return Event('server', 'warning', msg)


    def _on_port_attr_chg (self, port_id, attr):
        if not port_id in self.ports:
            return

        diff = self.ports[port_id].async_event_port_attr_changed(attr)

            
        msg = "port {0} attributes changed".format(port_id)
        for key, (old_val, new_val) in diff.items():
            msg += '\n  {key}: {old} -> {new}'.format(
                key = key, 
                old = old_val.lower() if type(old_val) is str else old_val,
                new = new_val.lower() if type(new_val) is str else new_val)
        
        return Event('server', 'info', msg)


############################     private     #############################
############################     functions   #############################
############################                 #############################

    # transmit request on the RPC link
    def _transmit(self, method_name, params = None):
        return self.conn.rpc.transmit(method_name, params)


    # execute 'method' for a port list
    def _for_each_port (self, method, port_id_list = None, *args, **kwargs):

        # specific port params
        pargs = {}

        if 'pargs' in kwargs:
            pargs = kwargs['pargs']
            del kwargs['pargs']


        # handle single port case
        port_id_list = listify_if_int(port_id_list)

        # none means all
        port_id_list = port_id_list if port_id_list is not None else self.get_all_ports()
        
        rc = RC()

        for port_id in port_id_list:
            # get port specific
            pkwargs = pargs.get(port_id, {})
            if pkwargs:
                pkwargs.update(kwargs)
            else:
                pkwargs = kwargs

            func = getattr(self.ports[port_id], method)
            rc.add(func(*args, **pkwargs))

        return rc


    # connect to server
    def _connect(self):

        # before we connect to the server - reject any async updates until fully init
        self.ctx.event_handler.disable()

        # connect to the server
        rc = self.conn.connect()
        if not rc:
            return rc

        # version
        rc = self._transmit("get_version")
        if not rc:
            return rc

        self.ctx.server_version = rc.data()

        # cache system info
        rc = self._transmit("get_system_info")
        if not rc:
            return rc

        self.ctx.system_info = rc.data()

        # cache supported commands
        rc = self._transmit("get_supported_cmds")
        if not rc:
            return rc

        self.supported_cmds = sorted(rc.data())

        # STX specific code: create ports, clear stats 
        rc = self._on_connect(self.ctx.system_info)
        if not rc:
            return rc


        # sync the ports
        rc = self._for_each_port('sync')
        if not rc:
            return rc


        # mark the event handler as ready to process async updates
        self.ctx.event_handler.enable()


        # clear stats to baseline
        with self.ctx.logger.suppress(verbose = "warning"):
            self.clear_stats(ports = self.get_all_ports())

        return RC_OK()


    # disconenct from server
    def _disconnect(self, release_ports = True):
        # release any previous acquired ports
        if self.conn.is_connected() and release_ports:
            self._for_each_port('release', self.get_acquired_ports())

        # disconnect the link to the server
        self.conn.disconnect()

        return RC_OK()


############################     API        #############################
############################                #############################
############################                #############################

  

############################   Getters   #############################
############################             #############################
############################             #############################
    
    @client_api('getter', False)
    def probe_server (self):
        """
        Probe the server for the version / mode

        Can be used to determine stateless or advanced statefull mode

        :parameters:
          none

        :return:
          dictionary describing server version and configuration

        :raises:
          None

        """

        rc = self.conn.probe_server()
        if not rc:
            raise TRexError(rc)
        
        return rc.data()


    @property
    def logger (self):
        """
        Get the associated logger

        :parameters:
          none

        :return:
            Logger object

        :raises:
          None
        """

        return self.ctx.logger


    @client_api('getter', False)
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

        return self.ctx.logger.get_verbose()


    @client_api('getter', True)
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


    @client_api('getter', False)
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



    @client_api('getter', True)
    def get_connection_info (self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return {'server'     : self.ctx.server,
                'sync_port'  : self.ctx.sync_port,
                'async_port' : self.ctx.async_port,
                'username'   : self.ctx.username}


    @client_api('getter', True)
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


    @client_api('getter', True)
    def get_server_version(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.ctx.server_version


    @client_api('getter', True)
    def get_server_system_info(self):
        """ 

        :parameters:
          None

        :return:
            Connection dict 

        :raises:
          None

        """

        return self.ctx.system_info


    @property
    def system_info (self):
        # provided for backward compatability
        return self.ctx.system_info

    
    @client_api('getter', True)
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


    @client_api('getter', True)
    def get_port (self, port_id):
        """

        :parameters:
          port_id - int

        :return:
            Port object

        :raises:
          None

        """
        port = self.ports.get(port_id, None)
        if (port != None):
            return port
        else:
            raise TRexArgumentError('port id', port_id, valid_values = self.get_all_ports())


    @client_api('getter', True)
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

    
    @client_api('getter', True)
    def get_acquired_ports(self):
        """ 

        :parameters:
          None

        :return:
            list of all acquired ports

        :raises:
          None

        """

        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired()]

    
    @client_api('getter', True)
    def get_active_ports(self, owned = True):
        """ 

        :parameters:
          owned - bool
            if 'True' apply only-owned filter

        :return:
            List of all active ports

        :raises:
          None

        """

        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_active() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_active()]


    @client_api('getter', True)
    def get_resolvable_ports (self):
        """ 

        :parameters:
          None

        :return:
            List of all resolveable ports (configured with L3 mode)

        :raises:
          None

        """

        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired() and port_obj.is_l3_mode()]
    

    @client_api('getter', True)
    def get_resolved_ports (self):
        """ 

        :parameters:
          None

        :return:
            List of all resolved ports

        :raises:
          None

        """

        return [port_id
                for port_id, port_obj in self.ports.items()
                if port_obj.is_acquired() and port_obj.is_resolved()]


    @client_api('getter', True)
    def get_service_enabled_ports(self, owned = True):
        """ 

        :parameters:
          owned - bool
            if 'True' apply only-owned filter

        :return:
            List of all ports

        :raises:
          None

        """
        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_service_mode_on() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_service_mode_on()]
      
        
    @client_api('getter', True)
    def get_paused_ports (self, owned = True):
        """ 

        :parameters:
          owned - bool
            if 'True' apply only-owned filter

        :return:
            List of all paused ports

        :raises:
          None

        """

        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_paused() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_paused()]


    @client_api('getter', True)
    def get_transmitting_ports (self, owned = True):
        """ 

        :parameters:
          owned - bool
            if 'True' apply only-owned filter

        :return:
            List of all transmitting ports

        :raises:
          None

        """

        if owned:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_transmitting() and port_obj.is_acquired()]
        else:
            return [port_id
                    for port_id, port_obj in self.ports.items()
                    if port_obj.is_transmitting()]



    @client_api('getter', True)
    def is_traffic_active (self, ports = None):
        """
            Return if specified port(s) have traffic 

            :parameters:
                ports : list
                    Ports on which to execute the command


            :raises:
                + :exc:`STLTimeoutError` - in case timeout has expired
                + :exe:'TRexError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.psv.validate('is_traffic_active', ports)

        return set(self.get_active_ports()).intersection(ports)

           
    @client_api('getter', True)
    def get_port_attr (self, port):
        """
            get the port attributes currently set
            
            :parameters:
                port - for which port to return port attributes
           
                     
            :raises:
                + :exe:'TRexError'
                
        """
        validate_type('port', port, int)
        if port not in self.get_all_ports():
            raise TRexError("'{0}' is not a valid port id".format(port))
            
        return self.ports[port].get_formatted_info()
            

    @client_api('getter', True)
    def fetch_capture_packets (self, capture_id, output, pkt_count = 1000):
        """
            Fetch packets from existing active capture

            :parameters:
                capture_id: int
                    an active capture ID

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

                pkt_count: int
                    maximum packets to fetch

            :raises:
                + :exe:'TRexError'

        """

        write_to_file = isinstance(output, basestring)

        self.ctx.logger.pre_cmd("Writing up to {0} packets to '{1}'".format(pkt_count, output if write_to_file else 'list'))

        # create a PCAP file
        if write_to_file:
            writer = RawPcapWriter(output, linktype = 1)
            writer._write_header(None)
        else:
            # clear the list
            del output[:]

        pending = pkt_count
        rc = RC_OK()

        # fetch with iterations - each iteration up to 50 packets
        while pending > 0 and pkt_count > 0:
            rc = self._transmit("capture", params = {'command': 'fetch', 'capture_id': capture_id, 'pkt_limit': min(50, pkt_count)})
            if not rc:
                self.ctx.logger.post_cmd(rc)
                raise TRexError(rc)

            pkts      = rc.data()['pkts']
            pending   = rc.data()['pending']
            start_ts  = rc.data()['start_ts']
            pkt_count -= len(pkts)

            # write packets
            for pkt in pkts:
                ts = pkt['ts'] - start_ts

                pkt['binary'] = base64.b64decode(pkt['binary'])

                if write_to_file:
                    ts_sec, ts_usec = sec_split_usec(ts)
                    writer._write_packet(pkt['binary'], sec = ts_sec, usec = ts_usec)
                else:
                    output.append(pkt)


        self.ctx.logger.post_cmd(rc)


    @client_api('getter', True)
    def get_capture_status (self):
        """
            Returns a dictionary where each key is an capture ID
            Each value is an object describing the capture

        """
        rc = self._transmit("capture", params = {'command': 'status'})
        if not rc:
            raise TRexError(rc)

        # reformat as dictionary
        output = {}
        for c in rc.data():
            output[c['id']] = c

        return output
            

    @client_api('getter', False)
    def get_events (self, ev_type_filter = None):
        """
            returns list of the events recorded
            ev_type_filter: list
                combination of: 'warning', 'info'
            
        """
        return self.ctx.event_handler.get_events(ev_type_filter)


    @client_api('getter', False)
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


    @client_api('getter', False)
    def get_info (self):
        """
        returns all the info logged events

        :parameters:
          None

        :return:
            info logged events

        :raises:
          None

        """
        return self.get_events(ev_type_filter = 'info')


    # get port(s) info as a list of dicts
    @client_api('getter', True)
    def get_port_info (self, ports = None):

        ports = ports if ports is not None else self.get_all_ports()

        ports = self.psv.validate('release', ports)

        return [self.ports[port_id].get_formatted_info() for port_id in listify_if_int(ports)]


    @client_api('command', True)
    def get_util_stats(self):
        """
            Get utilization stats:
            History of TRex CPU utilization per thread (list of lists)
            MBUFs memory consumption per CPU socket.

            :parameters:
                None

            :raises:
                + :exc:`TRexError`

        """
        self.ctx.logger.pre_cmd('Getting Utilization stats')

        # update and convert to dict
        self.util_stats.update_sync(self.conn.rpc)
        return self.util_stats.to_dict()



    @client_api('command', True)
    def get_xstats(self, port_id):
        """
            Get extended stats of port: all the counters as dict.

            :parameters:
                port_id: int

            :returns:
                Dict with names of counters as keys and values of uint64. Actual keys may vary per NIC.

            :raises:
                + :exc:`TRexError`

        """
        self.ctx.logger.pre_cmd('Getting xstats')

        # update and convert to dict
        xstats = self.ports[port_id].get_xstats()

        xstats.update_sync(self.conn.rpc)
        return xstats.to_dict()



############################   Commands  #############################
############################             #############################
############################             #############################

    @client_api('command', False)
    def set_verbose (self, level):
        """
            Sets verbose level

            :parameters:
                level : str
                    "none" - be silent no matter what
                    "critical"
                    "error" - show only errors (default client mode)
                    "warning"
                    "info"
                    "debug" - print everything

            :raises:
                None

        """
        validate_type('level', level, basestring)

        self.ctx.logger.set_verbose(level)


    @client_api('command', False)
    def connect (self):
        """

            Connects to the TRex server

            :parameters:
                None

            :raises:
                + :exc:`TRexError`

        """
        
        # cleanup from previous connection
        self._disconnect()
        
        rc = self._connect()
        if not rc:
            self._disconnect()
            raise TRexError(rc)
        

    @client_api('command', False)
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
            except TRexError:
                pass


        self.ctx.logger.pre_cmd("Disconnecting from server at '{0}':'{1}'".format(self.ctx.server,
                                                                                  self.ctx.sync_port))
        rc = self._disconnect(release_ports)
        self.ctx.logger.post_cmd(rc)



    @client_api('command', True)
    def release (self, ports = None):
        """
            Release ports

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`TRexError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()

        # validate ports
        ports = self.psv.validate('release', ports, PSV_ACQUIRED)

        self.ctx.logger.pre_cmd("Releasing ports {0}:".format(ports))
        rc = self._for_each_port('release', ports)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

            

    @client_api('command', True)
    def ping_rpc_server(self):
        """
            Pings the RPC server

            :parameters:
                 None

            :return:
                 Timestamp of server

            :raises:
                + :exc:`TRexError`

        """
        self.ctx.logger.pre_cmd("Pinging the server on '{0}' port '{1}': ".format(self.ctx.server, self.ctx.sync_port))
        rc = self._transmit("ping")

        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        return rc.data()


    @client_api('command', True)
    def set_l2_mode (self, port, dst_mac):
        """
            Sets the port mode to L2

            :parameters:
                 port      - the port to set the source address
                 dst_mac   - destination MAC
            :raises:
                + :exc:`TRexError`
        """
        validate_type('port', port, int)

        self.psv.validate('set_l2_mode', port)
            
        if not is_valid_mac(dst_mac):
            raise TRexError("dest_mac is not a valid MAC address: '{0}'".format(dst_mac))
        
            
        self.ctx.logger.pre_cmd("Setting port {0} in L2 mode: ".format(port))
        rc = self.ports[port].set_l2_mode(dst_mac)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            raise TRexError(rc)
            
            
    @client_api('command', True)
    def set_l3_mode (self, port, src_ipv4, dst_ipv4, vlan = None):
        """
            Sets the port mode to L3

            :parameters:
                 port      - the port to set the source address
                 src_ipv4  - IPv4 source address for the port
                 dst_ipv4  - IPv4 destination address
                 vlan      - VLAN configuration - can be an int or a list up to two ints
            :raises:
                + :exc:`TRexError`
        """
    
        self.psv.validate('set_l3_mode', port, (PSV_ACQUIRED, PSV_SERVICE))
        
        if not is_valid_ipv4(src_ipv4):
            raise TRexError("src_ipv4 is not a valid IPv4 address: '{0}'".format(src_ipv4))
            
        if not is_valid_ipv4(dst_ipv4):
            raise TRexError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))
    
        # if VLAN was given - set it
        if vlan is not None:
            self.set_vlan(ports = port, vlan = vlan)
            
        self.ctx.logger.pre_cmd("Setting port {0} in L3 mode: ".format(port))
        rc = self.ports[port].set_l3_mode(src_ipv4, dst_ipv4)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            raise TRexError(rc)
    
        # resolve the port
        self.resolve(ports = port)
     
        
    @client_api('command', True)
    def conf_ipv6(self, port, enabled, src_ipv6 = None):
        """
            Configure IPv6 of port.

            :parameters:
                 port      - the port to disable ipv6
                 enabled   - bool wherever IPv6 should be enabled
                 src_ipv6  - src IPv6 of port or empty string for auto-address
            :raises:
                + :exc:`TRexError`
        """

        self.psv.validate('Configure IPv6', port, (PSV_ACQUIRED, PSV_SERVICE))
        validate_type('enabled', enabled, bool)
        if enabled and src_ipv6 and not is_valid_ipv6(src_ipv6):
            raise TRexError("src_ipv6 is not a valid IPv6 address: '%s'" % src_ipv6)

        self.logger.pre_cmd('Configuring IPv6 on port %s' % port)
        rc = self.ports[port].conf_ipv6(enabled, src_ipv6)
        self.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    def set_vlan (self, ports = None, vlan = None):
        """
            Sets the port VLAN.
            VLAN tagging will be applied to control traffic
            such as ARP resolution of the port
            and periodic gratidious ARP

            :parameters:
                 ports     - the port(s) to set the source address
            
                 vlan      - can be either None, int or a list of up to two ints
                             each value representing a VLAN tag
                             when two are supplied, provide QinQ tagging.
                             The first TAG is outer and the second is inner
                             
                 
            :raises:
                + :exc:`TRexError`
        """
    
        # validate ports and state
        ports = ports if ports is not None else self.get_acquired_ports()

        # validate ports
        ports = self.psv.validate('set_vlan', ports, (PSV_ACQUIRED, PSV_SERVICE, PSV_IDLE))
        
        vlan = VLAN(vlan)
    
        if vlan:
            self.ctx.logger.pre_cmd("Setting port(s) {0} with {1}: ".format(ports, vlan.get_desc()))
        else:
            self.ctx.logger.pre_cmd("Clearing port(s) {0} VLAN configuration: ".format(ports))
        
        rc = self._for_each_port('set_vlan', ports, vlan)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            raise TRexError(rc)
            
       
            
    @client_api('command', True)
    def clear_vlan (self, ports = None):
        """
            Clear any VLAN configuration on the port

            :parameters:
                 ports - on which ports to clear VLAN config
                 
            :raises:
                + :exc:`TRexError`
        """
    
        # validate ports and state
        self.set_vlan(ports = ports, vlan = [])
        
         
    @client_api('command', True)
    def ping_ip (self, src_port, dst_ip, pkt_size = 64, count = 5, interval_sec = 1, vlan = None):
        """
            Pings an IP address through a port

            :parameters:
                 src_port     - on which port_id to send the ICMP PING request
                 dst_ip       - which IP to ping
                 pkt_size     - packet size to use
                 count        - how many times to ping
                 interval_sec - how much time to wait between pings
                 vlan         - one or two VLAN tags o.w it will be taken from the src port configuration

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
                + :exc:`TRexError`

        """
        
        if not (is_valid_ipv4(dst_ip) or is_valid_ipv6(dst_ip)):
            raise TRexError("dst_ip is not a valid IPv4/6 address: '{0}'".format(dst_ip))
            
        if (pkt_size < 64) or (pkt_size > 9216):
            raise TRexError("pkt_size should be a value between 64 and 9216: '{0}'".format(pkt_size))
        
        validate_type('count', count, int)
        validate_type('interval_sec', interval_sec, (int, float))
        
        # validate src port
        if is_valid_ipv4(dst_ip):
            self.psv.validate('ping', src_port, (PSV_ACQUIRED, PSV_SERVICE, PSV_L3))
        else:
            self.psv.validate('ping', src_port, (PSV_ACQUIRED, PSV_SERVICE))
        
        # if vlan was given use it, otherwise generate it from the port
        vlan = VLAN(self.ports[src_port].get_vlan_cfg() if vlan is None else vlan)
        
        if vlan:
            self.ctx.logger.pre_cmd("Pinging {0} from port {1} over {2} with {3} bytes of data:".format(dst_ip,
                                                                                                    src_port,
                                                                                                    vlan.get_desc(),
                                                                                                    pkt_size))
        else:
            self.ctx.logger.pre_cmd("Pinging {0} from port {1} with {2} bytes of data:".format(dst_ip,
                                                                                           src_port,
                                                                                           pkt_size))
        
        
        if is_valid_ipv4(dst_ip):
            return self._ping_ipv4(src_port, vlan, dst_ip, pkt_size, count, interval_sec)
        else:
            return self._ping_ipv6(src_port, vlan, dst_ip, pkt_size, count, interval_sec)
        
            
         
    # IPv4 ping           
    def _ping_ipv4 (self, src_port, vlan, dst_ip, pkt_size, count, interval_sec):
        
        ctx = self.create_service_ctx(port = src_port)
        ping = ServiceICMP(ctx, dst_ip = dst_ip, pkt_size = pkt_size, vlan = vlan)
        
        records = []
        
        self.ctx.logger.info('')
        for i in range(count):
            ctx.run(ping)
            
            records.append(ping.get_record())
            self.ctx.logger.info(ping.get_record())
            
            if i != (count - 1):
                time.sleep(interval_sec)
            
        return records
        
        
    # IPv6 ping 
    def _ping_ipv6 (self, src_port, vlan, dst_ip, pkt_size, count, interval_sec):
        
        ctx = self.create_service_ctx(port = src_port)
        ping = ServiceICMPv6(ctx, dst_ip = dst_ip, pkt_size = pkt_size, vlan = vlan)
        
        records = []
        
        self.ctx.logger.info('')
        for i in range(count):
            ctx.run(ping)

            record = ping.get_record()
            records.append(record)
            self.ctx.logger.info(record['formatted_string'])
            
            if i != (count - 1):
                time.sleep(interval_sec)
            
        return records

        
    @client_api('command', True)
    def server_shutdown (self, force = False):
        """
            Sends the server a request for total shutdown

            :parameters:
                force - shutdown server even if some ports are owned by another
                        user

            :raises:
                + :exc:`TRexError`

        """

        self.ctx.logger.pre_cmd("Sending shutdown request for the server")

        rc = self._transmit("shutdown", params = {'force': force, 'user': self.ctx.username})

        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    def push_packets (self, pkts, ports = None, force = False, ipg_usec = 0):
        """
            Pushes a list of packets to the server
            a 'packet' can be anything with a bytes representation
            such as Scapy object, a simple string, a byte array and etc.

            Total size, as for PCAP pushing is limited to 1MB
            unless 'force' is specified

            :parameters:
                pkts       - scapy pkt or a list of scapy pkts
                ports      - on which ports to push the packets
                force      - ignore size higer than 1 MB
                ipg_usec   - IPG in usec
        """
        
        # by default, take acquire ports
        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('push_packets', ports)

        # pkts should be scapy, bytes, str or a list of them
        pkts = listify(pkts)
        for pkt in pkts:
            if not isinstance(pkt, (Ether, bytes, PacketBuffer)):
                raise TRexTypeError('pkts', type(pkt), (Ether, bytes, PacketBuffer))
        
        # for each packet, if scapy turn to bytes and then encode64 and transform to string
        pkts_base64 = []
        for pkt in pkts:
            
            if isinstance(pkt, Ether):
                # scapy
                binary = bytes(pkt)
                use_port_dst_mac = 'dst' not in pkt.fields
                use_port_src_mac = 'src' not in pkt.fields

            elif isinstance(pkt, PacketBuffer):
                binary = pkt.buffer
                use_port_dst_mac = pkt.port_dst
                use_port_src_mac = pkt.port_src

            else:
                # binary
                binary = pkt
                use_port_dst_mac = True
                use_port_src_mac = True
                
                
            pkts_base64.append( {'binary': base64.b64encode(binary).decode(),
                                 'use_port_dst_mac': use_port_dst_mac,
                                 'use_port_src_mac': use_port_src_mac} )
        
            
        self.ctx.logger.pre_cmd("Pushing {0} packets on port(s) {1}:".format(len(pkts), ports))
        rc = self._for_each_port('push_packets', ports, pkts_base64, force ,ipg_usec)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        return rc


################################ consolidation HERE #####################################

    @client_api('command', True)
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

            :raises:
                + :exc:`TRexTimeoutError` - in case timeout has expired
                + :exe:'TRexError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('wait_on_traffic', ports, PSV_ACQUIRED)

        timer = PassiveTimer(timeout)

        # wait while any of the required ports are active
        while set(self.get_active_ports()).intersection(ports):

            time.sleep(0.01)
            if timer.has_expired():
                raise TRexTimeoutError(timeout)



    @client_api('command', True)
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
                + :exe:'TRexError'

        """

        ports = listify_if_int(ports) if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('set_port_attr', ports)

        # check arguments
        validate_type('promiscuous', promiscuous, (bool, type(None)))
        validate_type('link_up', link_up, (bool, type(None)))
        validate_type('led_on', led_on, (bool, type(None)))
        validate_type('flow_ctrl', flow_ctrl, (int, type(None)))
        validate_type('multicast', multicast, (bool, type(None)))
    

        self.ctx.logger.pre_cmd("Applying attributes on port(s) {0}:".format(ports))

        rc = self._for_each_port('set_attr',
                                 ports,
                                 promiscuous = promiscuous,
                                 link_up = link_up,
                                 led_on = led_on,
                                 flow_ctrl = flow_ctrl,
                                 multicast = multicast)

        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        return

        """

        # common attributes for all ports
        cmn_attr_dict = {}

        cmn_attr_dict['promiscuous']     = promiscuous
        cmn_attr_dict['link_status']     = link_up
        cmn_attr_dict['led_status']      = led_on
        cmn_attr_dict['flow_ctrl_mode']  = flow_ctrl
        cmn_attr_dict['multicast']       = multicast
        
        # each port starts with a set of the common attributes
        attr_dict = [dict(cmn_attr_dict) for _ in ports]
    
        self.ctx.logger.pre_cmd("Applying attributes on port(s) {0}:".format(ports))

        rc = RC()
        for port_id, port_attr_dict in zip(ports, attr_dict):
            rc.add(self.ports[port_id].set_attr(**port_attr_dict))

        self.ctx.logger.post_cmd(rc)
            
        if not rc:
            raise TRexError(rc)
        """
      
        
    @client_api('command', True)
    def set_service_mode (self, ports = None, enabled = True):
        """
            Set service mode for port(s)
            In service mode ports will respond to ARP, PING and etc.

            :parameters:
                ports          - for which ports to configure service mode on/off
                enabled        - True for activating service mode, False for disabling
            :raises:
                + :exe:'TRexError'

        """
        # by default take all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('set_service_mode', ports, PSV_ACQUIRED)
        
        if enabled:
            self.ctx.logger.pre_cmd('Enabling service mode on port(s) {0}:'.format(ports))
        else:
            self.ctx.logger.pre_cmd('Disabling service mode on port(s) {0}:'.format(ports))
            
        rc = self._for_each_port('set_service_mode', ports, enabled)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            raise TRexError(rc)
          
              
    @contextmanager
    def service_mode (self, ports):
        self.set_service_mode(ports = ports)
        try:
            yield
        finally:
            self.set_service_mode(ports = ports, enabled = False)
        
        
    @client_api('command', True)
    def resolve (self, ports = None, retries = 0, verbose = True, vlan = None):
        """
            Resolves ports (ARP resolution)

            :parameters:
                ports          - which ports to resolve
                retries        - how many times to retry on each port (intervals of 100 milliseconds)
                verbose        - log for each request the response
                vlan           - one or two VLAN tags o.w it will be taken from the src port configuration
            :raises:
                + :exe:'TRexError'

        """
        # by default - resolve all the ports that are configured with IPv4 dest
        ports = ports if ports is not None else self.get_resolvable_ports()
        ports = self.psv.validate('arp', ports, (PSV_ACQUIRED, PSV_SERVICE, PSV_L3))
        
        # create a VLAN object - might throw exception on error
        vlan = VLAN(vlan)
        
        if vlan:
            self.ctx.logger.pre_cmd('Resolving destination over {0} on port(s) {1}:'.format(vlan.get_desc(), ports))
        else:
            self.ctx.logger.pre_cmd('Resolving destination on port(s) {0}:'.format(ports))
        
        # generate the context
        arps = []
        ports = listify_if_int(ports)
        for port in ports:

            self.ports[port].invalidate_arp()
            
            src_ipv4 = self.ports[port].get_layer_cfg()['ipv4']['src']
            dst_ipv4 = self.ports[port].get_layer_cfg()['ipv4']['dst']
            
            port_vlan = self.ports[port].get_vlan_cfg() if vlan.is_default() else vlan
            
            ctx = self.create_service_ctx(port)
            
            # retries
            for i in range(retries + 1):
                arp = ServiceARP(ctx, dst_ip = dst_ipv4, src_ip = src_ipv4, vlan = port_vlan)
                ctx.run(arp)
                if arp.get_record():
                    self.ports[port].set_l3_mode(src_ipv4, dst_ipv4, arp.get_record().dst_mac)
                    break
            
            arps.append(arp)
            
        
        self.ctx.logger.post_cmd(all([arp.get_record() for arp in arps]))
        
        for port, arp in zip(ports, arps):
            if arp.get_record():
                self.ctx.logger.info(format_text("Port {0} - {1}".format(port, arp.get_record()), 'bold'))
            else:
                self.ctx.logger.info(format_text("Port {0} - *** {1}".format(port, arp.get_record()), 'bold'))
        
        self.ctx.logger.info('')
     

    # alias
    arp = resolve


    @client_api('command', True)
    def scan6(self, ports = None, timeout = 3, verbose = False):
        """
            Search for IPv6 devices on ports

            :parameters:
                ports          - at which ports to run scan6
                timeout        - how much time to wait for responses
                verbose        - log for each request the response
            :return:
                list of dictionaries per neighbor:

                    * type   - type of device: 'Router' or 'Host'
                    * mac    - MAC address of device
                    * ipv6   - IPv6 address of device
            :raises:
                + :exe:'TRexError'

        """

        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('scan6', ports, [PSV_ACQUIRED, PSV_SERVICE])

        self.ctx.logger.pre_cmd('Scanning network for IPv6 nodes on port(s) {0}:'.format(ports))
        replies_per_port = {}

        with self.ctx.logger.supress():
            for port in ports:
                try:
                    ctx = self.create_service_ctx(port)
                    scan6 = ServiceIPv6Scan(ctx, dst_ip = 'ff02::1', timeout = timeout)
                    ctx.run(scan6)
                    replies_per_port[port] = scan6.get_record()
                except:
                    self.ctx.logger.post_cmd(True)
                    raise

        self.ctx.logger.post_cmd(True)

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
                    self.ctx.logger.info(format_text(resp, 'bold'))
                    node_types = defaultdict(list)
                    for reply in replies:
                        node_types[reply['type']].append(reply)
                    for key in sorted(node_types.keys()):
                        for reply in node_types[key]:
                            scan_table.add_row([key, reply['mac'], reply['ipv6']])
                    self.ctx.logger.info(scan_table.draw())
                    self.ctx.logger.info('')
                else:
                    self.ctx.logger.info(format_text('Port %s: no replies! Try to ping with explicit address.' % port, 'bold'))

        return replies_per_port


    @client_api('command', True)
    def start_capture (self, tx_ports = None, rx_ports = None, limit = 1000, mode = 'fixed', bpf_filter = ''):
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
                                  
                bpf_filter: str
                    A Berkeley Packet Filter pattern
                    Only packets matching the filter will be appended to the capture
                    
            :returns:
                returns a dictionary:
                {'id: <new_id>, 'ts': <starting timestamp>}
                
                where 'id' is the new capture ID for future commands
                and 'ts' is that server monotonic timestamp when
                the capture was created
                
            :raises:
                + :exe:'TRexError'

        """

        # default values for TX / RX ports
        tx_ports = tx_ports if tx_ports is not None else []
        rx_ports = rx_ports if rx_ports is not None else []
        
        # listify if single port
        tx_ports = listify_if_int(tx_ports)
        rx_ports = listify_if_int(rx_ports)

        # check arguments
        self.psv.validate('start_capture', list_remove_dup(tx_ports + rx_ports), [PSV_SERVICE])

        validate_type('limit', limit, (int))
        if limit < 0:
            raise TRexError("'limit' must be a non zero value")

        if mode not in ('fixed', 'cyclic'):
            raise TRexError("'mode' must be either 'fixed' or 'cyclic'")
        
        # actual job
        self.ctx.logger.pre_cmd("Starting packet capturing up to {0} packets".format(limit))

        # capture RPC parameters
        params = {'command'   : 'start',
                  'limit'     : limit,
                  'mode'      : mode,
                  'tx'        : tx_ports,
                  'rx'        : rx_ports,
                  'filter'    : bpf_filter}
        
        rc = self._transmit("capture", params = params)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        return {'id': rc.data()['capture_id'], 'ts': rc.data()['start_ts']}


        
    @client_api('command', True)
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
                + :exe:'TRexError'

        """
        
        # stopping a capture requires:
        # 1. stopping
        # 2. fetching
        # 3. saving to file
        
        
        validate_type('capture_id', capture_id, (int))
        validate_type('output', output, (type(None), str, list))
        
        # stop
        self.ctx.logger.pre_cmd("Stopping packet capture {0}".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'stop', 'capture_id': capture_id})
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc)
        
        # pkt count
        pkt_count = rc.data()['pkt_count']
        
        # fetch packets
        if output is not None:
            self.fetch_capture_packets(capture_id, output, pkt_count)
        
        # remove
        self.ctx.logger.pre_cmd("Removing PCAP capture {0} from server".format(capture_id))
        rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': capture_id})
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc)
        
                    
    @client_api('command', True)
    def remove_all_captures (self):
        """
            Removes any existing captures
        """
        captures = self.get_capture_status()
        
        self.ctx.logger.pre_cmd("Removing all packet captures from server")
        
        for capture_id in captures.keys():
            # remove
            rc = self._transmit("capture", params = {'command': 'remove', 'capture_id': capture_id})
            if not rc:
                raise TRexError(rc)

        self.ctx.logger.post_cmd(RC_OK())
                

    @client_api('command', False)
    def clear_events (self):
        """
            Clear all events

            :parameters:
                None

            :raises:
                None

        """
        self.ctx.event_handler.clear_events()


    @client_api('command', False)
    def create_service_ctx (self, port):
        """
            Generates a service context.
            Services can be added to the context,
            and then executed
        """
        
        return ServiceCtx(self, port)




############################   deprecated   #############################
############################                #############################
############################                #############################

    @client_api('command', True)
    def set_rx_queue (self, ports = None, size = 1000):
        """
            Sets RX queue for port(s)
            The queue is cyclic and will hold last 'size' packets

            :parameters:
                ports          - for which ports to apply a queue
                size           - size of the queue
            :raises:
                + :exe:'TRexError'

        """
        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('set_rx_queue', ports, PSV_ACQUIRED)

        # check arguments
        validate_type('size', size, (int))
        if size <= 0:
            raise TRexError("'size' must be a positive value")

        self.ctx.logger.pre_cmd("Setting RX queue on port(s) {0}:".format(ports))
        rc = self._for_each_port('set_rx_queue', ports, size)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)



    @client_api('command', True)
    def remove_rx_queue (self, ports = None):
        """
            Removes RX queue from port(s)

            :parameters:
                ports          - for which ports to remove the RX queue
            :raises:
                + :exe:'TRexError'

        """
        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('remove_rx_queue', ports, PSV_ACQUIRED)

        self.ctx.logger.pre_cmd("Removing RX queue on port(s) {0}:".format(ports))
        rc = self._for_each_port('remove_rx_queue', ports)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


############################   private   #############################
############################   common    #############################
############################  functions  #############################

    def _acquire_common (self, ports = None, force = False):
      
        # by default use all ports
        ports = ports if ports is not None else self.get_all_ports()
    
        # validate ports
        ports = self.psv.validate('acquire', ports)
    
        if force:
            self.ctx.logger.pre_cmd("Force acquiring ports {0}:".format(ports))
        else:
            self.ctx.logger.pre_cmd("Acquiring ports {0}:".format(ports))
    
        rc = self._for_each_port('acquire', ports, force)
    
        self.ctx.logger.post_cmd(rc)
    
        if not rc:
            # cleanup
            self._for_each_port('release', ports)
            raise TRexError(rc)
    
        for port_id in listify_if_int(ports):
            if not self.ports[port_id].is_resolved():
                self.ctx.logger.info(format_text('*** Warning - Port {0} destination is unresolved ***'.format(port_id), 'bold'))
    


    def _get_stats_common (self, ports = None, sync_now = True, ext_stats = None):
        """
            A common method for STL/ASTF to generate stats output
        """
    
        # by default use all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()
    
        # validate
        ports = self.psv.validate('get_stats', ports)
        validate_type('sync_now', sync_now, bool)
    
        # create the stats mapping
        stats = {'global': self.global_stats}
        stats.update({port.port_id : port.get_stats() for port in self.ports.values()})
    
        # add the extended stats provided by the specific class (if any)
        if ext_stats:
            stats.update(ext_stats)
    
        # if required, update the stats list
        if sync_now:
            rc = StatsBatch.update(stats.values(), self.conn.rpc)
            if not rc:
                raise TRexError(rc)
    
    
        # generate the output
        output = {}
        for k, v in stats.items():
            output[k] = v.to_dict()
    
        # create total for ports
        ps_sum = PortStatsSum()
        for p in [port.get_stats() for port in self.ports.values()]:
            ps_sum += p

        output['total'] = ps_sum.to_dict()


        return output



    def _clear_stats_common (self, ports = None, clear_global = True, clear_xstats = True, ext_stats = None):
        """
            A common method for STL/ASTF to clear stats
        """

        # by default use all acquired ports
        ports = ports if ports is not None else self.get_acquired_ports()

        # validate
        ports = self.psv.validate('clear_stats', ports)

        stats = []

        if clear_global:
            stats.append(self.global_stats)

        if clear_xstats:
            stats += [port.get_xstats() for port in self.ports.values()]

        # ports stats
        stats += [port.get_stats() for port in self.ports.values()]
        
        # reset
        
        self.ctx.logger.pre_cmd("Clearing stats :")

        rc = StatsBatch.reset(stats, self.conn.rpc)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


############################   console   #############################
############################   commands  #############################
############################             #############################

    @console_api('connect', 'common', False)
    def connect_line (self, line):
        '''Connects to the TRex server and acquire ports'''
        parser = parsing_opts.gen_parser(self,
                                         "connect",
                                         self.connect_line.__doc__,
                                         parsing_opts.FORCE,
                                         parsing_opts.READONLY)

        opts = parser.parse_args(line.split())

        self.connect()
        if not opts.readonly:
            self.acquire(force = opts.force)



    @console_api('disconnect', 'common')
    def disconnect_line (self, line):
        '''Disconnect from the TRex server'''

        parser = parsing_opts.gen_parser(self,
                                         "disconnect",
                                         self.disconnect_line.__doc__)

        opts = parser.parse_args(line.split())

        self.disconnect()


    @console_api('ping', 'common', True)
    def ping_line (self, line):
        '''Pings the server / specific IP'''
        
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
                                         parsing_opts.VLAN_TAGS,
                                         parsing_opts.PING_COUNT)

        opts = parser.parse_args(line.split(), verify_acquired = True)
            
        # IP ping
        # source ports maps to ports as a single port
        self.ping_ip(opts.ports[0], opts.ping_ip, opts.pkt_size, opts.count, vlan = opts.vlan)


    @console_api('acquire', 'common', True)
    def acquire_line (self, line):
        '''Acquire ports\n'''

        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "acquire",
                                         self.acquire_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split(), default_ports = self.get_all_ports())

        # filter out all the already owned ports
        ports = list_difference(opts.ports, self.get_acquired_ports())
        if not ports:
            raise TRexError("acquire - all of port(s) {0} are already acquired".format(opts.ports))

        self.acquire(ports = ports, force = opts.force)

        # show time if success
        return True

        

    @console_api('release', 'common', True)
    def release_line (self, line):
        '''Release ports\n'''

        parser = parsing_opts.gen_parser(self,
                                         "release",
                                         self.release_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports())

        ports = list_intersect(opts.ports, self.get_acquired_ports())
        if not ports:
            if not opts.ports:
                raise TRexError("no acquired ports")
            else:
                raise TRexError("none of port(s) {0} are acquired".format(opts.ports))
        
        self.release(ports = ports)

        # show time if success
        return True


    @console_api('l2', 'common', True)
    def set_l2_mode_line (self, line):
        '''Configures a port in L2 mode'''

        parser = parsing_opts.gen_parser(self,
                                         "l2",
                                         self.set_l2_mode_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.DST_MAC)

        opts = parser.parse_args(line.split())

        # source ports maps to ports as a single port
        self.set_l2_mode(opts.ports[0], dst_mac = opts.dst_mac)

        return True
        
        
    @console_api('l3', 'common', True)
    def set_l3_mode_line (self, line):
        '''Configures a port in L3 mode'''

        parser = parsing_opts.gen_parser(self,
                                         "l3",
                                         self.set_l3_mode_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.SRC_IPV4,
                                         parsing_opts.DST_IPV4,
                                         parsing_opts.VLAN_TAGS,
                                         )

        opts = parser.parse_args(line.split())

        # source ports maps to ports as a single port
        self.set_l3_mode(opts.ports[0], src_ipv4 = opts.src_ipv4, dst_ipv4 = opts.dst_ipv4, vlan = opts.vlan)

        return True



    @console_api('ipv6', 'common', True)
    def conf_ipv6_line(self, line):
        '''Configures IPv6 of a port'''

        parser = parsing_opts.gen_parser(self,
                                         "port",
                                         self.conf_ipv6_line.__doc__,
                                         parsing_opts.SINGLE_PORT,
                                         parsing_opts.IPV6_OPTS_CMD,
                                         )

        opts = parser.parse_args(line.split())

        if opts.off:
            self.conf_ipv6(opts.ports[0], False)
        elif opts.auto:
            self.conf_ipv6(opts.ports[0], True)
        else:
            self.conf_ipv6(opts.ports[0], True, opts.src_ipv6)

        return True


    @console_api('vlan', 'common', True)
    def set_vlan_line (self, line):
        '''Configures VLAN tagging for a port.
        control generated traffic such as ARP will be tagged'''

        parser = parsing_opts.gen_parser(self,
                                         "vlan",
                                         self.set_vlan_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.VLAN_CFG,
                                         )

        opts = parser.parse_args(line.split())

        if opts.clear_vlan:
            self.clear_vlan(ports = opts.ports)
        else:
            self.set_vlan(ports = opts.ports, vlan = opts.vlan)

        return True


    @console_api('scan6', 'common', True)
    def scan6_line(self, line):
        '''Search for IPv6 neighbors'''

        parser = parsing_opts.gen_parser(self,
                                         "scan6",
                                         self.scan6_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.TIMEOUT)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)

        kw = {}
        if opts.timeout is not None:
            kw['timeout'] = opts.timeout
        rc_per_port = self.scan6(ports = opts.ports, verbose = True, **kw)

        return True


    @console_api('arp', 'common', True)
    def resolve_line (self, line):
        '''Performs a port ARP resolution'''

        parser = parsing_opts.gen_parser(self,
                                         "resolve",
                                         self.resolve_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.VLAN_TAGS,
                                         parsing_opts.RETRIES)

        opts = parser.parse_args(line.split(), default_ports = self.get_resolvable_ports(), verify_acquired = True)
        
        self.resolve(ports = opts.ports, retries = opts.retries, vlan = opts.vlan)

        return True


    @console_api('reset', 'common', True)
    def reset_line (self, line):
        '''Reset ports'''

        parser = parsing_opts.gen_parser(self,
                                         "reset",
                                         self.reset_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.PORT_RESTART)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)

        self.reset(ports = opts.ports, restart = opts.restart)

        return True


    @console_api('pkt', 'common', True)
    def pkt_line (self, line):
        '''Sends a Scapy format packet'''
        
        parser = parsing_opts.gen_parser(self,
                                         "pkt",
                                         self.pkt_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.DRY_RUN,
                                         parsing_opts.SCAPY_PKT_CMD,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
            
        # show layers option
        if opts.layers:
            self.logger.info(format_text('\nRegistered Layers:\n', 'underline'))
            self.logger.info(parsing_opts.ScapyDecoder.formatted_layers())
            return

        # dry run option
        if opts.dry:
            self.logger.info(format_text('\nPacket (Size: {0}):\n'.format(format_num(len(opts.scapy_pkt), suffix = 'B')), 'bold', 'underline'))
            opts.scapy_pkt.show2()
            self.logger.info(format_text('\n*** DRY RUN - no traffic was injected ***\n', 'bold'))
            return
   
            
        self.push_packets(pkts = opts.scapy_pkt, ports = opts.ports, force = opts.force)
        
        return True


    @console_api('shutdown', 'common', True)
    def shutdown_line (self, line):
        '''Shutdown the server'''
        parser = parsing_opts.gen_parser(self,
                                         "shutdown",
                                         self.shutdown_line.__doc__,
                                         parsing_opts.FORCE)

        opts = parser.parse_args(line.split())

        self.server_shutdown(force = opts.force)

        return True



    @console_api('portattr', 'common', True)
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
                                         parsing_opts.MULTICAST)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), allow_empty = False)

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
            info = self.ports[opts.ports[0]].get_formatted_info() # assume for now all ports are same
            print('')
            print('Supported attributes for current NICs:')
            print('  Promiscuous:   %s' % info['prom_supported'])
            print('  Multicast:     yes')
            print('  Link status:   %s' % info['link_change_supported'])
            print('  LED status:    %s' % info['led_change_supported'])
            print('  Flow control:  %s' % info['fc_supported'])
            print('')
        else:
            if not opts.ports:
                raise TRexError('No acquired ports!')
            self.set_port_attr(opts.ports,
                               opts.prom,
                               opts.link,
                               opts.led,
                               opts.flow_ctrl,
                               multicast = opts.mult)



    @console_api('events', 'basic', False)
    def get_events_line (self, line):
        '''Shows events log\n'''

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


        ev_type_filter = []

        if opts.info:
            ev_type_filter.append('info')

        if opts.warn:
            ev_type_filter.append('warning')

        if not ev_type_filter:
            ev_type_filter = None

        events = self.get_events(ev_type_filter)
        for ev in events:
            self.logger.info(ev)

        if opts.clear:
            self.clear_events()



    @console_api('clear', 'common', False)
    def clear_stats_line (self, line):
        '''Clear cached local statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "clear",
                                         self.clear_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())

        self.clear_stats(opts.ports)

        return RC_OK()



    def get_console_methods (self):
        def predicate (x):
            return inspect.ismethod(x) and getattr(x, 'api_type', None) == 'console'

        return {cmd[1].name : cmd[1] for cmd in inspect.getmembers(self ,predicate = predicate)}

    ################## private common console functions ##################
    
    def _show_global_stats (self, buffer = sys.stdout):

        self.global_stats.update_sync(self.conn.rpc)

        table = self.global_stats.to_table()
        text_tables.print_table_with_header(table, table.title, buffer = buffer)


    def _show_port_stats (self, ports, buffer = sys.stdout):

        port_stats = [self.ports[port_id].get_port_stats() for port_id in ports]

        # update in a batch
        StatsBatch.update(port_stats, self.conn.rpc)

        tables = [stat.to_table() for stat in port_stats]

        # total if more than 1
        if len(port_stats) > 1:
            sum = PortStatsSum()
            for stats in port_stats:
                sum += stats

            tables.append(sum.to_table())

        # merge
        table = TRexTextTable.merge(tables)

        # show
        text_tables.print_table_with_header(table, table.title, buffer = buffer)


    def _show_port_xstats (self, ports, include_zero_lines):
        port_xstats = [self.ports[port_id].get_port_xstats() for port_id in ports]

        # update in a batch
        StatsBatch.update(port_xstats, self.conn.rpc)

        # merge
        table = TRexTextTable.merge([stat.to_table(True) for stat in port_xstats],
                                    row_filter = lambda row: include_zero_lines or any([v != '0' for v in row]))
        
        # show
        #table = port_xstats[0].to_table(include_zero_lines)
        text_tables.print_table_with_header(table, table.title)


    def _show_port_status (self, ports):
        # for each port, fetch port status
        port_status = [self.ports[port_id].get_port_status() for port_id in ports]

        # merge
        #table = TRexTextTable.merge([status.to_table() for status in port_status])
        table = TRexTextTable.merge(port_status)
        text_tables.print_table_with_header(table, table.title)


    def _show_cpu_util (self, buffer = sys.stdout):
        self.util_stats.update_sync(self.conn.rpc)

        table = self.util_stats.to_table('cpu')
        text_tables.print_table_with_header(table, table.title, buffer = buffer)


    def _show_mbuf_util (self, buffer = sys.stdout):
        self.util_stats.update_sync(self.conn.rpc)

        table = self.util_stats.to_table('mbuf')
        text_tables.print_table_with_header(table, table.title, buffer = buffer)

