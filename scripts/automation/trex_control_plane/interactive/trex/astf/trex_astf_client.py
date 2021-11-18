from __future__ import print_function
from argparse import ArgumentParser
import hashlib
import sys
import time
import os
import shlex

from ..utils.common import get_current_user, ip2int, user_input, PassiveTimer
from ..utils import parsing_opts, text_tables

from ..common.trex_api_annotators import client_api, console_api
from ..common.trex_client import TRexClient, NO_TCP_UDP_MASK
from ..common.trex_events import Event
from ..common.trex_exceptions import TRexError, TRexTimeoutError
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
    'STATE_ASTF_CLEANUP',
    'STATE_ASTF_DELETE']

class Tunnel:
    def __init__(self, sip, dip, sport, teid, version):
        self.sip      = sip
        self.dip      = dip
        self.sport = sport
        self.teid     = teid
        self.version  = version

class ASTFClient(TRexClient):
    port_states = [getattr(ASTFPort, state, 0) for state in astf_states]

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

        api_ver = {'name': 'ASTF', 'major': 2, 'minor': 1}

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
        self.last_profile_error = {}
        self.epoch = None
        self.state = None
        for index, state in enumerate(astf_states):
            setattr(self, state, index)
        self.transient_states = [
            self.STATE_ASTF_PARSE,
            self.STATE_ASTF_BUILD,
            self.STATE_ASTF_CLEANUP,
            self.STATE_ASTF_DELETE]
        self.astf_profile_state = {'_': 0}
        self.profile_state_change = {}

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
        port_map = {}
        for port_info in system_info['ports']:
            port_id = port_info['index']
            port_map[port_id] = ASTFPort(self.ctx, port_id, self.conn.rpc, port_info)
        return self._assign_ports(port_map)

    def _on_connect_clear_stats(self):
        self.traffic_stats.reset()
        self.latency_stats.reset()
        with self.ctx.logger.suppress(verbose = "warning"):
            self.clear_stats(ports = self.get_all_ports(), clear_xstats = False, clear_traffic = False)
        return RC_OK()

    def _on_astf_state_chg(self, ctx_state, error, epoch):
        if ctx_state < 0 or ctx_state >= len(astf_states):
            raise TRexError('Unhandled ASTF state: %s' % ctx_state)

        if epoch is None or self.epoch is None:
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

        if epoch is None or self.epoch is None:
            return

        if error:
            self.last_profile_error[profile_id] = error
            if not self.sync_waiting:
                self.ctx.logger.error('Last profile %s command failed: %s' % (profile_id, error))

        # update profile state
        self._set_profile_state(profile_id, ctx_state)

        if error:
            return Event('server', 'error', 'Moved to profile %s state: %s after error: %s' % (profile_id, ctx_state, error))
        else:
            return Event('server', 'info', 'Moved to profile %s state: %s' % (profile_id, ctx_state))

    def _on_astf_profile_cleared(self, profile_id, error, epoch):
        if epoch is None or self.epoch is None:
            return

        if error:
            self.last_profile_error[profile_id] = error
            if not self.sync_waiting:
                self.ctx.logger.error('Last profile %s command failed: %s' % (profile_id, error))

        # remove profile and statistics references (including template group name)
        self._remove_profile_state(profile_id)
        self.traffic_stats._remove_stats(profile_id)

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

        if ALL_PROFILE_ID in profile_list:
            if start == True:
                raise TRexError("Cannot have %s as a profile value for start command" % ALL_PROFILE_ID)
            else:
                # return profiles can be operational only for the requests.
                # STATE_IDLE is operational for 'profile_clear.'
                return [pid for pid, state in self.astf_profile_state.items()
                                if state is not self.STATE_ASTF_DELETE]

        # Check if profile_id is a valid profile name
        for profile_id in profile_list:
            if profile_id not in list(self.astf_profile_state.keys()):
                if start == True:
                    self.astf_profile_state[profile_id] = self.STATE_IDLE
                else:
                    raise TRexError("ASTF profile_id %s does not exist." % profile_id)

            if start == True:
                if self.is_dynamic and self.astf_profile_state.get(profile_id) not in ok_states:
                    raise TRexError("%s state:Transmitting, should be one of following:Idle, Loaded profile" % profile_id)

            if profile_id not in valid_pids:
                valid_pids.append(profile_id)

        return valid_pids

    def apply_port_states(self):
        port_state = self.port_states[self.state]
        for port in self.ports.values():
            port.state = port_state
        return port_state

    def wait_for_steady(self, profile_id=None):
        while True:
            state = self.__get_profile_state_with_change(profile_id) if profile_id else self.state
            if state not in self.transient_states:
                break
            time.sleep(0.001)
        return state

    def wait_for_transient(self, profile_id=None):
        while True:
            state = self.__get_profile_state_with_change(profile_id) if profile_id else self.state
            if state in self.transient_states:
                break
            time.sleep(0.001)
        return state

    def wait_for_profile_state(self, profile_id, wait_state, timeout = None):
        timer = PassiveTimer(timeout)
        while self.__get_profile_state_with_change(profile_id) != wait_state:
            if timer.has_expired():
                raise TRexTimeoutError(timeout)
            time.sleep(0.001)

    def inc_epoch(self):
        rc = self._transmit('inc_epoch', {'handler': self.handler})
        if not rc:
            raise TRexError(rc.err())
        self.sync()

    def _set_profile_state(self, profile_id, state):
        self.astf_profile_state[profile_id] = state
        self.__update_profile_state_change(profile_id, state)

    def _remove_profile_state(self, profile_id):
        self.astf_profile_state.pop(profile_id, None)
        self.__remove_profile_state_change(profile_id)

    def _get_profile_state(self, profile_id):
        return self.astf_profile_state.get(profile_id, self.STATE_IDLE) if self.is_dynamic else self.state

    def __reset_profile_state_change(self, profile_id, reset_state = None):
        if profile_id in self.profile_state_change:
            state_changes = self.profile_state_change[profile_id]
            if reset_state in state_changes:
                # remove unused stacked changes
                if state_changes[-1] == reset_state:
                    self.profile_state_change[profile_id] = []
                else:
                    while state_changes and state_changes.pop(0) != reset_state:
                        pass
        else:
            self.profile_state_change[profile_id] = []

    def __remove_profile_state_change(self, profile_id):
        self.profile_state_change.pop(profile_id, None)

    def __update_profile_state_change(self, profile_id, state):
        if profile_id not in self.profile_state_change:
            self.profile_state_change[profile_id] = []
        elif state > self.STATE_ASTF_LOADED:
            self.__reset_profile_state_change(profile_id, self.STATE_ASTF_LOADED)
        self.profile_state_change[profile_id].append(state)

    def __get_profile_state_with_change(self, profile_id):
        if profile_id and self.is_dynamic:
            state_changes = self.profile_state_change.get(profile_id)
            if state_changes:
                state = state_changes.pop(0)
            else:
                state = self._get_profile_state(profile_id)
            return state
        else:
            return self.state

    def _transmit_async(self, rpc_func, ok_states, bad_states = None, ready_state = None, **k):
        profile_id = k['params']['profile_id'] 
        ok_states = listify(ok_states)
        if bad_states is not None:
            bad_states = listify(bad_states)

        #self.wait_for_steady()
        #if rpc_func == 'start' and self.state is not self.STATE_TX:
        #    self.inc_epoch()

        self.sync_waiting = True
        try:
            state = self.wait_for_steady(profile_id)
            if ready_state:
                assert ready_state not in self.transient_states
                if state != ready_state:
                    self.wait_for_profile_state(profile_id, ready_state, timeout=60)

            rc = self._transmit(rpc_func, **k)
            if not rc:
                return rc
            state = self.wait_for_transient(profile_id)

            state_changes = [state]
            while True:
                state = self.__get_profile_state_with_change(profile_id)
                if not state_changes or state_changes[-1] != state:
                    state_changes.append(state)

                if state in ok_states:
                    return RC_OK()

                if self.last_profile_error.get(profile_id) or (bad_states and state in bad_states):
                    error = self.last_profile_error.pop(profile_id, None)
                    general_error = 'Unexpected state: {}, profile: {}, state changes: {}'.format(state, profile_id, state_changes)
                    return RC_ERR(error or general_error)

                time.sleep(0.001)
        finally:
            self.sync_waiting = False

    def check_states(self, ok_states):
        while True:
            if self.state in ok_states:
                break
            time.sleep(0.1) # 100ms
    
    def _is_service_req(self):
        ''' Return False as service mode check is not required in ASTF '''
        return False

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
                self.stop(False, pid_input=ALL_PROFILE_ID)
                self.check_states(ok_states=[self.STATE_ASTF_LOADED, self.STATE_IDLE])
                self.stop_latency()
                self.traffic_stats.reset()
                self.latency_stats.reset()
                self.clear_profile(False, pid_input=ALL_PROFILE_ID)
                self.check_states(ok_states=[self.STATE_IDLE])
                self.clear_stats(ports)
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
                  'user':  self.ctx.username,
                  'session_id':  self.ctx.session_id}

        rc = self._transmit('acquire', params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError('Could not acquire context: %s' % rc.err())

        self.handler = rc.data()['handler']
        for port_id, port_rc in rc.data()['ports'].items():
            self.ports[int(port_id)]._set_handler(port_rc)

        self._post_acquire_common(ports)

    @client_api('command', True)
    def sync(self):
        if self.epoch is not None:
            return self.astf_profile_state

        params = {'profile_id': "sync"}
        rc = self._transmit('sync', params)

        if not rc:
            raise TRexError(rc.err())

        self.state = rc.data()['state']
        self.apply_port_states()
        if self.is_dynamic:
            self.astf_profile_state = rc.data()['state_profile']
        else:
            self.astf_profile_state[DEFAULT_PROFILE_ID] = self.state
        self.epoch = rc.data()['epoch']

        return self.astf_profile_state

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
    def set_service_mode (self, ports = None, enabled = True, filtered = False, mask = None):
        ''' based on :meth:`trex.astf.trex_astf_client.ASTFClient.set_service_mode_base` '''
        
        # call the base method
        self.set_service_mode_base(ports = ports, enabled = enabled, filtered = filtered, mask = mask)
        
        # in ASTF send to all ports with the handler of the ctx
        params = {"handler": self.handler,
                    "enabled": enabled,
                    "filtered": filtered}
        if filtered:
            params['mask'] = mask
        
        # transmit server once for all the ports
        rc = self._transmit('service', params)

        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc)
        else:
            # sending all ports in order to change their attributes
            self._for_each_port('set_service_mode', None, enabled, filtered, mask)

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
                self._remove_profile_state(pid_input)
                raise TRexError('Could not load profile: %s' % e)

            #when ".. -t --help", is called then return
            if profile is None:
                return

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
    def clear_profile(self, block = True, pid_input = DEFAULT_PROFILE_ID):
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
            profile_state = self._get_profile_state(profile_id)

            if profile_state in ok_states:
                params = {
                    'handler': self.handler,
                    'profile_id': profile_id
                }
                self.ctx.logger.pre_cmd('Clearing loaded profile.')
                if block:
                    rc = self._transmit_async('profile_clear', params = params, ok_states = self.STATE_IDLE)
                else:
                    rc = self._transmit('profile_clear', params = params)
                self.ctx.logger.post_cmd(rc)
                if not rc:
                    raise TRexError(rc.err())
            else:
                self.logger.info(format_text("Cannot remove a profile: %s is not state IDLE and state LOADED.\n" % profile_id, "bold", "magenta"))

    @client_api('command', True)
    def start(self, mult = 1, duration = -1, nc = False, block = True, latency_pps = 0, ipv6 = False, pid_input = DEFAULT_PROFILE_ID, client_mask = 0xffffffff, e_duration = 0, t_duration = 0):
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

                e_duration: float
                    Maximum time to wait for one flow to be established.
                    Stop the flow generation when this time is over. Stop immediately with nc.
                    Disabled by default. Enabled by positive values.

                t_duration: float
                    Maximum time to wait for all the flow to terminate gracefully after duration.
                    Stop immediately (overrides nc pararmeter) when this time is over.
                    Disabled by default. Enabled by non-zero values.

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
            'e_duration': e_duration,
            't_duration': t_duration,
            }

        self.ctx.logger.pre_cmd('Starting traffic.')

        valid_pids = self.validate_profile_id_input(pid_input, start = True)

        for profile_id in valid_pids:

            if block:
                rc = self._transmit_async('start', params = params, ok_states = self.STATE_TX, bad_states = self.STATE_ASTF_LOADED, ready_state = self.STATE_ASTF_LOADED)
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
            profile_state = self._get_profile_state(profile_id)

            # 'stop' will be silently ignored in server-side PARSE/BUILD state.
            # So, TX state should be forced to avoid unexpected hanging situation.
            if profile_state in {self.STATE_ASTF_PARSE, self.STATE_ASTF_BUILD}:
                self.wait_for_profile_state(profile_id, self.STATE_TX)
                profile_state = self._get_profile_state(profile_id)

            if profile_state is self.STATE_TX:
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

                profile_state = self._get_profile_state(profile_id)

            if is_remove:
                if profile_state is self.STATE_ASTF_CLEANUP:
                    self.wait_for_profile_state(profile_id, self.STATE_ASTF_LOADED)

                self.clear_profile(block = block, pid_input = profile_id)

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
    def wait_on_traffic(self, timeout = None, profile_id = None):
        """
            Block until traffic stops

            :parameters:
                timeout: int
                    Timeout in seconds
                    Default is blocking

                profile_id: string
                    Profile ID

            :raises:
                + :exc:`TRexTimeoutError` - in case timeout has expired
                + :exc:`TRexError`
        """

        if profile_id is None:
            ports = self.get_all_ports()
            TRexClient.wait_on_traffic(self, ports, timeout)
        else:
            self.wait_for_profile_state(profile_id, self.STATE_ASTF_LOADED, timeout)

    # get stats
    @client_api('getter', True)
    def get_stats(self, ports = None, sync_now = True, skip_zero = True, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        """
            Gets all statistics on given ports, traffic and latency.

            :parameters:
                ports: list

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
                ports: list

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
        self.clear_traffic_stats(is_sum = True)

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
    def get_profiles_state(self):
        """
            Gets an dictionary with the states of all the profiles.

            :returns:
                Dictionary containing profiles and their states. Keys are strings, `pid` (profile ID). Each profile can be in one of the following states:
                ['STATE_IDLE', 'STATE_ASTF_LOADED', 'STATE_ASTF_PARSE', 'STATE_ASTF_BUILD', 'STATE_TX', 'STATE_ASTF_CLEANUP', 'STATE_ASTF_DELETE', 'STATE_UNKNOWN'].
        """
        states = {}
        for key, value in self.astf_profile_state.items():
            states[key] = astf_states[value] if value in range(len(astf_states)) else "STATE_UNKNOWN"
        return states

    @client_api('getter', True)
    def is_traffic_stats_error(self, stats):
        '''
        Return Tuple if there is an error and what is the error (Bool,Errors)

        :parameters:
            stats: dict from get_traffic_stats output 

        '''
        return self.traffic_stats.is_traffic_stats_error(stats)

    @client_api('getter', True)
    def clear_traffic_stats(self, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        """
            Clears traffic statistics.

            :parameters:
                pid_input: string
                    Input profile ID

        """
        return self.traffic_stats.clear_stats(pid_input, is_sum)

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

    # private function to form json data for GTP tunnel
    def _update_gtp_tunnel(self, client_list):

        json_attr = []

        for key, value in client_list.items():
            json_attr.append({'client_ip' : key, 'sip': value.sip, 'dip' : value.dip, 'sport' : value.sport, 'teid' : value.teid, "version" :value.version})
 
        return json_attr

    # execute 'method' for inserting/updateing tunnel info for clients
    @client_api('command', True)
    def update_tunnel_client_record (self, client_list, tunnel_type):

        json_attr = []
        
        if tunnel_type == parsing_opts.TunnelType.GTPU:
           json_attr = self._update_gtp_tunnel(client_list)
        else:
           raise TRexError('Invalid Tunnel Type: %d' % tunnel_type)
        
        params = {"tunnel_type": tunnel_type,
                  "attr": json_attr }

        self.ctx.logger.pre_cmd("update_tunnel_client.\n")
        rc = self._transmit("update_tunnel_client", params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())

    # check if the tunnel-type is supported
    @client_api('command', True)
    def is_tunnel_supported(self, tunnel_type=1):
        '''
        parameters:
            tunnel_type: currently only type 1 (gtpu) is supported
        ret: 
            dict {is_tunnel_supported: true/false,
                  error_msg: why the tunnel is not supported}
        '''
        params = {'tunnel_type': tunnel_type}
        rc = self._transmit("is_tunnel_supported", params = params)
        if not rc:
            raise TRexError(rc.err())
        return rc.data()

    #activate/deactivate tunnel mode
    @client_api('command', True)
    def activate_tunnel(self, tunnel_type=1 ,activate=True, loopback=False):
        if activate == True:
            tunnel_supported_info = self.is_tunnel_supported(tunnel_type)
            if not tunnel_supported_info['is_tunnel_supported']:
                raise TRexError(tunnel_supported_info['error_msg'])

        params = {
            'tunnel_type': tunnel_type,
            'activate': activate,
            'loopback': loopback
            }

        if tunnel_type != parsing_opts.TunnelType.GTPU:
           raise TRexError('Invalid Tunnel Type: %d' % tunnel_type)

        prefix = "Activating"
        if not activate:
            prefix = "Deactivating"
        pre_cmd = prefix + ' tunnel mode'
        if loopback:
            pre_cmd += ' with loopback'

        self.ctx.logger.pre_cmd(pre_cmd + '.\n')
        rc = self._transmit("activate_tunnel_mode", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())
        return True

    # execute 'method' for Making  a client active/inactive
    def set_client_enable(self, client_list, is_enable):
        '''
        Version: 1 
        API to toggle state of client
        Input: List of clients and Action : state flag
        '''

        json_attr = []
        for key in client_list:
           json_attr.append({'client_ip' : key})
 
        params = {"is_enable": is_enable,
                  "is_range": False,
                  "attr": json_attr }

        return self._transmit("enable_disable_client", params)


    # execute 'method' for Making  a client active/inactive
    def set_client_enable_range(self, client_start, client_end, is_enable):
        ''' 
        Version: 2
        API to toggle state of client
        Input: Client range and Action : state flag
        '''

        json_attr = []
        json_attr.append({'client_start_ip' : client_start, 'client_end_ip' : client_end})
 
        params = {"is_enable": is_enable,
                  "is_range": True,
                  "attr": json_attr }

        return self._transmit("enable_disable_client", params)


     # execute 'method' for getting clients stats
    def get_clients_info (self, client_list):
        '''
        Version 1 
        API to get client information: Currently only state and if client is present. 
        Input: List of clients  
        '''
         
        json_attr = []
        for key in client_list:
           json_attr.append({'client_ip' : key})

        params = {"is_range": False,
                  "attr": json_attr }

        return self._transmit("get_clients_info", params)


     # execute 'method' for getting clients stats
    def get_clients_info_range (self, client_start, client_end):
        '''
        Version 2 
        API to get client information: Currently only state and if client is present. 
        Input: Client range 
        '''
         
        json_attr = []
        json_attr.append({'client_start_ip' : client_start, 'client_end_ip' : client_end})
 

        params = {"is_range": True,
                  "attr": json_attr }

        return self._transmit("get_clients_info", params)



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

        # parser for parsing the start command arguments
        parser = parsing_opts.gen_parser(self,
                                         'start',
                                         self.start_line.__doc__,
                                         parsing_opts.FILE_PATH,
                                         parsing_opts.MULTIPLIER_NUM,
                                         parsing_opts.DURATION,
                                         parsing_opts.ESTABLISH_DURATION,
                                         parsing_opts.TERMINATE_DURATION,
                                         parsing_opts.ARGPARSE_TUNABLES,
                                         parsing_opts.ASTF_NC,
                                         parsing_opts.ASTF_LATENCY,
                                         parsing_opts.ASTF_IPV6,
                                         parsing_opts.ASTF_CLIENT_CTRL,
                                         parsing_opts.ASTF_PROFILE_LIST
                                         )

        opts = parser.parse_args(shlex.split(line))
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

        valid_pids = self.validate_profile_id_input(opts.profiles, start = True)
        for profile_id in valid_pids:

            self.load_profile(opts.file[0], tunable_dict, pid_input = profile_id)

            #when ".. -t --help", is called the help message is being printed once and then it returns to the console
            if any(h in opts.tunables for h in help_flags):
                break

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

            self.start(opts.mult, opts.duration, opts.nc, False, opts.latency_pps, opts.ipv6, pid_input = profile_id, e_duration = opts.e_duration, t_duration = opts.t_duration, **kw)

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

    @console_api('service', 'ASTF', True)
    def service_line (self, line):
        '''Configures port for service mode.
           In service mode ports will reply to ARP, PING
           and etc.
           In ASTF, command will apply on all ports.
        '''

        parser = parsing_opts.gen_parser(self,
                                         "service",
                                         self.service_line.__doc__,
                                         parsing_opts.SERVICE_GROUP)

        opts = parser.parse_args(line.split())
        enabled, filtered, mask = self._get_service_params(opts)

        if mask is not None and ((mask & NO_TCP_UDP_MASK) == 0):
            raise TRexError('Cannot set NO_TCP_UDP off in ASTF!')

        self.set_service_mode(enabled = enabled, filtered = filtered, mask = mask)
        
        return True

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

    @console_api('clear', 'common', False)
    def clear_stats_line (self, line):
        '''Clear cached local statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "clear",
                                         self.clear_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)

        opts = parser.parse_args(line.split())
        self.clear_stats(opts.ports, pid_input = ALL_PROFILE_ID)

        return RC_OK()

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

        astf_profiles_state = self.get_profiles_state()
        valid_pids = list(astf_profiles_state.keys())
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

        if not opts or not opts.command:
            parser.print_help()
            return

        pid_input = opts.profiles
        valid_pids = self.validate_profile_id_input(pid_input)

        for profile_id in valid_pids:
            if opts.command == 'names':
                print(format_text("Profile ID: %s" % profile_id, 'bold'))
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

        profiles_state = sorted(self.get_profiles_state().items())
        for profile_id, state in profiles_state:
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


    @console_api('update_tunnel', 'ASTF', True)
    def update_clients_tunnel(self, line):
        '''update the clients tunnel context'''
        parser = parsing_opts.gen_parser(
            self,
            'update_clients_tunnel',
            self.update_clients_tunnel.__doc__,
            parsing_opts.CLIENT_START,
            parsing_opts.CLIENT_END,
            parsing_opts.TEID,
            parsing_opts.VERSION,
            parsing_opts.SPORT,
            parsing_opts.SRC_IP,
            parsing_opts.DST_IP,
            parsing_opts.TUNNEL_TYPE
            )
        opts = parser.parse_args(shlex.split(line))
        if opts.ipv6 and (not is_valid_ipv6(opts.src_ip) or not is_valid_ipv6(opts.dst_ip)):
            raise TRexError("invalid IPv6 address: '{0}', {1}".format(opts.src_ip, opts.dst_ip))

        if not opts.ipv6 and (not is_valid_ipv4(opts.src_ip) or not is_valid_ipv4(opts.dst_ip)):
            raise TRexError("invalid IPv4 address: '{0}', {1}".format(opts.src_ip, opts.dst_ip))

        c_start = ip2int(opts.c_start)
        c_end = ip2int(opts.c_end)
        if (c_end < c_start):
            raise TRexError("invalid Client range: client_start: '{0}', client_end: {1}".format(opts.c_start, opts.c_end))
        print("src port is:{0}".format(opts.sport))
        version = 4
        if opts.ipv6:
            version = 6

        client_db = dict()
        count = 0
        for ip_num in range(c_start, c_end + 1):
            client_db[ip_num] = Tunnel(opts.src_ip, opts.dst_ip, opts.sport, opts.teid + count, version)
            count+=1

        self.update_tunnel_client_record(client_db, opts.type)



    @console_api('tunnel', 'ASTF', True)
    def activate_tunnel_line(self, line):
        '''activate/deactivate tunnel mode command'''

        parser = parsing_opts.gen_parser(
            self,
            'tunnel',
            self.activate_tunnel_line.__doc__,
            parsing_opts.TUNNEL_TYPE,
            parsing_opts.TUNNEL_OFF,
            parsing_opts.LOOPBACK
            )
        opts = parser.parse_args(shlex.split(line))
        self.activate_tunnel(opts.type, not opts.off, opts.loopback)