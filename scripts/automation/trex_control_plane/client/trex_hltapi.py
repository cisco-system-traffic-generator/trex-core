#!/router/bin/python

'''
Supported function/arguments/defaults:
'''
# connect()
connect_kwargs = {
    'device': 'localhost',                  # ip or hostname of TRex
    'port_list': None,                      # list of ports
    'username': 'TRexUser',
    'reset': True,
    'break_locks': False,
}

# cleanup_session()
cleanup_session_kwargs = {
    'maintain_lock': False,                 # release ports at the end or not
    'port_list': None,
    'port_handle': None,
}

# traffic_config()
traffic_config_kwargs = {
    'mode': None,                           # ( create | modify | remove | reset )
    'port_handle': None,
    'transmit_mode': 'continuous',          # ( continuous | multi_burst | single_burst )
    'rate_pps': 1,
    'stream_id': None,
    'name': None,
    'bidirectional': 0,
    # stream builder parameters
    'pkts_per_burst': 1,
    'burst_loop_count': 1,
    'inter_burst_gap': 12,
    'length_mode': 'fixed',                 #  ( auto | fixed | increment | decrement | random | imix )
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
    'frame_size_step': 1,                   # has to be 1
    'l2_encap': 'ethernet_ii',              # ( ethernet_ii )
    'mac_src': '00:00:01:00:00:01',
    'mac_dst': '00:00:00:00:00:00',
    'mac_src2': '00:00:01:00:00:01',
    'mac_dst2': '00:00:00:00:00:00',
    'vlan_user_priority': 1,
    'vlan_id': 0,
    'vlan_cfi': 1,
    #L3, IP
    'l3_protocol': 'ipv4',                  # ( ipv4 )
    'ip_tos_field': 0,
    'l3_length': 50,
    'ip_id': 0,
    'ip_fragment_offset': 0,
    'ip_ttl': 64,
    'ip_checksum': None,
    'ip_src_addr': '0.0.0.0',
    'ip_dst_addr': '192.0.0.1',
    'ip_src_mode': 'fixed',                 # ( fixed | increment | decrement | random )
    'ip_src_step': 1,                       # has to be 1
    'ip_src_count': 1,
    'ip_dst_mode': 'fixed',                 # ( fixed | increment | decrement | random )
    'ip_dst_step': 1,                       # has to be 1
    'ip_dst_count': 1,
    'l3_length_min': 110,
    'l3_length_max': 238,
    'l3_length_step': 1,                    # has to be 1
    #L3, IPv6 (TODO: add)
    'ipv6_src_addr': None,
    'ipv6_dst_addr': None,
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
    'tcp_src_port_step': 1,                 # has to be 1
    'tcp_src_port_count': 1,
    'tcp_dst_port_mode': 'increment',       # ( increment | decrement | random )
    'tcp_dst_port_step': 1,                 # has to be 1
    'tcp_dst_port_count': 1,
    # L4, UDP
    'udp_src_port': 1024,
    'udp_dst_port': 80,
    'udp_length': None,
    'udp_checksum': None,
    'udp_dst_port_mode': 'increment',       # ( increment | decrement | random )
    'udp_src_port_step': 1,                 # has to be 1
    'udp_src_port_count': 1,
    'udp_src_port_mode': 'increment',       # ( increment | decrement | random )
    'udp_dst_port_step': 1,                 # has to be 1
    'udp_dst_port_count': 1,
}

# traffic_control()
traffic_control_kwargs = {
    'action': None,                         # ( run | stop )
    'port_handle': None
}

# traffic_stats()
traffic_stats_kwargs = {
    'mode': 'aggregate',                    # ( aggregate )
    'port_handle': None
}


import sys
import os

CURRENT_PATH        = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(CURRENT_PATH, '../stl/'))

from trex_stl_lib.api import *

from client_utils.general_utils import get_number
import socket
import copy
from misc_methods import print_r


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

# change MACs from HLT formats {a b c d e f} or a-b-c-d-e-f or a.b.c.d.e.f to Scapy format a:b:c:d:e:f
def correct_macs(kwargs):
    list_of_mac_args = ['mac_src', 'mac_dst', 'mac_src2', 'mac_dst2']
    for mac_arg in list_of_mac_args:
        if mac_arg in kwargs:
            if type(kwargs[mac_arg]) is not str: raise Exception('Argument %s should be str' % mac_arg)
            kwargs[mac_arg] = kwargs[mac_arg].replace('{', '').replace('}', '').strip().replace('-', ':').replace(' ', ':').replace('.', ':')
            kwargs[mac_arg] = ':'.join(kwargs[mac_arg].split(':'))      # remove duplicate ':' if any
            try:
                mac2str(kwargs[mac_arg])                                # verify we are ok
            except:
                raise Exception('Incorrect MAC %s=%s, please use 01:23:45:67:89:10 or 01-23-45-67-89-10 or 01.23.45.67.89.10 or {01 23 45 67 89 10}' % (mac_arg, kwargs[mac_arg]))

# dict of streams per port
# hlt_history = False: holds list of stream_id per port
# hlt_history = True:  act as dictionary (per port) stream_id -> hlt arguments used for build
class CStreamsPerPort(dict):
    def __init__(self, port_list, hlt_history = False):
        if type(port_list) is not list:
            raise Exception('CStreamsPerPort: should be init with list of ports')
        self.hlt_history = hlt_history
        if self.hlt_history:
            dict.__init__(self, dict((port, {}) for port in port_list))
        else:
            dict.__init__(self, dict((port, []) for port in port_list))

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
        if self.hlt_history: raise Exception('CStreamsPerPort: this object is not meant for HLT history, try init with hlt_history = False')
        if not isinstance(res, dict): raise Exception('CStreamsPerPort: res should be dict')
        if res.get('status') != 1: raise Exception('CStreamsPerPort: res has status %s' % res.get('status'))
        res_streams = res.get('stream_id')
        if not isinstance(res_streams, dict):
            raise Exception('CStreamsPerPort: stream_id in res should be dict')
        for port, port_stream_ids in res_streams.items():
            if port not in self:
                raise Exception('CStreamsPerPort: port %s not defined' % port)
            if type(port_stream_ids) is not list:
                port_stream_ids = [port_stream_ids]
            self[port].extend(port_stream_ids)

    # save HLT args to modify streams later
    def save_stream_args(self, ports_list, stream_id, stream_hlt_args):
        if not self.hlt_history: raise Exception('CStreamsPerPort: this object works only with HLT history, try init with hlt_history = True')
        if type(stream_id) not in (int, long): raise Exception('CStreamsPerPort: stream_id should be number')
        if not isinstance(stream_hlt_args, dict): raise Exception('CStreamsPerPort: stream_hlt_args should be dict')
        if not isinstance(ports_list, list):
            ports_list = [ports_list]
        for port in ports_list:
            if port not in self:
                raise Exception('CStreamsPerPort: port %s not defined' % port)
            del stream_hlt_args['port_handle']
            if stream_id in self[port]:
                self[port][stream_id].update(stream_hlt_args)
            else:
                self[port][stream_id] = stream_hlt_args

    def remove_stream(self, ports_list, stream_id):
        if not isinstance(ports_list, list):
            ports_list = [ports_list]
        if not isinstance(stream_id, dict):
            raise Exception('CStreamsPerPort: stream_hlt_args should be dict')
        for port in ports_list:
            if port not in self:
                raise Exception('CStreamsPerPort: port %s not defined' % port)
            if stream_id not in self[port]:
                raise Exception('CStreamsPerPort: stream_id %s not found at port %s' % (port, stream_id))
            if self.hlt_history:
                del self[port][stream_id]
            else:
                self[port].pop(stream_id)

class CTRexHltApi(object):

    def __init__(self, verbose = 0):
        self.trex_client = None
        self.connected = False
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
            self.trex_client = None
            return HLT_ERR('Could not init stateless client %s: %s' % (device, e))
        try:
            self.trex_client.connect()
        #except Exception as e:
        except ValueError as e:
            self.trex_client = None
            return HLT_ERR('Could not connect to device %s: %s' % (device, e))

        # connection successfully created with server, try acquiring ports of TRex
        try:
            port_list = self.parse_port_list(kwargs['port_list'])
            self.trex_client.acquire(ports = port_list, force = kwargs['break_locks'])
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not acquire ports %s: %s' % (port_list, e))

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
                return HLT_ERR('Error in reset traffic: %s' % e)

        self._streams_history = CStreamsPerPort(port_handle, hlt_history = True)
        self.connected = True
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
                    port_list = self.parse_port_list(port_list)
            except Exception as e:
                return HLT_ERR('Unable to determine which ports to release: %s' % e)
            try:
                self.trex_client.release(port_list)
            except Exception as e:
                return HLT_ERR('Unable to release ports %s: %s' % (port_list, e))
        try:
            self.trex_client.disconnect(stop_traffic = False, release_ports = False)
        except Exception as e:
            return HLT_ERR('Error disconnecting: %s' % e)
        self.trex_client = None
        self.connected = False
        return HLT_OK()

    def interface_config(self, port_handle, mode='config'):
        if not self.connected:
            return HLT_ERR('connect first')
        ALLOWED_MODES = ['config', 'modify', 'destroy']
        if mode not in ALLOWED_MODES:
            return HLT_ERR('Mode must be one of the following values: %s' % ALLOWED_MODES)
        # pass this function for now...
        return HLT_ERR('interface_config not implemented yet')


###########################
#    Traffic functions    #
###########################

    def traffic_config(self, **user_kwargs):
        if not self.connected:
            return HLT_ERR('connect first')
        try:
            correct_macs(user_kwargs)
        except Exception as e:
            return HLT_ERR(e)
        kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
        stream_id = kwargs['stream_id']
        if type(stream_id) is list:
            for each_stream_id in stream_id:
                user_kwargs[stream_id] = each_stream_id
                res = self.traffic_config(stream_id = each_stream_id, **user_kwargs)
                if type(res) is HLT_ERR:
                    return res
            return HLT_OK()

        mode = kwargs['mode']
        port_handle = kwargs['port_handle']
        if type(port_handle) is not list:
            port_handle = [port_handle]
        ALLOWED_MODES = ['create', 'modify', 'remove', 'enable', 'disable', 'reset']
        if mode not in ALLOWED_MODES:
            return HLT_ERR('Mode must be one of the following values: %s' % ALLOWED_MODES)

        if mode == 'reset':
            try:
                self.trex_client.remove_all_streams(port_handle)
                return HLT_OK()
            except Exception as e:
                return HLT_ERR('Could not reset streams at ports %s: %s' % (port_handle, e))

        if mode == 'remove':
            if stream_id is None:
                return HLT_ERR('Please specify stream_id to remove.')
            if type(stream_id) is str and stream_id == 'all':
                try:
                    self.trex_client.remove_all_streams(port_handle)
                except Exception as e:
                    return HLT_ERR('Could not remove all streams at ports %s: %s' % (port_handle, e))
            else:
                try:
                    self._remove_stream(stream_id, port_handle)
                except Exception as e:
                    return HLT_ERR('Could not remove streams with specified by %s, error: %s' % (stream_id, e))
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
            #except Exception as e:
            except ValueError as e:
                return HLT_ERR('Could not remove stream(s) %s from port(s) %s: %s' % (stream_id, port_handle, e))

        if mode == 'create' or mode == 'modify':
            # create a new stream with desired attributes, starting by creating packet
            streams_per_port = CStreamsPerPort(port_handle)
            if kwargs['bidirectional']: # two streams with opposite directions
                del user_kwargs['bidirectional']
                save_to_yaml = user_kwargs.get('save_to_yaml')
                bidirect_err = 'When using bidirectional flag, '
                if len(port_handle) != 2:
                    return HLT_ERR(bidirect_err + 'number of ports should be exactly 2')
                try:
                    if save_to_yaml and type(save_to_yaml) is str:
                        user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_bi1.yaml')
                    user_kwargs['port_handle'] = port_handle[0]
                    res1 = self.traffic_config(**user_kwargs)
                    if res1['status'] == 0:
                        raise Exception('Could not create bidirectional stream 1: %s' % res1['log'])
                    streams_per_port.add_streams_from_res(res1)
                    user_kwargs['mac_src'] = kwargs['mac_src2']
                    user_kwargs['mac_dst'] = kwargs['mac_dst2']
                    user_kwargs['ip_src_addr'] = kwargs['ip_dst_addr']
                    user_kwargs['ip_dst_addr'] = kwargs['ip_src_addr']
                    user_kwargs['ipv6_src_addr'] = kwargs['ipv6_dst_addr']
                    user_kwargs['ipv6_dst_addr'] = kwargs['ipv6_src_addr']
                    if save_to_yaml and type(save_to_yaml) is str:
                        user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_bi2.yaml')
                    user_kwargs['port_handle'] = port_handle[1]
                    res2 = self.traffic_config(**user_kwargs)
                    if res2['status'] == 0:
                        raise Exception('Could not create bidirectional stream 2: %s' % res2['log'])
                    streams_per_port.add_streams_from_res(res2)
                except Exception as e:
                    return HLT_ERR('Could not generate bidirectional traffic: %s' % e)
                return HLT_OK(stream_id = streams_per_port)

            if kwargs['length_mode'] == 'imix': # several streams with given length
                user_kwargs['length_mode'] = 'fixed'
                if kwargs['l3_imix1_size'] < 32 or kwargs['l3_imix2_size'] < 32 or kwargs['l3_imix3_size'] < 32 or kwargs['l3_imix4_size'] < 32:
                    return HLT_ERR('l3_imix*_size should be at least 32')
                total_rate = kwargs['l3_imix1_ratio'] + kwargs['l3_imix2_ratio'] + kwargs['l3_imix3_ratio'] + kwargs['l3_imix4_ratio']
                if total_rate == 0:
                    return HLT_ERR('Used length_mode imix, but all the ratio are 0')
                save_to_yaml = kwargs.get('save_to_yaml')
                rate_pps = float(kwargs['rate_pps'])
                try:
                    if kwargs['l3_imix1_ratio'] > 0:
                        if save_to_yaml and type(save_to_yaml) is str:
                            user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix1.yaml')
                        user_kwargs['frame_size'] = kwargs['l3_imix1_size']
                        user_kwargs['rate_pps'] = rate_pps * kwargs['l3_imix1_ratio'] / total_rate
                        res = self.traffic_config(**user_kwargs)
                        if res['status'] == 0:
                            return HLT_ERR('Could not create stream imix1: %s' % res['log'])
                        streams_per_port.add_streams_from_res(res)

                    if kwargs['l3_imix2_ratio'] > 0:
                        if save_to_yaml and type(save_to_yaml) is str:
                            user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix2.yaml')
                        user_kwargs['frame_size'] = kwargs['l3_imix2_size']
                        user_kwargs['rate_pps'] = rate_pps * kwargs['l3_imix2_ratio'] / total_rate
                        res = self.traffic_config(**user_kwargs)
                        if res['status'] == 0:
                            return HLT_ERR('Could not create stream imix2: %s' % res['log'])
                        streams_per_port.add_streams_from_res(res)
                    if kwargs['l3_imix3_ratio'] > 0:
                        if save_to_yaml and type(save_to_yaml) is str:
                            kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix3.yaml')
                        user_kwargs['frame_size'] = kwargs['l3_imix3_size']
                        user_kwargs['rate_pps'] = rate_pps * kwargs['l3_imix3_ratio'] / total_rate
                        res = self.traffic_config(**user_kwargs)
                        if res['status'] == 0:
                            return HLT_ERR('Could not create stream imix3: %s' % res['log'])
                        streams_per_port.add_streams_from_res(res)
                    if kwargs['l3_imix4_ratio'] > 0:
                        if save_to_yaml and type(save_to_yaml) is str:
                            user_kwargs['save_to_yaml'] = save_to_yaml.replace('.yaml', '_imix4.yaml')
                        user_kwargs['frame_size'] = kwargs['l3_imix4_size']
                        user_kwargs['rate_pps'] = rate_pps * kwargs['l3_imix4_ratio'] / total_rate
                        res = self.traffic_config(**user_kwargs)
                        if res['status'] == 0:
                            return HLT_ERR('Could not create stream imix4: %s' % res['log'])
                        streams_per_port.add_streams_from_res(res)
                except Exception as e:
                    return HLT_ERR('Could not generate imix streams: %s' % e)
                return HLT_OK(stream_id = streams_per_port)
            try:
                stream_obj = CTRexHltApiBuilder.generate_stream(**kwargs)
            except Exception as e:
                return HLT_ERR('Could not create stream: %s' % e)

            # try adding the stream per ports
            try:
                stream_id_arr = self.trex_client.add_streams(streams=stream_obj,
                                                             ports=port_handle)
                #print stream_id_arr, port_handle
                #print self._streams_history
                for port in port_handle:
                    #print type(user_kwargs)
                    self._streams_history.save_stream_args(port_handle, stream_id_arr[0], user_kwargs)
                    #print 'done'
            except Exception as e:
                return HLT_ERR('Could not add stream to ports: %s' % e)
            return HLT_OK(stream_id = dict((port, stream_id_arr) for port in port_handle))

        return HLT_ERR('Got to the end of traffic_config, mode not implemented or forgot "return" function somewhere.')

    def traffic_control(self, **user_kwargs):
        if not self.connected:
            return HLT_ERR('connect first')
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
                return HLT_ERR('Could not start traffic: %s' % e)
            return HLT_OK(stopped = 0)

        elif action == 'stop':

            try:
                self.trex_client.stop(ports = port_handle)
            except Exception as e:
                return HLT_ERR('Could not start traffic: %s' % e)
            return HLT_OK(stopped = 1)
        else:
            return HLT_ERR("Action '{0}' is not supported yet on TRex".format(action))

        # if we arrived here, this means that operation FAILED!
        return HLT_ERR("Probably action '%s' is not implemented" % action)

    def traffic_stats(self, **user_kwargs):
        if not self.connected:
            return HLT_ERR('connect first')
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
                return HLT_ERR('Could not retrieve stats: %s' % e)
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
            self.trex_client.remove_streams(get_number(stream_id), port_handle) # actual remove
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
                    raise Exception('_remove_stream: wrong range param %s' % stream_id_min)
                if stream_id_max is None:
                    raise Exception('_remove_stream: wrong range param %s' % stream_id_max)
                if stream_id_max < stream_id_min:
                    raise Exception('_remove_stream: right range param is smaller than left one: %s-%s' % (stream_id_min, stream_id_max))
                for each_stream_id in xrange(stream_id_min, stream_id_max + 1):
                    self._remove_stream(each_stream_id, port_handle)                    # recurse
                return
        raise Exception('_remove_stream: wrong param %s' % stream_id)


###########################
#    Private functions    #
###########################

    # obsolete
    @staticmethod
    def process_response(port_list, response):
        log = response.data() if response.good() else response.err()
        if isinstance(port_list, list):
            log = CTRexHltApi.join_batch_response(log)
        return response.good(), log

    @staticmethod
    def parse_port_list(port_list):
        if isinstance(port_list, str):
            return [int(port)
                    for port in port_list.split()]
        elif isinstance(port_list, list):
            return [int(port)
                    for port in port_list]
        else:
            return port_list

    @staticmethod
    def join_batch_response(responses):
        if type(responses) is list():
            return '\n'. join([str(response) for response in responses])
        return responses

class CTRexHltApiBuilder:
    @staticmethod
    def generate_stream(**user_kwargs):
        kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
        try:
            packet = CTRexHltApiBuilder.generate_packet(**kwargs)
        #except Exception as e:
        except ValueError as e:
            raise Exception('Could not generate packet: %s' % e)

        try:
            transmit_mode = kwargs['transmit_mode']
            rate_pps = kwargs['rate_pps']
            pkts_per_burst = kwargs['pkts_per_burst']
            if transmit_mode == 'continuous':
                transmit_mode_class = STLTXCont(pps = rate_pps)
            elif transmit_mode == 'single_burst':
                transmit_mode_class = STLTXSingleBurst(pps = rate_pps, total_pkts = pkts_per_burst)
            elif transmit_mode == 'multi_burst':
                transmit_mode_class = STLTXMultiBurst(pps = rate_pps, total_pkts = pkts_per_burst,
                                                      count = kwargs['burst_loop_count'], ibg = kwargs['inter_burst_gap'])
            else:
                raise Exception('transmit_mode %s not supported/implemented')
        except Exception as e:
            raise Exception('Could not create transmit_mode class %s: %s' % (transmit_mode, e))

        try:
            stream_obj = STLStream(packet = packet,
                                   #enabled = True,
                                   #self_start = True,
                                   mode = transmit_mode_class,
                                   #rx_stats = rx_stats,
                                   #next_stream_id = -1,
                                   stream_id = kwargs.get('stream_id'),
                                   name = kwargs.get('name'),
                                   )
        except Exception as e:
            raise Exception('Could not create stream: %s' % e)

        debug_filename = kwargs.get('save_to_yaml')
        if type(debug_filename) is str:
            stream_obj.dump_to_yaml(debug_filename)
        return stream_obj

    @staticmethod
    def generate_packet(**user_kwargs):
        kwargs = merge_kwargs(traffic_config_kwargs, user_kwargs)
        pkt = STLPktBuilder()

        vm_cmds = []
        fix_ipv4_checksum = False

        ### L2 ###
        if kwargs['l2_encap'] in ('ethernet_ii', 'ethernet_ii_vlan'):
                    #fields_desc = [ MACField("dst","00:00:00:01:00:00"),
                    #                MACField("src","00:00:00:02:00:00"), 
                    #                XShortEnumField("type", 0x9000, ETHER_TYPES) ]
            l2_layer = Ether(src = kwargs['mac_src'], dst = kwargs['mac_dst'])
            if kwargs['l2_encap'] == 'ethernet_ii_vlan':
                    #fields_desc =  [ BitField("prio", 0, 3),
                    #                 BitField("id", 0, 1),
                    #                 BitField("vlan", 1, 12),
                    #                 XShortEnumField("type", 0x0000, ETHER_TYPES) ]
                l2_layer /= Dot1Q(prio = kwargs['vlan_user_priority'],
                                  vlan = kwargs['vlan_id'],
                                  id   = kwargs['vlan_cfi'],
                                  )
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
                    #                #IPField("src", "127.0.0.1"),
                    #                #Emph(SourceIPField("src","dst")),
                    #                Emph(IPField("src", "16.0.0.1")),
                    #                Emph(IPField("dst", "48.0.0.1")),
                    #                PacketListField("options", [], IPOption, length_from=lambda p:p.ihl*4-20) ]
            l3_layer = IP(tos = kwargs['ip_tos_field'],
                          len    = None if kwargs['length_mode'] == 'auto' else kwargs['l3_length'],
                          id     = kwargs['ip_id'],
                          frag   = kwargs['ip_fragment_offset'],
                          ttl    = kwargs['ip_ttl'],
                          chksum = kwargs['ip_checksum'],
                          src    = kwargs['ip_src_addr'],
                          dst    = kwargs['ip_dst_addr'],
                          )
            # IPv4 VM
            if kwargs['ip_src_mode'] != 'fixed':
                fix_ipv4_checksum = True
                if kwargs['ip_src_step'] != 1:
                    raise Exception('ip_src_step has to be 1 (TRex limitation)')
                if kwargs['ip_src_count'] < 1:
                    raise Exception('ip_src_count has to be at least 1')
                ip_src_addr_num = ipv4_str_to_num(is_valid_ipv4(kwargs['ip_src_addr']))
                if kwargs['ip_src_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_src', size = 4, op = 'inc',
                                                      min_value = ip_src_addr_num,
                                                      max_value = ip_src_addr_num + kwargs['ip_src_count'] - 1))
                elif kwargs['ip_src_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_src', size = 4, op = 'dec',
                                                       min_value = ip_src_addr_num - kwargs['ip_src_count'] + 1,
                                                       max_value = ip_src_addr_num))
                elif kwargs['ip_src_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_src', size = 4, op = 'random',
                                                       min_value = ip_src_addr_num,
                                                       max_value = ip_src_addr_num + kwargs['ip_src_count'] - 1))
                else:
                    raise Exception('ip_src_mode %s is not supported' % kwargs['ip_src_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='ip_src', pkt_offset = 'IP.src'))

            if kwargs['ip_dst_mode'] != 'fixed':
                fix_ipv4_checksum = True
                if kwargs['ip_dst_step'] != 1:
                    raise Exception('ip_dst_step has to be 1 (TRex limitation)')
                if kwargs['ip_dst_count'] < 1:
                    raise Exception('ip_dst_count has to be at least 1')
                ip_dst_addr_num = ipv4_str_to_num(is_valid_ipv4(kwargs['ip_dst_addr']))
                if kwargs['ip_dst_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_dst', size = 4, op = 'inc',
                                                       min_value = ip_dst_addr_num,
                                                       max_value = ip_dst_addr_num + kwargs['ip_dst_count'] - 1))
                elif kwargs['ip_dst_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_dst', size = 4, op = 'dec',
                                                       min_value = ip_dst_addr_num - kwargs['ip_dst_count'] + 1,
                                                       max_value = ip_dst_addr_num))
                elif kwargs['ip_dst_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'ip_dst', size = 4, op = 'random',
                                                       min_value = ip_dst_addr_num,
                                                       max_value = ip_dst_addr_num + kwargs['ip_dst_count'] - 1))
                else:
                    raise Exception('ip_dst_mode %s is not supported' % kwargs['ip_dst_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='ip_dst', pkt_offset = 'IP.dst'))
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
            if kwargs['tcp_src_port_count'] != 1:
                fix_ipv4_checksum = True
                if kwargs['tcp_src_port_step'] != 1:
                    raise Exception('tcp_src_port_step has to be 1 (TRex limitation)')
                if kwargs['tcp_src_port_count'] < 1:
                    raise Exception('tcp_src_port_count has to be at least 1')
                if kwargs['tcp_src_port_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_src', size = 2, op = 'inc',
                                                      min_value = kwargs['tcp_src_port'],
                                                      max_value = kwargs['tcp_src_port'] + kwargs['tcp_src_port_count'] - 1))
                elif kwargs['tcp_src_port_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_src', size = 2, op = 'dec',
                                                      min_value = kwargs['tcp_src_port'] - kwargs['tcp_src_port_count'] +1,
                                                      max_value = kwargs['tcp_src_port']))
                elif kwargs['tcp_src_port_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_src', size = 2, op = 'random',
                                                      min_value = kwargs['tcp_src_port'],
                                                      max_value = kwargs['tcp_src_port'] + kwargs['tcp_src_port_count'] - 1))
                else:
                    raise Exception('tcp_src_port_mode %s is not supported' % kwargs['tcp_src_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='tcp_src', pkt_offset = 'TCP.sport'))

            if kwargs['tcp_dst_port_count'] != 1:
                fix_ipv4_checksum = True
                if kwargs['tcp_dst_port_step'] != 1:
                    raise Exception('tcp_dst_port_step has to be 1 (TRex limitation)')
                if kwargs['tcp_dst_port_count'] < 1:
                    raise Exception('tcp_dst_port_count has to be at least 1')
                if kwargs['tcp_dst_port_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_dst', size = 2, op = 'inc',
                                                      min_value = kwargs['tcp_dst_port'],
                                                      max_value = kwargs['tcp_dst_port'] + kwargs['tcp_dst_port_count'] - 1))
                elif kwargs['tcp_dst_port_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_dst', size = 2, op = 'dec',
                                                      min_value = kwargs['tcp_dst_port'] - kwargs['tcp_dst_port_count'] +1,
                                                      max_value = kwargs['tcp_dst_port']))
                elif kwargs['tcp_dst_port_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'tcp_dst', size = 2, op = 'random',
                                                      min_value = kwargs['tcp_dst_port'],
                                                      max_value = kwargs['tcp_dst_port'] + kwargs['tcp_dst_port_count'] - 1))
                else:
                    raise Exception('tcp_dst_port_mode %s is not supported' % kwargs['tcp_dst_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='tcp_dst', pkt_offset = 'TCP.dport'))


        elif kwargs['l4_protocol'] == 'udp':
                    #fields_desc = [ ShortEnumField("sport", 53, UDP_SERVICES),
                    #                ShortEnumField("dport", 53, UDP_SERVICES),
                    #                ShortField("len", None),
                    #                XShortField("chksum", None), ]
            l4_layer = UDP(sport  = kwargs['udp_src_port'],
                           dport  = kwargs['udp_dst_port'],
                           len    = kwargs['udp_length'],
                           chksum = kwargs['udp_checksum'])
            # UDP VM
            if kwargs['udp_src_port_count'] != 1:
                fix_ipv4_checksum = True
                if kwargs['udp_src_port_step'] != 1:
                    raise Exception('udp_src_port_step has to be 1 (TRex limitation)')
                if kwargs['udp_src_port_count'] < 1:
                    raise Exception('udp_src_port_count has to be at least 1')
                if kwargs['udp_src_port_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_src', size = 2, op = 'inc',
                                                      min_value = kwargs['udp_src_port'],
                                                      max_value = kwargs['udp_src_port'] + kwargs['udp_src_port_count'] - 1))
                elif kwargs['udp_src_port_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_src', size = 2, op = 'dec',
                                                      min_value = kwargs['udp_src_port'] - kwargs['udp_src_port_count'] +1,
                                                      max_value = kwargs['udp_src_port']))
                elif kwargs['udp_src_port_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_src', size = 2, op = 'random',
                                                      min_value = kwargs['udp_src_port'],
                                                      max_value = kwargs['udp_src_port'] + kwargs['udp_src_port_count'] - 1))
                else:
                    raise Exception('udp_src_port_mode %s is not supported' % kwargs['udp_src_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='udp_src', pkt_offset = 'UDP.sport'))

            if kwargs['udp_dst_port_count'] != 1:
                fix_ipv4_checksum = True
                if kwargs['udp_dst_port_step'] != 1:
                    raise Exception('udp_dst_port_step has to be 1 (TRex limitation)')
                if kwargs['udp_dst_port_count'] < 1:
                    raise Exception('udp_dst_port_count has to be at least 1')
                if kwargs['udp_dst_port_mode'] == 'increment':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_dst', size = 2, op = 'inc',
                                                      min_value = kwargs['udp_dst_port'],
                                                      max_value = kwargs['udp_dst_port'] + kwargs['udp_dst_port_count'] - 1))
                elif kwargs['udp_dst_port_mode'] == 'decrement':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_dst', size = 2, op = 'dec',
                                                      min_value = kwargs['udp_dst_port'] - kwargs['udp_dst_port_count'] +1,
                                                      max_value = kwargs['udp_dst_port']))
                elif kwargs['udp_dst_port_mode'] == 'random':
                    vm_cmds.append(CTRexVmDescFlowVar(name = 'udp_dst', size = 2, op = 'random',
                                                      min_value = kwargs['udp_dst_port'],
                                                      max_value = kwargs['udp_dst_port'] + kwargs['udp_dst_port_count'] - 1))
                else:
                    raise Exception('udp_dst_port_mode %s is not supported' % kwargs['udp_dst_port_mode'])
                vm_cmds.append(CTRexVmDescWrFlowVar(fv_name='udp_dst', pkt_offset = 'UDP.dport'))
        else:
            raise NotImplementedError("l4_protocol '%s' is not supported by TRex yet." % kwargs['l3_protocol'])
        base_pkt /= l4_layer

        if kwargs['length_mode'] == 'auto':
            payload_len = 0
        elif kwargs['length_mode'] == 'fixed':
            payload_len = kwargs['frame_size'] - len(base_pkt)
        elif kwargs['length_mode'] == 'imix':
            raise Exception("Should not use length_mode 'imix' directly in packet building, convert it to other mode at higher levels (stream?)")
        else:
            fix_ipv4_checksum = True
            if kwargs['frame_size_step'] != 1 or kwargs['l3_length_step'] != 1:
                raise Exception('frame_size_step and l3_length_step has to be 1 (TRex limitation)')
            trim_dict = {'increment': 'inc', 'decrement': 'dec'}
            if kwargs['frame_size_min'] != 64 or kwargs['frame_size_max'] != 64: # size is determined by L2, higher priority over L3 size
                if kwargs['frame_size_min'] < 12 or kwargs['frame_size_max'] < 12:
                    raise Exception('frame_size_min and frame_size_max should be at least 12')
                vm_cmds.append(CTRexVmDescFlowVar(name = 'fv_rand', size=2, op=trim_dict.get(kwargs['length_mode'], kwargs['length_mode']),
                                                  min_value = kwargs['frame_size_min'], max_value = kwargs['frame_size_max']))
                payload_len = kwargs['frame_size_max'] - len(base_pkt)
            else: # size is determined by L3
                vm_cmds.append(CTRexVmDescFlowVar(name = 'fv_rand', size=2, op=trim_dict.get(kwargs['length_mode'], kwargs['length_mode']),
                                                  min_value = kwargs['l3_length_min'] + len(l2_layer), max_value = kwargs['l3_length_max'] + len(l2_layer)))
                payload_len = kwargs['l3_length_max'] + len(l2_layer) - len(base_pkt)

            vm_cmds.append(CTRexVmDescTrimPktSize('fv_rand'))
            if l3_layer.name == 'IP' or l4_layer.name == 'UDP': # add here other things need to fix due to size change
                if l3_layer.name == 'IP':
                    vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = 'fv_rand', pkt_offset = 'IP.len', add_val = -len(l2_layer)))
                if l4_layer.name == 'UDP':
                    vm_cmds.append(CTRexVmDescWrFlowVar(fv_name = 'fv_rand', pkt_offset = 'UDP.len', add_val = -len(l2_layer) - len(l3_layer)))

        if payload_len < 0:
            raise Exception('Packet length is bigger than defined by frame_size* or l3_length*')
        base_pkt /= '!' * payload_len

        pkt.set_packet(base_pkt)
        if fix_ipv4_checksum and l3_layer.name == 'IP' and kwargs['ip_checksum'] is None:
            vm_cmds.append(CTRexVmDescFixIpv4(offset = 'IP'))
        if vm_cmds:
            pkt.add_command(CTRexScRaw(vm_cmds))

        # debug (only the base packet, without VM)
        debug_filename = kwargs.get('save_to_pcap')
        if type(debug_filename) is str:
            pkt.dump_pkt_to_pcap(debug_filename)
        #pkt.compile()
        #pkt.dump_scripts()
        return pkt

