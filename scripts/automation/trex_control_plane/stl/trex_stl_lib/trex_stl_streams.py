#!/router/bin/python

from .trex_stl_exceptions import *
from .trex_stl_types import verify_exclusive_arg, validate_type
from .trex_stl_packet_builder_interface import CTrexPktBuilderInterface
from .trex_stl_packet_builder_scapy import STLPktBuilder, Ether, IP, UDP, TCP, RawPcapReader
from collections import OrderedDict, namedtuple

from scapy.utils import ltoa
from scapy.error import Scapy_Exception
import random
import yaml
import base64
import string
import traceback
import copy
import imp

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
            :caption: STLTXMode Example

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
                raise STLArgumentError('percentage', percentage)

            self.fields['rate']['type']  = 'percentage'
            self.fields['rate']['value'] = percentage

        

    def to_json (self):
        return self.fields


# continuous mode
class STLTXCont(STLTXMode):
    """ Continuous mode """

    def __init__ (self, **kwargs):
        """ 
        Continuous mode
        
         see :class:`trex_stl_lib.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python
            :caption: STLTXCont Example

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

         see :class:`trex_stl_lib.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python
            :caption: STLTXSingleBurst Example

                   mode = STLTXSingleBurst( pps = 10, total_pkts = 1)

        """


        if not isinstance(total_pkts, int):
            raise STLArgumentError('total_pkts', total_pkts)

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

         see :class:`trex_stl_lib.trex_stl_streams.STLTXMode` for rate 

        .. code-block:: python
            :caption: STLTXMultiBurst Example

                   mode = STLTXMultiBurst(pps = 10, pkts_per_burst = 1,count 10, ibg=10.0)

        """


        if not isinstance(pkts_per_burst, int):
            raise STLArgumentError('pkts_per_burst', pkts_per_burst)

        if not isinstance(ibg, (int, float)):
            raise STLArgumentError('ibg', ibg)

        if not isinstance(count, int):
            raise STLArgumentError('count', count)

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

# RX stats class
class STLFlowStats(object):
    """ Define per stream stats  

        .. code-block:: python
            :caption: STLFlowStats Example

            flow_stats = STLFlowStats(pg_id = 7)

    """

    def __init__ (self, pg_id):
        self.fields = {}

        self.fields['enabled']         = True
        self.fields['stream_id']       = pg_id
        self.fields['seq_enabled']     = False
        self.fields['latency_enabled'] = False


    def to_json (self):
        """ Dump as json"""
        return dict(self.fields)

    @staticmethod
    def defaults ():
        return {'enabled' : False}

class STLStream(object):
    """ One stream object. Includes mode, Field Engine mode packet template and Rx stats  

        .. code-block:: python
            :caption: STLStream Example


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
                  mac_src_override_by_pkt=None,
                  mac_dst_override_mode=None    #see  STLStreamDstMAC_xx
                  ):
        """ 
        Stream object 

        :parameters:

                  name  : string 
                       Name of the stream. Required if this stream is dependent on another stream, and another stream needs to refer to this stream by name.

                  packet :  STLPktBuilder see :class:`trex_stl_lib.trex_stl_packet_builder_scapy.STLPktBuilder`
                       Template packet and field engine program. Example: packet = STLPktBuilder(pkt = base_pkt/pad) 

                  mode :  :class:`trex_stl_lib.trex_stl_streams.STLTXCont` or :class:`trex_stl_lib.trex_stl_streams.STLTXSingleBurst`  or  :class:`trex_stl_lib.trex_stl_streams.STLTXMultiBurst` 

                  enabled : bool 
                      Indicates whether the stream is enabled. 

                  self_start : bool 
                      If False, another stream activates it.

                  isg : float 
                     Inter-stream gap in usec. Time to wait until the stream sends the first packet.

                  flow_stats : :class:`trex_stl_lib.trex_stl_streams.STLFlowStats` 
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
        """


        # type checking
        validate_type('mode', mode, STLTXMode)
        validate_type('packet', packet, (type(None), CTrexPktBuilderInterface))
        validate_type('enabled', enabled, bool)
        validate_type('self_start', self_start, bool)
        validate_type('isg', isg, (int, float))
        validate_type('stream_id', stream_id, (type(None), int))
        validate_type('random_seed',random_seed,int);

        if (type(mode) == STLTXCont) and (next != None):
            raise STLError("Continuous stream cannot have a next stream ID")

        # tag for the stream and next - can be anything
        self.name = name
        self.next = next

        self.mac_src_override_by_pkt = mac_src_override_by_pkt # save for easy construct code from stream object
        self.mac_dst_override_mode = mac_dst_override_mode
        self.id = stream_id


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

        self.fields['flags'] = (int_mac_src_override_by_pkt&1) +  ((int_mac_dst_override_mode&3)<<1)

        self.fields['action_count'] = action_count

        # basic fields
        self.fields['enabled'] = enabled
        self.fields['self_start'] = self_start
        self.fields['isg'] = isg

        if random_seed !=0 :
            self.fields['random_seed'] = random_seed # optional

        # mode
        self.fields['mode'] = mode.to_json()
        self.mode_desc      = str(mode)


        # packet
        self.fields['packet'] = {}
        self.fields['vm'] = {}

        if not packet:
            packet = STLPktBuilder(pkt = Ether()/IP())

        self.scapy_pkt_builder = packet
        # packet builder
        packet.compile()

        # packet and VM
        self.fields['packet'] = packet.dump_pkt()
        self.fields['vm']     = packet.get_vm_data()

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

    def to_json (self):
        """ 
        Return json format
        """
        return dict(self.fields)

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

    def get_pkt (self):
        """ Get packet as string """
        return self.pkt

    def get_pkt_len (self, count_crc = True):
       """ Get packet number of bytes  """
       pkt_len = len(self.get_pkt())
       if count_crc:
           pkt_len += 4

       return pkt_len


    def get_pkt_type (self):
        """ Get packet description. Example: IP:UDP """
        if self.packet_desc == None:
            self.packet_desc = STLPktBuilder.pkt_layers_desc_from_buffer(self.get_pkt())

        return self.packet_desc

    def get_mode (self):
        return self.mode_desc

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



    def to_yaml (self):
        """ Convert to YAML  """
        y = {}

        if self.name:
            y['name'] = self.name

        if self.next:
            y['next'] = self.next

        y['stream'] = copy.deepcopy(self.fields)
        
        # some shortcuts for YAML
        rate_type  = self.fields['mode']['rate']['type']
        rate_value = self.fields['mode']['rate']['value']

        y['stream']['mode'][rate_type] = rate_value
        del y['stream']['mode']['rate']

        return y

    # returns the Python code (text) to build this stream, inside the code it will be in variable "stream"
    def to_code (self):
        """ Convert to Python code as profile  """
        packet = Ether(self.pkt)
        layer = packet
        while layer:                    # remove checksums
            for chksum_name in ('cksum', 'chksum'):
                if chksum_name in layer.fields:
                    del layer.fields[chksum_name]
            layer = layer.payload
        packet.hide_defaults()          # remove fields with default values
        payload = packet.getlayer('Raw')
        packet_command = packet.command()
        imports_arr = []
        if 'MPLS(' in packet_command:
            imports_arr.append('from scapy.contrib.mpls import MPLS')
            
        imports = '\n'.join(imports_arr)
        if payload:
            payload.remove_payload() # fcs etc.
            data = payload.fields.get('load', '')

            good_printable = [c for c in string.printable if ord(c) not in range(32)]
            good_printable.remove("'")

            if type(data) is str:
                new_data = ''.join([c if c in good_printable else r'\x{0:02x}'.format(ord(c)) for c in data])
            else:
                new_data = ''.join([chr(c) if chr(c) in good_printable else r'\x{0:02x}'.format(c) for c in data])

            payload_start = packet_command.find("Raw(load=")
            if payload_start != -1:
                packet_command = packet_command[:payload_start-1]
        layers = packet_command.split('/')

        if payload:
            if len(new_data) and new_data == new_data[0] * len(new_data):
                layers.append("Raw(load='%s' * %s)" % (new_data[0], len(new_data)))
            else:
                layers.append("Raw(load='%s')" % new_data)

        packet_code = 'packet = (' + (' / \n          ').join(layers) + ')'
        vm_list = []
        for inst in self.fields['vm']['instructions']:
            if inst['type'] == 'flow_var':
                vm_list.append("STLVmFlowVar(name='{name}', size={size}, op='{op}', init_value={init_value}, min_value={min_value}, max_value={max_value}, step={step})".format(**inst))
            elif inst['type'] == 'write_flow_var':
                vm_list.append("STLVmWrFlowVar(fv_name='{name}', pkt_offset={pkt_offset}, add_val={add_value}, is_big={is_big_endian})".format(**inst))
            elif inst['type'] == 'write_mask_flow_var':
                inst = copy.copy(inst)
                inst['mask'] = hex(inst['mask'])
                vm_list.append("STLVmWrMaskFlowVar(fv_name='{name}', pkt_offset={pkt_offset}, pkt_cast_size={pkt_cast_size}, mask={mask}, shift={shift}, add_value={add_value}, is_big={is_big_endian})".format(**inst))
            elif inst['type'] == 'fix_checksum_ipv4':
                vm_list.append("STLVmFixIpv4(offset={pkt_offset})".format(**inst))
            elif inst['type'] == 'trim_pkt_size':
                vm_list.append("STLVmTrimPktSize(fv_name='{name}')".format(**inst))
            elif inst['type'] == 'tuple_flow_var':
                inst = copy.copy(inst)
                inst['ip_min'] = ltoa(inst['ip_min'])
                inst['ip_max'] = ltoa(inst['ip_max'])
                vm_list.append("STLVmTupleGen(name='{name}', ip_min='{ip_min}', ip_max='{ip_max}', port_min={port_min}, port_max={port_max}, limit_flows={limit_flows}, flags={flags})".format(**inst))
        vm_code = 'vm = STLScVmRaw([' + ',\n                 '.join(vm_list) + '], split_by_field = %s)' % STLStream.__add_quotes(self.fields['vm'].get('split_by_var'))
        stream_params_list = []
        stream_params_list.append('packet = STLPktBuilder(pkt = packet, vm = vm)')
        if default_STLStream.name != self.name:
            stream_params_list.append('name = %s' % STLStream.__add_quotes(self.name))
        if default_STLStream.fields['enabled'] != self.fields['enabled']:
            stream_params_list.append('enabled = %s' % self.fields['enabled'])
        if default_STLStream.fields['self_start'] != self.fields['self_start']:
            stream_params_list.append('self_start = %s' % self.fields['self_start'])
        if default_STLStream.fields['isg'] != self.fields['isg']:
            stream_params_list.append('isg = %s' % self.fields['isg'])
        if default_STLStream.fields['flow_stats'] != self.fields['flow_stats']:
            stream_params_list.append('flow_stats = STLFlowStats(%s)' % self.fields['flow_stats']['stream_id'])
        if default_STLStream.next != self.next:
            stream_params_list.append('next = %s' % STLStream.__add_quotes(self.next))
        if default_STLStream.id != self.id:
            stream_params_list.append('stream_id = %s' % self.id)
        if default_STLStream.fields['action_count'] != self.fields['action_count']:
            stream_params_list.append('action_count = %s' % self.fields['action_count'])
        if 'random_seed' in self.fields:
            stream_params_list.append('random_seed = %s' % self.fields.get('random_seed', 0))
        if default_STLStream.mac_src_override_by_pkt != self.mac_src_override_by_pkt:
            stream_params_list.append('mac_src_override_by_pkt = %s' % self.mac_src_override_by_pkt)
        if default_STLStream.mac_dst_override_mode != self.mac_dst_override_mode:
            stream_params_list.append('mac_dst_override_mode = %s' % self.mac_dst_override_mode)

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
            raise STLError('Could not determine mode: %s' % self.mode_desc)

        stream = "stream = STLStream(" + ',\n                   '.join(stream_params_list) + ')'
        return '\n'.join([imports, packet_code, vm_code, stream])

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

    def dump_to_yaml (self, yaml_file = None):
        """ Print as yaml  """
        yaml_dump = yaml.dump([self.to_yaml()], default_flow_style = False)

        # write to file if provided
        if yaml_file:
            with open(yaml_file, 'w') as f:
                f.write(yaml_dump)

        return yaml_dump

class YAMLLoader(object):

    def __init__ (self, yaml_file):
        self.yaml_path = os.path.dirname(yaml_file)
        self.yaml_file = yaml_file


    def __parse_packet (self, packet_dict):

        packet_type = set(packet_dict).intersection(['binary', 'pcap'])
        if len(packet_type) != 1:
            raise STLError("Packet section must contain either 'binary' or 'pcap'")

        if 'binary' in packet_type:
            try:
                pkt_str = base64.b64decode(packet_dict['binary'])
            except TypeError:
                raise STLError("'binary' field is not a valid packet format")

            builder = STLPktBuilder(pkt_buffer = pkt_str)

        elif 'pcap' in packet_type:
            pcap = os.path.join(self.yaml_path, packet_dict['pcap'])

            if not os.path.exists(pcap):
                raise STLError("'pcap' - cannot find '{0}'".format(pcap))

            builder = STLPktBuilder(pkt = pcap)

        return builder


    def __parse_mode (self, mode_obj):
        if not mode_obj:
            return None

        rate_parser = set(mode_obj).intersection(['pps', 'bps_L1', 'bps_L2', 'percentage'])
        if len(rate_parser) != 1:
            raise STLError("'rate' must contain exactly one from 'pps', 'bps_L1', 'bps_L2', 'percentage'")

        rate_type  = rate_parser.pop()
        rate = {rate_type : mode_obj[rate_type]}

        mode_type = mode_obj.get('type')

        if mode_type == 'continuous':
            mode = STLTXCont(**rate)

        elif mode_type == 'single_burst':
            defaults = STLTXSingleBurst()
            mode = STLTXSingleBurst(total_pkts  = mode_obj.get('total_pkts', defaults.fields['total_pkts']),
                                    **rate)

        elif mode_type == 'multi_burst':
            defaults = STLTXMultiBurst()
            mode = STLTXMultiBurst(pkts_per_burst = mode_obj.get('pkts_per_burst', defaults.fields['pkts_per_burst']),
                                   ibg            = mode_obj.get('ibg', defaults.fields['ibg']),
                                   count          = mode_obj.get('count', defaults.fields['count']),
                                   **rate)

        else:
            raise STLError("mode type can be 'continuous', 'single_burst' or 'multi_burst")


        return mode



    def __parse_flow_stats (self, flow_stats_obj):

        # no such object
        if not flow_stats_obj or flow_stats_obj.get('enabled') == False:
            return None

        pg_id = flow_stats_obj.get('stream_id') 
        if pg_id == None:
            raise STLError("Enabled RX stats section must contain 'stream_id' field")

        return STLFlowStats(pg_id = pg_id)


    def __parse_stream (self, yaml_object):
        s_obj = yaml_object['stream']

        # parse packet
        packet = s_obj.get('packet')
        if not packet:
            raise STLError("YAML file must contain 'packet' field")

        builder = self.__parse_packet(packet)


        # mode
        mode = self.__parse_mode(s_obj.get('mode'))

        # rx stats
        flow_stats = self.__parse_flow_stats(s_obj.get('flow_stats'))
        

        defaults = default_STLStream
        # create the stream
        stream = STLStream(name       = yaml_object.get('name'),
                           packet     = builder,
                           mode       = mode,
                           flow_stats   = flow_stats,
                           enabled    = s_obj.get('enabled', defaults.fields['enabled']),
                           self_start = s_obj.get('self_start', defaults.fields['self_start']),
                           isg        = s_obj.get('isg', defaults.fields['isg']),
                           next       = yaml_object.get('next'),
                           action_count = s_obj.get('action_count', defaults.fields['action_count']),
                           mac_src_override_by_pkt = s_obj.get('mac_src_override_by_pkt', 0),
                           mac_dst_override_mode = s_obj.get('mac_src_override_by_pkt', 0) 
                           )

        # hack the VM fields for now
        if 'vm' in s_obj:
            stream.fields['vm'].update(s_obj['vm'])

        return stream


    def parse (self):
        with open(self.yaml_file, 'r') as f:
            # read YAML and pass it down to stream object
            yaml_str = f.read()

            try:
                objects = yaml.load(yaml_str)
            except yaml.parser.ParserError as e:
                raise STLError(str(e))

            streams = [self.__parse_stream(object) for object in objects]
            
            return streams


# profile class
class STLProfile(object):
    """ Describe a list of streams   

        .. code-block:: python
            :caption: STLProfile Example

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

                  streams  : list of :class:`trex_stl_lib.trex_stl_streams.STLStream` 
                       a list of stream objects  

        """


        if streams == None:
            streams = []

        if not type(streams) == list:
            streams = [streams]

        if not all([isinstance(stream, STLStream) for stream in streams]):
            raise STLArgumentError('streams', streams, valid_values = STLStream)

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
    def load_yaml (yaml_file):
        """ Load (from YAML file) a profile with a number of streams"""

        # check filename
        if not os.path.isfile(yaml_file):
            raise STLError("file '{0}' does not exists".format(yaml_file))

        yaml_loader = YAMLLoader(yaml_file)
        streams = yaml_loader.parse()

        profile = STLProfile(streams)
        profile.meta = {'type': 'yaml'}

        return profile

    @staticmethod
    def get_module_tunables(module):
        # remove self and variables
        func = module.register().get_streams
        argc = func.__code__.co_argcount
        tunables = func.__code__.co_varnames[1:argc]

        # fetch defaults
        defaults = func.__defaults__
        if len(defaults) != (argc - 1):
            raise STLError("Module should provide default values for all arguments on get_streams()")

        output = {}
        for t, d in zip(tunables, defaults):
            output[t] = d

        return output


    @staticmethod
    def load_py (python_file, direction = 0, port_id = 0, **kwargs):
        """ Load from Python profile """

        # check filename
        if not os.path.isfile(python_file):
            raise STLError("File '{0}' does not exist".format(python_file))

        basedir = os.path.dirname(python_file)
        sys.path.append(basedir)

        try:
            file    = os.path.basename(python_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module) # reload the update 

            t = STLProfile.get_module_tunables(module)
            for arg in kwargs:
                if not arg in t:
                    raise STLError("Profile {0} does not support tunable '{1}' - supported tunables are: '{2}'".format(python_file, arg, t))

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
            raise STLError(summary)


        finally:
            sys.path.remove(basedir)

    
    # loop_count = 0 means loop forever
    @staticmethod
    def load_pcap (pcap_file, ipg_usec = None, speedup = 1.0, loop_count = 1, vm = None):
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

                 :return: STLProfile

        """

        # check filename
        if not os.path.isfile(pcap_file):
            raise STLError("file '{0}' does not exists".format(pcap_file))

        # make sure IPG is not less than 1 usec
        if ipg_usec is not None and ipg_usec < 0.001:
            raise STLError("ipg_usec cannot be less than 0.001 usec: '{0}'".format(ipg_usec))

        if loop_count < 0:
            raise STLError("'loop_count' cannot be negative")

        streams = []
        last_ts_usec = 0

        try:
            pkts = RawPcapReader(pcap_file).read_all()
        except Scapy_Exception as e:
            raise STLError("failed to open PCAP file '{0}'".format(pcap_file))

        
        for i, (cap, meta) in enumerate(pkts, start = 1):
            # IPG - if not provided, take from cap
            if ipg_usec == None:
                ts_usec = (meta[0] * 1e6 + meta[1]) / float(speedup)
            else:
                ts_usec = (ipg_usec * i) / float(speedup)

            # handle last packet
            if i == len(pkts):
                next = 1
                action_count = loop_count
            else:
                next = i + 1
                action_count = 0

            streams.append(STLStream(name = i,
                                     packet = STLPktBuilder(pkt_buffer = cap, vm = vm),
                                     mode = STLTXSingleBurst(total_pkts = 1, percentage = 100),
                                     self_start = True if (i == 1) else False,
                                     isg = (ts_usec - last_ts_usec),  # seconds to usec
                                     action_count = action_count,
                                     next = next))
        
            last_ts_usec = ts_usec

        
        profile = STLProfile(streams)
        profile.meta = {'type': 'pcap'}

        return profile

      

    @staticmethod
    def load (filename, direction = 0, port_id = 0, **kwargs):
        """ Load a profile by its type. Supported types are: 
           * py
           * yaml 
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

        elif suffix == 'yaml':
            profile = STLProfile.load_yaml(filename)

        elif suffix in ['cap', 'pcap']:
            profile = STLProfile.load_pcap(filename, speedup = 1, ipg_usec = 1e6)

        else:
            raise STLError("unknown profile file type: '{0}'".format(suffix))

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

    def dump_to_yaml (self, yaml_file = None):
        """ Convert the profile to yaml """
        yaml_list = [stream.to_yaml() for stream in self.streams]
        yaml_str = yaml.dump(yaml_list, default_flow_style = False)

        # write to file if provided
        if yaml_file:
            with open(yaml_file, 'w') as f:
                f.write(yaml_str)

        return yaml_str

    def dump_to_code (self, profile_file = None):
        """ Convert the profile to Python native profile. """
        profile_dump = '''# !!! Auto-generated code !!!
from trex_stl_lib.api import *

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

default_STLStream = STLStream()
