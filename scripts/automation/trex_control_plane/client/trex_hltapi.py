#!/router/bin/python

'''
Supported function/arguments:
connect()
    device
    port_list
    username
    reset
    break_locks

cleanup_session()
    maintain_lock
    port_list
    port_handle

traffic_config()
    mode                ( create | modify | remove | reset )
    port_handle
    transmit_mode       ( continuous | multi_burst | single_burst )
    rate_pps
    stream_id
    bidirectional
    l2_encap
    mac_src
    mac_dst
    l3_protocol
    ip_src_addr
    ip_dst_addr
    l3_length
    l4_protocol

traffic_control()
    action              ( run | stop )
    port_handle

traffic_stats()
    mode                ( aggregate )
    port_handle

'''



import trex_root_path
from client_utils.packet_builder import CTRexPktBuilder
from trex_stateless_client import STLClient
from common.trex_streams import *
from client_utils.general_utils import get_integer
import dpkt
import socket
from misc_methods import print_r
import traceback

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


class CTRexHltApi(object):

    def __init__(self, verbose = 1):
        self.trex_client = None
        self.connected = False
        self.verbose = verbose
        self._hlt_streams_history = {} # streams per stream_id in format of HLT arguments for modify later


###########################
#    Session functions    #
###########################

    # device: ip or hostname
    def connect(self, device, port_list, username='', reset=False, break_locks=False):

        try:
            device = socket.gethostbyname(device) # work with ip
        except: # give it another try
            try:
                device = socket.gethostbyname(device)
            except Exception as e:
                return HLT_ERR('Could not translate hostname "%s" to IP: %s' % (device, e))

        try:
            # sync = RPC, async = ZMQ
            self.trex_client = STLClient(username, device, sync_port = 4501, async_port = 4500, verbose_level = self.verbose)
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not init stateless client %s: %s' % (device, e))
        try:
            self.trex_client.connect()
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not connect to device %s: %s' % (device, e))

        # connection successfully created with server, try acquiring ports of TRex
        try:
            port_list = self.parse_port_list(port_list)
            self.trex_client.acquire(ports = port_list, force = break_locks)
        except Exception as e:
            self.trex_client = None
            return HLT_ERR('Could not acquire ports %s: %s' % (port_list, e))

        # since only supporting single TRex at the moment, 1:1 map
        port_handle = self.trex_client.get_acquired_ports()

        # arrived here, all desired ports were successfully acquired
        if reset:
            # remove all port traffic configuration from TRex
            try:
                self.trex_client.reset(ports = port_list)
            except Exception as e:
                self.trex_client = None
                return HLT_ERR('Error in reset traffic: %s' % e)

        self.connected = True
        return HLT_OK(port_handle = port_handle)

    def cleanup_session(self, maintain_lock = False, **kwargs):
        if not maintain_lock:
            # release taken ports
            port_list = kwargs.get('port_list', kwargs.get('port_handle', 'all'))
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
        ALLOWED_MODES = ['config', 'modify', 'destroy']
        if mode not in ALLOWED_MODES:
            return HLT_ERR('Mode must be one of the following values: %s' % ALLOWED_MODES)
        # pass this function for now...
        return HLT_ERR('interface_config not implemented yet')


###########################
#    Traffic functions    #
###########################

    def traffic_config(self, mode, port_handle, **kwargs):
        stream_id = kwargs.get('stream_id')
        if type(stream_id) is list:
            del kwargs['stream_id']
            for each_stream_id in stream_id:
                res = self.traffic_config(mode, port_handle, stream_id = each_stream_id, **kwargs)
                if type(res) is HLT_ERR:
                    return res
            return HLT_OK()

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
                    return HLT_ERR('Could not remove streams with specified by %s, error: %s' % (stream_id, log))
            return HLT_OK()

        #if mode == 'enable':
        #    stream_id = kwargs.get('stream_id')
        #    if stream_id is None:
        #        return HLT_ERR('Please specify stream_id to enable.')
        #    if stream_id not in self._hlt_streams_history:
        #        return HLT_ERR('This stream_id (%s) was not used before, please create new.' % stream_id)
        #    self._hlt_streams_history[stream_id].update(kwargs) # <- the modification
            

        if mode == 'modify': # we remove stream and create new one with same stream_id
            stream_id = kwargs.get('stream_id')
            if stream_id is None:
                return HLT_ERR('Please specify stream_id to modify.')
            if stream_id not in self._hlt_streams_history:
                return HLT_ERR('This stream_id (%s) was not used before, please create new.' % stream_id)
            self._hlt_streams_history[stream_id].update(kwargs) # <- the modification
            kwargs = self._hlt_streams_history[stream_id]
            #for port_id in port_handle:
            #    if stream_id not in self.trex_client.get_stream_id_list(port_id):
            #        return HLT_ERR('Port %s does not have stream_id %s.' % (port_id, stream_id))
            try:
                self.trex_client.remove_streams(stream_id, port_handle)
            except Exception as e:
                return HLT_ERR('Could not remove streams specified by %s: %s' % (stream_id, e))

        if mode == 'create' or mode == 'modify':
            # create a new stream with desired attributes, starting by creating packet

            if 'bidirectional' in kwargs: # two streams with opposite src and dst MAC
                del kwargs['bidirectional']
                bidirect_err = 'When using bidirectional flag, '
                if len(port_handle) != 2:
                    return HLT_ERR(bidirect_err + 'number of ports should be exactly 2')
                if mac_src not in kwargs or mac_dst not in kwargs:
                    return HLT_ERR(bidirect_err + 'mac_src and mac_dst should be specified')
                try:
                    res1 = self.traffic_config(mode, port_handle[0], **kwargs)
                    res2 = self.traffic_config(mode, port_handle[1], mac_src = kwargs['mac_dst'], mac_dst = kwargs['mac_src'], **kwargs)
                except Exception as e:
                    return HLT_ERR('Could not generate bidirectional traffic: %s' % e)
                return HLT_OK(stream_id = [res1['stream_id'], res2['stream_id']])

            try:
                packet = CTRexHltApi._generate_stream(**kwargs)
            except Exception as e:
                return HLT_ERR('Could not generate stream: %s' % e)
            # set transmission attributes
            #try:
            #    tx_mode = CTxMode(type = transmit_mode, pps = rate_pps, **kwargs)
            #except Exception as e:
            #    return HLT_ERR('Could not init CTxMode: %s' % e)

            try:
                # set rx_stats
                #rx_stats = CRxStats()   # defaults with disabled
                rx_stats = None
            except Exception as e:
                return HLT_ERR('Could not init CTxMode: %s' % e)

            try:
                transmit_mode = kwargs.get('transmit_mode', 'continuous')
                rate_pps = kwargs.get('rate_pps', 1)
                pkts_per_burst = kwargs.get('pkts_per_burst', 1)
                burst_loop_count = kwargs.get('burst_loop_count', 1)
                inter_burst_gap = kwargs.get('inter_burst_gap', 12)
                if transmit_mode == 'continuous':
                    transmit_mode_class = STLTXCont(pps = rate_pps)
                elif transmit_mode == 'single_burst':
                    transmit_mode_class = STLTXSingleBurst(pps = rate_pps, total_pkts = pkts_per_burst)
                elif transmit_mode == 'multi_burst':
                    transmit_mode_class = STLTXMultiBurst(pps = rate_pps, total_pkts = pkts_per_burst, count = burst_loop_count, ibg = inter_burst_gap)
                else:
                    return HLT_ERR('transmit_mode %s not supported/implemented')
            except Exception as e:
                # some exception happened during the stream creation
                return HLT_ERR('Could not create transmit_mode class %s: %s' % (transmit_mode, e))

            try:
                # join the generated data into stream
                    
                stream_obj = STLStream(packet = packet,
                                       #enabled = True,
                                       #self_start = True,
                                       mode = transmit_mode_class,
                                       rx_stats = rx_stats,
                                       #next_stream_id = -1
                                       )
                # using CStream
                #stream_obj_params = {'enabled': False,
                #                     'self_start': True,
                #                     'next_stream_id': -1,
                #                     'isg': 0.0,
                #                     'mode': tx_mode,
                #                     'rx_stats': rx_stats,
                #                     'packet': packet}  # vm is excluded from this list since CTRexPktBuilder obj is passed
                #stream_obj.load_data(**stream_obj_params)
                #print stream_obj.get_id()
            except Exception as e:
                # some exception happened during the stream creation
                return HLT_ERR(e)

            stream_id = stream_obj.get_id()
            #print stream_obj
            # try adding the stream per ports
            try:
                self.trex_client.add_streams(streams=stream_obj,
                                             ports=port_handle)
                self._hlt_streams_history[stream_id] = kwargs
            except Exception as e:
                return HLT_ERR('Could not add stream to ports: %s' % e)
            return HLT_OK(stream_id = stream_obj.get_id())

        return HLT_ERR('Got to the end of traffic_config, mode not implemented or forgot "return" function somewhere.')

    def traffic_control(self, action, port_handle, **kwargs):
        ALLOWED_ACTIONS = ['clear_stats', 'run', 'stop', 'sync_run']
        if action not in ALLOWED_ACTIONS:
            return HLT_ERR('Action must be one of the following values: {actions}'.format(actions=ALLOWED_ACTIONS))
        if type(port_handle) is not list:
            port_handle = [port_handle]

        if action == 'run':

            try:
                self.trex_client.start(duration = kwargs.get('duration', -1), ports = port_handle)
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

    def traffic_stats(self, port_handle, mode):
        ALLOWED_MODES = ['aggregate', 'streams', 'all']
        if mode not in ALLOWED_MODES:
            return HLT_ERR("'mode' must be one of the following values: %s" % ALLOWED_MODES)
        if mode == 'streams':
            return HLT_ERR("'mode = streams' not implemented'")
        if mode in ('all', 'aggregate'):
            hlt_stats_dict = {}
            try:
                stats = self.trex_client.get_stats(port_handle)
            except Exception as e:
                return HLT_ERR('Could not retrieve stats: %s' % e)
            for port_id, stat_dict in stats.iteritems():
                if type(port_id) is int:
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
        if get_integer(stream_id) is not None: # exact value of int or str
            self.trex_client.remove_stream(get_integer(stream_id), port_handle)    # actual remove
        if type(stream_id) is list: # list of values/strings
            for each_stream_id in stream_id:
                self._remove_stream(each_stream_id, port_handle)                   # recurse
            return
        if type(stream_id) is str: # range or list in string
            if stream_id.find(',') != -1:
                for each_stream_id_element in stream_id.split(','):
                    self._remove_stream(each_stream_id_element, port_handle)       # recurse
                return
            if stream_id.find('-') != -1:
                stream_id_min, stream_id_max = stream_id.split('-', 1)
                stream_id_min = get_integer(stream_id_min)
                stream_id_max = get_integer(stream_id_max)
                if stream_id_min is None:
                    raise Exception('_remove_stream: wrong range param %s' % stream_id_min)
                if stream_id_max is None:
                    raise Exception('_remove_stream: wrong range param %s' % stream_id_max)
                if stream_id_max < stream_id_min:
                    raise Exception('_remove_stream: right range param is smaller than left one: %s-%s' % (stream_id_min, stream_id_max))
                for each_stream_id in xrange(stream_id_min, stream_id_max + 1):
                    self._remove_stream(each_stream_id, port_handle)               # recurse
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

    @staticmethod
    def _generate_stream(l2_encap = 'ethernet_ii', mac_src = '00:00:01:00:00:01', mac_dst = '00:00:00:00:00:00',
                         l3_protocol = 'ipv4', ip_src_addr = '0.0.0.0', ip_dst_addr = '192.0.0.1', l3_length = 110,
                         l4_protocol = 'tcp',
                         **kwards):
        ALLOWED_L3_PROTOCOL = {'ipv4': dpkt.ethernet.ETH_TYPE_IP,
                               'ipv6': dpkt.ethernet.ETH_TYPE_IP6,
                               'arp': dpkt.ethernet.ETH_TYPE_ARP}
        ALLOWED_L4_PROTOCOL = {'tcp': dpkt.ip.IP_PROTO_TCP,
                               'udp': dpkt.ip.IP_PROTO_UDP,
                               'icmp': dpkt.ip.IP_PROTO_ICMP,
                               'icmpv6': dpkt.ip.IP_PROTO_ICMP6,
                               'igmp': dpkt.ip.IP_PROTO_IGMP,
                               'rtp': dpkt.ip.IP_PROTO_IRTP,
                               'isis': dpkt.ip.IP_PROTO_ISIS,
                               'ospf': dpkt.ip.IP_PROTO_OSPF}

        pkt_bld = CTRexPktBuilder()
        if l2_encap == 'ethernet_ii':
                    #('dst', '6s', ''),
                    #('src', '6s', ''),
                    #('type', 'H', ETH_TYPE_IP)
            pkt_bld.add_pkt_layer('l2', dpkt.ethernet.Ethernet())
            # set Ethernet layer attributes
            pkt_bld.set_eth_layer_addr('l2', 'src', mac_src)
            pkt_bld.set_eth_layer_addr('l2', 'dst', mac_dst)
        else:
            raise NotImplementedError("l2_encap does not support the desired encapsulation '{0}'".format(l2_encap))
        # set l3 on l2
        if l3_protocol not in ALLOWED_L3_PROTOCOL:
            raise ValueError('l3_protocol must be one of the following: {0}'.format(ALLOWED_L3_PROTOCOL))
        pkt_bld.set_layer_attr('l2', 'type', ALLOWED_L3_PROTOCOL[l3_protocol])

        # set l3 attributes
        if l3_protocol == 'ipv4':
                    #('v_hl', 'B', (4 << 4) | (20 >> 2)),
                    #('tos', 'B', 0),
                    #('len', 'H', 20),
                    #('id', 'H', 0),
                    #('off', 'H', 0),
                    #('ttl', 'B', 64),
                    #('p', 'B', 0),
                    #('sum', 'H', 0),
                    #('src', '4s', '\x00' * 4),
                    #('dst', '4s', '\x00' * 4)
            pkt_bld.add_pkt_layer('l3', dpkt.ip.IP())
            pkt_bld.set_ip_layer_addr('l3', 'src', ip_src_addr)
            pkt_bld.set_ip_layer_addr('l3', 'dst', ip_dst_addr)
            pkt_bld.set_layer_attr('l3', 'len', l3_length)
        else:
            raise NotImplementedError("l3_protocol '{0}' is not supported by TRex yet.".format(l3_protocol))

        # set l4 on l3
        if l4_protocol not in ALLOWED_L4_PROTOCOL:
            raise ValueError('l4_protocol must be one of the following: {0}'.format(ALLOWED_L3_PROTOCOL))
        pkt_bld.set_layer_attr('l3', 'p', ALLOWED_L4_PROTOCOL[l4_protocol])

        # set l4 attributes
        if l4_protocol == 'tcp':
            pkt_bld.add_pkt_layer('l4', dpkt.tcp.TCP())
                    #('sport', 'H', 0xdead),
                    #('dport', 'H', 0),
                    #('seq', 'I', 0xdeadbeefL),
                    #('ack', 'I', 0),
                    #('off_x2', 'B', ((5 << 4) | 0)),
                    #('flags', 'B', TH_SYN),
                    #('win', 'H', TCP_WIN_MAX),
                    #('sum', 'H', 0),
                    #('urp', 'H', 0)
            #pkt_bld.set_ip_layer_addr('l4', 'sport', ip_src_addr)
            #pkt_bld.set_ip_layer_addr('l4', 'dport', ip_dst_addr)
        else:
            raise NotImplementedError("l4_protocol '{0}' is not supported by TRex yet.".format(l3_protocol))

        pkt_bld.set_pkt_payload('Hello, World' + '!'*58)

        # debug
        #pkt_bld.dump_pkt_to_pcap('stream_test.pcap')
        return pkt_bld



if __name__ == '__main__':
    pass

