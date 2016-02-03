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
    mac_src2
    mac_dst
    mac_dst2
    l3_protocol
    ip_tos_field
    l3_length
    ip_id
    ip_fragment_offset
    ip_ttl
    ip_checksum
    ip_src_addr
    ip_dst_addr
    l4_protocol
    tcp_src_port
    tcp_dst_port
    tcp_seq_num
    tcp_ack_num
    tcp_data_offset
    tcp_fin_flag
    tcp_syn_flag
    tcp_rst_flag
    tcp_psh_flag
    tcp_ack_flag
    tcp_urg_flag
    tcp_window
    tcp_checksum
    tcp_urgent_ptr

traffic_control()
    action              ( run | stop )
    port_handle

traffic_stats()
    mode                ( aggregate )
    port_handle

'''



import trex_root_path
import client_utils.scapy_packet_builder as pkt_bld
from trex_stateless_client import STLClient
from common.trex_streams import *
from client_utils.general_utils import get_integer
import dpkt
import socket
from misc_methods import print_r
import traceback
import time

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

    def __init__(self, verbose = 0):
        self.trex_client = None
        self.connected = False
        self.verbose = verbose
        self._hlt_streams_history = {} # streams per stream_id in format of HLT arguments for modify later


###########################
#    Session functions    #
###########################

    # device: ip or hostname
    def connect(self, device, port_list, username = '', reset = False, break_locks = False):

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
                try:
                    res1 = self.traffic_config(mode, port_handle[0], **kwargs)
                    res2 = self.traffic_config(mode, port_handle[1],
                            mac_src = kwargs.get(mac_src2, '00-00-01-00-00-01'),
                            mac_dst = kwargs.get(mac_dst2, '00-00-00-00-00-00'),
                            ip_src_addr = kwargs.get(ip_dst_addr, '192.0.0.1'),
                            ip_dst_addr = kwargs.get(ip_src_addr, '0.0.0.0'),
                            ipv6_src_addr = kwargs.get(ipv6_dst_addr, 'fe80:0:0:0:0:0:0:22'),
                            ipv6_dst_addr = kwargs.get(ipv6_src_addr, 'fe80:0:0:0:0:0:0:12'),
                            **kwargs)
                except Exception as e:
                    return HLT_ERR('Could not generate bidirectional traffic: %s' % e)
                return HLT_OK(stream_id = {port_handle[0]: res1['stream_id'], port_handle[1]: res2['stream_id']})

            try:
                stream_obj = CTRexHltApi._generate_stream(**kwargs)
            except Exception as e:
                return HLT_ERR('Could not create stream: %s' % e)
            stream_id = stream_obj.get_id()

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
    def _generate_stream(**kwargs):
        try:
            packet = CTRexHltApi._generate_packet(**kwargs)
        except Exception as e:
            raise Exception('Could not generate packet: %s' % e)

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
                raise Exception('transmit_mode %s not supported/implemented')
        except Exception as e:
            raise Exception('Could not create transmit_mode class %s: %s' % (transmit_mode, e))

        try:
            stream_obj = STLStream(packet = packet,
                                   #enabled = True,
                                   #self_start = True,
                                   mode = transmit_mode_class,
                                   #rx_stats = rx_stats,
                                   #next_stream_id = -1
                                   )
        except Exception as e:
            raise Exception('Could not create stream: %s' % e)

        debug_filename = kwargs.get('save_to_yaml')
        if type(debug_filename) is str:
            stream_obj.dump_to_yaml(debug_filename, stream_obj)
        return stream_obj

    @staticmethod
    def _generate_packet(
                # L2
                frame_size = 64,
                l2_encap = 'ethernet_ii',
                mac_src = '00:00:01:00:00:01',
                mac_dst = '00:00:00:00:00:00',

                # L3 IPv4
                l3_protocol = 'ipv4',
                ip_tos_field = 0,
                l3_length = 50,
                ip_id = 0,
                ip_fragment_offset = 0,
                ip_ttl = 64,
                ip_checksum = None,
                ip_src_addr = '0.0.0.0',
                ip_dst_addr = '192.0.0.1',

                # L3 IPv4 FE
                ip_src_mode = 'fixed',
                ip_src_step = 1,
                ip_src_count = 1,
                
                l4_protocol = 'tcp',
                tcp_src_port = 1024,
                tcp_dst_port = 80,
                tcp_seq_num = 1,
                tcp_ack_num = 1,
                tcp_data_offset = 1,
                tcp_fin_flag = 0,
                tcp_syn_flag = 0,
                tcp_rst_flag = 0,
                tcp_psh_flag = 0,
                tcp_ack_flag = 0,
                tcp_urg_flag = 0,
                tcp_window = 4069,
                tcp_checksum = None,
                tcp_urgent_ptr = 0,
                **kwargs):

        pkt_plus_vm = pkt_bld.CScapyTRexPktBuilder()

        ### L2 ###
        if l2_encap == 'ethernet_ii':
                    #fields_desc = [ MACField("dst","00:00:00:01:00:00"),
                    #                MACField("src","00:00:00:02:00:00"), 
                    #                XShortEnumField("type", 0x9000, ETHER_TYPES) ]
            pkt = pkt_bld.Ether(src = mac_src, dst = mac_dst)
        else:
            raise NotImplementedError("l2_encap does not support the desired encapsulation '%s'" % l2_encap)

        ### L3 ###
        if l3_protocol == 'ipv4':
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
            pkt /= pkt_bld.IP(tos = ip_tos_field,
                              len = l3_length,
                              id = ip_id,
                              frag = ip_fragment_offset,
                              ttl = ip_ttl,
                              chksum = ip_checksum,
                              src = ip_src_addr,
                              dst = ip_dst_addr
                              )
            # IP FE (Field Engine)
            print dir(pkt_bld.IP.src.fld)
            print_r(pkt_bld.IP.src.fld)
            if ip_src_mode == 'increment':
                ip_src_vm = pkt_bld.CTRexScRaw([ pkt_bld.CTRexVmDescFlowVar(name='a', min_value='16.0.0.1', max_value='16.0.0.10', init_value='16.0.0.1', size=4, op='inc'),
                          pkt_bld.CTRexVmDescWrFlowVar (fv_name='a', pkt_offset = 'IP.src'),
                          pkt_bld.CTRexVmDescFixIpv4(offset = 'IP')])
                pkt_plus_vm.add_command(ip_src_vm)

                #print '>> is inc'
                #if ip_src_step != 1:
                #    return HLT_ERR('ip_src_step has to be 1 (TRex limitation)')
                #FE_var_ip_src_addr = pkt_bld.vm.CTRexVMFlowVariable('ip_src_addr')
                #FE_var_ip_src_addr.set_field('size', 4)
                #FE_var_ip_src_addr.set_field('operation', 'inc')
                #FE_var_ip_src_addr.set_field('init_value', ip_src_addr)
                #FE_var_ip_src_addr.set_field('init_value', 1)
                #FE_var_ip_src_addr.set_field('max_value', ip_src_count)
                #pkt_bld.vm.vm_var.set_field('split_by_core', False)
        else:
            raise NotImplementedError("l3_protocol '%s' is not supported by TRex yet." % l3_protocol)

        ### L4 ###
        if l4_protocol == 'tcp':
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
            tcp_flags = ('F' if tcp_fin_flag else '' +
                         'S' if tcp_syn_flag else '' +
                         'R' if tcp_rst_flag else '' +
                         'P' if tcp_psh_flag else '' +
                         'A' if tcp_ack_flag else '' +
                         'U' if tcp_urg_flag else '')

            pkt /= pkt_bld.TCP(sport = tcp_src_port,
                               dport = tcp_dst_port,
                               seq = tcp_seq_num,
                               ack = tcp_ack_num,
                               dataofs = tcp_data_offset,
                               flags = tcp_flags,
                               window = tcp_window,
                               chksum = tcp_checksum,
                               urgptr = tcp_urgent_ptr,
                               )
        else:
            raise NotImplementedError("l4_protocol '%s' is not supported by TRex yet." % l3_protocol)
        pkt /= 'payload'
        pkt_plus_vm.set_packet(pkt)
        #payload = frame_size - pkt
        #payload = pkt_bld.payload_gen.gen_repeat_ptrn('Hello, World!')
        #pkt_bld.set_pkt_payload(payload)
            
        #print pkt_bld.vm
        # debug (only base packet, without FE)
        debug_filename = kwargs.get('save_to_pcap')
        if type(debug_filename) is str:
            pkt_plus_vm.dump_pkt_to_pcap(debug_filename)
        pkt_plus_vm.compile()
        return pkt_plus_vm



if __name__ == '__main__':
    pass

