from __future__ import print_function
import hashlib
import sys
import time
import os
import shlex

from ..utils.common import get_current_user, user_input
from ..utils import parsing_opts, text_tables

from ..common.trex_api_annotators import client_api, console_api
from ..common.trex_client import TRexClient
from ..common.trex_events import Event
from ..common.trex_exceptions import TRexError
from ..common.trex_types import *
from ..common.trex_types import DEFAULT_PROFILE_ID, ALL_PROFILE_ID

from .trex_astf_port import ASTFPort
from .trex_astf_profile import ASTFProfile
from .topo import ASTFTopologyManager
from .stats.traffic import CAstfTrafficStats
from .stats.latency import CAstfLatencyStats
from ..utils.common import  is_valid_ipv4, is_valid_ipv6
from ..utils.text_opts import format_text
from ..astf.trex_astf_exceptions import ASTFErrorBadTG

astf_states = [
    'STATE_IDLE',
    'STATE_ASTF_LOADED',
    'STATE_ASTF_PARSE',
    'STATE_ASTF_BUILD',
    'STATE_TX',
    'STATE_ASTF_CLEANUP']

class ASTFClient(TRexClient):
    port_states = [getattr(ASTFPort, state) for state in astf_states]

    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = "error",
                 logger = None,
                 sync_timeout = None,
                 async_timeout = None):

        """ 
        TRex advance stateful client

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

        api_ver = {'name': 'ASTF', 'major': 1, 'minor': 7}

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
        self.handler = ''
        self.traffic_stats = CAstfTrafficStats(self.conn.rpc)
        self.latency_stats = CAstfLatencyStats(self.conn.rpc)
        self.topo_mngr     = ASTFTopologyManager(self)
        self.sync_waiting = False
        self.last_error = ''
        self.epoch = None
        self.state = None
        for index, state in enumerate(astf_states):
            setattr(self, state, index)
        self.transient_states = [
            self.STATE_ASTF_PARSE,
            self.STATE_ASTF_BUILD,
            self.STATE_ASTF_CLEANUP]
        self.astf_profile_state = {'_': 0}


    def get_mode(self):
        return "ASTF"

############################    called       #############################
############################    by base      #############################
############################    TRex Client  #############################

    def _on_connect(self):
        self.sync_waiting = False
        self.last_error = ''
        self.sync()
        self.topo_mngr.sync_with_server()
        return RC_OK()


    def _on_connect_create_ports(self, system_info):
        """
            called when connecting to the server
            triggered by the common client object
        """

        # create ports
        self.ports.clear()
        for port_info in system_info['ports']:
            port_id = port_info['index']
            self.ports[port_id] = ASTFPort(self.ctx, port_id, self.conn.rpc, port_info)
        return RC_OK()

    def _on_connect_clear_stats(self):
        self.traffic_stats.reset()
        self.latency_stats.reset()
        with self.ctx.logger.suppress(verbose = "warning"):
            self.clear_stats(ports = self.get_all_ports(), clear_xstats = False, clear_traffic = False)
        return RC_OK()

    def _on_astf_state_chg(self, ctx_state, error, epoch):
        if ctx_state < 0 or ctx_state >= len(astf_states):
            raise TRexError('Unhandled ASTF state: %s' % ctx_state)

        if epoch is None or epoch != self.epoch:
            return

        self.last_error = error
        if error and not self.sync_waiting:
            self.ctx.logger.error('Last command failed: %s' % error)

        self.state = ctx_state
        port_state = self.apply_port_states()

        port_state_name = ASTFPort.STATES_MAP[port_state].capitalize()

        if error:
            return Event('server', 'error', 'Moved to state: %s after error: %s' % (port_state_name, error))
        else:
            return Event('server', 'info', 'Moved to state: %s' % port_state_name)

    def _on_astf_profile_state_chg(self, profile_id, ctx_state, error, epoch):

        if ctx_state < 0 or ctx_state >= len(astf_states):
            raise TRexError('Unhandled ASTF state: %s' % ctx_state)

        if epoch is None or epoch != self.epoch:
            return

        self.last_error = error
        if error and not self.sync_waiting:
            self.ctx.logger.error('Last command failed: %s' % error)

        # update profile state
        self.astf_profile_state[profile_id] = ctx_state

        if error:
            return Event('server', 'error', 'Moved to profile %s state: %s after error: %s' % (profile_id, ctx_state, error))
        else:
            return Event('server', 'info', 'Moved to profile %s state: %s' % (profile_id, ctx_state))


    def _on_astf_profile_cleared(self, profile_id, error, epoch):

        if epoch is None or epoch != self.epoch:
            return

        self.last_error = error
        if error and not self.sync_waiting:
            self.ctx.logger.error('Last command failed: %s' % error)

        # remove profile and template group name
        self.astf_profile_state.pop(profile_id, None)
        self.traffic_stats._clear_tg_name(profile_id) 

        if error:
            return Event('server', 'error', 'Can\'t remove profile %s after error: %s' % (profile_id, error))
        else:
            return Event('server', 'info', 'Removed profile : %s' % profile_id)


############################     helper     #############################
############################     funcs      #############################
############################                #############################

    # Check console API ports argument

    def validate_profile_id_input(self, pid_input = DEFAULT_PROFILE_ID, start = False):

        valid_pids = []
        ok_states = [self.STATE_IDLE, self.STATE_ASTF_LOADED]
        
        # check profile ID's type
        if type(pid_input) is not list:
            profile_list = pid_input.split()
        else:
            profile_list = pid_input

        # Check if profile_id is a valid profile name
        for profile_id in profile_list:
            if profile_id == ALL_PROFILE_ID:
                if start == True:
                    raise TRexError("Cannot have %s as a profile value for start command" % ALL_PROFILE_ID)
                else:
                    valid_pids =  list(self.astf_profile_state.keys())
                break
            else:
                if profile_id in list(self.astf_profile_state.keys()):
                    if start == True:
                        if self.is_dynamic and self.astf_profile_state[profile_id] not in ok_states:
                            raise TRexError("%s state:Transmitting, should be one of following:Idle, Loaded profile" % profile_id)
                else:
                    if start == True:
                        self.astf_profile_state[profile_id] = self.STATE_IDLE
                    else:
                        raise TRexError("ASTF profile_id %s does not exist." % profile_id)

                if profile_id not in valid_pids:
                    valid_pids.append(profile_id)

        return valid_pids


    def apply_port_states(self):
        port_state = self.port_states[self.state]
        for port in self.ports.values():
            port.state = port_state
        return port_state

    def sync(self):
        self.epoch = None
        params = {'profile_id': "sync"}
        rc = self._transmit('sync', params)

        if not rc:
            raise TRexError(rc.err())

        self.state = rc.data()['state']
        self.apply_port_states()
        self.epoch = rc.data()['epoch']
        if self.is_dynamic:
            self.astf_profile_state = rc.data()['state_profile']
        else:
            self.astf_profile_state[DEFAULT_PROFILE_ID] = self.state
               

    def wait_for_steady(self, pid_input = DEFAULT_PROFILE_ID):

        while True:
            self.sync()
            state = self.astf_profile_state[pid_input] if self.is_dynamic else self.state
            if state not in self.transient_states:
                break
            time.sleep(0.1)

    def inc_epoch(self):
        rc = self._transmit('inc_epoch', {'handler': self.handler})
        if not rc:
            raise TRexError(rc.err())
        self.sync()

    def _transmit_async(self, rpc_func, ok_states, bad_states = None, **k):
        profile_id = k['params']['profile_id'] 
        ok_states = listify(ok_states)
        if bad_states is not None:
            bad_states = listify(bad_states)

        self.sync()
        self.wait_for_steady(profile_id)
        self.inc_epoch()
        self.sync_waiting = True
        try:
            self.last_error = ''
            rc = self._transmit(rpc_func, **k)
            self.wait_for_steady(profile_id)
            if not rc:
                return rc
            while True:
                self.sync()
                state = self.astf_profile_state[profile_id] if self.is_dynamic else self.state
                if state in ok_states:
                    return RC_OK()

                if self.last_error or (bad_states and self.state in bad_states):
                    error = self.last_error
                    return RC_ERR(error or 'Unknown error')
                time.sleep(0.2)
        finally:
            self.sync_waiting = False


############################       ASTF     #############################
############################       API      #############################
############################                #############################

    @client_api('command', True)
    def reset(self, restart = False):
        """
            Force acquire ports, stop the traffic, remove loaded traffic and clear stats

            :parameters:
                restart: bool
                   Restart the NICs (link down / up)

            :raises:
                + :exc:`TRexError`

        """

        ports = self.get_all_ports()

        if restart:
            self.ctx.logger.pre_cmd("Hard resetting ports {0}:".format(ports))
        else:
            self.ctx.logger.pre_cmd("Resetting ports {0}:".format(ports))

        try:
            with self.ctx.logger.suppress():
            # force take the port and ignore any streams on it
                self.acquire(force = True)
                self.stop(pid_input = ALL_PROFILE_ID)
                self.stop_latency()
                self.clear_stats(ports, pid_input = ALL_PROFILE_ID)
                self.clear_profile(pid_input = ALL_PROFILE_ID)
                self.set_port_attr(ports,
                                   promiscuous = False if self.any_port.is_prom_supported() else None,
                                   link_up = True if restart else None)
                self.remove_rx_queue(ports)
                self.remove_all_captures()
                self._for_each_port('stop_capture_port', ports)
            self.ctx.logger.post_cmd(RC_OK())

        except TRexError as e:
            self.ctx.logger.post_cmd(False)
            raise


    @client_api('command', True)
    def acquire(self, force = False):
        """
            Acquires ports for executing commands

            :parameters:
                force : bool
                    Force acquire the ports.

            :raises:
                + :exc:`TRexError`

        """
        ports = self.get_all_ports()

        if force:
            self.ctx.logger.pre_cmd('Force acquiring ports %s:' % ports)
        else:
            self.ctx.logger.pre_cmd('Acquiring ports %s:' % ports)

        params = {'force': force,
                  'user':  self.ctx.username}
        rc = self._transmit('acquire', params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError('Could not acquire context: %s' % rc.err())

        self.handler = rc.data()['handler']
        for port_id, port_rc in rc.data()['ports'].items():
            self.ports[int(port_id)]._set_handler(port_rc)

        self._post_acquire_common(ports)


    @client_api('command', True)
    def release(self, force = False):
        """
            Release ports

            :parameters:
                none

            :raises:
                + :exc:`TRexError`

        """

        ports = self.get_acquired_ports()
        self.ctx.logger.pre_cmd("Releasing ports {0}:".format(ports))
        params = {'handler': self.handler}
        rc = self._transmit('release', params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError('Could not release context: %s' % rc.err())

        self.handler = ''
        for port_id in ports:
            self.ports[port_id]._clear_handler()


    def _upload_fragmented(self, rpc_cmd, upload_string, pid_input = DEFAULT_PROFILE_ID):
        index_start = 0
        fragment_length = 1000 # first fragment is small, we compare hash before sending the rest
        while len(upload_string) > index_start:
            index_end = index_start + fragment_length
            params = {
                'handler': self.handler,
                'profile_id' : pid_input,
                'fragment': upload_string[index_start:index_end],
                }
            if index_start == 0:
                params['frag_first'] = True
            if index_end >= len(upload_string):
                params['frag_last'] = True
            if params.get('frag_first') and not params.get('frag_last'):
                params['md5'] = hashlib.md5(upload_string.encode()).hexdigest()

            rc = self._transmit(rpc_cmd, params = params)
            if not rc:
                return rc
            if params.get('frag_first') and not params.get('frag_last'):
                if rc.data() and rc.data().get('matches_loaded'):
                    break
            index_start = index_end
            fragment_length = 500000 # rest of fragments are larger
        return RC_OK()


    @client_api('command', True)
    def load_profile(self, profile, tunables = {}, pid_input = DEFAULT_PROFILE_ID):
        """ Upload ASTF profile to server

            :parameters:
                profile: string or ASTFProfile
                    Path to profile filename or profile object

                tunables: dict
                    forward those key-value pairs to the profile file 

                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`

        """

        if not isinstance(profile, ASTFProfile):
            try:
                profile = ASTFProfile.load(profile, **tunables)
            except Exception as e:
                self.astf_profile_state.pop(pid_input, None)
                raise TRexError('Could not load profile: %s' % e)
        profile_json = profile.to_json_str(pretty = False, sort_keys = True)

        self.ctx.logger.pre_cmd('Loading traffic at acquired ports.')
        rc = self._upload_fragmented('profile_fragment', profile_json, pid_input = pid_input)
        if not rc:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Could not load profile, error: %s' % rc.err())
        self.ctx.logger.post_cmd(True)


    @client_api('command', False)
    def get_traffic_distribution(self, start_ip, end_ip, dual_ip, seq_split):
        ''' Get distribution of IP range per TRex port per core

            :parameters:
                start_ip: IP string
                    Related to "ip_range" argument of ASTFIPGenDist

                end_ip: IP string
                    Related to "ip_range" argument of ASTFIPGenDist

                dual_ip: IP string
                    Related to "ip_offset" argument of ASTFIPGenGlobal

                seq_split: bool
                    Related to "per_core_distribution" argument of ASTFIPGenDist, "seq" => seq_split=True
        '''
        if not is_valid_ipv4(start_ip):
            raise TRexError("start_ip is not a valid IPv4 address: '%s'" % start_ip)
        if not is_valid_ipv4(end_ip):
            raise TRexError("end_ip is not a valid IPv4 address: '%s'" % end_ip)
        if not is_valid_ipv4(dual_ip):
            raise TRexError("dual_ip is not a valid IPv4 address: '%s'" % dual_ip)

        params = {
            'start_ip': start_ip,
            'end_ip': end_ip,
            'dual_ip': dual_ip,
            'seq_split': seq_split,
        }
        rc = self._transmit('get_traffic_dist', params = params)
        if not rc:
            raise TRexError(rc.err())
        res = {}
        for port_id, port_data in rc.data().items():
            core_dict = {}
            for core_id, core_data in port_data.items():
                core_dict[int(core_id)] = core_data
            res[int(port_id)] = core_dict
        return res


    @client_api('command', True)
    def clear_profile(self, pid_input = DEFAULT_PROFILE_ID):
        """
            Clear loaded profile

            :parameters:
                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`
        """
        ok_states = [self.STATE_IDLE, self.STATE_ASTF_LOADED]
        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            if self.astf_profile_state[profile_id] in ok_states:
                params = {
                    'handler': self.handler,
                    'profile_id': profile_id
                }
                self.ctx.logger.pre_cmd('Clearing loaded profile.')

                rc = self._transmit('profile_clear', params = params)
                self.ctx.logger.post_cmd(rc)
                if not rc:
                    raise TRexError(rc.err())
            else:
                self.logger.info(format_text("Cannot remove a profile: %s is not state IDLE and state LOADED.\n" % profile_id, "bold", "magenta"))


    @client_api('command', True)
    def start(self, mult = 1, duration = -1, nc = False, block = True, latency_pps = 0, ipv6 = False, pid_input = DEFAULT_PROFILE_ID, client_mask = 0xffffffff):
        """
            Start the traffic on loaded profile. Procedure is async.

            :parameters:
                mult: int
                    Multiply total CPS of profile by this value.

                duration: float
                    Start new flows for this duration.
                    Negative value means infinite

                nc: bool
                    Do not wait for flows to close at end of duration.

                block: bool
                    Wait for traffic to be started (operation is async).

                latency_pps: uint32_t
                    Rate of latency packets. Zero value means disable.

                ipv6: bool
                    Convert traffic to IPv6.

                client_mask: uint32_t
                    Bitmask of enabled client ports.

                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`
        """

        params = {
            'handler': self.handler,
            'profile_id': pid_input,
            'mult': mult,
            'nc': nc,
            'duration': duration,
            'latency_pps': latency_pps,
            'ipv6': ipv6,
            'client_mask': client_mask,
            }

        self.ctx.logger.pre_cmd('Starting traffic.')

        valid_pids = self.validate_profile_id_input(pid_input, start = True)

        for profile_id in valid_pids:

            if block:
                rc = self._transmit_async('start', params = params, ok_states = self.STATE_TX, bad_states = self.STATE_ASTF_LOADED)
            else:
                rc = self._transmit('start', params = params)
            self.ctx.logger.post_cmd(rc)

            if not rc:
                raise TRexError(rc.err())


    @client_api('command', True)
    def stop(self, block = True, pid_input = DEFAULT_PROFILE_ID, is_remove = False):
        """
            Stop the traffic.

            :parameters:
                block: bool
                    Wait for traffic to be stopped (operation is async)
                    Default is True

                pid_input: string
                    Input profile ID

                is_remove: bool
                    Remove the profile id
                    Default is False

            :raises:
                + :exc:`TRexError`
        """

        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            params = {
                'handler': self.handler,
                'profile_id': profile_id
                }
            self.ctx.logger.pre_cmd('Stopping traffic.')
            if block or is_remove:
                rc = self._transmit_async('stop', params = params, ok_states = [self.STATE_IDLE, self.STATE_ASTF_LOADED])
            else:
                rc = self._transmit('stop', params = params)
            self.ctx.logger.post_cmd(rc)

            if not rc:
                raise TRexError(rc.err())

            if is_remove:
                self.clear_profile(pid_input = profile_id)


    @client_api('command', True)
    def update(self, mult, pid_input = DEFAULT_PROFILE_ID):
        """
            Update the rate of running traffic.

            :parameters:
                mult: int
                    Multiply total CPS of profile by this value (not relative to current running rate)
                    Default is 1

                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`

        """
        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            params = {
                'handler': self.handler,
                'profile_id': profile_id,
                'mult': mult,
                }
            self.ctx.logger.pre_cmd('Updating traffic.')
            rc = self._transmit('update', params = params)
            self.ctx.logger.post_cmd(rc)
            if not rc:
                raise TRexError(rc.err())


    @client_api('command', True)
    def get_profiles(self):
        """
            Get profile list from Server.

        """
        params = {
            'handler': self.handler,
            }
        self.ctx.logger.pre_cmd('Getting profile list.')
        rc = self._transmit('get_profile_list', params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def wait_on_traffic(self, timeout = None):
        """
            Block until traffic stops

            :parameters:
                timeout: int
                    Timeout in seconds
                    Default is blocking

            :raises:
                + :exc:`TRexTimeoutError` - in case timeout has expired
                + :exc:`TRexError`
        """

        ports = self.get_all_ports()
        TRexClient.wait_on_traffic(self, ports, timeout)


    # get stats
    @client_api('getter', True)
    def get_stats(self, ports = None, sync_now = True, skip_zero = True, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        """
            Gets all statistics on given ports, traffic and latency.

            :parameters:
                port: list

                sync_now: boolean

                skip_zero: boolean

                pid_input: string
                    Input profile ID

                is_sum: boolean
                    Get total counter values

        """
        stats = self._get_stats_common(ports, sync_now)
        stats['traffic'] = self.get_traffic_stats(skip_zero, pid_input, is_sum = is_sum)
        stats['latency'] = self.get_latency_stats(skip_zero)

        return stats



    # clear stats
    @client_api('getter', True)
    def clear_stats(self,
                    ports = None,
                    clear_global = True,
                    clear_xstats = True,
                    clear_traffic = True,
                    pid_input = DEFAULT_PROFILE_ID):
        """
            Clears statistics in given ports.

            :parameters:
                port: list

                clear_global: boolean

                clear_xstats: boolean

                clear_traffic: boolean

                pid_input: string
                    Input profile ID

        """

        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            if clear_traffic:
               self.clear_traffic_stats(profile_id)

        return self._clear_stats_common(ports, clear_global, clear_xstats)


    @client_api('getter', True)
    def get_tg_names(self, pid_input = DEFAULT_PROFILE_ID):
        """
            Returns a list of the names of all template groups defined in the current profile.

            :parameters:
                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`

        """
        return self.traffic_stats.get_tg_names(pid_input)


    @client_api('getter', True)
    def get_traffic_tg_stats(self, tg_names, skip_zero=True, pid_input = DEFAULT_PROFILE_ID):
        """
            Returns the traffic statistics for the template groups specified in tg_names.

            :parameters:
                tg_names: list or string
                    Contains the names of the template groups for which we want to get traffic statistics.

                skip_zero: boolean

                pid_input: string
                    Input profile ID

            :raises:
                + :exc:`TRexError`
                + :exc:`ASTFErrorBadTG`
                    Can be thrown if tg_names is empty or contains a invalid name.

        """
        validate_type('tg_names', tg_names, (list, basestring))
        return self.traffic_stats.get_traffic_tg_stats(tg_names, skip_zero, pid_input = pid_input)


    @client_api('getter', True)
    def get_traffic_stats(self, skip_zero = True, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        """
            Returns aggregated traffic statistics.

            :parameters:
                skip_zero: boolean

                pid_input: string
                    Input profile ID

                is_sum: boolean
                    Get total counter values

        """
        return self.traffic_stats.get_stats(skip_zero, pid_input = pid_input, is_sum = is_sum)


    @client_api('getter', True)
    def is_traffic_stats_error(self, stats):
        '''
        Return Tuple if there is an error and what is the error (Bool,Errors)

        :parameters:
            stats: dict from get_traffic_stats output 

        '''
        return self.traffic_stats.is_traffic_stats_error(stats)


    @client_api('getter', True)
    def clear_traffic_stats(self, pid_input = DEFAULT_PROFILE_ID):
        """
            Clears traffic statistics.

            :parameters:
                pid_input: string
                    Input profile ID

        """
        return self.traffic_stats.clear_stats(pid_input)


    @client_api('getter', True)
    def get_latency_stats(self,skip_zero =True):
        """
            Gets latency statistics.

            :parameters:
                skip_zero: boolean

        """
        return self.latency_stats.get_stats(skip_zero)


    @client_api('command', True)
    def start_latency(self, mult = 1, src_ipv4="16.0.0.1", dst_ipv4="48.0.0.1", ports_mask=0x7fffffff, dual_ipv4 = "1.0.0.0"):
        '''
           Start ICMP latency traffic.

            :parameters:
                mult: float 
                    number of packets per second

                src_ipv4: IP string
                    IPv4 source address for the port

                dst_ipv4: IP string
                    IPv4 destination address

                ports_mask: uint32_t
                    bitmask of ports

                dual_ipv4: IP string
                    IPv4 address to be added for each pair of ports (starting from second pair)

            .. note::
                VLAN will be taken from interface configuration

            :raises:
                + :exc:`TRexError`

        '''
        if not is_valid_ipv4(src_ipv4):
            raise TRexError("src_ipv4 is not a valid IPv4 address: '{0}'".format(src_ipv4))

        if not is_valid_ipv4(dst_ipv4):
            raise TRexError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))

        if not is_valid_ipv4(dual_ipv4):
            raise TRexError("dual_ipv4 is not a valid IPv4 address: '{0}'".format(dual_ipv4))

        params = {
            'handler': self.handler,
            'mult': mult,
            'src_addr': src_ipv4,
            'dst_addr': dst_ipv4,
            'dual_port_addr': dual_ipv4,
            'mask': ports_mask,
            }

        self.ctx.logger.pre_cmd('Starting latency traffic.')
        rc = self._transmit("start_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def stop_latency(self):
        '''
           Stop latency traffic.
        '''

        params = {
            'handler': self.handler
            }

        self.ctx.logger.pre_cmd('Stopping latency traffic.')
        rc = self._transmit("stop_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def update_latency(self, mult = 1):
        '''
           Update rate of latency traffic.

            :parameters:
                mult: float 
                    number of packets per second

            :raises:
                + :exc:`TRexError`
        '''

        params = {
            'handler': self.handler,
            'mult': mult,
            }

        self.ctx.logger.pre_cmd('Updating latency rate.')
        rc = self._transmit("update_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def topo_load(self, topology, tunables = {}):
        ''' Load network topology

            :parameters:
                topology: string or ASTFTopology
                    | Path to topology filename or topology object
                    | Supported file formats:
                    | * JSON
                    | * YAML
                    | * Python

                tunables: dict
                    forward those key-value pairs to the topology Python file

            :raises:
                + :exc:`TRexError`
        '''
        self.topo_mngr.load(topology, **tunables)
        print('')


    @client_api('command', True)
    def topo_clear(self):
        ''' Clear network topology '''

        self.topo_mngr.clear()


    @client_api('command', True)
    def topo_resolve(self, ports = None):
        ''' Resolve current network topology. On success, upload to server '''

        self.topo_mngr.resolve(ports)


    @client_api('command', False)
    def topo_show(self, ports = None):
        ''' Show current network topology status '''
        self.topo_mngr.show(ports)
        print('')


    @client_api('command', False)
    def topo_save(self, filename):
        '''
            Save current topology to file

            :parameters:
                filename: string
                    | Path to topology filename, supported formats:
                    | * JSON
                    | * YAML
                    | * Python
        '''

        if os.path.exists(filename):
            if os.path.islink(filename) or not os.path.isfile(filename):
                raise TRexError("Given path exists and it's not a file!")
            sys.stdout.write('\nFilename %s already exists, overwrite? (y/N) ' % filename)
            ans = user_input().strip()
            if ans.lower() not in ('y', 'yes'):
                print('Not saving.')
                return

        try:
            if filename.endswith('.json'):
                self.ctx.logger.pre_cmd('Saving topology to JSON: %s' % filename)
                code = self.topo_mngr.to_json(False)
            elif filename.endswith('.yaml'):
                self.ctx.logger.pre_cmd('Saving topology to YAML: %s' % filename)
                code = self.topo_mngr.to_yaml()
            elif filename.endswith('.py'):
                self.ctx.logger.pre_cmd('Saving topology to Python script: %s' % filename)
                code = self.topo_mngr.to_code()
            else:
                self.ctx.logger.error('Saved filename should be .py or .json or .yaml')
                return

            with open(filename, 'w') as f:
                f.write(code)

        except Exception as e:
            self.ctx.logger.post_cmd(False)
            raise TRexError('Saving file failed: %s' % e)

        self.ctx.logger.post_cmd(True)


############################   console   #############################
############################   commands  #############################
############################             #############################

    @console_api('acquire', 'common', True)
    def acquire_line (self, line):
        '''Acquire ports\n'''

        # define a parser
        parser = parsing_opts.gen_parser(
            self,
            'acquire',
            self.acquire_line.__doc__,
            parsing_opts.FORCE)

        opts = parser.parse_args(shlex.split(line))
        self.acquire(force = opts.force)
        return True


    @console_api('reset', 'common', True)
    def reset_line(self, line):
        '''Reset ports'''

        parser = parsing_opts.gen_parser(
            self,
            'reset',
            self.reset_line.__doc__,
            parsing_opts.PORT_RESTART
            )

        opts = parser.parse_args(shlex.split(line))
        self.reset(restart = opts.restart)
        return True


    @console_api('start', 'ASTF', True)
    def start_line(self, line):
        '''Start traffic command'''

        parser = parsing_opts.gen_parser(
            self,
            'start',
            self.start_line.__doc__,
            parsing_opts.FILE_PATH,
            parsing_opts.MULTIPLIER_NUM,
            parsing_opts.DURATION,
            parsing_opts.TUNABLES,
            parsing_opts.ASTF_NC,
            parsing_opts.ASTF_LATENCY,
            parsing_opts.ASTF_IPV6,
            parsing_opts.ASTF_CLIENT_CTRL,
            parsing_opts.ASTF_PROFILE_LIST
            )

        opts = parser.parse_args(shlex.split(line))
       
        valid_pids = self.validate_profile_id_input(opts.profiles, start = True)
        for profile_id in valid_pids:

            self.load_profile(opts.file[0], opts.tunables, pid_input = profile_id)
            kw = {}
            if opts.clients:
                for client in opts.clients:
                    if client not in self.ports:
                        raise TRexError('Invalid client interface: %d' % client)
                    if client & 1:
                        raise TRexError('Following interface is not client: %d' % client)
                kw['client_mask'] = self._calc_port_mask(opts.clients)
            elif opts.servers_only:
                kw['client_mask'] = 0

            self.start(opts.mult, opts.duration, opts.nc, False, opts.latency_pps, opts.ipv6, pid_input = profile_id, **kw)

        return True


    @console_api('stop', 'ASTF', True)
    def stop_line(self, line):
        '''Stop traffic command'''

        parser = parsing_opts.gen_parser(
            self,
            'stop',
            self.stop_line.__doc__,
            parsing_opts.ASTF_PROFILE_DEFAULT_LIST,
            parsing_opts.REMOVE
            )

        opts = parser.parse_args(shlex.split(line))
        self.stop(False, pid_input = opts.profiles, is_remove = opts.remove)
       

    @console_api('update', 'ASTF', True)
    def update_line(self, line):
        '''Update traffic multiplier'''

        parser = parsing_opts.gen_parser(
            self,
            'update',
            self.update_line.__doc__,
            parsing_opts.MULTIPLIER_NUM,
            parsing_opts.ASTF_PROFILE_DEFAULT_LIST
            )

        opts = parser.parse_args(shlex.split(line))
        self.update(opts.mult, pid_input = opts.profiles)


    @staticmethod
    def _calc_port_mask(ports):
        mask =0
        for p in ports:
            mask += (1<<p)
        return mask


    @console_api('latency', 'ASTF', True)
    def latency_line(self, line):
        '''Latency-related commands'''

        parser = parsing_opts.gen_parser(
            self,
            'latency',
            self.latency_line.__doc__)

        def latency_add_parsers(subparsers, cmd, help = '', **k):
            return subparsers.add_parser(cmd, description = help, help = help, **k)

        subparsers = parser.add_subparsers(title = 'commands', dest = 'command', metavar = '')
        start_parser = latency_add_parsers(subparsers, 'start', help = 'Start latency traffic')
        latency_add_parsers(subparsers, 'stop', help = 'Stop latency traffic')
        update_parser = latency_add_parsers(subparsers, 'update', help = 'Update rate of running latency')
        latency_add_parsers(subparsers, 'show', help = 'alias for stats -l')
        latency_add_parsers(subparsers, 'hist', help = 'alias for stats --lh')
        latency_add_parsers(subparsers, 'counters', help = 'alias for stats --lc')

        start_parser.add_arg_list(
            parsing_opts.MULTIPLIER_NUM,
            parsing_opts.SRC_IPV4,
            parsing_opts.DST_IPV4,
            parsing_opts.PORT_LIST,
            parsing_opts.DUAL_IPV4
            )

        update_parser.add_arg_list(
            parsing_opts.MULTIPLIER_NUM,
            )

        opts = parser.parse_args(shlex.split(line))

        if opts.command == 'start':
            ports_mask = self._calc_port_mask(opts.ports)
            self.start_latency(opts.mult, opts.src_ipv4, opts.dst_ipv4, ports_mask, opts.dual_ip)

        elif opts.command == 'stop':
            self.stop_latency()

        elif opts.command == 'update':
            self.update_latency(mult = opts.mult)

        elif opts.command == 'show' or not opts.command:
            self._show_latency_stats()

        elif opts.command == 'hist':
            self._show_latency_histogram()

        elif opts.command == 'counters':
            self._show_latency_counters()

        else:
            raise TRexError('Unhandled command %s' % opts.command)

        return True

    @console_api('topo', 'ASTF', True, True)
    def topo_line(self, line):
        '''Topology-related commands'''

        parser = parsing_opts.gen_parser(
            self,
            'topo',
            self.topo_line.__doc__)

        def topology_add_parsers(subparsers, cmd, help = '', **k):
            return subparsers.add_parser(cmd, description = help, help = help, **k)

        subparsers = parser.add_subparsers(title = 'commands', dest = 'command', metavar = '')
        load_parser = topology_add_parsers(subparsers, 'load', help = 'Load topology from file')
        reso_parser = topology_add_parsers(subparsers, 'resolve', help = 'Resolve loaded topology, push to server on success')
        show_parser = topology_add_parsers(subparsers, 'show', help = 'Show current topology status')
        topology_add_parsers(subparsers, 'clear', help = 'Clear current topology')
        save_parser = topology_add_parsers(subparsers, 'save', help = 'Save topology to file')

        load_parser.add_arg_list(
            parsing_opts.FILE_PATH,
            parsing_opts.TUNABLES,
            )

        reso_parser.add_arg_list(
            parsing_opts.PORT_LIST_NO_DEFAULT,
            )

        show_parser.add_arg_list(
            parsing_opts.PORT_LIST_NO_DEFAULT,
            )

        save_parser.add_arg_list(
            parsing_opts.FILE_PATH_NO_CHECK,
            )

        opts = parser.parse_args(shlex.split(line))

        if opts.command == 'load':
            self.topo_load(opts.file[0], opts.tunables)
            return False

        elif opts.command == 'resolve':
            self.topo_resolve(opts.ports_no_default)

        elif opts.command == 'show' or not opts.command:
            self.topo_show(opts.ports_no_default)
            return False

        elif opts.command == 'clear':
            self.topo_clear()

        elif opts.command == 'save':
            self.topo_save(opts.file[0])

        else:
            raise TRexError('Unhandled command %s' % opts.command)

        return True


    @console_api('stats', 'common', True)
    def show_stats_line (self, line):
        '''Show various statistics\n'''

        # define a parser
        parser = parsing_opts.gen_parser(
            self,
            'stats',
            self.show_stats_line.__doc__,
            parsing_opts.PORT_LIST,
            parsing_opts.ASTF_STATS_GROUP,
            parsing_opts.ASTF_PROFILE_STATS)

        valid_pids = list(self.astf_profile_state.keys())
        opts = parser.parse_args(shlex.split(line))
        if not opts:
            return

        # without parameters show only global and ports
        if not opts.stats:
            self._show_global_stats()
            self._show_port_stats(opts.ports)
            return

        if self.is_dynamic == True and opts.pfname == None:
            is_sum = True
            valid_pids = self.validate_profile_id_input(pid_input = DEFAULT_PROFILE_ID)
        else:
            is_sum = False
            valid_pids = self.validate_profile_id_input(pid_input = opts.pfname)

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

        elif opts.stats == 'astf':
            for profile_id in valid_pids:
                self._show_traffic_stats(False, pid_input = profile_id, is_sum = is_sum)

        elif opts.stats == 'astf_inc_zero':
            for profile_id in valid_pids:
                self._show_traffic_stats(True, pid_input = profile_id, is_sum = is_sum)

        elif opts.stats == 'latency':
            self._show_latency_stats()

        elif opts.stats == 'latency_histogram':
            self._show_latency_histogram()

        elif opts.stats == 'latency_counters':
            self._show_latency_counters()

        else:
            raise TRexError('Unhandled stat: %s' % opts.stats)


    @console_api('template_group', 'ASTF', True)
    def template_group_line(self, line):
        "Template group commands"

        parser = parsing_opts.gen_parser(
            self,
            'template_group',
            self.template_group_line.__doc__
            )

        def template_group_add_parsers(subparsers, cmd, help = '', **k):
            return subparsers.add_parser(cmd, description = help, help = help, **k)

        subparsers = parser.add_subparsers(title = 'commands', dest = 'command', metavar = '')
        names_parser = template_group_add_parsers(subparsers, 'names', help = 'Get template group names')
        stats_parser = template_group_add_parsers(subparsers, 'stats', help = 'Get stats for template group')

        names_parser.add_arg_list(parsing_opts.TG_NAME_START)
        names_parser.add_arg_list(parsing_opts.TG_NAME_AMOUNT)
        names_parser.add_arg_list(parsing_opts.ASTF_PROFILE_LIST)
        stats_parser.add_arg_list(parsing_opts.TG_STATS)
        stats_parser.add_arg_list(parsing_opts.ASTF_PROFILE_LIST)

        opts = parser.parse_args(shlex.split(line))

        if not opts:
            return

        pid_input = opts.profiles
        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            if opts.command == 'names':
                self.traffic_stats._show_tg_names(start=opts.start, amount=opts.amount, pid_input = profile_id)
            elif opts.command == 'stats':
                try:
                    self.get_tg_names(profile_id)
                    tgid = self.traffic_stats._translate_names_to_ids(opts.name, pid_input = profile_id)
                    self._show_traffic_stats(include_zero_lines=False, tgid = tgid[0], pid_input = profile_id)
                except ASTFErrorBadTG:
                    print(format_text("Template group name %s doesn't exist!" % opts.name, 'bold'))
            else:
                raise TRexError('Unhandled command: %s' % opts.command)


    def _get_num_of_tgids(self, pid_input = DEFAULT_PROFILE_ID):
        return self.traffic_stats._get_num_of_tgids(pid_input)


    def _show_traffic_stats(self, include_zero_lines, buffer = sys.stdout, tgid = 0, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        table = self.traffic_stats.to_table(include_zero_lines, tgid, pid_input, is_sum = is_sum)
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_latency_stats(self, buffer = sys.stdout):
        table = self.latency_stats.to_table_main()
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_latency_histogram(self, buffer = sys.stdout):
        table = self.latency_stats.histogram_to_table()
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_latency_counters(self, buffer = sys.stdout):
        table = self.latency_stats.counters_to_table()
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_profiles_states(self, buffer = sys.stdout):

        table = text_tables.TRexTextTable()
        table.set_cols_align(["c"] + ["c"])
        table.set_cols_width([20]  + [20])
        table.header(["ID", "State"])

        profile_list = list(self.astf_profile_state.keys())
        for profile_id in profile_list:
            profile_state = self.astf_profile_state[profile_id]
            state = "Unknown" if profile_state is None else astf_states[profile_state]
            table.add_row([
                profile_id,
                state
                ])

        return table

    @console_api('profiles', 'ASTF', True, True)
    def profiles_line(self, line):
        '''Get loaded to profiles information'''
        parser = parsing_opts.gen_parser(self,
                                         "profiles",
                                         self.profiles_line.__doc__)

        opts = parser.parse_args(line.split())
        if not opts:
            return opts

        table = self._show_profiles_states() 

        if not table:
            self.logger.info(format_text("No profiles found with desired filter.\n", "bold", "magenta"))

        text_tables.print_table_with_header(table, header = 'Profile states')
