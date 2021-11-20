import time
import sys
import os
from collections import OrderedDict
from functools import wraps

from ..utils.common import get_current_user, list_intersect, is_sub_list, user_input, list_difference, parse_ports_from_profiles
from ..utils import parsing_opts, text_tables
from ..utils.text_opts import format_text, format_num

from ..common.trex_exceptions import *
from ..common.trex_events import Event
from ..common.trex_logger import Logger
from ..common.trex_client import TRexClient, PacketBuffer
from ..common.trex_types import *
from ..common.trex_types import PortProfileID, ALL_PROFILE_ID
from ..common.trex_psv import *
from ..common.trex_api_annotators import client_api, console_api

from .trex_stl_port import STLPort

from .trex_stl_streams import STLStream, STLProfile
from .trex_stl_stats import CPgIdStats

from texttable import ansi_len

def validate_port_input(port_arg):
    """Decorator to support PortProfileID type input.
       Convert int,str argument to PortProfileID type
    """
    def wrap (func):
        @wraps(func)
        def wrapper(self, *args, **kwargs):
            code = func.__code__
            fname = func.__name__
            names = code.co_varnames[:code.co_argcount]
            argname = port_arg

            try:
                port_index = names.index(argname) - 1
                argval = args[port_index]
                args = list(args)
                args[port_index] = convert_port_to_profile(argval)
                args = tuple(args)
            except (ValueError, IndexError):
                argval = kwargs.get(argname)
                kwargs[argname] = convert_port_to_profile(argval)

            return func(self, *args, **kwargs)

        def convert_port_to_profile(port):
            if port is None:
                return port

            if isinstance(port, list):
                result = list(port)
                for idx, val in enumerate(result):
                    validate_type('port', val, (int, str, PortProfileID))
                    result[idx] = PortProfileID(str(val))
            else:
                validate_type('port', port, (int, str, PortProfileID))
                result = PortProfileID(str(port))

            return result

        return wrapper
    return wrap

class STLClient(TRexClient):

    # different modes for attaching traffic to ports
    CORE_MASK_SPLIT  = 1
    CORE_MASK_PIN    = 2
    CORE_MASK_SINGLE = 3


    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = "error",
                 logger = None,
                 sync_timeout = None,
                 async_timeout = None
                 ):
        """ 
        TRex stateless client

        :parameters:
             username : string 
                the user name, for example imarom

              server  : string 
                the server name or ip 

              sync_port : int 
                the RPC port 

              async_port : int 
                the ASYNC port (subscriber port)

              verbose_level: str
                one of "none", "critical", "error", "info", "debug"

              logger: instance of AbstractLogger
                if None, will use ScreenLogger


              sync_timeout: int 
                   time in sec for timeout for RPC commands. for local lab keep it as default (3 sec) 
                   higher number would be more resilient for Firewalls but slower to identify real server crash 

              async_timeout: int 
                   time in sec for timeout for async notification. for local lab keep it as default (3 sec) 
                   higher number would be more resilient for Firewalls but slower to identify real server crash 

        """

        api_ver = {'name': 'STL', 'major': 5, 'minor': 0}

        TRexClient.__init__(self,
                            api_ver,
                            username,
                            server,
                            sync_port,
                            async_port,
                            verbose_level,
                            logger,
                            sync_timeout,
                            async_timeout)

        self.pgid_stats = CPgIdStats(self.conn.rpc)


    def get_mode (self):
        return "STL"


############################    called       #############################
############################    by base      #############################
############################    TRex Client  #############################   

    def _on_connect(self):
        return RC_OK()


    def _on_connect_create_ports(self, system_info):
        """
            called when connecting to the server
            triggered by the common client object
        """

        # create ports
        port_map = {}
        for info in system_info['ports']:
            port_id = info['index']
            port_map[port_id] = STLPort(self.ctx, port_id, self.conn.rpc, info, self.is_dynamic)
        return self._assign_ports(port_map)


    def _on_connect_clear_stats(self):
        # clear stats to baseline
        with self.ctx.logger.suppress(verbose = "warning"):
            self.clear_stats(ports = self.get_all_ports(), clear_xstats = False)

        #self.get_pgid_stats()
        #self.pgid_stats.clear_stats()
        return RC_OK()


############################     events     #############################
############################                #############################
############################                #############################

    # register all common events
    def _register_events (self):
        super(STLClient, self)._register_events()
        self.ctx.event_handler.register_event_handler("profile started",     self._on_profile_started)
        self.ctx.event_handler.register_event_handler("profile stopped",     self._on_profile_stopped)
        self.ctx.event_handler.register_event_handler("profile paused",      self._on_profile_paused)
        self.ctx.event_handler.register_event_handler("profile resumed",      self._on_profile_resumed)
        self.ctx.event_handler.register_event_handler("profile finished tx", self._on_profile_finished_tx)
        self.ctx.event_handler.register_event_handler("profile error",       self._on_profile_error)


    def _on_profile_started (self, port_id, profile_id):
        msg = "Profile {0}.{1} has started".format(port_id, profile_id)
        if port_id in self.ports:
            self.ports[port_id].async_event_profile_started(profile_id)

        return Event('server', 'info', msg)


    def _on_profile_stopped (self, port_id, profile_id):
        msg = "Profile {0}.{1} has stopped".format(port_id, profile_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_profile_stopped(profile_id)

        return Event('server', 'info', msg)


    def _on_profile_paused (self, port_id, profile_id):
        msg = "Profile {0}.{1} has paused".format(port_id, profile_id)
        if port_id in self.ports:
            self.ports[port_id].async_event_profile_paused(profile_id)

        return Event('server', 'info', msg)


    def _on_profile_resumed (self, port_id, profile_id):
        msg = "Profile {0}.{1} has resumed".format(port_id, profile_id)
        if port_id in self.ports:
            self.ports[port_id].async_event_profile_resumed(profile_id)

        return Event('server', 'info', msg)


    def _on_profile_finished_tx (self, port_id, profile_id):
        msg = "Profile {0}.{1} job done".format(port_id, profile_id)

        if port_id in self.ports:
            self.ports[port_id].async_event_profile_job_done(profile_id)

        ev = Event('server', 'info', msg)
        if port_id in self.get_acquired_ports():
            self.ctx.logger.info(ev)

        return ev


    def _on_profile_error (self, port_id, profile_id):
        msg = "Profile {0}.{1} job failed".format(port_id, profile_id)

        return Event('server', 'warning', msg)


#########################    private/helper     #########################
############################    functions   #############################
############################                #############################

    # remove all RX filters in a safe manner
    @validate_port_input("ports")
    def _remove_rx_filters (self, ports, rx_delay_ms):

        # get the enabled RX profiles
        rx_ports = [p for p in ports if self.ports[p.port_id].has_profile_rx_enabled(p.profile_id)]

        if not rx_ports:
            return RC_OK()

        # block while any RX configured profile has not yet have it's delay expired
        while any([not self.ports[p.port_id].has_rx_delay_expired(p.profile_id, rx_delay_ms) for p in rx_ports]):
            time.sleep(0.01)

        # remove RX filters
        return self._for_each_port('remove_rx_filters', rx_ports)

    # Check console API ports argument
    def validate_profile_input(self, input_profiles):
        ports = []
        result_profiles = []
        for profile in input_profiles:
            if profile.profile_id == ALL_PROFILE_ID:
                if int(profile) not in ports:
                    ports.append(int(profile))
                else:
                    raise TRexError("Cannot have more than on %d.* in the params" %int(profile))

        for pid in ports:
            for profile in input_profiles:
                if int(profile) == pid and profile.profile_id != ALL_PROFILE_ID:
                    raise TRexError("Cannot have %d.* and %s passed together as --ports" %(int(profile), str(profile)))

            port_profiles = self.ports[pid].get_port_profiles("all")
            result_profiles.extend(port_profiles)

        for profile in input_profiles:
            if profile.profile_id != ALL_PROFILE_ID:
                if profile not in result_profiles:
                    result_profiles.append(profile)

        return result_profiles

    # Get all profiles with the certain state from ports
    # state = {"active", "transmitting", "paused", "streams"}
    def get_profiles_with_state(self, state):
        active_ports = self.get_acquired_ports()
        active_profiles = []
        for port in active_ports:
            port_profiles = self.ports[port].get_port_profiles(state)
            active_profiles.extend(port_profiles)
        return active_profiles

############################    Stateless   #############################
############################       API      #############################
############################                #############################

    @client_api('command', True)
    def reset(self, ports = None, restart = False):
        """
            Force acquire ports, stop the traffic, remove all streams and clear stats
    
            :parameters:
                ports : list
                   Ports on which to execute the command
    
                restart: bool
                   Restart the NICs (link down / up)
    
            :raises:
                + :exc:`TRexError`
    
        """
        ports = ports if ports is not None else self.get_all_ports()
        ports = self.psv.validate('reset', ports)
        all_profiles = []
        for port in ports:
            profile = PortProfileID(str(port) + ".*")
            all_profiles.append(profile)
    
        if restart:
            if not all([p.is_link_change_supported() for p in self.ports.values()]):
                 raise TRexError("NICs of this type do not support link down, can't use restart flag.")

            self.ctx.logger.pre_cmd("Hard resetting ports {0}:".format(ports))
        else:
            self.ctx.logger.pre_cmd("Resetting ports {0}:".format(ports))
    
    
        try:
            with self.ctx.logger.suppress():
                # force take the port and ignore any streams on it
                self.acquire(ports, force = True, sync_streams = False)
                self.stop(all_profiles)
                self.remove_all_streams(all_profiles)
                self.clear_stats(ports)
                self.set_port_attr(ports,
                                   promiscuous = False if self.any_port.is_prom_supported() else None,
                                   link_up = True if restart else None)
                self.remove_rx_queue(ports)
                self._for_each_port('stop_capture_port', ports)
                self.remove_all_captures()
                self.set_service_mode(ports, False)
    
            self.ctx.logger.post_cmd(RC_OK())
    
    
        except TRexError as e:
            self.ctx.logger.post_cmd(False)
            raise


    @client_api('command', True)
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
                + :exc:`TRexError`

        """
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

        self._post_acquire_common(ports)

        # sync streams
        if sync_streams:
            rc = self._for_each_port('sync_streams', ports)
            if not rc:
                raise TRexError(rc)


    @client_api('command', True)
    def release(self, ports = None):
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
    def set_service_mode (self, ports = None, enabled = True, filtered = False, mask = None):
        ''' based on :meth:`trex.stl.trex_stl_client.STLClient.set_service_mode_base` '''

        # call the base method
        self.set_service_mode_base(ports, enabled, filtered, mask)
        
        rc = self._for_each_port('set_service_mode', ports, enabled, filtered, mask)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            raise TRexError(rc)

    @client_api('command', True)
    @validate_port_input("ports")
    def remove_all_streams (self, ports = None):
        """
            remove all streams from port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command


            :raises:
                + :exc:`TRexError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()
        # validate ports
        ports = self.psv.validate('remove_all_streams', ports, (PSV_ACQUIRED, PSV_IDLE))

        self.ctx.logger.pre_cmd("Removing all streams from port(s) {0}:".format(ports))
        rc = self._for_each_port('remove_all_streams', ports)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

 
    @client_api('command', True)
    @validate_port_input("ports")
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
                + :exc:`TRexError`

        """

        ports = ports if ports is not None else self.get_acquired_ports()

        # validate ports
        ports = self.psv.validate('add_streams', ports, (PSV_ACQUIRED, PSV_IDLE))

        if isinstance(streams, STLProfile):
            streams = streams.get_streams()

        # transform single stream
        if not isinstance(streams, list):
            streams = [streams]

        # check streams
        if not all([isinstance(stream, STLStream) for stream in streams]):
            raise TRexArgumentError('streams', streams)

        self.ctx.logger.pre_cmd("Attaching {0} streams to port(s) {1}:".format(len(streams), ports))

        rc = self._for_each_port('add_streams', ports, streams)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        # return the stream IDs
        return rc.data()


    @client_api('command', True)
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
                + :exc:`TRexError`

        """

        validate_type('filename', filename, basestring)
        profile = STLProfile.load(filename, **kwargs)
        return self.add_streams(profile.get_streams(), ports)


    @client_api('command', True)
    @validate_port_input("ports")
    def remove_streams (self, stream_id_list, ports = None):
        """
            Remove a list of streams from ports

            :parameters:
            
                stream_id_list: int or list of ints
                    Stream id list to remove
                    
                ports : list
                    Ports on which to execute the command
                


            :raises:
                + :exc:`TRexError`

        """
        validate_type('streams_id_list', stream_id_list, (int, list))
        
        # transform single stream
        stream_id_list = listify(stream_id_list)
        
        # check at least one exists
        if not stream_id_list:
            raise TRexError("remove_streams - 'stream_id_list' cannot be empty")
            
        # check stream IDs
        for i, stream_id in enumerate(stream_id_list):
            validate_type('stream ID:{0}'.format(i), stream_id, int)
            
        ports = ports if ports is not None else self.get_acquired_ports()
        ports = self.psv.validate('remove_streams', ports, (PSV_ACQUIRED, PSV_IDLE))

        # transform single stream
        if not isinstance(stream_id_list, list):
            stream_id_list = [stream_id_list]

        # check streams
        for stream_id in stream_id_list:
            validate_type('stream_id', stream_id, int)

        # remove streams
        self.ctx.logger.pre_cmd("Removing {0} streams from port(s) {1}:".format(len(stream_id_list), ports))
        rc = self._for_each_port("remove_streams", ports, stream_id_list)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

    # check that either port is resolved or all streams have explicit dest MAC
    def __check_streams_explicit_dest(self, streams_per_port):
        for port_id, streams in streams_per_port.items():
            if self.ports[port_id].is_resolved():
                continue
            for stream in streams:
                if not stream.is_explicit_dst_mac():
                    err = 'Port %s dest MAC is invalid and there are streams without explicit dest MAC.' % port_id
                    raise TRexError(err)

    # common checks for start API
    def __pre_start_check (self, cmd_name, ports, force, streams_per_port = None):
        ports = listify(ports)
        for port in ports:
            if isinstance(port, PortProfileID):
                if port.profile_id == ALL_PROFILE_ID:
                    err = 'Profile id * is invalid for starting the traffic. Please assign a specific profile id'
                    raise TRexError(err)

        if force:
            return self.psv.validate(cmd_name, ports)

        states = {PSV_UP:           "check the connection or specify 'force'",
                  PSV_IDLE:         "please stop them or specify 'force'",
                  PSV_NON_SERVICE:  "please disable service mode or specify 'force'"}

        if streams_per_port:
            self.__check_streams_explicit_dest(streams_per_port)
        else:
            states[PSV_RESOLVED] = "please resolve them or specify 'force'";

        return self.psv.validate(cmd_name, ports, states)
            
    
    def __decode_core_mask (self, ports, core_mask):
        available_modes = [self.CORE_MASK_PIN, self.CORE_MASK_SPLIT, self.CORE_MASK_SINGLE]

        # predefined modes
        if isinstance(core_mask, int):
            if core_mask not in available_modes:
                raise TRexError("'core_mask' can be either %s or a list of masks" % ', '.join(available_modes))

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
                raise TRexError("'core_mask' list must be the same length as 'ports' list")

            decoded_mask = {}
            for i, port in enumerate(ports):
                decoded_mask[port] = core_mask[i]

            return decoded_mask
    
                
    @client_api('command', True)
    @validate_port_input("ports")
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
                + :exc:`TRexError`

        """

        if ports is None:
            ports = []
            for pid in self.get_acquired_ports():
                port = PortProfileID(pid)
                ports.append(port)
        else:
            ports = listify(ports)

        port_id_list = parse_ports_from_profiles(ports)

        streams_per_port = {}
        for port in port_id_list:
            streams_per_port[port] = self.ports[port].streams.values()

        ports = self.__pre_start_check('START', ports, force, streams_per_port)

        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('duration', duration, (int, float))
        validate_type('total', total, bool)
        validate_type('core_mask', core_mask, (type(None), int, list))

        
        #########################
        # decode core mask argument
        if core_mask is None:
            core_mask = self.CORE_MASK_SINGLE if synchronized else self.CORE_MASK_SPLIT
        decoded_mask = self.__decode_core_mask(port_id_list, core_mask)
        #######################
        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = False,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise TRexArgumentError('mult', mult)


        # stop active ports if needed
        active_profiles = list_intersect(self.get_profiles_with_state("active"), ports)
        if active_profiles and force:
            self.stop(active_profiles)

        if synchronized:
            # start synchronized (per pair of ports) traffic
            if len(ports) % 2:
                raise TRexError('Must use even number of ports in synchronized mode')
            for port in ports:
                pair_port = int(port) ^ 0x1
                if isinstance(port, PortProfileID):
                    pair_port = str(pair_port) + "." + str(port.profile_id)
                    pair_port = PortProfileID(pair_port)

                if pair_port not in ports:
                    raise TRexError('Must use adjacent ports in synchronized mode. Port "%s" has not pair.' % port)

            start_time = time.time()
            with self.ctx.logger.supress():
                ping_data = self.ping_rpc_server()
            start_at_ts = ping_data['ts'] + max((time.time() - start_time), 0.5) * len(ports)
            synchronized_str = 'synchronized '
        else:
            start_at_ts = 0
            synchronized_str = ''

        # clear flow stats and latency stats when starting traffic. (Python cache only)
        self.pgid_stats.clear_stats(clear_flow_stats=True, clear_latency_stats=True)

        # start traffic
        self.ctx.logger.pre_cmd("Starting {}traffic on port(s) {}:".format(synchronized_str, ports))

        # mask is port specific information
        pargs = {k:{'mask': v} for k, v in decoded_mask.items()}

        rc = self._for_each_port("start", ports, mult_obj, duration, force, start_at_ts = start_at_ts, pargs = pargs)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        return rc
        

    @client_api('command', True)
    @validate_port_input("ports")
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
                + :exc:`TRexError`

        """

        if ports is None:
            ports = self.get_profiles_with_state("active")
            if not ports:
                return

        ports = self.psv.validate('STOP', ports, PSV_ACQUIRED)
        if not ports:
            return

        port_id_list = parse_ports_from_profiles(ports)

        self.ctx.logger.pre_cmd("Stopping traffic on port(s) {0}:".format(ports))
        rc = self._for_each_port('stop', ports)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)

        if rx_delay_ms is None:
            if self.ports[port_id_list[0]].is_virtual(): # assume all ports have same type
                rx_delay_ms = 100
            else:
                rx_delay_ms = 10

        # remove any RX filters
        rc = self._remove_rx_filters(ports, rx_delay_ms)
        if not rc:
            raise TRexError(rc)

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

                rx_delay_ms : int
                    Time to wait (in milliseconds) after last packet was sent, until RX filters used for
                    measuring flow statistics and latency are removed.
                    This value should reflect the time it takes packets which were transmitted to arrive
                    to the destination.
                    After this time, RX filters will be removed, and packets arriving for per flow statistics feature and latency flows will be counted as errors.

            :raises:
                + :exc:`TRexTimeoutError` - in case timeout has expired
                + :exe:'TRexError'

        """

        # call the base implementation
        ports = ports if ports is not None else self.get_acquired_ports()

        ports = self.psv.validate('wait_on_traffic', ports, PSV_ACQUIRED)

        TRexClient.wait_on_traffic(self, ports, timeout)

        if rx_delay_ms is None:
            if self.ports[ports[0]].is_virtual(): # assume all ports have same type
                rx_delay_ms = 100
            else:
                rx_delay_ms = 10

        # remove any RX filters
        rc = self._remove_rx_filters(ports, rx_delay_ms = rx_delay_ms)
        if not rc:
            raise TRexError(rc)

        
    @client_api('command', True)
    @validate_port_input("ports")
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
                + :exc:`TRexError`

        """

        ports = ports if ports is not None else self.get_profiles_with_state("active")
        ports = self.psv.validate('update', ports, (PSV_ACQUIRED, PSV_TX))

        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('total', total, bool)

        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult,
                                                  allow_update = True,
                                                  divide_count = len(ports) if total else 1)
        if not mult_obj:
            raise TRexArgumentError('mult', mult)


        # call low level functions
        self.ctx.logger.pre_cmd("Updating traffic on port(s) {0}:".format(ports))
        rc = self._for_each_port("update", ports, mult_obj, force)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    @validate_port_input("port")
    def update_streams(self, port, mult = "1", force = False, stream_ids = None):
        """
            | Temporary hack to update specific streams.
            | Do not rely on this function, might be removed in future!
            | Warning: Changing rates of specific streams causes out of sync between CP and DP regarding streams rate.
            | In order to update rate of whole port, need to revert changes made to rates of those streams.

            :parameters:
                port : int
                    Port on which to execute the command

                mult : str
                    Multiplier in a form of pps, bps, or line util in %
                    Examples: "5kpps", "10gbps", "85%", "32mbps"

                force : bool
                    If the port are not in stopped mode or do not have sufficient bandwidth for the traffic, determines whether to stop the current traffic and force start.
                    True: Force start
                    False: Do not force start

            :raises:
                + :exc:`TRexError`

        """
        validate_type('mult', mult, basestring)
        validate_type('force', force, bool)
        validate_type('stream_ids', stream_ids, list)

        ports = self.psv.validate('update_streams', port, (PSV_ACQUIRED, PSV_TX))

        if not stream_ids:
            raise TRexError('Please specify stream IDs to update')

        # verify multiplier
        mult_obj = parsing_opts.decode_multiplier(mult, allow_update = False)
        if not mult_obj:
            raise TRexArgumentError('mult', mult)

        # call low level functions
        self.ctx.logger.pre_cmd('Updating streams %s on port %s:' % (stream_ids, port))
        rc = self._for_each_port("update_streams", port, mult_obj, force, stream_ids)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    @validate_port_input("ports")
    def pause (self, ports = None):
        """
            Pause traffic on port(s). Works only for ports that are active, and only if all streams are in Continuous mode.

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`TRexError`

        """
        ports = ports if ports is not None else self.get_profiles_with_state("transmitting")
        ports = self.psv.validate('pause', ports, (PSV_ACQUIRED, PSV_TX))

        self.ctx.logger.pre_cmd("Pausing traffic on port(s) {0}:".format(ports))
        rc = self._for_each_port("pause", ports)

        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    @validate_port_input("port")
    def pause_streams(self, port, stream_ids):
        """
            Temporary hack to pause specific streams.
            Does not change state of port.
            Do not rely on this function, might be removed in future!

            :parameters:
                port : int
                    Port on which to execute the command
                stream_ids : list
                    Stream IDs to pause

            :raises:
                + :exc:`TRexError`

        """

        validate_type('stream_ids', stream_ids, list)

        ports = self.psv.validate('pause_streams', port, (PSV_ACQUIRED, PSV_TX))

        if not stream_ids:
            raise TRexError('Please specify stream IDs to pause')

        self.ctx.logger.pre_cmd('Pause streams %s on port %s:' % (stream_ids, port))
        rc = self._for_each_port("pause_streams", port, stream_ids)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    @validate_port_input("ports")
    def resume (self, ports = None):
        """
            Resume traffic on port(s)

            :parameters:
                ports : list
                    Ports on which to execute the command

            :raises:
                + :exc:`TRexError`

        """

        ports = ports if ports is not None else self.get_profiles_with_state("paused")
        ports = self.psv.validate('resume', ports, (PSV_ACQUIRED, PSV_PAUSED))

        self.ctx.logger.pre_cmd("Resume traffic on port(s) {0}:".format(ports))
        rc = self._for_each_port('resume', ports)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
    @validate_port_input("port")
    def resume_streams(self, port, stream_ids):
        """
            Temporary hack to resume specific streams.
            Does not change state of port.
            Do not rely on this function, might be removed in future!

            :parameters:
                port : int
                    Port on which to execute the command
                stream_ids : list
                    Stream IDs to resume

            :raises:
                + :exc:`TRexError`

        """

        validate_type('stream_ids', stream_ids, list)

        ports = self.psv.validate('resume_streams', port, (PSV_ACQUIRED))

        if not stream_ids:
            raise TRexError('Please specify stream IDs to resume')

        self.ctx.logger.pre_cmd('Resume streams %s on port %s:' % (stream_ids, port))
        rc = self._for_each_port("resume_streams", port, stream_ids)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)




    def __push_remote (self, pcap_filename, port_id_list, ipg_usec, speedup, count, duration, is_dual, min_ipg_usec):

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


    @client_api('command', True)
    def push_remote (self,
                     pcap_filename,
                     ports = None,
                     ipg_usec = None,
                     speedup = 1.0,
                     count = 1,
                     duration = -1,
                     is_dual = False,
                     min_ipg_usec = None,
                     force  = False,
                     src_mac_pcap = False,
                     dst_mac_pcap = False):
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
 
                src_mac_pcap : bool
                    Source MAC address will be taken from pcap file if True.

                dst_mac_pcap : bool
                    Destination MAC address will be taken from pcap file if True.
                
            :raises:
                + :exc:`TRexError`

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
        validate_type('src_mac_pcap', src_mac_pcap, bool)
        validate_type('dst_mac_pcap', dst_mac_pcap, bool)

        # if force - stop any active ports
        if force:
            active_ports = list(set(self.get_active_ports()).intersection(ports))
            all_profiles = []
            for port in active_ports:
                profile = PortProfileID(str(port) + ".*")
                all_profiles.append(profile)
            if all_profiles:
                self.stop(all_profiles)

        # for dual mode check that all are masters
        if is_dual:
            if not pcap_filename.endswith('erf'):
                raise TRexError("dual mode: only ERF format is supported for dual mode")

            for port in ports:
                master = port
                slave = port ^ 0x1

                if slave in ports:
                    raise TRexError("dual mode: cannot provide adjacent ports ({0}, {1}) in a batch".format(master, slave))

                if slave not in self.get_acquired_ports():
                    raise TRexError("dual mode: adjacent port {0} must be owned during dual mode".format(slave))

        # overload the count in new version, workaround instead of passing new variable     
        if count & 0xC0000000:
            raise TRexError("count is limited to 0x3fff,ffff")

        count = count & 0x3FFFFFFF
        if src_mac_pcap:
            count |= 0x80000000
        if dst_mac_pcap:
            count |= 0x40000000

        self.ctx.logger.pre_cmd("Pushing remote PCAP on port(s) {0}:".format(ports))
        rc = self.__push_remote(pcap_filename, ports, ipg_usec, speedup, count, duration, is_dual, min_ipg_usec)
        self.ctx.logger.post_cmd(rc)

        if not rc:
            raise TRexError(rc)


    @client_api('command', True)
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
                   min_ipg_usec = None,
                   src_mac_pcap = False,
                   dst_mac_pcap = False):
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
                    Requires that all the ports will be in master mode
                    with their adjacent ports as slaves

                min_ipg_usec : float
                    Minimum inter-packet gap in microseconds to guard from too small ipg.
                    Exclusive with ipg_usec

                src_mac_pcap : bool
                    Source MAC address will be taken from pcap file if True.

                dst_mac_pcap : bool
                    Destination MAC address will be taken from pcap file if True.

            :raises:
                + :exc:`TRexError`

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
        validate_type('src_mac_pcap', src_mac_pcap, bool)
        validate_type('dst_mac_pcap', dst_mac_pcap, bool)
        if all([ipg_usec, min_ipg_usec]):
            raise TRexError('Please specify either ipg or minimal ipg, not both.')

        # if force - stop any active ports
        if force:
            active_ports = list(set(self.get_active_ports()).intersection(ports))
            if active_ports:
                self.stop(active_ports)


        # no support for > 1MB PCAP - use push remote
        file_size = os.path.getsize(pcap_filename)
        if not force and file_size > (1024 * 1024):
            file_size_str = format_num(file_size, suffix = 'B')
            url = 'https://trex-tgn.cisco.com/trex/doc/trex_stateless.html#_pcap_based_traffic'
            raise TRexError("PCAP size of {:} is too big for local push - consider using remote (-r):\n{}".format(file_size_str, url))

        if is_dual:
            for port in ports:
                master = port
                slave = port ^ 0x1

                if slave in ports:
                    raise TRexError("dual mode: please specify only one of adjacent ports ({0}, {1}) in a batch".format(master, slave))

                if slave not in self.get_acquired_ports():
                    raise TRexError("dual mode: adjacent port {0} must be owned during dual mode".format(slave))

        # regular push
        if not is_dual:

            # create the profile from the PCAP
            try:
                self.ctx.logger.pre_cmd("Converting '{0}' to streams:".format(pcap_filename))
                profile = STLProfile.load_pcap(pcap_filename,
                                               ipg_usec,
                                               speedup,
                                               count,
                                               vm = vm,
                                               packet_hook = packet_hook,
                                               min_ipg_usec = min_ipg_usec,
                                               src_mac_pcap = src_mac_pcap,
                                               dst_mac_pcap = dst_mac_pcap)
                self.ctx.logger.post_cmd(RC_OK())
            except TRexError as e:
                self.ctx.logger.post_cmd(RC_ERR(e))
                raise


            self.remove_all_streams(ports = ports)
            id_list = self.add_streams(profile.get_streams(), ports)
            
            return self.start(ports = ports, duration = duration, force = force)

        else:

            # create a dual profile
            split_mode = 'MAC'
            if (ipg_usec and ipg_usec < 1000 * speedup) or (min_ipg_usec and min_ipg_usec < 1000):
                self.ctx.logger.warning('In order to get synchronized traffic, ensure that effective ipg is at least 1000 usec')

            try:
                self.ctx.logger.pre_cmd("Analyzing '{0}' for dual ports based on {1}:".format(pcap_filename, split_mode))
                profile_a, profile_b = STLProfile.load_pcap(pcap_filename,
                                                            ipg_usec,
                                                            speedup,
                                                            count,
                                                            vm = vm,
                                                            packet_hook = packet_hook,
                                                            split_mode = split_mode,
                                                            min_ipg_usec = min_ipg_usec,
                                                            src_mac_pcap = src_mac_pcap,
                                                            dst_mac_pcap = dst_mac_pcap)

                self.ctx.logger.post_cmd(RC_OK())

            except TRexError as e:
                self.ctx.logger.post_cmd(RC_ERR(e))
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




    # get stats
    @client_api('getter', True)
    def get_stats (self, ports = None, sync_now = True):
        """
            Gets all statistics on given ports, flow stats and latency.

            :parameters:
                ports: list

                sync_now: boolean
        """

        output = self._get_stats_common(ports, sync_now)

        # TODO: move this to a generic protocol (AbstractStats)
        pgid_stats = self.get_pgid_stats()
        if not pgid_stats:
            raise TRexError(pgid_stats)

        output['flow_stats'] = pgid_stats.get('flow_stats', {})
        output['latency']    = pgid_stats.get('latency', {})

        return output


    # clear stats
    @client_api('command', True)
    def clear_stats (self,
                     ports = None,
                     clear_global = True,
                     clear_flow_stats = True,
                     clear_latency_stats = True,
                     clear_xstats = True):
        """
            Clears statistics in given ports.

            :parameters:
                ports: list

                clear_global: boolean

                clear_flow_stats: boolean

                clear_latency_stats: boolean

                clear_xstats: boolean
        """

        self._clear_stats_common(ports, clear_global, clear_xstats)

        # TODO: move this to a generic protocol
        if clear_flow_stats or clear_latency_stats:
            self.pgid_stats.clear_stats(clear_flow_stats=clear_flow_stats, clear_latency_stats=clear_latency_stats)



    @client_api('getter', True)
    def get_active_pgids(self):
        """
            Get active packet group IDs

            :Parameters:
                None

            :returns:
                Dict with entries 'latency' and 'flow_stats'. Each entry contains list of used packet group IDs
                of the given type.

            :Raises:
                + :exc:`TRexError`

        """
        return self.pgid_stats.get_active_pgids()


    @client_api('getter', True)
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
                + :exc:`TRexError`

        """

        # transform single stream
        pgid_list = listify(pgid_list)
        return self.pgid_stats.get_stats(pgid_list)

############################   console   #############################
############################   commands  #############################
############################             #############################

    def _show_streams_stats(self, buffer = sys.stdout):
        all_pg_ids = self.get_active_pgids()
        # Display data for at most 4 pgids. If there are latency PG IDs, use them first
        pg_ids = all_pg_ids['latency'][:4]
        pg_ids += all_pg_ids['flow_stats'][:4 - len(pg_ids)]
        table = self.pgid_stats.streams_stats_to_table(pg_ids)
        # show
        text_tables.print_table_with_header(table, table.title, buffer = buffer)

    def _show_latency_stats(self, buffer = sys.stdout):
        all_pg_ids = self.get_active_pgids()
        # Display data for at most 5 pgids.
        pg_ids = all_pg_ids['latency'][:5]
        table = self.pgid_stats.latency_stats_to_table(pg_ids)
        # show
        text_tables.print_table_with_header(table, table.title, buffer = buffer)

    def _show_latency_histogram(self, buffer = sys.stdout):
        all_pg_ids = self.get_active_pgids()
        # Display data for at most 5 pgids.
        pg_ids = all_pg_ids['latency'][:5]
        table = self.pgid_stats.latency_histogram_to_table(pg_ids)
        # show
        text_tables.print_table_with_header(table, table.title, buffer = buffer)


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


    @console_api('stats', 'common', True)
    def show_stats_line (self, line):
        '''Show various statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.show_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STL_STATS)

        opts = parser.parse_args(line.split())

        # without parameters show only global and ports
        if not opts.stats:
            self._show_global_stats()
            self._show_port_stats(opts.ports)
            return


        # decode which stats to show
        if opts.stats == 'global':
            self._show_global_stats()

        elif opts.stats == 'ports':
            self._show_port_stats(opts.ports)

        elif opts.stats == 'xstats':
            self._show_port_xstats(opts.ports, False)

        elif opts.stats == 'xstats_inc_zero':
            self._show_port_xstats(opts.ports, True)

        elif opts.stats == 'status':
            self._show_port_status(opts.ports)

        elif opts.stats == 'cpu':
            self._show_cpu_util()

        elif opts.stats == 'mbuf':
            self._show_mbuf_util()

        elif opts.stats == 'streams':
            self._show_streams_stats()

        elif opts.stats == 'latency':
            self._show_latency_stats()

        elif opts.stats == 'latency_histogram':
            self._show_latency_histogram()

        else:
            raise TRexError('Unhandled stats: %s' % opts.stats)

    def _get_profiles(self, port_id_list):
        profiles_per_port = OrderedDict()
        for port_id in port_id_list:
            data = self.ports[port_id].generate_loaded_profiles()
            if data:
                profiles_per_port[port_id] = data
        return profiles_per_port

    def _get_streams(self, port_id_list, streams_mask, table_format):
        streams_per_port = OrderedDict()
        for port_id in port_id_list:
            data = self.ports[port_id].generate_loaded_streams_sum(streams_mask, table_format)
            if data:
                streams_per_port[port_id] = data
        return streams_per_port

    @console_api('profiles', 'STL', True, True)
    def profiles_line(self, line):
        '''Get loaded to server profiles information'''
        parser = parsing_opts.gen_parser(self,
                                         "profiles",
                                         self.profiles_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        profiles_per_port = self._get_profiles(opts.ports)

        if not profiles_per_port:
            self.logger.info(format_text("No profiles found with desired filter.\n", "bold", "magenta"))

        for port_id, port_profiles_table in profiles_per_port.items():
            if port_profiles_table:
                text_tables.print_table_with_header(port_profiles_table,
                                                    header = 'Port %s:' % port_id)

    @console_api('streams', 'STL', True, True)
    def streams_line(self, line):
        '''Get loaded to server streams information'''
        parser = parsing_opts.gen_parser(self,
                                         "streams",
                                         self.streams_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.STREAMS_MASK,
                                         parsing_opts.STREAMS_CODE)

        opts = parser.parse_args(line.split())

        if not opts:
            return opts

        streams_per_port = self._get_streams(opts.ports, set(opts.ids), table_format = opts.code is None)

        if not streams_per_port:
            self.logger.info(format_text("No streams found with desired filter.\n", "bold", "magenta"))

        elif opts.code is None: # Just print the summary table of streams

            for port_id, port_streams_table in streams_per_port.items():
                if port_streams_table:
                    text_tables.print_table_with_header(port_streams_table,
                                                        header = 'Port %s:' % port_id)

        elif opts.code: # Save the code that generates streams to file

            if not opts.code.endswith('.py'):
                raise TRexError('Saved filename should end with .py')
            is_several_ports = len(streams_per_port) > 1
            if is_several_ports:
                print(format_text('\nWarning: several ports specified, will save in separate file per port.', 'bold'))
            for port_id, port_streams_data in streams_per_port.items():
                if not port_streams_data:
                    print('No streams to save at port %s, skipping.' % port_id)
                    continue
                filename = ('%s_port%s.py' % (opts.code[:-3], port_id)) if is_several_ports else opts.code
                if os.path.exists(filename):
                    sys.stdout.write('\nFilename %s already exists, overwrite? (y/N) ' % filename)
                    ans = user_input().strip()
                    if ans.lower() not in ('y', 'yes'):
                        print('Not saving.')
                        continue
                self.logger.pre_cmd('Saving file as: %s' % filename)
                try:
                    profile = STLProfile(list(port_streams_data.values()))
                    with open(filename, 'w') as f:
                        f.write(profile.dump_to_code())
                except Exception as e:
                    self.logger.post_cmd(False)
                    print(e)
                    print('')
                else:
                    self.logger.post_cmd(True)

        else: # Print the code that generates streams

            for port_id, port_streams_data in streams_per_port.items():
                if not port_streams_data:
                    continue
                print(format_text('Port: %s' % port_id, 'cyan', 'underline') + '\n')
                for stream_id, stream in port_streams_data.items():
                    print(format_text('Stream ID: %s' % stream_id, 'cyan', 'underline'))
                    print('    ' + '\n    '.join(stream.to_code().splitlines()) + '\n')


    @console_api('push', 'STL', True)
    def push_line(self, line):
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
                parsing_opts.DUAL,
                parsing_opts.SRC_MAC_PCAP,
                parsing_opts.DST_MAC_PCAP]

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
                             is_dual        = opts.dual,
                             src_mac_pcap   = opts.src_mac_pcap,
                             dst_mac_pcap   = opts.dst_mac_pcap)

        else:
            self.push_pcap(opts.file[0],
                           ports          = opts.ports,
                           ipg_usec       = opts.ipg_usec,
                           min_ipg_usec   = opts.min_ipg_usec,
                           speedup        = opts.speedup,
                           count          = opts.count,
                           duration       = opts.duration,
                           force          = opts.force,
                           is_dual        = opts.dual,
                           src_mac_pcap   = opts.src_mac_pcap,
                           dst_mac_pcap   = opts.dst_mac_pcap)

        return RC_OK()


    @console_api('service', 'STL', True)
    def service_line (self, line):
        '''Configures port for service mode.
           In service mode ports will reply to ARP, PING
           and etc.
        '''

        parser = parsing_opts.gen_parser(self,
                                         "service",
                                         self.service_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.SERVICE_GROUP)

        opts = parser.parse_args(line.split())
        enabled, filtered, mask = self._get_service_params(opts)

        self.set_service_mode(ports = opts.ports, enabled = enabled, filtered = filtered, mask = mask)
        
        return True

    @console_api('start', 'STL', True)
    def start_line (self, line):
        '''Start selected traffic on specified ports on TRex\n'''
        # parser for parsing the start command arguments
        parser = parsing_opts.gen_parser(self,
                            "start",
                            self.start_line.__doc__,
                            parsing_opts.PROFILE_LIST,
                            parsing_opts.TOTAL,
                            parsing_opts.FORCE,
                            parsing_opts.FILE_PATH,
                            parsing_opts.DURATION,
                            parsing_opts.ARGPARSE_TUNABLES,
                            parsing_opts.MULTIPLIER_STRICT,
                            parsing_opts.DRY_RUN,
                            parsing_opts.CORE_MASK_GROUP,
                            parsing_opts.SYNCHRONIZED)

        opts = parser.parse_args(line.split(), default_ports = self.get_acquired_ports(), verify_acquired = True)
        help_flags = ('-h', '--help')

        # if the user chose to pass the tunables arguments in previous version (-t var1=x1,var2=x2..)
        # we decode the tunables and then convert the output from dictionary to list in order to have the same format with the
        # newer version.
        tunable_dict = {}
        if "-t" in line and '=' in line:
            tun_list = opts.tunables
            tunable_dict = parsing_opts.decode_tunables(tun_list[0])
            opts.tunables = parsing_opts.convert_old_tunables_to_new_tunables(tun_list[0])
            opts.tunables.extend(tun_list[1:])
        tunable_dict["tunables"] = opts.tunables

        ports = []
        for port in opts.ports:
            if not isinstance(port, PortProfileID):
                port = PortProfileID(port)
            ports.append(port)

        port_id_list = parse_ports_from_profiles(ports)

        # core mask
        if opts.core_mask is not None:
            core_mask =  opts.core_mask
        else:
            core_mask = self.CORE_MASK_PIN if opts.pin_cores else self.CORE_MASK_SPLIT

        # just for sanity - will be checked on the API as well
        self.__decode_core_mask(port_id_list, core_mask)

        streams_per_profile = {}
        streams_per_port = {}
        # pack the profile
        try:
            for profile in ports:
                profile_name = str(profile)
                port_id = int(profile)
                profile = STLProfile.load(opts.file[0],
                                          direction = port_id % 2,
                                          port_id = port_id,
                                          **tunable_dict)
                if any(h in opts.tunables for h in help_flags):
                    return True
                if profile is None:
                    print('Failed to convert STL profile')
                    return False
                stream_list = profile.get_streams()
                streams_per_profile[profile_name] = stream_list
                if port_id not in streams_per_port:
                    streams_per_port[port_id] = list(stream_list)
                else:
                    streams_per_port[port_id].extend(list(stream_list))
        except TRexError as e:
            s = format_text("\nError loading profile '{0}'\n".format(opts.file[0]), 'bold')
            s += "\n" + e.brief()
            raise TRexError(s)
        # for better use experience - check this before any other action on port
        self.__pre_start_check('START', ports, opts.force, streams_per_port)
        ports = self.validate_profile_input(ports)

        # stop ports if needed
        active_profiles = list_intersect(self.get_profiles_with_state("active"), ports)
        if active_profiles and opts.force:
            self.stop(active_profiles)

        # remove all streams
        self.remove_all_streams(ports)

        for profile in ports:
            profile_name = str(profile)
            self.add_streams(streams_per_profile[profile_name], ports = profile)

        if opts.dry:
            self.validate(ports, opts.mult, opts.duration, opts.total)
        else:
            self.start(ports,
                       opts.mult,
                       opts.force,
                       opts.duration,
                       opts.total,
                       core_mask,
                       opts.sync)

        return True


    @console_api('stop', 'STL', True)
    def stop_line (self, line):
        '''Stop active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "stop",
                                         self.stop_line.__doc__,
                                         parsing_opts.PROFILE_LIST_WITH_ALL,
                                         parsing_opts.REMOVE)

        opts = parser.parse_args(line.split(), default_ports = self.get_profiles_with_state("active"), verify_acquired = True, allow_empty = True)
        ports = self.validate_profile_input(opts.ports)

        # find the relevant ports
        port_id_list = parse_ports_from_profiles(ports)
        active_ports = list_intersect(ports, self.get_profiles_with_state("active"))
        if not active_ports:
            if not ports:
                msg = 'no active ports'
            else:
                msg = 'no active traffic on ports {0}'.format(ports)
            print(msg)
        else:
            # call API
            self.stop(active_ports)

        if opts.remove:
            streams_ports = list_intersect(ports, self.get_profiles_with_state("streams"))
            if not streams_ports:
                if not ports:
                    msg = 'no ports with streams'
                else:
                    msg = 'no streams on ports {0}'.format(ports)
                print(msg)
            else:
                # call API
                self.remove_all_streams(ports)

        return True


    @console_api('update', 'STL', True)
    def update_line (self, line):
        '''Update port(s) speed currently active\n'''
        parser = parsing_opts.gen_parser(self,
                                         "update",
                                         self.update_line.__doc__,
                                         parsing_opts.PROFILE_LIST,
                                         parsing_opts.MULTIPLIER,
                                         parsing_opts.TOTAL,
                                         parsing_opts.FORCE,
                                         parsing_opts.STREAMS_MASK)

        opts = parser.parse_args(line.split(), default_ports = self.get_profiles_with_state("active"), verify_acquired = True)
        ports = self.validate_profile_input(opts.ports)

        if opts.ids:
            if len(ports) != 1:
                raise TRexError('must provide exactly one port when specifying stream_ids, got: %s' % ports)

            self.update_streams(ports[0], opts.mult, opts.force, opts.ids)
            return True

        # find the relevant ports
        profiles = list_intersect(ports, self.get_profiles_with_state("active"))
        if not profiles:
            if not ports:
                msg = 'no active ports'
            else:
                msg = 'no active traffic on ports {0}'.format(ports)

            raise TRexError(msg)

        self.update(profiles, opts.mult, opts.total, opts.force)

        return True


    @console_api('pause', 'STL', True)
    def pause_line (self, line):
        '''Pause active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "pause",
                                         self.pause_line.__doc__,
                                         parsing_opts.PROFILE_LIST,
                                         parsing_opts.STREAMS_MASK)

        opts = parser.parse_args(line.split(), default_ports = self.get_profiles_with_state("transmitting"), verify_acquired = True)
        ports = self.validate_profile_input(opts.ports)

        if opts.ids:
            if len(ports) != 1:
                raise TRexError('pause - must provide exactly one port when specifying stream_ids, got: %s' % ports)

            self.pause_streams(ports[0], opts.ids)

            return True

        # check for already paused case
        if ports and is_sub_list(ports, self.get_profiles_with_state("paused")):
            raise TRexError('all of ports(s) {0} are already paused'.format(ports))

        # find the relevant ports
        profiles = list_intersect(ports, self.get_profiles_with_state("transmitting"))
        if not profiles:
            if not ports:
                msg = 'no transmitting ports'
            else:
                msg = 'none of ports {0} are transmitting'.format(ports)

            raise TRexError(msg)

        self.pause(profiles)

        return True


    @console_api('resume', 'STL', True)
    def resume_line (self, line):
        '''Resume active traffic on specified ports on TRex\n'''
        parser = parsing_opts.gen_parser(self,
                                         "resume",
                                         self.resume_line.__doc__,
                                         parsing_opts.PROFILE_LIST,
                                         parsing_opts.STREAMS_MASK)

        opts = parser.parse_args(line.split(), default_ports = self.get_profiles_with_state("paused"), verify_acquired = True)
        ports = self.validate_profile_input(opts.ports)

        if opts.ids:
            if len(ports) != 1:
                raise TRexError('must provide exactly one port when specifying stream_ids, got: %s' % ports)

            self.resume_streams(ports[0], opts.ids)
            return True

        # find the relevant ports
        profiles = list_intersect(ports, self.get_profiles_with_state("paused"))
        if not profiles:
            if not ports:
                msg = 'no paused ports'
            else:
                msg = 'none of ports {0} are paused'.format(ports)

            raise TRexError(msg)


        self.resume(profiles)

        # true means print time
        return True

