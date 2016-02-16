#!/router/bin/python

from trex_stl_exceptions import *
from trex_stl_packet_builder_interface import CTrexPktBuilderInterface
from trex_stl_packet_builder_scapy import CScapyTRexPktBuilder, Ether, IP, RawPcapReader
from collections import OrderedDict, namedtuple

from dpkt import pcap
import random
import yaml
import base64
import string
import traceback

def random_name (l):
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(l))


# base class for TX mode
class STLTXMode(object):
    def __init__ (self):
        self.fields = {}

    def to_json (self):
        return self.fields


# continuous mode
class STLTXCont(STLTXMode):

    def __init__ (self, pps = 1):

        if not isinstance(pps, (int, float)):
            raise STLArgumentError('pps', pps)

        super(STLTXCont, self).__init__()

        self.fields['type'] = 'continuous'
        self.fields['pps']  = pps

    def __str__ (self):
        return "Continuous"

# single burst mode
class STLTXSingleBurst(STLTXMode):

    def __init__ (self, pps = 1, total_pkts = 1):

        if not isinstance(pps, (int, float)):
            raise STLArgumentError('pps', pps)

        if not isinstance(total_pkts, int):
            raise STLArgumentError('total_pkts', total_pkts)

        super(STLTXSingleBurst, self).__init__()

        self.fields['type'] = 'single_burst'
        self.fields['pps']  = pps
        self.fields['total_pkts'] = total_pkts

    def __str__ (self):
        return "Single Burst"

# multi burst mode
class STLTXMultiBurst(STLTXMode):

    def __init__ (self,
                  pps = 1,
                  pkts_per_burst = 1,
                  ibg = 0.0,   # usec not SEC
                  count = 1):

        if not isinstance(pps, (int, float)):
            raise STLArgumentError('pps', pps)

        if not isinstance(pkts_per_burst, int):
            raise STLArgumentError('pkts_per_burst', pkts_per_burst)

        if not isinstance(ibg, (int, float)):
            raise STLArgumentError('ibg', ibg)

        if not isinstance(count, int):
            raise STLArgumentError('count', count)

        super(STLTXMultiBurst, self).__init__()

        self.fields['type'] = 'multi_burst'
        self.fields['pps'] = pps
        self.fields['pkts_per_burst'] = pkts_per_burst
        self.fields['ibg'] = ibg
        self.fields['count'] = count

    def __str__ (self):
        return "Multi Burst"

STLStreamDstMAC_CFG_FILE=0
STLStreamDstMAC_PKT     =1
STLStreamDstMAC_ARP     =2



class STLStream(object):

    def __init__ (self,
                  name = None,
                  packet = None,
                  mode = STLTXCont(1),
                  enabled = True,
                  self_start = True,
                  isg = 0.0,
                  rx_stats = None,
                  next = None,
                  stream_id = None,
                  action_count = 0,
                  mac_src_override_by_pkt=None,
                  mac_dst_override_mode=None    #see  STLStreamDstMAC_xx
                  ):

        # type checking
        if not isinstance(mode, STLTXMode):
            raise STLArgumentError('mode', mode)

        if packet and not isinstance(packet, CTrexPktBuilderInterface):
            raise STLArgumentError('packet', packet)

        if not isinstance(enabled, bool):
            raise STLArgumentError('enabled', enabled)

        if not isinstance(self_start, bool):
            raise STLArgumentError('self_start', self_start)

        if not isinstance(isg, (int, float)):
            raise STLArgumentError('isg', isg)

        if (type(mode) == STLTXCont) and (next != None):
            raise STLError("continuous stream cannot have a next stream ID")

        # tag for the stream and next - can be anything
        self.name = name
        self.next = next

        # ID
        self.set_id(stream_id)
        self.set_next_id(None)


        self.fields = {}

        int_mac_src_override_by_pkt = 0;
        int_mac_dst_override_mode   = 0;


        if mac_src_override_by_pkt == None:
            int_mac_src_override_by_pkt=0
            if packet :
                if packet.is_def_src_mac ()==False:
                    int_mac_src_override_by_pkt=1

        else:
            int_mac_src_override_by_pkt = int(mac_src_override_by_pkt);

        if mac_dst_override_mode == None:
            int_mac_dst_override_mode   = 0;
            if packet :
                if packet.is_def_dst_mac ()==False:
                    int_mac_dst_override_mode=STLStreamDstMAC_PKT
        else:
            int_mac_dst_override_mode = int(mac_dst_override_mode);


        self.fields['flags'] = (int_mac_src_override_by_pkt&1) +  ((int_mac_dst_override_mode&3)<<1)

        self.fields['action_count'] = action_count

        # basic fields
        self.fields['enabled'] = enabled
        self.fields['self_start'] = self_start
        self.fields['isg'] = isg

        # mode
        self.fields['mode'] = mode.to_json()
        self.mode_desc      = str(mode)

        self.fields['packet'] = {}
        self.fields['vm'] = {}

        if not packet:
            packet = CScapyTRexPktBuilder(pkt = Ether()/IP())

        # packet builder
        packet.compile()

        # packet and VM
        self.fields['packet'] = packet.dump_pkt()
        self.fields['vm']     = packet.get_vm_data()


        # this is heavy, calculate lazy
        self.packet_desc = None

        if not rx_stats:
            self.fields['rx_stats'] = {}
            self.fields['rx_stats']['enabled'] = False
        else:
            self.fields['rx_stats'] = rx_stats


    def __str__ (self):
        s =  "Stream Name: {0}\n".format(self.name)
        s += "Stream Next: {0}\n".format(self.next)
        s += "Stream JSON:\n{0}\n".format(json.dumps(self.fields, indent = 4, separators=(',', ': '), sort_keys = True))
        return s

    def to_json (self):
        return dict(self.fields)

    def get_id (self):
        return self.id

    def set_id (self, id):
        self.id = id

    def get_next_id (self):
        return self.next_id

    def set_next_id (self, next_id):
        self.next_id = next_id

    def get_name (self):
        return self.name

    def get_next (self):
        return self.next

    def get_pkt_type (self):
        if self.packet_desc == None:
            self.packet_desc = CScapyTRexPktBuilder.pkt_layers_desc_from_buffer(base64.b64decode(self.fields['packet']['binary']))

        return self.packet_desc

    def get_pkt_len (self, count_crc = True):
       pkt_len = len(base64.b64decode(self.fields['packet']['binary']))
       if count_crc:
           pkt_len += 4

       return pkt_len

    def get_pps (self):
        return self.fields['mode']['pps']

    def get_mode (self):
        return self.mode_desc


    def to_yaml (self):
        y = {}

        if self.name:
            y['name'] = self.name

        if self.next:
            y['next'] = self.next

        y['stream'] = self.fields

        return y
  
    def dump_to_yaml (self, yaml_file = None):
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
            raise STLError("packet section must contain either 'binary' or 'pcap'")

        if 'binary' in packet_type:
            try:
                pkt_str = base64.b64decode(packet_dict['binary'])
            except TypeError:
                raise STLError("'binary' field is not a valid packet format")

            builder = CScapyTRexPktBuilder(pkt_buffer = pkt_str)

        elif 'pcap' in packet_type:
            pcap = os.path.join(self.yaml_path, packet_dict['pcap'])

            if not os.path.exists(pcap):
                raise STLError("'pcap' - cannot find '{0}'".format(pcap))

            builder = CScapyTRexPktBuilder(pkt = pcap)

        return builder


    def __parse_mode (self, mode_obj):

        mode_type = mode_obj.get('type')

        if mode_type == 'continuous':
            defaults = STLTXCont()
            mode = STLTXCont(pps = mode_obj.get('pps', defaults.fields['pps']))

        elif mode_type == 'single_burst':
            defaults = STLTXSingleBurst()
            mode = STLTXSingleBurst(pps         = mode_obj.get('pps', defaults.fields['pps']),
                                    total_pkts  = mode_obj.get('total_pkts', defaults.fields['total_pkts']))

        elif mode_type == 'multi_burst':
            defaults = STLTXMultiBurst()
            mode = STLTXMultiBurst(pps            = mode_obj.get('pps', defaults.fields['pps']),
                                   pkts_per_burst = mode_obj.get('pkts_per_burst', defaults.fields['pkts_per_burst']),
                                   ibg            = mode_obj.get('ibg', defaults.fields['ibg']),
                                   count          = mode_obj.get('count', defaults.fields['count']))

        else:
            raise STLError("mode type can be 'continuous', 'single_burst' or 'multi_burst")


        return mode


    def __parse_stream (self, yaml_object):
        s_obj = yaml_object['stream']

        # parse packet
        packet = s_obj.get('packet')
        if not packet:
            raise STLError("YAML file must contain 'packet' field")

        builder = self.__parse_packet(packet)


        # mode
        mode_obj = s_obj.get('mode')
        if not mode_obj:
            raise STLError("YAML file must contain 'mode' field")

        mode = self.__parse_mode(mode_obj)

        
        defaults = STLStream()

        # create the stream
        stream = STLStream(name       = yaml_object.get('name'),
                           packet     = builder,
                           mode       = mode,
                           enabled    = s_obj.get('enabled', defaults.fields['enabled']),
                           self_start = s_obj.get('self_start', defaults.fields['self_start']),
                           isg        = s_obj.get('isg', defaults.fields['isg']),
                           rx_stats   = s_obj.get('rx_stats', defaults.fields['rx_stats']),
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
    def __init__ (self, streams = None):
        if streams == None:
            streams = []

        if not type(streams) == list:
            streams = [streams]

        if not all([isinstance(stream, STLStream) for stream in streams]):
            raise STLArgumentError('streams', streams, valid_values = STLStream)

        self.streams = streams


    def get_streams (self):
        return self.streams

    def __str__ (self):
        return '\n'.join([str(stream) for stream in self.streams])


    @staticmethod
    def load_yaml (yaml_file):
        # check filename
        if not os.path.isfile(yaml_file):
            raise STLError("file '{0}' does not exists".format(yaml_file))

        yaml_loader = YAMLLoader(yaml_file)
        streams = yaml_loader.parse()

        return STLProfile(streams)


    @staticmethod
    def load_py (python_file):
        # check filename
        if not os.path.isfile(python_file):
            raise STLError("file '{0}' does not exists".format(python_file))

        basedir = os.path.dirname(python_file)
        sys.path.append(basedir)

        try:
            file    = os.path.basename(python_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], -1)
            reload(module) # reload the update 

            streams = module.register().get_streams()

            return STLProfile(streams)

        except Exception as e:
            a, b, tb = sys.exc_info()
            x =''.join(traceback.format_list(traceback.extract_tb(tb)[1:])) + a.__name__ + ": " + str(b) + "\n"

            summary = "\nPython Traceback follows:\n\n" + x
            raise STLError(summary)


        finally:
            sys.path.remove(basedir)

    @staticmethod
    def load_pcap (pcap_file, ipg_usec = None, speedup = 1.0, loop = False):
        # check filename
        if not os.path.isfile(pcap_file):
            raise STLError("file '{0}' does not exists".format(pcap_file))

        streams = []
        last_ts_usec = 0

        pkts = RawPcapReader(pcap_file).read_all()
        
        for i, (cap, meta) in enumerate(pkts, start = 1):
            # IPG - if not provided, take from cap
            if ipg_usec == None:
                ts_usec = (meta[0] * 1e6 + meta[1]) / float(speedup)
            else:
                ts_usec = (ipg_usec * i) / float(speedup)

            # handle last packet
            if i == len(pkts):
                next = 1 if loop else None
            else:
                next = i + 1

            
            streams.append(STLStream(name = i,
                                     packet = CScapyTRexPktBuilder(pkt_buffer = cap),
                                     mode = STLTXSingleBurst(total_pkts = 1),
                                     self_start = True if (i == 1) else False,
                                     isg = (ts_usec - last_ts_usec),  # seconds to usec
                                     next = next))
        
            last_ts_usec = ts_usec


        return STLProfile(streams)

      

    @staticmethod
    def load (filename):
        x = os.path.basename(filename).split('.')
        suffix = x[1] if (len(x) == 2) else None

        if suffix == 'py':
            profile = STLProfile.load_py(filename)

        elif suffix == 'yaml':
            profile = STLProfile.load_yaml(filename)

        elif suffix in ['cap', 'pcap']:
            profile = STLProfile.load_pcap(filename, speedup = 1, ipg_usec = 1e6)

        else:
            raise STLError("unknown profile file type: '{0}'".format(suffix))

        return profile


    def dump_to_yaml (self, yaml_file = None):
        yaml_list = [stream.to_yaml() for stream in self.streams]
        yaml_str = yaml.dump(yaml_list, default_flow_style = False)

        # write to file if provided
        if yaml_file:
            with open(yaml_file, 'w') as f:
                f.write(yaml_str)

        return yaml_str


