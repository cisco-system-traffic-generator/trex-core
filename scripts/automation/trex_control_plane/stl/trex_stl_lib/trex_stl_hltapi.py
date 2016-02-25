#!/router/bin/python

'''
Supported functions/arguments/defaults:
'''

connect_kwargs = {
    'device': 'localhost',                  # ip or hostname of TRex
    'port_list': None,                      # list of ports
    'username': 'TRexUser',
    'reset': True,
    'break_locks': False,
}

cleanup_session_kwargs = {
    'maintain_lock': False,                 # release ports at the end or not
    'port_list': None,
    'port_handle': None,
}

traffic_config_kwargs = {
    'mode': None,                           # ( create | modify | remove | reset )
    'split_by_cores': 'split',              # ( split | duplicate | single ) TRex extention: split = split traffic by cores, duplicate = duplicate traffic for all cores, single = run only with sinle core (not implemented yet)
    'consistent_random': False,             # TRex extention: False (default): random sequence will be different every run, True: random sequence will be same every run
    'port_handle': None,
    'port_handle2': None,
    # stream builder parameters
    'transmit_mode': 'continuous',          # ( continuous | multi_burst | single_burst )
    'rate_pps': None,
    'rate_bps': None,
    'rate_percent': 10,
    'stream_id': None,
    'name': None,
    'bidirectional': 0,
    'direction': 0,                         # ( 0 | 1 ) TRex extention: 1 = exchange sources and destinations
    'pkts_per_burst': 1,
    'burst_loop_count': 1,
    'inter_burst_gap': 12,
    'length_mode': 'fixed',                 # ( auto | fixed | increment | decrement | random | imix )
    'l3_imix1_size': 60,
    'l3_imix1_ratio': 28,
    'l3_imix2_size': 590,
    'l3_imix2_ratio': 20,
    'l3_imix3_size': 1514,
    'l3_imix3_ratio': 4,
    'l3_imix4_size': 9226,
    'l3_imix4_ratio': 0,
    #L2
    'frame_size': 64,
    'frame_size_min': 64,
    'frame_size_max': 64,
    'frame_size_step': 1,
    'l2_encap': 'ethernet_ii',              # ( ethernet_ii | ethernet_ii_vlan )
    'mac_src': '00:00:01:00:00:01',
    'mac_dst': '00:00:00:00:00:00',
    'mac_src2': '00:00:01:00:00:01',
    'mac_dst2': '00:00:00:00:00:00',
    'mac_src_mode': 'fixed',                # ( fixed | increment | decrement | random )
    'mac_src_step': 1,
    'mac_src_count': 1,
    'mac_dst_mode': 'fixed',                # ( fixed | increment | decrement | random )
    'mac_dst_step': 1,
    'mac_dst_count': 1,
    'mac_src2_mode': 'fixed',                # ( fixed | increment | decrement | random )
    'mac_src2_step': 1,
    'mac_src2_count': 1,
    'mac_dst2_mode': 'fixed',                # ( fixed | increment | decrement | random )
    'mac_dst2_step': 1,
    'mac_dst2_count': 1,
    # vlan options below can have multiple values for nested Dot1Q headers
    'vlan_user_priority': 1,
    'vlan_priority_mode': 'fixed',          # ( fixed | increment | decrement | random )
    'vlan_priority_count': 1,
    'vlan_priority_step': 1,
    'vlan_id': 0,
    'vlan_id_mode': 'fixed',                # ( fixed | increment | decrement | random )
    'vlan_id_count': 1,
    'vlan_id_step': 1,
    'vlan_cfi': 1,
    'vlan_protocol_tag_id': None,
    #L3, general
    'l3_protocol': 'ipv4',                  # ( ipv4 | ipv6 )
    'l3_length_min': 110,
    'l3_length_max': 238,
    'l3_length_step': 1,
    #L3, IPv4
    'ip_precedence': 0,
    'ip_tos_field': 0,
    'ip_mbz': 0,
    'ip_delay': 0,
    'ip_throughput': 0,
    'ip_reliability': 0,
    'ip_cost': 0,
    'ip_reserved': 0,
    'ip_dscp': 0,
    'ip_cu': 0,
    'l3_length': None,
    'ip_id': 0,
    'ip_fragment_offset': 0,
    'ip_ttl': 64,
    'ip_checksum': None,
    'ip_src_addr': '0.0.0.0',
    'ip_dst_addr': '192.0.0.1',
    'ip_src_mode': 'fixed',                 # ( fixed | increment | decrement | random )
    'ip_src_step': 1,                       # ip or number
    'ip_src_count': 1,
    'ip_dst_mode': 'fixed',                 # ( fixed | increment | decrement | random )
    'ip_dst_step': 1,                       # ip or number
    'ip_dst_count': 1,
    #L3, IPv6
    'ipv6_traffic_class': 0,
    'ipv6_flow_label': 0,
    'ipv6_length': None,
    'ipv6_next_header': None,
    'ipv6_hop_limit': 64,
    'ipv6_src_addr': 'fe80:0:0:0:0:0:0:12',
    'ipv6_dst_addr': 'fe80:0:0:0:0:0:0:22',
    'ipv6_src_mode': 'fixed',               # ( fixed | increment | decrement | random )
    'ipv6_src_step': 1,                     # we are changing only 32 lowest bits; can be ipv6 or number
    'ipv6_src_count': 1,
    'ipv6_dst_mode': 'fixed',               # ( fixed | increment | decrement | random )
    'ipv6_dst_step': 1,                     # we are changing only 32 lowest bits; can be ipv6 or number
    'ipv6_dst_count': 1,
    #L4, TCP
    'l4_protocol': 'tcp',                   # ( tcp | udp )
    'tcp_src_port': 1024,
    'tcp_dst_port': 80,
    'tcp_seq_num': 1,
    'tcp_ack_num': 1,
    'tcp_data_offset': 5,
    'tcp_fin_flag': 0,
    'tcp_syn_flag': 0,
    'tcp_rst_flag': 0,
    'tcp_psh_flag': 0,
    'tcp_ack_flag': 0,
    'tcp_urg_flag': 0,
    'tcp_window': 4069,
    'tcp_checksum': None,
    'tcp_urgent_ptr': 0,
    'tcp_src_port_mode': 'increment',       # ( increment | decrement | random )
    'tcp_src_port_step': 1,
    'tcp_src_port_count': 1,
    'tcp_dst_port_mode': 'increment',       # ( increment | decrement | random )
    'tcp_dst_port_step': 1,
    'tcp_dst_port_count': 1,
    # L4, UDP
    'udp_src_port': 1024,
    'udp_dst_port': 80,
    'udp_length': None,
    'udp_dst_port_mode': 'increment',       # ( increment | decrement | random )
    'udp_src_port_step': 1,
    'udp_src_port_count': 1,
    'udp_src_port_mode': 'increment',       # ( increment | decrement | random )
    'udp_dst_port_step': 1,
    'udp_dst_port_count': 1,
}

traffic_control_kwargs = {
    'action': None,                         # ( run | stop )
    'port_handle': None
}

traffic_stats_kwargs = {
    'mode': 'aggregate',                    # ( aggregate )
    'port_handle': None
}


import sys
import os
import socket
import copy
from trex_stl_lib.api import *
from utils.common import get_number
from collections import defaultdict


class HLT_ERR(dict):
    def __init__(self, log = 'Unknown error', **kwargs):
        dict.__init__(self, {'status': 0})
        if type(log) is dict:
            dict.update(self, log)
        elif type(log) is str and not log.startswith('[ERR]'):
            self['log'] = '[ERR] ' + log
        else:
            self['log'] = log
        dict.update(self, kwargs)

class HLT_OK(dict):
    def __init__(self, init_dict = {}, **kwargs):
        dict.__init__(self, {'status': 1, 'log': None})
        dict.update(self, init_dict)
        dict.update(self, kwargs)

def merge_kwargs(default_kwargs, user_kwargs):
    kwargs = copy.deepcopy(default_kwargs)
    for key, value in user_kwargs.items():
        if key in kwargs:
            kwargs[key] = value
        elif key in ('save_to_yaml', 'save_to_pcap'): # internal debug arguments
            kwargs[key] = value
        else:
            print("Warning: provided parameter '%s' is not supported" % key)
    return kwargs

# change MACs from formats 01-23-45-67-89-10 or 0123.4567.8910 or {01 23 45 67 89 10} to Scapy format 01:23:45:67:89:10
def correct_macs(kwargs):
    list_of_mac_args = ['mac_src', 'mac_dst', 'mac_src2', 'mac_dst2']
    list_of_mac_steps = ['mac_src_step', 'mac_dst_step', 'mac_src2_step', 'mac_dst2_step']
    for mac_arg in list_of_mac_args + list_of_mac_steps:
        if mac_arg in kwargs:
            mac_value = kwargs[mac_arg]
            if type(mac_value) in (int, long) and mac_arg in list_of_mac_steps: # step can be number
                continue
            if type(mac_value) is not str: raise STLError('Argument %s should be str' % mac_arg)
            mac_value = mac_value.replace('{', '').replace('}', '').strip().replace('-', ' ').replace(':', ' ').replace('.', ' ')
            if mac_value[4] == ' ' and mac_value[9] == ' ':
                mac_value = ' '.join([mac_value[0:2], mac_value[2:7], mac_value[7:12], mac_value[12:14]])
            mac_value = ':'.join(mac_value.split())
            try:
                mac2str(mac_value)                                # verify we are ok
                kwargs[mac_arg] = mac_value
            except:
                raise STLError('Incorrect MAC %s=%s, please use 01:23:45:67:89:10 or 01-23-45-67-89-10 or 0123.4567.8910 or {01 23 45 67 89 10}' % (mac_arg, kwargs[mac_arg]))

def is_true(input):
    if input in (True, 'True', 'true', 1, '1', 'enable', 'Enable', 'Yes', 'yes', 'y', 'Y', 'enabled', 'Enabled'):
        return True
    return False

# dict of streams per port
# hlt_history = False: holds list of stream_id per port
# hlt_history = True:  act as dictionary (per port) stream_id -> hlt arguments used for build
class CStreamsPerPort(defaultdict):
    def __init__(self, hlt_history = False):
        self.hlt_history = hlt_history
        if self.hlt_history:
            defaultdict.__init__(self, dict)
        else:
            defaultdict.__init__(self, list)

    def get_stream_list(self, ports_list = None):
        if self.hlt_history:
            if ports_list is None:
                ports_list = self.keys()
            elif not isinstance(ports_list, list):
                ports_list = [ports_list]
            ret = {}
            for port in ports_list:
                ret[port] = self[port].keys()
            return ret
        else:
            return self

    # add to stream_id list per port, no HLT args, res = HLT result
    def add_streams_from_res(self, res):
        if self.hlt_history: raise STLError('CStreamsPerPort: this object is not meant for HLT history, try init with hlt_history = False')
        if not isinstance(res, dict): raise STLError('CStreamsPerPort: res should be dict')
        if res.get('status') != 1: raise STLError('CStreamsPerPort: res has status %s' % res.get('status'))
        res_streams = res.get('stream_id')
        if not isinstance(res_streams, dict):
            raise STLError('CStreamsPerPort: stream_id in res should be dict')
        for port, port_stream_ids in res_streams.items():
            if type(port_stream_ids) is not list:
                port_stream_ids = [port_stream_ids]
            self[port].extend(port_stream_ids)

    # save HLT args to modify streams later
    def save_stream_args(self, ports_list, stream_id, stream_hlt_args):
        if not self.hlt_history: raise STLError('CStreamsPerPort: this object works only with HLT history, try init with hlt_history = True')
        if type(stream_id) not in (int, long): raise STLError('CStreamsPerPort: stream_id should be number')
        if not isinstance(stream_hlt_args, dict): raise STLError('CStreamsPerPort: stream_hlt_args should be dict')
        if not isinstance(ports_list, list):
            ports_list = [ports_list]
        for port in ports_list:
            if stream_id in self[port]:
                self[port][stream_id].update(stream_hlt_args)
            else:
                self[port][stream_id] = stream_hlt_args

    def remove_stream(self, ports_list, stream_id):
        if not isinstance(ports_list, list):
            ports_list = [ports_list]
        if not isinstance(stream_id, dict):
            raise STLError('CStreamsPerPort: stream_hlt_args should be dict')
        for port in ports_list:
            if port not in self:
                raise STLError('CStreamsPerPort: port %s not defined' % port)
            if stream_id not in self[port]:
                raise STLError('CStreamsPerPort: stream_id %s not found at port %s' % (port, stream_id))
            if self.hlt_history:
                del self[port][stream_id]
            else:
                self[port].pop(stream_id)

class CTRexHltApi(object):

    def __init__(self, verbose = 0):
        self.trex_client = None
        self.verbose = verbose
        self._streams_history = {} # streams per stream_id per port in format of HLT arguments for modify later


###########################
#    Session functions    #
###########################

    def connect(self, **user_kwargs):
        kwargs = merge_kwargs(connect_kwargs, user_kwargs)
        device = kwargs['device']
        try:
            device = socket.gethostbyname(device) # work with ip
        except: # give it another try
            try:
                device = socket.gethostbyname(device)
            except Exception as e:
                return HLT_ERR('Could not translate hostname "%s" to IP: %s' % (device, e))

        try:
            # sync = RPC, async = ZMQ
            self.trex_client = STLClient(kwargs['username'], device, sync_port = 4501, async_port = 4500, verbose_level = self.verbose)
        except Exception as e:
            return HLT_ERR('Could not init stateless client %s: %s' % (device, e if isinstance(e, STLError) else traceback.format_exc()))

        try:
            self.trex_client.connect()
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not connect to device %s: %s' % (device, e if isinstance(e, STLError) else traceback.format_exc()))

        # connection successfully created with server, try acquiring ports of TRex
        try:
            port_list = self._parse_port_list(kwargs['port_list'])
            self.trex_client.acquire(ports = port_list, force = kwargs['break_locks'])
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not acquire ports %s: %s' % (port_list, e if isinstance(e, STLError) else traceback.format_exc()))

        # since only supporting single TRex at the moment, 1:1 map
        port_handle = self.trex_client.get_acquired_ports()

        # arrived here, all desired ports were successfully acquired
        if kwargs['reset']:
            # remove all port traffic configuration from TRex
            try:
                self.trex_client.stop(ports = port_list)
                self.trex_client.reset(ports = port_list)
            except Exception as e:
                self.trex_client = None
                return HLT_ERR('Error in reset traffic: %s' % e if isinstance(e, STLError) else traceback.format_exc())

        self._streams_history = CStreamsPerPort(hlt_history = True)
        return HLT_OK(port_handle = port_handle)

    def cleanup_session(self, **user_kwargs):
        kwargs = merge_kwargs(cleanup_session_kwargs, user_kwargs)
        if not kwargs['maintain_lock']:
            # release taken ports
            port_list = kwargs['port_list'] or kwargs['port_handle'] or 'all'
            try:
                if port_list == 'all':
                    port_list = self.trex_client.get_acquired_ports()
                else:
                    port_list = self._parse_port_list(port_list)
            except Exception as e:
                return HLT_ERR('Unable to determine which ports to release: %s' % e if isinstance(e, STLError) else traceback.format_exc())
            try:
                self.trex_client.release(port_list)
            except Exception as e:
                return HLT_ERR('Unable to release ports %s: %s' % (port_list, e))
        try:
            self.trex_client.disconnect(stop_traffic = False, release_ports = False)
        except Exception as e:
            return HLT_ERR('Error disconnecting: %s' % e)
        self.trex_client = None
        return HLT_OK()

    def interface_config(self, port_handle, mode='config'):
        if not self.trex_client:
            return HLT_ERR('Connect first')
        ALLOWED_MODES = ['config', 'modify', 'destroy']
        if mode not in ALLOWED_MODES:
            return HLT_ERR('Mode must be one of the following values: %s' % ALLOWED_MODES)
        # pass this function for now...
        return HLT_ERR('interface_config not implemented yet')


###########################
#    Traffic functions    #
###########################

    def traffic_config(self, **user_kwargs):
        if not self.trex_client:
            return HLT_ERR('Connect first')
        try:
            correct_macs(user_kwargs)
        except Exception as e:
            return HLT_ERR(e if isinstance(e, STLError) else traceback.format_exc())
        kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
        stream_id = kwargs['stream_id']
        mode = kwargs['mode']
        if type(stream_id) is list:
            if len(stream_id) > 1:
                streams_per_port = CStreamsPerPort()
                for each_stream_id in stream_id:
                    user_kwargs[stream_id] = each_stream_id
                    res = self.traffic_config(**user_kwargs)
                    if type(res) is HLT_ERR:
                        return res
                    streams_per_port.add_streams_from_res(res)
                if mode == 'create':
                    return HLT_OK(stream_id = streams_per_port)
                else:
                    return HLT_OK()
            else:
                stream_id = stream_id[0]

        port_handle = port_list = self._parse_port_list(kwargs['port_handle'])
        ALLOWED_MODES = ['create', 'modify', 'remove', 'enable', 'disable', 'reset']
        if mode not in ALLOWED_MODES:
            return HLT_ERR('Mode must be one of the following values: %s' % ALLOWED_MODES)

        if mode == 'reset':
            try:
                self.trex_client.remove_all_streams(port_handle)
                for port in port_handle:
                    if port in self._streams_history:
                        del self._streams_history[port]
                return HLT_OK()
            except Exception as e:
                return HLT_ERR('Could not reset streams at ports %s: %s' % (port_handle, e if isinstance(e, STLError) else traceback.format_exc()))

        if mode == 'remove':
            if stream_id is None:
                return HLT_ERR('Please specify stream_id to remove.')
            if type(stream_id) is str and stream_id == 'all':
                try:
                    self.trex_client.remove_all_streams(port_handle)
                    for port in port_handle:
                        if port in self._streams_history:
                            del self._streams_history[port]
                except Exception as e:
                    return HLT_ERR('Could not remove all streams at ports %s: %s' % (port_handle, e if isinstance(e, STLError) else traceback.format_exc()))
            else:
                try:
                    self._remove_stream(stream_id, port_handle)
                except Exception as e:
                    return HLT_ERR('Could not remove streams with specified by %s, error: %s' % (stream_id, e if isinstance(e, STLError) else traceback.format_exc()))
            return HLT_OK()

        #if mode == 'enable':
        #    stream_id = kwargs.get('stream_id')
        #    if stream_id is None:
        #        return HLT_ERR('Please specify stream_id to enable.')
        #    if stream_id not in self._streams_history:
        #        return HLT_ERR('This stream_id (%s) was not used before, please create new.' % stream_id)
        #    self._streams_history[stream_id].update(kwargs) # <- the modification

        if mode == 'modify': # we remove stream and create new one with same stream_id
            stream_id = kwargs.get('stream_id')
            if stream_id is None:
                return HLT_ERR('Please specify stream_id to modify.')

            if len(port_handle) > 1:
                for port in port_handle:
                    user_kwargs[port_handle] = port
                    res = self.traffic_config(**user_kwargs) # recurse per port, each port can have different stream with such id
            else:
                if type(port_handle) is list:
                    port = port_handle[0]
                else:
                    port = port_handle
                if port not in self._streams_history:
                    return HLT_ERR('Port %s was not used/acquired' % port)
                if stream_id not in self._streams_history[port]:
                    return HLT_ERR('This stream_id (%s) was not used before at port %s, please create new.' % (stream_id, port))
                kwargs.update(self._streams_history[port][stream_id])
                kwargs.update(user_kwargs)
            try:
                self.trex_client.remove_streams(stream_id, port_handle)
            except Exception as e:
                return HLT_ERR('Could not remove stream(s) %s from port(s) %s: %s' % (stream_id, port_handle, e if isinstance(e, STLError) else traceback.format_exc()))

        if mode == 'create' or mode == 'modify':
            # create a new stream with desired attributes, starting by creating packet
            streams_per_port = CStreamsPerPort()
            if is_true(kwargs['bidirectional']): # two streams with opposite directions
                del user_kwargs['bidirectional']
                save_to_yaml = user_kwargs.get('save_to_yaml')
                bidirect_err = 'When using bidirectional flag, '
                if len(port_handle) != 1:
                    return HLT_ERR(bidirect_err + 'port_handle1 should be single port handle.')
                port_handle2 = kwargs['port_handle2']
                if (type(port_handle2) is list and len(port_handle2) > 1) or port_handle2 is None:
                    return HLT_ERR(bidirect_err + 'port_handle2 should be single port handle.')
                try:
                    if save_to_yaml and type(save_to_yaml) is str:
                        user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_bi1.yaml')
                    user_kwargs['port_handle'] = port_handle[0]
                    res1 = self.traffic_config(**user_kwargs)
                    if res1['status'] == 0:
                        raise STLError('Could not create bidirectional stream 1: %s' % res1['log'])
                    streams_per_port.add_streams_from_res(res1)
                    kwargs['direction'] = 1 - kwargs['direction'] # not
                    correct_direction(user_kwargs, kwargs)
                    if save_to_yaml and type(save_to_yaml) is str:
                        user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_bi2.yaml')
                    user_kwargs['port_handle'] = port_handle2
                    res2 = self.traffic_config(**user_kwargs)
                    if res2['status'] == 0:
                        raise STLError('Could not create bidirectional stream 2: %s' % res2['log'])
                    streams_per_port.add_streams_from_res(res2)
                except Exception as e:
                    return HLT_ERR('Could not generate bidirectional traffic: %s' % e if isinstance(e, STLError) else traceback.format_exc())
                if mode == 'create':
                    return HLT_OK(stream_id = streams_per_port)
                else:
                    return HLT_OK()

            try:
                stream_obj = STLHltStream(**user_kwargs)
            except Exception as e:
                return HLT_ERR('Could not create stream: %s' % e if isinstance(e, STLError) else traceback.format_exc())

            # try adding the stream per ports
            try:
                stream_id_arr = self.trex_client.add_streams(streams=stream_obj,
                                                             ports=port_handle)
                for port in port_handle:
                    self._streams_history.save_stream_args(port_handle, stream_id_arr[0], user_kwargs)
            except Exception as e:
                return HLT_ERR('Could not add stream to ports: %s' % e if isinstance(e, STLError) else traceback.format_exc())
            if mode == 'create':
                return HLT_OK(stream_id = dict((port, stream_id_arr[0]) for port in port_handle))
            else:
                return HLT_OK()

        return HLT_ERR('Got to the end of traffic_config, mode not implemented or forgot "return" somewhere.')

    def traffic_control(self, **user_kwargs):
        if not self.trex_client:
            return HLT_ERR('Connect first')
        kwargs = merge_kwargs(traffic_control_kwargs, user_kwargs)
        action = kwargs['action']
        port_handle = kwargs['port_handle']
        ALLOWED_ACTIONS = ['clear_stats', 'run', 'stop', 'sync_run']
        if action not in ALLOWED_ACTIONS:
            return HLT_ERR('Action must be one of the following values: {actions}'.format(actions=ALLOWED_ACTIONS))
        if type(port_handle) is not list:
            port_handle = [port_handle]

        if action == 'run':
            try:
                self.trex_client.start(ports = port_handle)
            except Exception as e:
                return HLT_ERR('Could not start traffic: %s' % e if isinstance(e, STLError) else traceback.format_exc())
            return HLT_OK(stopped = 0)

        elif action == 'stop':
            try:
                self.trex_client.stop(ports = port_handle)
            except Exception as e:
                return HLT_ERR('Could not start traffic: %s' % e if isinstance(e, STLError) else traceback.format_exc())
            return HLT_OK(stopped = 1)
        else:
            return HLT_ERR("Action '{0}' is not supported yet on TRex".format(action))

        # if we arrived here, this means that operation FAILED!
        return HLT_ERR("Probably action '%s' is not implemented" % action)

    def traffic_stats(self, **user_kwargs):
        if not self.trex_client:
            return HLT_ERR('Connect first')
        kwargs = merge_kwargs(traffic_stats_kwargs, user_kwargs)
        mode = kwargs['mode']
        port_handle = kwargs['port_handle']
        ALLOWED_MODES = ['aggregate', 'streams', 'all']
        if mode not in ALLOWED_MODES:
            return HLT_ERR("'mode' must be one of the following values: %s" % ALLOWED_MODES)
        if mode == 'streams':
            return HLT_ERR("mode 'streams' not implemented'")
        if mode in ('all', 'aggregate'):
            hlt_stats_dict = {}
            try:
                stats = self.trex_client.get_stats(port_handle)
            except Exception as e:
                return HLT_ERR('Could not retrieve stats: %s' % e if isinstance(e, STLError) else traceback.format_exc())
            for port_id, stat_dict in stats.iteritems():
                if type(port_id) in (int, long):
                    hlt_stats_dict[port_id] = {
                        'aggregate': {
                            'tx': {
                                'pkt_bit_rate': stat_dict.get('tx_bps'),
                                'pkt_byte_count': stat_dict.get('obytes'),
                                'pkt_count': stat_dict.get('opackets'),
                                'pkt_rate': stat_dict.get('tx_pps'),
                                'total_pkt_bytes': stat_dict.get('obytes'),
                                'total_pkt_rate': stat_dict.get('tx_pps'),
                                'total_pkts': stat_dict.get('opackets'),
                                },
                            'rx': {
                                'pkt_bit_rate': stat_dict.get('rx_bps'),
                                'pkt_byte_count': stat_dict.get('ibytes'),
                                'pkt_count': stat_dict.get('ipackets'),
                                'pkt_rate': stat_dict.get('rx_pps'),
                                'total_pkt_bytes': stat_dict.get('ibytes'),
                                'total_pkt_rate': stat_dict.get('rx_pps'),
                                'total_pkts': stat_dict.get('ipackets'),
                                }
                            }
                        }
            return HLT_OK(hlt_stats_dict)


    # remove streams from given port(s).
    # stream_id can be:
    #    * int    - exact stream_id value
    #    * list   - list of stream_id values or strings (see below)
    #    * string - exact stream_id value, mix of ranges/list separated by comma: 2, 4-13
    def _remove_stream(self, stream_id, port_handle):
        if get_number(stream_id) is not None: # exact value of int or str
            self.trex_client.remove_streams(get_number(stream_id), port_handle)         # actual remove
            for port in port_handle:
                del self._streams_history[port][get_number(stream_id)]
            return
        if type(stream_id) is list: # list of values/strings
            for each_stream_id in stream_id:
                self._remove_stream(each_stream_id, port_handle)                        # recurse
            return
        if type(stream_id) is str: # range or list in string
            if stream_id.find(',') != -1:
                for each_stream_id_element in stream_id.split(','):
                    self._remove_stream(each_stream_id_element, port_handle)            # recurse
                return
            if stream_id.find('-') != -1:
                stream_id_min, stream_id_max = stream_id.split('-', 1)
                stream_id_min = get_number(stream_id_min)
                stream_id_max = get_number(stream_id_max)
                if stream_id_min is None:
                    raise STLError('_remove_stream: wrong range param %s' % stream_id_min)
                if stream_id_max is None:
                    raise STLError('_remove_stream: wrong range param %s' % stream_id_max)
                if stream_id_max < stream_id_min:
                    raise STLError('_remove_stream: right range param is smaller than left one: %s-%s' % (stream_id_min, stream_id_max))
                for each_stream_id in xrange(stream_id_min, stream_id_max + 1):
                    self._remove_stream(each_stream_id, port_handle)                    # recurse
                return
        raise STLError('_remove_stream: wrong param %s' % stream_id)


###########################
#    Private functions    #
###########################

    @staticmethod
    def _parse_port_list(port_list):
        if type(port_list) is str:
            return [int(port) for port in port_list.strip().split()]
        elif type(port_list) is list:
            return [int(port) for port in port_list]
        elif type(port) in (int, long):
            return [int(port_list)]
        raise STLError('port_list should be string with ports, list, or single number')

def STLHltStream(**user_kwargs):
    kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
    # verify rate is given by at most one arg
    rate_args = set(['rate_pps', 'rate_bps', 'rate_percent'])
    intersect_rate_args = list(rate_args & set(user_kwargs.keys()))
    if len(intersect_rate_args) > 1:
        raise STLError('More than one rate argument specified: %s' % intersect_rate_args)
    try:
        rate_key = intersect_rate_args[0]
    except IndexError:
        rate_key = 'rate_percent'

    if kwargs['length_mode'] == 'imix': # several streams with given length
        streams_arr = []
        user_kwargs['length_mode'] = 'fixed'
        if kwargs['l3_imix1_size'] < 32 or kwargs['l3_imix2_size'] < 32 or kwargs['l3_imix3_size'] < 32 or kwargs['l3_imix4_size'] < 32:
            raise STLError('l3_imix*_size should be at least 32')
        total_ratio = kwargs['l3_imix1_ratio'] + kwargs['l3_imix2_ratio'] + kwargs['l3_imix3_ratio'] + kwargs['l3_imix4_ratio']
        if total_ratio == 0:
            raise STLError('Used length_mode imix, but all the ratios are 0')
        save_to_yaml = kwargs.get('save_to_yaml')
        total_rate = float(kwargs[rate_key])
        if kwargs['l3_imix1_ratio'] > 0:
            if save_to_yaml and type(save_to_yaml) is str:
                user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix1.yaml')
            user_kwargs['frame_size'] = kwargs['l3_imix1_size']
            user_kwargs[rate_key] = total_rate * kwargs['l3_imix1_ratio'] / total_ratio
            streams_arr.append(STLHltStream(**user_kwargs))
        if kwargs['l3_imix2_ratio'] > 0:
            if save_to_yaml and type(save_to_yaml) is str:
                user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix2.yaml')
            user_kwargs['frame_size'] = kwargs['l3_imix2_size']
            user_kwargs[rate_key] = total_rate * kwargs['l3_imix2_ratio'] / total_ratio
            streams_arr.append(STLHltStream(**user_kwargs))
        if kwargs['l3_imix3_ratio'] > 0:
            if save_to_yaml and type(save_to_yaml) is str:
                user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix3.yaml')
            user_kwargs['frame_size'] = kwargs['l3_imix3_size']
            user_kwargs[rate_key] = total_rate * kwargs['l3_imix3_ratio'] / total_ratio
            streams_arr.append(STLHltStream(**user_kwargs))
        if kwargs['l3_imix4_ratio'] > 0:
            if save_to_yaml and type(save_to_yaml) is str:
                user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix4.yaml')
            user_kwargs['frame_size'] = kwargs['l3_imix4_size']
            user_kwargs[rate_key] = total_rate * kwargs['l3_imix4_ratio'] / total_ratio
            streams_arr.append(STLHltStream(**user_kwargs))
        return streams_arr

    # packet generation
    packet = generate_packet(**user_kwargs)
    try:
        rate_types_dict = {'rate_pps': 'pps', 'rate_bps': 'bps_L2', 'rate_percent': 'percentage'}
        rate_stateless = {rate_types_dict[rate_key]: float(kwargs[rate_key])}
        transmit_mode = kwargs['transmit_mode']
        pkts_per_burst = kwargs['pkts_per_burst']
        if transmit_mode == 'continuous':
            transmit_mode_class = STLTXCont(**rate_stateless)
        elif transmit_mode == 'single_burst':
            transmit_mode_class = STLTXSingleBurst(total_pkts = pkts_per_burst, **rate_stateless)
        elif transmit_mode == 'multi_burst':
            transmit_mode_class = STLTXMultiBurst(total_pkts = pkts_per_burst, count = int(kwargs['burst_loop_count']),
                                                  ibg = kwargs['inter_burst_gap'], **rate_stateless)
        else:
            raise STLError('transmit_mode %s not supported/implemented')
    except Exception as e:
        raise STLError('Could not create transmit_mode class %s: %s' % (transmit_mode, e if isinstance(e, STLError) else traceback.format_exc()))

    # stream generation
    try:
        stream = STLStream(packet = packet,
                           random_seed = 1 if kwargs['consistent_random'] else 0,
                           #enabled = True,
                           #self_start = True,
                           mode = transmit_mode_class,
                           #rx_stats = rx_stats,
                           stream_id = kwargs['stream_id'],
                           name = kwargs['name'],
                           )
    except Exception as e:
        raise STLError('Could not create stream: %s' % e if isinstance(e, STLError) else traceback.format_exc())

    debug_filename = kwargs.get('save_to_yaml')
    if type(debug_filename) is str:
        print 'saving to %s' % debug_filename
        stream.dump_to_yaml(debug_filename)
    return stream

def generate_packet(**user_kwargs):
    correct_macs(user_kwargs)
    kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
    correct_direction(kwargs, kwargs)

    vm_cmds = []
    vm_variables_cache = {} # we will keep in cache same variables (inc/dec, var size in bytes, number of steps, step)
    fix_ipv4_checksum = False

    ### L2 ###
    if kwargs['l2_encap'] in ('ethernet_ii', 'ethernet_ii_vlan'):
                #fields_desc = [ MACField("dst","00:00:00:01:00:00"),
                #                MACField("src","00:00:00:02:00:00"), 
                #                XShortEnumField("type", 0x9000, ETHER_TYPES) ]
        l2_layer = Ether(src = kwargs['mac_src'], dst = kwargs['mac_dst'])

        # Eth VM
        # !!!! Need 8 bytes mask and vars !!!! for now will change only 32 lsb
        if kwargs['mac_src_mode'] != 'fixed':
            count = int(kwargs['mac_src_count']) - 1
            if count < 0:
                raise STLError('mac_src_count has to be at least 1')
            if count > 0 or kwargs['mac_src_mode'] == 'random':
                mac_src = mac_str_to_num(mac2str(kwargs['mac_src']))
                step = kwargs['mac_src_step']
                if step < 1:
                    raise STLError('mac_src_step has to be at least 1')
                if type(step) is str:
                    step = mac_str_to_num(mac2str(step))
                if kwargs['mac_src_mode'] == 'increment':
                    add_val = mac_src - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['mac_src_mode'] == 'decrement':
                    add_val = mac_src - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = 'mac_src', size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['mac_src_mode'] == 'random':
                    add_val = 0
                    var_name = 'mac_src_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('mac_src_mode %s is not supported' % kwargs['mac_src_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'Ethernet.src', offset_fixup = 2, add_val = add_val))

        if kwargs['mac_dst_mode'] != 'fixed':
            count = int(kwargs['mac_dst_count']) - 1
            if count < 0:
                raise STLError('mac_dst_count has to be at least 1')
            if count > 0 or kwargs['mac_dst_mode'] == 'random':
                mac_dst = mac_str_to_num(mac2str(kwargs['mac_dst']))
                step = kwargs['mac_dst_step']
                if step < 1:
                    raise STLError('mac_dst_step has to be at least 1')
                if type(step) is str:
                    step = mac_str_to_num(mac2str(step))
                if kwargs['mac_dst_mode'] == 'increment':
                    add_val = mac_dst - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['mac_dst_mode'] == 'decrement':
                    add_val = mac_dst - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = 'mac_dst', size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['mac_dst_mode'] == 'random':
                    add_val = 0
                    var_name = 'mac_dst_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('mac_dst_mode %s is not supported' % kwargs['mac_dst_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'Ethernet.dst', offset_fixup = 2, add_val = add_val))

        if kwargs['l2_encap'] == 'ethernet_ii_vlan' or (kwargs['l2_encap'] == 'ethernet_ii' and vlan_in_args(user_kwargs)):
                #fields_desc =  [ BitField("prio", 0, 3),
                #                 BitField("id", 0, 1),
                #                 BitField("vlan", 1, 12),
                #                 XShortEnumField("type", 0x0000, ETHER_TYPES) ]
            for i, vlan_kwargs in enumerate(split_vlan_args(kwargs)):
                vlan_id = int(vlan_kwargs['vlan_id'])
                dot1q_kwargs = {'prio': int(vlan_kwargs['vlan_user_priority']),
                                'vlan': vlan_id,
                                'id':   int(vlan_kwargs['vlan_cfi'])}
                vlan_protocol_tag_id = vlan_kwargs['vlan_protocol_tag_id']
                if vlan_protocol_tag_id is not None:
                    if type(vlan_protocol_tag_id) is str:
                        vlan_protocol_tag_id = int(vlan_protocol_tag_id, 16)
                    dot1q_kwargs['type'] = vlan_protocol_tag_id
                l2_layer /= Dot1Q(**dot1q_kwargs)

                # vlan VM
                vlan_id_mode = vlan_kwargs['vlan_id_mode']
                if vlan_id_mode != 'fixed':
                    count = int(vlan_kwargs['vlan_id_count']) - 1
                    if count < 0:
                        raise STLError('vlan_id_count has to be at least 1')
                    if count > 0 or vlan_id_mode == 'random':
                        var_name = 'vlan_id%s' % i
                        step = int(vlan_kwargs['vlan_id_step'])
                        if step < 1:
                            raise STLError('vlan_id_step has to be at least 1')
                        if vlan_id_mode == 'increment':
                            add_val = vlan_id - 0x7fff
                            var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                            if var_name not in vm_variables_cache:
                                vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'inc', step = step,
                                                                  min_value = 0x7fff,
                                                                  max_value = count * step + 0x7fff))
                                vm_variables_cache[var_name] = True
                        elif vlan_id_mode == 'decrement':
                            add_val = vlan_id - count * step - 0x7fff
                            var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                            if var_name not in vm_variables_cache:
                                vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'dec', step = step,
                                                                  min_value = 0x7fff,
                                                                  max_value = count * step + 0x7fff))
                                vm_variables_cache[var_name] = True
                        elif vlan_id_mode == 'random':
                            add_val = 0
                            var_name = 'vlan_id_random'
                            vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'random', max_value = 0xffff))
                        else:
                            raise STLError('vlan_id_mode %s is not supported' % vlan_id_mode)
                        vm_cmds.append(STLVmWrMaskFlowVar(fv_name = var_name, pkt_offset = '802|1Q:%s.vlan' % i,
                                                          pkt_cast_size = 2, mask = 0xfff, add_value = add_val))
    else:
        raise NotImplementedError("l2_encap does not support the desired encapsulation '%s'" % kwargs['l2_encap'])
    base_pkt = l2_layer

    ### L3 ###
    if kwargs['l3_protocol'] == 'ipv4':
                #fields_desc = [ BitField("version" , 4 , 4),
                #                BitField("ihl", None, 4),
                #                XByteField("tos", 0),
                #                ShortField("len", None),
                #                ShortField("id", 1),
                #                FlagsField("flags", 0, 3, ["MF","DF","evil"]),
                #                BitField("frag", 0, 13),
                #                ByteField("ttl", 64),
                #                ByteEnumField("proto", 0, IP_PROTOS),
                #                XShortField("chksum", None),
                #                Emph(IPField("src", "16.0.0.1")),
                #                Emph(IPField("dst", "48.0.0.1")),
                #                PacketListField("options", [], IPOption, length_from=lambda p:p.ihl*4-20) ]
        ip_tos = get_TOS(user_kwargs, kwargs)
        if ip_tos < 0 or ip_tos > 255:
            raise STLError('TOS %s is not in range 0-255' % ip_tos)
        l3_layer = IP(tos    = ip_tos,
                      len    = kwargs['l3_length'],
                      id     = kwargs['ip_id'],
                      frag   = kwargs['ip_fragment_offset'],
                      ttl    = kwargs['ip_ttl'],
                      chksum = kwargs['ip_checksum'],
                      src    = kwargs['ip_src_addr'],
                      dst    = kwargs['ip_dst_addr'],
                      )
        # IPv4 VM
        if kwargs['ip_src_mode'] != 'fixed':
            count = int(kwargs['ip_src_count']) - 1
            if count < 0:
                raise STLError('ip_src_count has to be at least 1')
            if count > 0 or kwargs['ip_src_mode'] == 'random':
                fix_ipv4_checksum = True
                ip_src_addr = kwargs['ip_src_addr']
                if type(ip_src_addr) is str:
                    ip_src_addr = ipv4_str_to_num(is_valid_ipv4(ip_src_addr))
                step = kwargs['ip_src_step']
                if step < 1:
                    raise STLError('ip_src_step has to be at least 1')
                if type(step) is str:
                    step = ipv4_str_to_num(is_valid_ipv4(step))
                if kwargs['ip_src_mode'] == 'increment':
                    add_val = ip_src_addr - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ip_src_mode'] == 'decrement':
                    add_val = ip_src_addr - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ip_src_mode'] == 'random':
                    add_val = 0
                    var_name = 'ip_src_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('ip_src_mode %s is not supported' % kwargs['ip_src_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'IP.src', add_val = add_val))

        if kwargs['ip_dst_mode'] != 'fixed':
            count = int(kwargs['ip_dst_count']) - 1
            if count < 0:
                raise STLError('ip_dst_count has to be at least 1')
            if count > 0 or kwargs['ip_dst_mode'] == 'random':
                fix_ipv4_checksum = True
                ip_dst_addr = kwargs['ip_dst_addr']
                if type(ip_dst_addr) is str:
                    ip_dst_addr = ipv4_str_to_num(is_valid_ipv4(ip_dst_addr))
                step = kwargs['ip_dst_step']
                if step < 1:
                    raise STLError('ip_dst_step has to be at least 1')
                if type(step) is str:
                    step = ipv4_str_to_num(is_valid_ipv4(step))
                if kwargs['ip_dst_mode'] == 'increment':
                    add_val = ip_dst_addr - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ip_dst_mode'] == 'decrement':
                    add_val = ip_dst_addr - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ip_dst_mode'] == 'random':
                    add_val = 0
                    var_name = 'ip_dst_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('ip_dst_mode %s is not supported' % kwargs['ip_dst_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'IP.dst', add_val = add_val))

    elif kwargs['l3_protocol'] == 'ipv6':
                #fields_desc = [ BitField("version" , 6 , 4),
                #                BitField("tc", 0, 8), #TODO: IPv6, ByteField ?
                #                BitField("fl", 0, 20),
                #                ShortField("plen", None),
                #                ByteEnumField("nh", 59, ipv6nh),
                #                ByteField("hlim", 64),
                #                IP6Field("dst", "::2"),
                #                #SourceIP6Field("src", "dst"), # dst is for src @ selection
                #                IP6Field("src", "::1") ]
        ipv6_kwargs = {'tc': kwargs['ipv6_traffic_class'],
                       'fl': kwargs['ipv6_flow_label'],
                       'plen': kwargs['ipv6_length'],
                       'hlim': kwargs['ipv6_hop_limit'],
                       'src': kwargs['ipv6_src_addr'],
                       'dst': kwargs['ipv6_dst_addr']}
        if kwargs['ipv6_next_header'] is not None:
            ipv6_kwargs['nh'] = kwargs['ipv6_next_header']
        l3_layer = IPv6(**ipv6_kwargs)

        # IPv6 VM
        if kwargs['ipv6_src_mode'] != 'fixed':
            count = int(kwargs['ipv6_src_count']) - 1
            if count < 0:
                raise STLError('ipv6_src_count has to be at least 1')
            if count > 0 or kwargs['ipv6_src_mode'] == 'random':
                ipv6_src_addr_num = ipv4_str_to_num(is_valid_ipv6(kwargs['ipv6_src_addr'])[-4:])
                step = kwargs['ipv6_src_step']
                if step < 1:
                    raise STLError('ipv6_src_step has to be at least 1')
                if type(step) is str: # convert ipv6 step to number
                    step = ipv4_str_to_num(is_valid_ipv6(step)[-4:])
                if kwargs['ipv6_src_mode'] == 'increment':
                    add_val = ipv6_src_addr_num - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ipv6_src_mode'] == 'decrement':
                    add_val = ipv6_src_addr_num - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ipv6_src_mode'] == 'random':
                    add_val = 0
                    var_name = 'ipv6_src_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('ipv6_src_mode %s is not supported' % kwargs['ipv6_src_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'IPv6.src', offset_fixup = 12, add_val = add_val))

        if kwargs['ipv6_dst_mode'] != 'fixed':
            count = int(kwargs['ipv6_dst_count']) - 1
            if count < 0:
                raise STLError('ipv6_dst_count has to be at least 1')
            if count > 0 or kwargs['ipv6_dst_mode'] == 'random':
                ipv6_dst_addr_num = ipv4_str_to_num(is_valid_ipv6(kwargs['ipv6_dst_addr'])[-4:])
                step = kwargs['ipv6_dst_step']
                if step < 1:
                    raise STLError('ipv6_dst_step has to be at least 1')
                if type(step) is str: # convert ipv6 step to number
                    step = ipv4_str_to_num(is_valid_ipv6(step)[-4:])
                if kwargs['ipv6_dst_mode'] == 'increment':
                    add_val = ipv6_dst_addr_num - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('inc', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'inc', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ipv6_dst_mode'] == 'decrement':
                    add_val = ipv6_dst_addr_num - count * step - 0x7fffffff
                    var_name = '%s_%s_%s_%s' % ('dec', 4, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'dec', step = step,
                                                          min_value = 0x7fffffff,
                                                          max_value = count * step + 0x7fffffff))
                        vm_variables_cache[var_name] = True
                elif kwargs['ipv6_dst_mode'] == 'random':
                    add_val = 0
                    var_name = 'ipv6_dst_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 4, op = 'random', max_value = 0xffffffff))
                else:
                    raise STLError('ipv6_dst_mode %s is not supported' % kwargs['ipv6_dst_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'IPv6.dst', offset_fixup = 12, add_val = add_val))

    else:
        raise NotImplementedError("l3_protocol '%s' is not supported by TRex yet." % kwargs['l3_protocol'])
    base_pkt /= l3_layer

    ### L4 ###
    if kwargs['l4_protocol'] == 'tcp':
                #fields_desc = [ ShortEnumField("sport", 20, TCP_SERVICES),
                #                ShortEnumField("dport", 80, TCP_SERVICES),
                #                IntField("seq", 0),
                #                IntField("ack", 0),
                #                BitField("dataofs", None, 4),
                #                BitField("reserved", 0, 4),
                #                FlagsField("flags", 0x2, 8, "FSRPAUEC"),
                #                ShortField("window", 8192),
                #                XShortField("chksum", None),
                #                ShortField("urgptr", 0),
                #                TCPOptionsField("options", {}) ]
        tcp_flags = ('F' if kwargs['tcp_fin_flag'] else '' +
                     'S' if kwargs['tcp_syn_flag'] else '' +
                     'R' if kwargs['tcp_rst_flag'] else '' +
                     'P' if kwargs['tcp_psh_flag'] else '' +
                     'A' if kwargs['tcp_ack_flag'] else '' +
                     'U' if kwargs['tcp_urg_flag'] else '')

        l4_layer = TCP(sport   = kwargs['tcp_src_port'],
                       dport   = kwargs['tcp_dst_port'],
                       seq     = kwargs['tcp_seq_num'],
                       ack     = kwargs['tcp_ack_num'],
                       dataofs = kwargs['tcp_data_offset'],
                       flags   = tcp_flags,
                       window  = kwargs['tcp_window'],
                       chksum  = kwargs['tcp_checksum'],
                       urgptr  = kwargs['tcp_urgent_ptr'],
                       )
        # TCP VM
        if kwargs['tcp_src_port_mode'] != 'fixed':
            count = int(kwargs['tcp_src_port_count']) - 1
            if count < 0:
                raise STLError('tcp_src_port_count has to be at least 1')
            if count > 0 or kwargs['tcp_src_port_mode'] == 'random':
                fix_ipv4_checksum = True
                step = kwargs['tcp_src_port_step']
                if step < 1:
                    raise STLError('tcp_src_port_step has to be at least 1')
                if kwargs['tcp_src_port_mode'] == 'increment':
                    add_val = kwargs['tcp_src_port'] - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('inc', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'inc', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['tcp_src_port_mode'] == 'decrement':
                    add_val = kwargs['tcp_src_port'] - count * step - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'dec', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['tcp_src_port_mode'] == 'random':
                    add_val = 0
                    var_name = 'tcp_src_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'random', max_value = 0xffff))
                else:
                    raise STLError('tcp_src_port_mode %s is not supported' % kwargs['tcp_src_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'TCP.sport', add_val = add_val))

        if kwargs['tcp_dst_port_mode'] != 'fixed':
            count = int(kwargs['tcp_dst_port_count']) - 1
            if count < 0:
                raise STLError('tcp_dst_port_count has to be at least 1')
            if count > 0 or kwargs['tcp_dst_port_mode'] == 'random':
                fix_ipv4_checksum = True
                step = kwargs['tcp_dst_port_step']
                if step < 1:
                    raise STLError('tcp_dst_port_step has to be at least 1')
                if kwargs['tcp_dst_port_mode'] == 'increment':
                    add_val = kwargs['tcp_dst_port'] - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('inc', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'inc', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['tcp_dst_port_mode'] == 'decrement':
                    add_val = kwargs['tcp_dst_port'] - count * step - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'dec', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['tcp_dst_port_mode'] == 'random':
                    add_val = 0
                    var_name = 'tcp_dst_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'random', max_value = 0xffff))
                else:
                    raise STLError('tcp_dst_port_mode %s is not supported' % kwargs['tcp_dst_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'TCP.dport', add_val = add_val))

    elif kwargs['l4_protocol'] == 'udp':
                #fields_desc = [ ShortEnumField("sport", 53, UDP_SERVICES),
                #                ShortEnumField("dport", 53, UDP_SERVICES),
                #                ShortField("len", None),
                #                XShortField("chksum", None), ]
        l4_layer = UDP(sport  = kwargs['udp_src_port'],
                       dport  = kwargs['udp_dst_port'],
                       len    = kwargs['udp_length'], chksum = None)
        # UDP VM
        if kwargs['udp_src_port_mode'] != 'fixed':
            count = int(kwargs['udp_src_port_count']) - 1
            if count < 0:
                raise STLError('udp_src_port_count has to be at least 1')
            if count > 0 or kwargs['udp_src_port_mode'] == 'random':
                fix_ipv4_checksum = True
                step = kwargs['udp_src_port_step']
                if step < 1:
                    raise STLError('udp_src_port_step has to be at least 1')
                if kwargs['udp_src_port_mode'] == 'increment':
                    add_val = kwargs['udp_src_port'] - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('inc', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'inc', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['udp_src_port_mode'] == 'decrement':
                    add_val = kwargs['udp_src_port'] - count * step - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'dec', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                        vm_variables_cache[var_name] = True
                elif kwargs['udp_src_port_mode'] == 'random':
                    add_val = 0
                    var_name = 'udp_src_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'random', max_value = 0xffff))
                else:
                    raise STLError('udp_src_port_mode %s is not supported' % kwargs['udp_src_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'UDP.sport', add_val = add_val))

        if kwargs['udp_dst_port_mode'] != 'fixed':
            count = int(kwargs['udp_dst_port_count']) - 1
            if count < 0:
                raise STLError('udp_dst_port_count has to be at least 1')
            if count > 0 or kwargs['udp_dst_port_mode'] == 'random':
                fix_ipv4_checksum = True
                step = kwargs['udp_dst_port_step']
                if step < 1:
                    raise STLError('udp_dst_port_step has to be at least 1')
                if kwargs['udp_dst_port_mode'] == 'increment':
                    add_val = kwargs['udp_dst_port'] - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('inc', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'inc', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                elif kwargs['udp_dst_port_mode'] == 'decrement':
                    add_val = kwargs['udp_dst_port'] - 0x7fff
                    var_name = '%s_%s_%s_%s' % ('dec', 2, count, step)
                    if var_name not in vm_variables_cache:
                        vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'dec', step = step,
                                                          min_value = 0x7fff,
                                                          max_value = count * step + 0x7fff))
                elif kwargs['udp_dst_port_mode'] == 'random':
                    add_val = 0
                    var_name = 'udp_dst_random'
                    vm_cmds.append(CTRexVmDescFlowVar(name = var_name, size = 2, op = 'random', max_value = 0xffff))
                else:
                    raise STLError('udp_dst_port_mode %s is not supported' % kwargs['udp_dst_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = var_name, pkt_offset = 'UDP.dport', add_val = add_val))
    else:
        raise NotImplementedError("l4_protocol '%s' is not supported by TRex yet." % kwargs['l3_protocol'])
    base_pkt /= l4_layer

    trim_dict = {'increment': 'inc', 'decrement': 'dec', 'random': 'random'}
    length_mode = kwargs['length_mode']
    if length_mode == 'auto':
        payload_len = 0
    elif length_mode == 'fixed':
        if 'frame_size' in user_kwargs: # L2 has higher priority over L3
            payload_len = kwargs['frame_size'] - len(base_pkt)
        elif 'l3_length' in user_kwargs:
            payload_len = kwargs['l3_length'] - (len(base_pkt) - len(l2_layer))
        else: # default
            payload_len = kwargs['frame_size'] - len(base_pkt)
    elif length_mode == 'imix':
        raise STLError("length_mode 'imix' should be treated at stream creating level.")
    elif length_mode in trim_dict:
        if 'frame_size_min' in user_kwargs or 'frame_size_max' in user_kwargs: # size is determined by L2, higher priority over L3 size
            if kwargs['frame_size_min'] < 44 or kwargs['frame_size_max'] < 44:
                raise STLError('frame_size_min and frame_size_max should be at least 44')
            if kwargs['frame_size_min'] > kwargs['frame_size_max']:
                raise STLError('frame_size_min is bigger than frame_size_max')
            if kwargs['frame_size_min'] != kwargs['frame_size_max']:
                fix_ipv4_checksum = True
                vm_cmds.append(CTRexVmDescFlowVar(name = 'pkt_len', size = 2, op = trim_dict[length_mode], step = kwargs['frame_size_step'],
                                                  min_value = kwargs['frame_size_min'],
                                                  max_value = kwargs['frame_size_max']))
                vm_cmds.append(CTRexVmDescTrimPktSize('pkt_len'))
            payload_len = kwargs['frame_size_max'] - len(base_pkt)
        else: # size is determined by L3
            if kwargs['l3_length_min'] < 40 or kwargs['l3_length_max'] < 40:
                raise STLError('l3_length_min and l3_length_max should be at least 40')
            if kwargs['l3_length_min'] > kwargs['l3_length_max']:
                raise STLError('l3_length_min is bigger than l3_length_max')
            if kwargs['l3_length_min'] != kwargs['l3_length_max']:
                fix_ipv4_checksum = True
                vm_cmds.append(CTRexVmDescFlowVar(name = 'pkt_len', size = 2, op = trim_dict[length_mode], step = kwargs['l3_length_step'],
                                                  min_value = kwargs['l3_length_min'] + len(l2_layer),
                                                  max_value = kwargs['l3_length_max'] + len(l2_layer)))
            payload_len = kwargs['l3_length_max'] + len(l2_layer) - len(base_pkt)
            vm_cmds.append(CTRexVmDescTrimPktSize('pkt_len'))

        if l3_layer.name == 'IP' or l4_layer.name == 'UDP': # add here other things need to fix due to size change
            if l3_layer.name == 'IP':
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = 'pkt_len', pkt_offset = 'IP.len', add_val = -len(l2_layer)))
            if l4_layer.name == 'UDP':
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = 'pkt_len', pkt_offset = 'UDP.len', add_val = -len(l2_layer) - len(l3_layer)))
    else:
        raise STLError('length_mode should be one of the following: %s' % ['auto', 'fixed'] + trim_dict.keys())

    if payload_len < 0:
        raise STLError('Packet length is bigger than defined by frame_size* or l3_length*. We got payload size %s' % payload_len)
    base_pkt /= '!' * payload_len

    pkt = STLPktBuilder()
    pkt.set_packet(base_pkt)
    if fix_ipv4_checksum and l3_layer.name == 'IP' and kwargs['ip_checksum'] is None:
        vm_cmds.append(CTRexVmDescFixIpv4(offset = 'IP'))
    if vm_cmds:
        split_by_field = None
        if kwargs['split_by_cores'] == 'split':
            max_length = 0
            for cmd in vm_cmds:
                if isinstance(cmd, CTRexVmDescFlowVar):
                    if cmd.op not in ('inc', 'dec'):
                        continue
                    length = float(cmd.max_value - cmd.min_value) / cmd.step
                    if cmd.name == 'ip_src' and length > 7: # priority is to split by ip_src
                        split_by_field = 'ip_src'
                        break
                    if length > max_length:
                        max_length = length
                        split_by_field = cmd.name
        elif kwargs['split_by_cores'] == 'single':
            raise STLError("split_by_cores 'single' not implemented yet")
        elif kwargs['split_by_cores'] != 'duplicate':
            raise STLError("split_by_cores '%s' is not supported" % kwargs['split_by_cores'])
        pkt.add_command(CTRexScRaw(vm_cmds, split_by_field))

    # debug (only the base packet, without VM)
    debug_filename = kwargs.get('save_to_pcap')
    if type(debug_filename) is str:
        pkt.dump_pkt_to_pcap(debug_filename)
    return pkt

def get_TOS(user_kwargs, kwargs):
    TOS0 = set(['ip_precedence', 'ip_tos_field', 'ip_mbz'])
    TOS1 = set(['ip_precedence', 'ip_delay', 'ip_throughput', 'ip_reliability', 'ip_cost', 'ip_reserved'])
    TOS2 = set(['ip_dscp', 'ip_cu'])
    user_args = set(user_kwargs.keys())
    if user_args & (TOS1 - TOS0) and user_args & (TOS0 - TOS1):
        raise STLError('You have mixed %s and %s TOS parameters' % (TOS0, TOS1))
    if user_args & (TOS2 - TOS0) and user_args & (TOS0 - TOS2):
        raise STLError('You have mixed %s and %s TOS parameters' % (TOS0, TOS2))
    if user_args & (TOS2 - TOS1) and user_args & (TOS1 - TOS2):
        raise STLError('You have mixed %s and %s TOS parameters' % (TOS1, TOS2))
    if user_args & (TOS0 - TOS1 - TOS2):
        return (kwargs['ip_precedence'] << 5) + (kwargs['ip_tos_field'] << 2) + kwargs['ip_mbz']
    if user_args & (TOS1 - TOS2):
        return (kwargs['ip_precedence'] << 5) + (kwargs['ip_delay'] << 4) + (kwargs['ip_throughput'] << 3) + (kwargs['ip_reliability'] << 2) + (kwargs['ip_cost'] << 1) + kwargs['ip_reserved']
    return (kwargs['ip_dscp'] << 2) + kwargs['ip_cu']

def vlan_in_args(user_kwargs):
    for arg in user_kwargs:
        if arg.startswith('vlan_'):
            return True
    return False

def split_vlan_arg(vlan_arg):
    if type(vlan_arg) is list:
        return vlan_arg
    if type(vlan_arg) in (int, long, type(None)):
        return [vlan_arg]
    if type(vlan_arg) is str:
        return vlan_arg.replace('{', '').replace('}', '').strip().split()
    raise STLError('vlan argument invalid (expecting list, int, long, str, None): %s' % vlan_arg)

def split_vlan_args(kwargs):
    vlan_args_dict = {}
    for arg, value in kwargs.items():
        if arg.startswith('vlan_'):
            vlan_args_dict[arg] = split_vlan_arg(value)
    dot1q_headers_count = max([len(x) for x in vlan_args_dict.values()])
    vlan_args_per_header = [{} for _ in range(dot1q_headers_count)]
    for arg, value in vlan_args_dict.items():
        for i in range(dot1q_headers_count):
            if len(value) > i:
                vlan_args_per_header[i][arg] = value[i]
            else:
                vlan_args_per_header[i][arg] = traffic_config_kwargs[arg]
    return vlan_args_per_header

def correct_direction(user_kwargs, kwargs):
    if kwargs['direction'] == 0:
        return
    user_kwargs['mac_src'] = kwargs['mac_src2']
    user_kwargs['mac_dst'] = kwargs['mac_dst2']
    if kwargs['l3_protocol'] == 'ipv4':
        for arg in kwargs.keys():
            if 'ip_src_' in arg:
                dst_arg = 'ip_dst_' + arg[7:]
                user_kwargs[arg], user_kwargs[dst_arg] = kwargs[dst_arg], kwargs[arg]
    elif kwargs['l3_protocol'] == 'ipv6':
        for arg in kwargs.keys():
            if 'ipv6_src_' in arg:
                dst_arg = 'ipv6_dst_' + arg[9:]
                user_kwargs[arg], user_kwargs[dst_arg] = kwargs[dst_arg], kwargs[arg]
