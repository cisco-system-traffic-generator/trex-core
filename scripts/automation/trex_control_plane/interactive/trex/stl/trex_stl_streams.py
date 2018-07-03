#!/router/bin/python

from collections import OrderedDict, namedtuple

from scapy.utils import ltoa
from scapy.error import Scapy_Exception
import random
import base64
import string
import traceback
import copy
import imp


from ..common.trex_exceptions import *
from ..common.trex_types import verify_exclusive_arg, validate_type
from ..utils.text_opts import format_num

from .trex_stl_packet_builder_interface import CTrexPktBuilderInterface
from .trex_stl_packet_builder_scapy import *


# base class for TX mode
class STLTXMode(object):
    """ mode rate speed """

    def __init__ (self, pps = None, bps_L1 = None, bps_L2 = None, percentage = None):
        """ 
        Speed can be given in packets per second (pps), L2/L1 bps, or port percent 
        Use only one unit. 
        you can enter pps =10000 oe bps_L1=10

        :parameters:
            pps : float 
               Packets per second 

            bps_L1 : float
               Bits per second L1 (with IPG) 

            bps_L2 : float 
               Bits per second L2 (Ethernet-FCS) 

            percentage : float 
               Link interface percent (0-100). Example: 10 is 10% of the port link setup 

        .. code-block:: python

            # STLTXMode Example

            mode = STLTXCont(pps = 10)

            mode = STLTXCont(bps_L1 = 10000000) #10mbps L1

            mode = STLTXCont(bps_L2 = 10000000) #10mbps L2

            mode = STLTXCont(percentage = 10)   #10%

        """

        args = [pps, bps_L1, bps_L2, percentage]

        # default
        if all([x is None for x in args]):
            pps = 1.0
        else:
            verify_exclusive_arg(args)

        self.fields = {'rate': {}}

        if pps is not None:
            validate_type('pps', pps, [float, int])

            self.fields['rate']['type']  = 'pps'
            self.fields['rate']['value'] = pps

        elif bps_L1 is not None:
            validate_type('bps_L1', bps_L1, [float, int])

            self.fields['rate']['type']  = 'bps_L1'
            self.fields['rate']['value'] = bps_L1

        elif bps_L2 is not None:
            validate_type('bps_L2', bps_L2, [float, int])

            self.fields['rate']['type']  = 'bps_L2'
            self.fields['rate']['value'] = bps_L2

        elif percentage is not None:
            validate_type('percentage', percentage, [float, int])
            if not (percentage > 0 and percentage <= 100):
                raise TRexArgumentError('percentage', percentage)

            self.fields['rate']['type']  = 'percentage'
            self.fields['rate']['value'] = percentage

        

    def to_json (self):
        return dict(self.fields)


    @staticmethod
    def from_json (json_data):
        try:
            mode = json_data['mode']
            rate = mode['rate']
            
            # check the rate type
            if rate['type'] not in ['pps', 'bps_L1', 'bps_L2', 'percentage']:
                raise TRexError("from_json: invalid rate type '{0}'".format(rate['type']))
            
            # construct the pair
            kwargs = {rate['type'] : rate['value']}
            
            if mode['type'] == 'single_burst':
                return STLTXSingleBurst(total_pkts = mode['total_pkts'], **kwargs)
                
            elif mode['type'] == 'multi_burst':
                return STLTXMultiBurst(pkts_per_burst = mode['pkts_per_burst'],
                                       ibg            = mode['ibg'],
                                       count          = mode['count'],
                                       **kwargs)
                
            elif mode['type'] == 'continuous':
                return STLTXCont(**kwargs)
                
            else:
                raise TRexError("from_json: unknown mode type '{0}'".format(mode['type']))
                
                
        except KeyError as e:
            raise TRexError("from_json: missing field {0} from JSON".format(e))
            
        
        
# continuous mode
class STLTXCont(STLTXMode):
    """ Continuous mode """

    def __init__ (self, **kwargs):
        """ 
        Continuous mode
        
         see :class:`trex.stl.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python

            # STLTXCont Example

            mode = STLTXCont(pps = 10)

        """
        super(STLTXCont, self).__init__(**kwargs)


        self.fields['type'] = 'continuous'

    @staticmethod
    def __str__ ():
        return "Continuous"

# single burst mode
class STLTXSingleBurst(STLTXMode):
    """ Single burst mode """

    def __init__ (self, total_pkts = 1, **kwargs):
        """ 
        Single burst mode

            :parameters:
                 total_pkts : int
                    Number of packets for this burst 

         see :class:`trex.stl.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python

            # STLTXSingleBurst Example

            mode = STLTXSingleBurst( pps = 10, total_pkts = 1)

        """


        if not isinstance(total_pkts, int):
            raise TRexArgumentError('total_pkts', total_pkts)

        super(STLTXSingleBurst, self).__init__(**kwargs)

        self.fields['type'] = 'single_burst'
        self.fields['total_pkts'] = total_pkts

    @staticmethod
    def __str__ ():
        return "Single Burst"

# multi burst mode
class STLTXMultiBurst(STLTXMode):
    """ Multi-burst mode """

    def __init__ (self,
                  pkts_per_burst = 1,
                  ibg = 0.0,   # usec not SEC
                  count = 1,
                  **kwargs):
        """ 
        Multi-burst mode

        :parameters:

             pkts_per_burst: int 
                Number of packets per burst 

              ibg : float 
                Inter-burst gap in usec 1,000,000.0 is 1 sec

              count : int 
                Number of bursts 

         see :class:`trex.stl.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python

            # STLTXMultiBurst Example

            mode = STLTXMultiBurst(pps = 10, pkts_per_burst = 1,count 10, ibg=10.0)

        """


        if not isinstance(pkts_per_burst, int):
            raise TRexArgumentError('pkts_per_burst', pkts_per_burst)

        if not isinstance(ibg, (int, float)):
            raise TRexArgumentError('ibg', ibg)

        if not isinstance(count, int):
            raise TRexArgumentError('count', count)

        super(STLTXMultiBurst, self).__init__(**kwargs)

        self.fields['type'] = 'multi_burst'
        self.fields['pkts_per_burst'] = pkts_per_burst
        self.fields['ibg'] = ibg
        self.fields['count'] = count

    @staticmethod
    def __str__ ():
        return "Multi Burst"

STLStreamDstMAC_CFG_FILE=0
STLStreamDstMAC_PKT     =1
STLStreamDstMAC_ARP     =2

class STLFlowStatsInterface(object):
    def __init__ (self, pg_id):
        self.fields = {}
        self.fields['enabled']         = True
        self.fields['stream_id']       = pg_id

    def to_json (self):
        """ Dump as json"""
        return dict(self.fields)


    @staticmethod
    def from_json (json_data):
        '''
        create the object from JSON output
        '''
        
        try:
            # no flow stats
            if not json_data['enabled']:
                return None
                
            # flow stats
            if json_data['rule_type'] == 'stats':
                return STLFlowStats(pg_id = json_data['stream_id'])
                
            # latency
            elif json_data['rule_type'] == 'latency':
                return STLFlowLatencyStats(pg_id = json_data['stream_id'])
                
            else:
                raise TRexError("from_json: invalid flow stats type {0}".format(json_data['rule_type']))
                
                
        except KeyError as e:
            raise TRexError("from_json: missing field {0} from JSON".format(e))

        
        
    @staticmethod
    def defaults ():
        return {'enabled' : False}


class STLFlowStats(STLFlowStatsInterface):
    """ Define per stream basic stats

    .. code-block:: python

        # STLFlowStats Example

        flow_stats = STLFlowStats(pg_id = 7)

    """

    def __init__(self, pg_id):
        super(STLFlowStats, self).__init__(pg_id)
        self.fields['rule_type'] = 'stats'


class STLFlowLatencyStats(STLFlowStatsInterface):
    """ Define per stream basic stats + latency, jitter, packet reorder/loss

    .. code-block:: python

        # STLFlowLatencyStats Example

        flow_stats = STLFlowLatencyStats(pg_id = 7)

    """

    def __init__(self, pg_id):
        super(STLFlowLatencyStats, self).__init__(pg_id)
        self.fields['rule_type'] = 'latency'


class STLStream(object):
    """ One stream object. Includes mode, Field Engine mode packet template and Rx stats  

        .. code-block:: python

            # STLStream Example


            base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
            pad = max(0, size - len(base_pkt)) * 'x'

            STLStream( isg = 10.0, # star in delay 
                       name    ='S0',
                       packet = STLPktBuilder(pkt = base_pkt/pad),
                       mode = STLTXSingleBurst( pps = 10, total_pkts = 1),
                       next = 'S1'), # point to next stream 


    """

    def __init__ (self,
                  name = None,
                  packet = None,
                  mode = STLTXCont(pps = 1),
                  enabled = True,
                  self_start = True,
                  isg = 0.0,
                  flow_stats = None,
                  next = None,
                  stream_id = None,
                  action_count = 0,
                  random_seed =0,
                  mac_src_override_by_pkt = None,
                  mac_dst_override_mode = None,    #see  STLStreamDstMAC_xx
                  dummy_stream = False,
                  start_paused = False,
                  ):
        """ 
        Stream object 

        :parameters:

                  name  : string 
                       Name of the stream. Required if this stream is dependent on another stream, and another stream needs to refer to this stream by name.

                  packet :  STLPktBuilder see :class:`trex.stl.trex_stl_packet_builder_scapy.STLPktBuilder`
                       Template packet and field engine program. Example: packet = STLPktBuilder(pkt = base_pkt/pad) 

                  mode :  :class:`trex.stl.trex_stl_streams.STLTXCont` or :class:`trex.stl.trex_stl_streams.STLTXSingleBurst`  or  :class:`trex.stl.trex_stl_streams.STLTXMultiBurst` 

                  enabled : bool 
                      Indicates whether the stream is enabled. 

                  self_start : bool 
                      If False, another stream activates it.

                  isg : float 
                     Inter-stream gap in usec. Time to wait until the stream sends the first packet.

                  flow_stats : :class:`trex.stl.trex_stl_streams.STLFlowStats` 
                      Per stream statistic object. See: STLFlowStats

                  next : string 
                      Name of the stream to activate.
                    
                  stream_id :
                        For use by HLTAPI.

                  action_count : uint16_t
                        If there is a next stream, number of loops before stopping. Default: 0 (unlimited).

                  random_seed: uint16_t 
                       If given, the seed for this stream will be this value. Useful if you need a deterministic random value.
                        
                  mac_src_override_by_pkt : bool 
                        Template packet sets src MAC. 

                  mac_dst_override_mode=None : STLStreamDstMAC_xx
                        Template packet sets dst MAC.

                  dummy_stream : bool
                        For delay purposes, will not be sent.

                  start_paused : bool
                        Experimental flag, might be removed in future!
                        Stream will not be transmitted until un-paused.
        """


        # type checking
        validate_type('name', name, (type(None), int, basestring))
        validate_type('next', next, (type(None), int, basestring))
        validate_type('mode', mode, STLTXMode)
        validate_type('packet', packet, (type(None), CTrexPktBuilderInterface))
        validate_type('flow_stats', flow_stats, (type(None), STLFlowStatsInterface))
        validate_type('enabled', enabled, bool)
        validate_type('self_start', self_start, bool)
        validate_type('isg', isg, (int, float))
        validate_type('stream_id', stream_id, (type(None), int))
        validate_type('random_seed',random_seed,int);
        validate_type('dummy_stream', dummy_stream, bool);
        validate_type('start_paused', start_paused, bool);

        if (type(mode) == STLTXCont) and (next != None):
            raise TRexError("Continuous stream cannot have a next stream ID")

        # tag for the stream and next - can be anything
        self.name = name
        self.next = next

        self.mac_src_override_by_pkt = mac_src_override_by_pkt # save for easy construct code from stream object
        self.mac_dst_override_mode = mac_dst_override_mode
        self.id = stream_id

        # set externally
        self.fields = {}

        int_mac_src_override_by_pkt = 0;
        int_mac_dst_override_mode   = 0;


        if mac_src_override_by_pkt == None:
            int_mac_src_override_by_pkt=0
            if packet :
                if packet.is_default_src_mac ()==False:
                    int_mac_src_override_by_pkt=1

        else:
            int_mac_src_override_by_pkt = int(mac_src_override_by_pkt);

        if mac_dst_override_mode == None:
            int_mac_dst_override_mode   = 0;
            if packet :
                if packet.is_default_dst_mac ()==False:
                    int_mac_dst_override_mode=STLStreamDstMAC_PKT
        else:
            int_mac_dst_override_mode = int(mac_dst_override_mode);


        self.is_default_mac = not (int_mac_src_override_by_pkt or int_mac_dst_override_mode)

        self.fields['flags'] = (int_mac_src_override_by_pkt&1) +  ((int_mac_dst_override_mode&3)<<1) + (int(dummy_stream) << 3)

        self.fields['action_count'] = action_count

        # basic fields
        self.fields['enabled'] = enabled
        self.fields['self_start'] = self_start
        self.fields['start_paused'] = start_paused
        self.fields['isg'] = isg

        if random_seed !=0 :
            self.fields['random_seed'] = random_seed # optional

        # mode
        self.fields['mode'] = mode.to_json()
        self.mode_desc      = str(mode)


        if not packet:
            packet = STLPktBuilder(pkt = Ether()/IP())

        self.scapy_pkt_builder = packet
        # packet builder
        packet.compile()

        # packet and VM
        pkt_json = packet.to_json()
        self.fields['packet'] = pkt_json['packet']
        self.fields['vm']     = pkt_json['vm']
        
        self.pkt = base64.b64decode(self.fields['packet']['binary'])

        # this is heavy, calculate lazy
        self.packet_desc = None

        if not flow_stats:
            self.fields['flow_stats'] = STLFlowStats.defaults()
        else:
            self.fields['flow_stats'] = flow_stats.to_json()


    def __str__ (self):
        s =  "Stream Name: {0}\n".format(self.name)
        s += "Stream Next: {0}\n".format(self.next)
        s += "Stream JSON:\n{0}\n".format(json.dumps(self.fields, indent = 4, separators=(',', ': '), sort_keys = True))
        return s


    def get_id (self):
        """ Get the stream id after resolution  """
        return self.id

    def has_custom_mac_addr (self):
        """ Return True if src or dst MAC were set as custom """
        return not self.is_default_mac

    def get_name (self):
        """ Get the stream name """
        return self.name

    def get_next (self):
        """ Get next stream object """
        return self.next


    def has_flow_stats (self):
        """ Return True if stream was configured with flow stats """
        return self.fields['flow_stats']['enabled']

    def get_pg_id (self):
        """ Returns packet group ID if exists """
        return self.fields['flow_stats'].get('stream_id')
        
    def get_flow_stats_type (self):
        """ Returns flow stats type if exists """
        return self.fields['flow_stats'].get('rule_type')
        
        
    def get_pkt (self):
        """ Get packet as string """
        return self.pkt

    def get_pkt_len (self, count_crc = True):
        """ Get packet number of bytes  """
        pkt_len = len(self.get_pkt())
        if count_crc:
            pkt_len += 4

        return pkt_len

    def is_dummy (self):
        """ return true if stream is marked as dummy stream """
        return ( (self.fields['flags'] & 0x8) == 0x8 )
        
    def get_pkt_type (self):
        """ Get packet description. Example: IP:UDP """
        if self.is_dummy():
            return '-'
        elif self.packet_desc == None:
            self.packet_desc = STLPktBuilder.pkt_layers_desc_from_buffer(self.get_pkt())

        return self.packet_desc

    def get_mode (self):
        return 'delay' if self.is_dummy() else self.mode_desc
        

    @staticmethod
    def get_rate_from_field (rate_json):
        """ Get rate from json  """
        t = rate_json['type']
        v = rate_json['value']

        if t == "pps":
            return format_num(v, suffix = "pps")
        elif t == "bps_L1":
            return format_num(v, suffix = "bps (L1)")
        elif t == "bps_L2":
            return format_num(v, suffix = "bps (L2)")
        elif t == "percentage":
            return format_num(v, suffix = "%")

    def get_rate (self):
        return self.get_rate_from_field(self.fields['mode']['rate'])

    def to_pkt_dump (self):
        """ Print packet description from Scapy  """
        if self.name:
            print("Stream Name: ",self.name)
        scapy_b = self.scapy_pkt_builder;
        if scapy_b and isinstance(scapy_b,STLPktBuilder):
            scapy_b.to_pkt_dump()
        else:
            print("Nothing to dump")

    # return True if FE variable is being written only to IP src or dst, to show its value as IP
    @staticmethod
    def __is_all_IP(vm_var_usage_list):
        for offsets_tuple in vm_var_usage_list:
            if type(offsets_tuple) is not tuple:
                return False
            if offsets_tuple[0] != 'IP' or offsets_tuple[2] not in ('src', 'dst'):
                return False
        return True

    # replace offset number by user-friendly string 'IP.src' etc.
    @staticmethod
    def __fix_offset_by_name(pkt, inst, name):
        if name in inst:
            ret = pkt.get_field_by_offset(inst[name])
            if ret:
                if inst['type'] in ('fix_checksum_ipv4', 'fix_checksum_hw'): # do not include field name
                    if ret[1] == 0: # layer index is redundant
                        inst[name] = "'%s'" % ret[0]
                    else: 
                        inst[name] = "'%s:%s'" % ret[0:2]
                else:
                    if ret[1] == 0:
                        inst[name] = "'%s.%s'" % (ret[0], ret[2])
                    else:
                        inst[name] = "'%s:%s.%s'" % ret[0:3]


    # returns the Python code (text) to build this stream, inside the code it will be in variable "stream"
    def to_code(self):
        """ Convert to Python code as profile  """
        layer = Ether(self.pkt)
        pkt = CTRexScapyPktUtl(layer)

        vm_var_usage = {}
        for inst in self.fields['vm']['instructions']:
            if inst['type'] == 'trim_pkt_size':
                fv_name = inst['name']
                if fv_name in vm_var_usage:
                    vm_var_usage[fv_name].append('trim')
                else:
                    vm_var_usage[fv_name] = ['trim']

            if 'pkt_offset' in inst:
                fv_name = inst.get('fv_name', inst.get('name'))
                if fv_name in vm_var_usage:
                    vm_var_usage[fv_name].append(pkt.get_field_by_offset(inst['pkt_offset']))
                else:
                    vm_var_usage[fv_name] = [pkt.get_field_by_offset(inst['pkt_offset'])]

        vm_list = ['vm = STLVM()']
        for inst in self.fields['vm']['instructions']:
            inst = dict(inst)
            #print inst
            self.__fix_offset_by_name(pkt, inst, 'pkt_offset')
            if 'is_big_endian' in inst:
                inst['byte_order'] = "'big'" if inst['is_big_endian'] else "'little'"
            if inst['type'] == 'flow_var':
                if inst['name'] in vm_var_usage and inst['size'] == 4 and self.__is_all_IP(vm_var_usage[inst['name']]):
                    inst['init_value'] = "'%s'" % ltoa(inst['init_value'])
                    inst['min_value'] = "'%s'" % ltoa(inst['min_value'])
                    inst['max_value'] = "'%s'" % ltoa(inst['max_value'])
                vm_list.append("vm.var(name='{name}', size={size}, op='{op}', init_value={init_value}, min_value={min_value}, max_value={max_value}, step={step})".format(**inst))
            elif inst['type'] == 'write_flow_var':
                vm_list.append("vm.write(fv_name='{name}', pkt_offset={pkt_offset}, add_val={add_value}, byte_order={byte_order})".format(**inst))
            elif inst['type'] == 'write_mask_flow_var':
                inst['mask'] = hex(inst['mask'])
                vm_list.append("vm.write_mask(fv_name='{name}', pkt_offset={pkt_offset}, pkt_cast_size={pkt_cast_size}, mask={mask}, shift={shift}, add_val={add_value}, byte_order={byte_order})".format(**inst))
            elif inst['type'] == 'fix_checksum_ipv4':
                vm_list.append("vm.fix_chksum(offset={pkt_offset})".format(**inst))
            elif inst['type'] == 'fix_checksum_hw':
                inst['l3_offset'] = inst['l2_len']
                inst['l4_offset'] = inst['l2_len'] + inst['l3_len']
                self.__fix_offset_by_name(pkt, inst, 'l3_offset')
                self.__fix_offset_by_name(pkt, inst, 'l4_offset')
                vm_list.append("vm.fix_chksum_hw(l3_offset={l3_offset}, l4_offset={l4_offset}, l4_type={l4_type})".format(**inst))
            elif inst['type'] == 'trim_pkt_size':
                vm_list.append("vm.trim(fv_name='{name}')".format(**inst))
            elif inst['type'] == 'tuple_flow_var':
                inst['ip_min'] = ltoa(inst['ip_min'])
                inst['ip_max'] = ltoa(inst['ip_max'])
                vm_list.append("vm.tuple_var(name='{name}', ip_min='{ip_min}', ip_max='{ip_max}', port_min={port_min}, port_max={port_max}, limit_flows={limit_flows}, flags={flags})".format(**inst))
            elif inst['type'] == 'flow_var_rand_limit':
                vm_list.append("vm.repeatable_random_var(fv_name='{name}', size={size}, limit={limit}, seed={seed}, min_value={min_value}, max_value={max_value})".format(**inst))
            else:
                raise TRexError('Got unhandled FE instruction type: %s' % inst['type'])
        if 'cache' in self.fields['vm']:
            vm_list.append('vm.set_cached(%s)' % self.fields['vm']['cache'])

        vm_code = '\n'.join(vm_list)

        stream_params_list = []
        stream_params_list.append('packet = STLPktBuilder(pkt = packet, vm = vm)')
        if default_STLStream.name != self.name:
            stream_params_list.append('name = %s' % STLStream.__add_quotes(self.name))
        if default_STLStream.fields['enabled'] != self.fields['enabled']:
            stream_params_list.append('enabled = %s' % self.fields['enabled'])
        if default_STLStream.fields['self_start'] != self.fields['self_start']:
            stream_params_list.append('self_start = %s' % self.fields['self_start'])
        if default_STLStream.fields['start_paused'] != self.fields['start_paused']:
            stream_params_list.append('start_paused = %s' % self.fields['start_paused'])
        if default_STLStream.fields['isg'] != self.fields['isg']:
            stream_params_list.append('isg = %s' % self.fields['isg'])
        if default_STLStream.fields['flow_stats'] != self.fields['flow_stats']:
            if 'rule_type' in self.fields['flow_stats']:
                stream_params_list.append('flow_stats = %s(%s)' % ('STLFlowStats' if self.fields['flow_stats']['rule_type'] == 'stats' else 'STLFlowLatencyStats', self.fields['flow_stats']['stream_id']))
        if default_STLStream.next != self.next:
            stream_params_list.append('next = %s' % STLStream.__add_quotes(self.next))
        if default_STLStream.id != self.id:
            stream_params_list.append('stream_id = %s' % self.id)
        if default_STLStream.fields['action_count'] != self.fields['action_count']:
            stream_params_list.append('action_count = %s' % self.fields['action_count'])
        if 'random_seed' in self.fields:
            stream_params_list.append('random_seed = %s' % self.fields.get('random_seed', 0))
        stream_params_list.append('mac_src_override_by_pkt = %s' % bool(self.fields['flags'] & 1))
        stream_params_list.append('mac_dst_override_mode = %s' % (self.fields['flags'] >> 1 & 3))
        if self.is_dummy():
            stream_params_list.append('dummy_stream = True')

        mode_args = ''
        for key, value in self.fields['mode'].items():
            if key not in ('rate', 'type'):
                mode_args += '%s = %s, ' % (key, value)
        mode_args += '%s = %s' % (self.fields['mode']['rate']['type'], self.fields['mode']['rate']['value'])
        if self.mode_desc == STLTXCont.__str__():
            stream_params_list.append('mode = STLTXCont(%s)' % mode_args)
        elif self.mode_desc == STLTXSingleBurst().__str__():
            stream_params_list.append('mode = STLTXSingleBurst(%s)' % mode_args)
        elif self.mode_desc == STLTXMultiBurst().__str__():
            stream_params_list.append('mode = STLTXMultiBurst(%s)' % mode_args)
        else:
            raise TRexError('Could not determine mode: %s' % self.mode_desc)

        stream = "stream = STLStream(" + ',\n                   '.join(stream_params_list) + ')'

        layer.hide_defaults()          # remove fields with default values
        imports_arr = []
        layers_commands = []
        # remove checksums, add imports if needed
        while layer:
            layer_class = layer.__class__.__name__
            if layer_class not in vars(scapy.layers.all): # custom import
                found_import = False
                for module_path, module in sys.modules.items():
                    if not module_path.startswith(('scapy.layers', 'scapy.contrib')):
                        continue
                    import_string = 'from %s import %s' % (module_path, layer_class)
                    if import_string in imports_arr: # already present in extra imports
                        found_import = True
                        break
                    if hasattr(module, layer_class): # add as extra import
                        imports_arr.append(import_string)
                        found_import = True
                        break
                if not found_import:
                    raise TRexError('Could not determine import of layer %s' % layer.name)

            payload = layer.payload
            layer.remove_payload()

            if isinstance(layer, Raw):
                payload_data = bytes(layer)
                if payload_data == payload_data[0:1] * len(payload_data): # compact form Raw('x' * 100) etc.
                    layer_command = '%s * %s)' % (Raw(payload_data[0:1]).command().rstrip(')'), len(payload_data))
                else:
                    layer_command = layer.command()
                layers_commands.append(layer_command)
            else:
                layers_commands.append(layer.command())

            layer = payload

        imports = '\n'.join(imports_arr)
        packet_code = 'packet = (' + (' / \n          ').join(layers_commands) + ')'

        if imports:
            return '\n'.join([imports, packet_code, vm_code, stream])
        return '\n'.join([packet_code, vm_code, stream])

    # add quoted for string, or leave as is if other type
    @staticmethod
    def __add_quotes(arg):
        if type(arg) is str:
            return "'%s'" % arg
        return arg

    # used to replace non-printable characters with hex
    @staticmethod
    def __replchars_to_hex(match):
        return r'\x{0:02x}'.format(ord(match.group()))

        

    def to_json (self):
        """ convert stream object to JSON """
        json_data = dict(self.fields)

        # required fields for 'from_json' - send it to the server
        if self.name:
            json_data['name'] = self.name 

        if self.next:
            json_data['next'] = self.next

        return json_data


    @staticmethod
    def from_json (json_data):
        
        # packet builder
        builder = STLPktBuilder.from_json(json_data)
        mode    = STLTXMode.from_json(json_data)
        
        # flow stats / latency
        fs = STLFlowStatsInterface.from_json(json_data['flow_stats'])
        
        try:
            return STLStream(name                     = json_data.get('name'),
                             next                     = json_data.get('next'),
                             packet                   = builder,
                             mode                     = mode,
                             flow_stats               = fs,
                             enabled                  = json_data['enabled'],
                             self_start               = json_data['self_start'],
                             isg                      = json_data['isg'],
                             action_count             = json_data['action_count'],
                             
                             stream_id                = json_data.get('stream_id'),
                             random_seed              = json_data.get('random_seed', 0),
                             
                             mac_src_override_by_pkt  = (json_data['flags'] & 0x1) == 0x1,
                             mac_dst_override_mode    = (json_data['flags'] >> 1 & 0x3),
                             dummy_stream             = (json_data['flags'] & 0x8) == 0x8,
                             start_paused             = json_data.get('start_paused', False))
            
        except KeyError as e:
            raise TRexError("from_json: missing field {0} from JSON".format(e))
            
        
    def clone (self):
        return STLStream.from_json(self.to_json())

        
# profile class
class STLProfile(object):
    """ Describe a list of streams   

        .. code-block:: python

            # STLProfile Example

            profile =  STLProfile( [ STLStream( isg = 10.0, # star in delay 
                                        name    ='S0',
                                        packet = STLPktBuilder(pkt = base_pkt/pad),
                                        mode = STLTXSingleBurst( pps = 10, total_pkts = self.burst_size),
                                        next = 'S1'), # point to next stream 

                             STLStream( self_start = False, # stream is  disabled enable trow S0
                                        name    ='S1',
                                        packet  = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode    = STLTXSingleBurst( pps = 10, total_pkts = self.burst_size),
                                        next    = 'S2' ),

                             STLStream(  self_start = False, # stream is  disabled enable trow S0
                                         name   ='S2',
                                         packet = STLPktBuilder(pkt = base_pkt2/pad),
                                         mode = STLTXSingleBurst( pps = 10, total_pkts = self.burst_size )
                                        )
                            ]).get_streams()



    """

    def __init__ (self, streams = None):
        """

            :parameters:

                  streams  : list of :class:`trex.stl.trex_stl_streams.STLStream` 
                       a list of stream objects  

        """


        if streams == None:
            streams = []

        if not type(streams) == list:
            streams = [streams]

        if not all([isinstance(stream, STLStream) for stream in streams]):
            raise TRexArgumentError('streams', streams, valid_values = STLStream)

        self.streams = streams
        self.meta = None


    def get_streams (self):
        """ Get the list of streams"""
        return self.streams

    def __str__ (self):
        return '\n'.join([str(stream) for stream in self.streams])

    def is_pauseable (self):
        return all([x.get_mode() == "Continuous" for x in self.get_streams()])

    def has_custom_mac_addr (self):
        return any([x.has_custom_mac_addr() for x in self.get_streams()])

    def has_flow_stats (self):
        return any([x.has_flow_stats() for x in self.get_streams()])

        
  
    @staticmethod
    def __flatten_json (stream_list):
        # GUI provides a YAML/JSON from the RPC capture - flatten it to match
        if not isinstance(stream_list, list):
            return
        
        for stream in stream_list:
            if 'stream' in stream:
                d = stream['stream']
                del stream['stream']
                stream.update(d)
        
                        
    @staticmethod
    def __load_plain (plain_file, fmt):
        """
            Load (from JSON / YAML file) a profile with a number of streams
            'fmt' can be either 'json' or 'yaml'
        """

        # check filename
        if not os.path.isfile(plain_file):
            raise TRexError("file '{0}' does not exists".format(plain_file))

        # read the content
        with open(plain_file) as f:
            try:
                data = json.load(f) if fmt == 'json' else yaml.load(f)
                STLProfile.__flatten_json(data)
                
            except (ValueError, yaml.parser.ParserError):
                raise TRexError("file '{0}' is not a valid {1} formatted file".format(plain_file, 'JSON' if fmt == 'json' else 'YAML'))
            
        return STLProfile.from_json(data)

                    
        
    @staticmethod
    def load_yaml (yaml_file):
        """ Load (from YAML file) a profile with a number of streams """
        return STLProfile.__load_plain(yaml_file, fmt = 'yaml')
    
        
    @staticmethod
    def load_json (json_file):
        """ Load (from JSON file) a profile with a number of streams """
        return STLProfile.__load_plain(json_file, fmt = 'json')

        
    @staticmethod
    def get_module_tunables(module):
        # remove self and variables
        func = module.register().get_streams
        argc = func.__code__.co_argcount
        tunables = func.__code__.co_varnames[1:argc]

        # fetch defaults
        defaults = func.__defaults__
        if defaults is None:
            return {}
        if len(defaults) != (argc - 1):
            raise TRexError("Module should provide default values for all arguments on get_streams()")

        output = {}
        for t, d in zip(tunables, defaults):
            output[t] = d

        return output


    @staticmethod
    def load_py (python_file, direction = 0, port_id = 0, **kwargs):
        """ Load from Python profile """

        # check filename
        if not os.path.isfile(python_file):
            raise TRexError("File '{0}' does not exist".format(python_file))

        basedir = os.path.dirname(python_file)
        sys.path.insert(0, basedir)

        try:
            file    = os.path.basename(python_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module) # reload the update 

            t = STLProfile.get_module_tunables(module)
            #for arg in kwargs:
            #    if not arg in t:
            #        raise TRexError("Profile {0} does not support tunable '{1}' - supported tunables are: '{2}'".format(python_file, arg, t))

            streams = module.register().get_streams(direction = direction,
                                                    port_id = port_id,
                                                    **kwargs)
            profile = STLProfile(streams)

            profile.meta = {'type': 'python',
                            'tunables': t}

            return profile

        except Exception as e:
            a, b, tb = sys.exc_info()
            x =''.join(traceback.format_list(traceback.extract_tb(tb)[1:])) + a.__name__ + ": " + str(b) + "\n"

            summary = "\nPython Traceback follows:\n\n" + x
            raise TRexError(summary)


        finally:
            sys.path.remove(basedir)

    
    # loop_count = 0 means loop forever
    @staticmethod
    def load_pcap (pcap_file,
                   ipg_usec = None,
                   speedup = 1.0,
                   loop_count = 1,
                   vm = None,
                   packet_hook = None,
                   split_mode = None,
                   min_ipg_usec = None):
        """ Convert a pcap file with a number of packets to a list of connected streams.  

        packet1->packet2->packet3 etc 

                :parameters:

                  pcap_file  : string 
                       Name of the pcap file 

                  ipg_usec   : float
                       Inter packet gap in usec. If IPG is None, IPG is taken from pcap file

                  speedup   : float 
                       When reading the pcap file, divide IPG by this "speedup" factor. Resulting IPG is sped up by this factor. 

                  loop_count : uint16_t 
                       Number of loops to repeat the pcap file 
                    
                  vm        :  list 
                        List of Field engine instructions 

                  packet_hook : Callable or function
                        will be applied to every packet

                  split_mode : str
                        should this PCAP be split to two profiles based on IPs / MACs
                        used for dual mode
                        can be 'MAC' or 'IP'

                  min_ipg_usec   : float
                       Minumum inter packet gap in usec. Used to guard from too small IPGs.

                 :return: STLProfile

        """

        if speedup <= 0:
            raise TRexError('Speedup should be positive.')
        if min_ipg_usec and min_ipg_usec < 0:
            raise TRexError('min_ipg_usec should not be negative.')


        # make sure IPG is not less than 0.001 usec
        if (ipg_usec is not None and (ipg_usec < 0.001 * speedup) and
                              (min_ipg_usec is None or min_ipg_usec < 0.001)):
            raise TRexError("ipg_usec cannot be less than 0.001 usec: '{0}'".format(ipg_usec))

        if loop_count < 0:
            raise TRexError("'loop_count' cannot be negative")

       
        try:
            if split_mode is None:
                pkts = PCAPReader(pcap_file).read_all(ipg_usec, min_ipg_usec, speedup)
                if len(pkts) == 0:
                    raise TRexError("'{0}' does not contain any packets".format(pcap_file))
                    
                return STLProfile.__pkts_to_streams(pkts,
                                                    loop_count,
                                                    vm,
                                                    packet_hook)
            else:
                pkts_a, pkts_b = PCAPReader(pcap_file).read_all(ipg_usec, min_ipg_usec, speedup, split_mode = split_mode)
                if not (pkts_a or pkts_b):
                    raise TRexError("'%s' does not contain any packets." % pcap_file)
                elif not (pkts_a and pkts_b):
                    raise TRexError("'%s' contains only one direction." % pcap_file)

                # swap is ts of first packet in b is earlier
                start_time_a = pkts_a[0][1]
                start_time_b = pkts_b[0][1]
                if start_time_b < start_time_a:
                    pkts_a, pkts_b = pkts_b, pkts_a

                # get last ts
                end_time_a = pkts_a[-1][1]
                end_time_b = pkts_b[-1][1]
                start_delay_usec = 1000
                if ipg_usec:
                    start_delay_usec = ipg_usec / speedup
                if min_ipg_usec and min_ipg_usec > start_delay_usec:
                    start_delay_usec = min_ipg_usec
                end_time = max(end_time_a, end_time_b)

                profile_a = STLProfile.__pkts_to_streams(pkts_a,
                                                         loop_count,
                                                         vm,
                                                         packet_hook,
                                                         start_delay_usec,
                                                         end_delay_usec = end_time - end_time_a)

                profile_b = STLProfile.__pkts_to_streams(pkts_b,
                                                         loop_count,
                                                         vm,
                                                         packet_hook,
                                                         start_delay_usec,
                                                         end_delay_usec = end_time - end_time_b)
                    
                return profile_a, profile_b


        except Scapy_Exception as e:
            raise TRexError("failed to open PCAP file {0}: '{1}'".format(pcap_file, str(e)))


    @staticmethod
    def __pkts_to_streams (pkts, loop_count, vm, packet_hook, start_delay_usec = 0, end_delay_usec = 0):
        streams = []
        if packet_hook:
            pkts = [(packet_hook(cap), meta) for (cap, meta) in pkts]

        last_ts = 0
        for i, (cap, ts) in enumerate(pkts, start = 1):
            isg = ts - last_ts
            last_ts = ts

            # handle last packet
            if i == len(pkts):
                if end_delay_usec:
                    next = 'delay_stream'
                    action_count = 0
                    streams.append(STLStream(name = 'delay_stream',
                                             mode = STLTXSingleBurst(total_pkts = 1, percentage = 100),
                                             self_start = False,
                                             isg = end_delay_usec,
                                             action_count = loop_count,
                                             dummy_stream = True,
                                             next = 1))
                else:
                    next = 1
                    action_count = loop_count
            else:
                next = i + 1
                action_count = 0

            if i == 1:
                streams.append(STLStream(name = 1,
                                         packet = STLPktBuilder(pkt_buffer = cap, vm = vm),
                                         mode = STLTXSingleBurst(total_pkts = 1, percentage = 100),
                                         self_start = True,
                                         isg = isg + start_delay_usec,  # usec
                                         action_count = action_count,
                                         next = next))
            else:
                streams.append(STLStream(name = i,
                                         packet = STLPktBuilder(pkt_buffer = cap, vm = vm),
                                         mode = STLTXSingleBurst(total_pkts = 1, percentage = 100),
                                         self_start = False,
                                         isg = isg,  # usec
                                         action_count = action_count,
                                         next = next))


        profile = STLProfile(streams)
        profile.meta = {'type': 'pcap'}

        return profile

      

    @staticmethod
    def load (filename, direction = 0, port_id = 0, **kwargs):
        """ Load a profile by its type. Supported types are: 
           * py
           * json
           * pcap file that converted to profile automaticly 

           :Parameters:
              filename  : string as filename 
              direction : profile's direction (if supported by the profile)
              port_id   : which port ID this profile is being loaded to
              kwargs    : forward those key-value pairs to the profile

        """

        x = os.path.basename(filename).split('.')
        suffix = x[1] if (len(x) == 2) else None

        if suffix == 'py':
            profile = STLProfile.load_py(filename, direction, port_id, **kwargs)

        elif suffix == 'json':
            profile = STLProfile.load_json(filename)

        elif suffix == 'yaml':
            profile = STLProfile.load_yaml(filename)
            
        elif suffix in ['cap', 'pcap']:
            profile = STLProfile.load_pcap(filename, speedup = 1, ipg_usec = 1e6)

        else:
            raise TRexError("unknown profile file type: '{0}'".format(suffix))

        profile.meta['stream_count'] = len(profile.get_streams()) if isinstance(profile.get_streams(), list) else 1
        return profile

    @staticmethod
    def get_info (filename):
        profile = STLProfile.load(filename)
        return profile.meta

    def dump_as_pkt (self):
        """ Dump the profile as Scapy packet. If the packet is raw, convert it to Scapy before dumping it."""
        cnt=0;
        for stream in self.streams:
            print("=======================")
            print("Stream %d" % cnt)
            print("=======================")
            cnt = cnt +1 
            stream.to_pkt_dump()

            
    def to_json (self):
        """ convert profile to JSON object """
        return [s.to_json() for s in self.get_streams()]
        
        
    @staticmethod
    def from_json (json_data):
        """ create profile object from JSON object """
        
        if not isinstance(json_data, list):
            raise TRexError("JSON should contain a list of streams")
                    
        streams = [STLStream.from_json(stream_json) for stream_json in json_data]

        profile = STLProfile(streams)
        profile.meta = {'type': 'json'}
        
        return profile
        
        
    
    def dump_to_code (self, profile_file = None):
        """ Convert the profile to Python native profile. """
        profile_dump = '''# !!! Auto-generated code !!!
from trex.stl.api import *

class STLS1(object):
    def get_streams(self, direction = 0, **kwargs):
        streams = []
'''
        for stream in self.streams:
            profile_dump += ' '*8 + stream.to_code().replace('\n', '\n' + ' '*8) + '\n'
            profile_dump += ' '*8 + 'streams.append(stream)\n'
        profile_dump += '''
        return streams

def register():
    return STLS1()
'''
        # write to file if provided
        if profile_file:
            with open(profile_file, 'w') as f:
                f.write(profile_dump)

        return profile_dump



    def __len__ (self):
        return len(self.streams)


class PCAPReader(object):
    def __init__(self, pcap_file):
        if not os.path.isfile(pcap_file):
            raise TRexError("File '{0}' does not exist.".format(pcap_file))
        self.pcap_file = pcap_file

    def read_all(self, ipg_usec, min_ipg_usec, speedup, split_mode = None):
        # get the packets
        if split_mode is None:
            pkts = RawPcapReader(self.pcap_file).read_all()
        else:
            pkts = rdpcap(self.pcap_file)

        if not pkts:
            raise TRexError("'%s' does not contain any packets." % self.pcap_file)

        self.pkts_arr = []
        last_ts = 0
        # fix times
        for pkt in pkts:
            if split_mode is None:
                pkt_data, meta = pkt
                ts_usec = meta[0] * 1e6 + meta[1]
            else:
                pkt_data = pkt
                ts_usec = pkt.time * 1e6

            if ipg_usec is None:
                if 'prev_time' in locals():
                    delta_usec = (ts_usec - prev_time) / float(speedup)
                else:
                    delta_usec = 0
                if min_ipg_usec and delta_usec < min_ipg_usec:
                    delta_usec = min_ipg_usec
                prev_time = ts_usec
                last_ts += delta_usec
            else: # user specified ipg
                if min_ipg_usec:
                    last_ts += min_ipg_usec
                elif ipg_usec:
                    last_ts += ipg_usec / float(speedup)
                else:
                    raise TRexError('Please specify either min_ipg_usec or ipg_usec, not both.')
            self.pkts_arr.append([pkt_data, last_ts])

        if split_mode is None:
            return self.pkts_arr

        # we need to split
        self.graph = Graph()

        self.pkt_groups = [ [], [] ]

        if split_mode == 'MAC':
            self.generate_mac_groups()
        elif split_mode == 'IP':
            self.generate_ip_groups()
        else:
            raise TRexError('unknown split mode for PCAP')

        return self.pkt_groups


    # generate two groups based on MACs
    def generate_mac_groups (self):
        for i, (pkt, _) in enumerate(self.pkts_arr):
            if not isinstance(pkt, (Ether, Dot3)):
                raise TRexError("Packet #{0} has an unknown L2 format: {1}".format(i, type(pkt)))
            self.graph.add(pkt.src, pkt.dst)

        # split the graph to two groups
        mac_groups = self.graph.split()

        for pkt, ts in self.pkts_arr:
            group = 1 if pkt.src in mac_groups[1] else 0
            self.pkt_groups[group].append((bytes(pkt), ts))


    # generate two groups based on IPs
    def generate_ip_groups (self):
        for i, (pkt, t) in enumerate(self.pkts_arr):
            if not isinstance(pkt, (Ether, Dot3) ):
                raise TRexError("Packet #{0} has an unknown L2 format: {1}".format(i, type(pkt)))
            ip = pkt.getlayer('IP')
            if not ip:
                ip = pkt.getlayer('IPv6')
            if not ip:
                continue
            self.graph.add(ip.src, ip.dst)

        # split the graph to two groups
        ip_groups = self.graph.split()

        for pkt, ts in self.pkts_arr:
            ip = pkt.getlayer('IP')
            if not ip:
                ip = pkt.getlayer('IPv6')
            group = 0
            if ip and ip.src in ip_groups[1]:
                group = 1
            self.pkt_groups[group].append((bytes(pkt), ts))



# a simple graph object - used to split to two groups
class Graph(object):
    def __init__ (self):
        self.db = OrderedDict()
        self.debug = False

    def log (self, msg):
        if self.debug:
            print(msg)

    # add a connection v1 --> v2
    def add (self, v1, v2):
        # init value for v1
        if not v1 in self.db:
            self.db[v1] = set()

        # init value for v2
        if not v2 in self.db:
            self.db[v2] = set()

        # ignore self to self edges
        if v1 == v2:
            return

        # undirected - add two ways
        self.db[v1].add(v2)
        self.db[v2].add(v1)


    # create a 2-color of the graph if possible
    def split (self):
        color_a = set()
        color_b = set()

        # start with all
        nodes = list(self.db.keys())

        # process one by one
        while len(nodes) > 0:
            node = nodes.pop(0)

            friends = self.db[node]

            # node has never been seen - move to color_a
            if not node in color_a and not node in color_b:
                self.log("<NEW> {0} --> A".format(node))
                color_a.add(node)

            # node color
            node_color, other_color = (color_a, color_b) if node in color_a else (color_b, color_a)

            # check that the coloring is possible
            bad_friends = friends.intersection(node_color)
            if bad_friends:
                raise TRexError("ERROR: failed to split PCAP file - {0} and {1} are in the same group".format(node, bad_friends))

            # add all the friends to the other color
            for friend in friends:
                self.log("<FRIEND> {0} --> {1}".format(friend, 'A' if other_color is color_a else 'B'))
                other_color.add(friend)


        return color_a, color_b


default_STLStream = STLStream()

