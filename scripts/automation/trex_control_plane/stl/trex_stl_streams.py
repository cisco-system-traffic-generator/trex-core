#!/router/bin/python

from trex_stl_exceptions import *
from trex_stl_packet_builder_interface import CTrexPktBuilderInterface
from trex_stl_packet_builder_scapy import CScapyTRexPktBuilder, Ether, IP
from collections import OrderedDict, namedtuple

from trex_control_plane.client_utils.yaml_utils import *

from dpkt import pcap
import random
import yaml
import base64

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


# multi burst mode
class STLTXMultiBurst(STLTXMode):

    def __init__ (self,
                  pps = 1,
                  pkts_per_burst = 1,
                  ibg = 0.0,
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


class STLStream(object):

    def __init__ (self,
                  packet = None,
                  mode = STLTXCont(1),
                  enabled = True,
                  self_start = True,
                  isg = 0.0,
                  rx_stats = None,
                  next_stream_id = -1,
                  stream_id = None):

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

        if (type(mode) == STLTXCont) and (next_stream_id != -1):
            raise STLError("continuous stream cannot have a next stream ID")

        self.fields = {}

        # use a random 31 bit for ID
        self.fields['stream_id'] = stream_id if stream_id is not None else random.getrandbits(31)

        # basic fields
        self.fields['enabled'] = enabled
        self.fields['self_start'] = self_start
        self.fields['isg'] = isg

        self.fields['next_stream_id'] = next_stream_id

        # mode
        self.fields['mode'] = mode.to_json()

        self.fields['packet'] = {}
        self.fields['vm'] = {}

        if not packet:
            packet = CScapyTRexPktBuilder(pkt = Ether()/IP())

        # packet builder
        packet.compile()
        # packet and VM
        self.fields['packet'] = packet.dump_pkt()
        self.fields['vm']     = packet.get_vm_data()

        self.fields['rx_stats'] = {}
        if not rx_stats:
            self.fields['rx_stats']['enabled'] = False


    def __str__ (self):
        return json.dumps(self.fields, indent = 4, separators=(',', ': '), sort_keys = True)

    def to_json (self):
        return self.fields

    def get_id (self):
        return self.fields['stream_id']



    @staticmethod
    def dump_to_yaml (stream_list, yaml_file = None):

        # type check
        if isinstance(stream_list, STLStream):
            stream_list = [stream_list]

        if not all([isinstance(stream, STLStream) for stream in stream_list]):
            raise STLArgumentError('stream_list', stream_list)


        names = {}
        for i, stream in enumerate(stream_list):
            names[stream.get_id()] = "stream-{0}".format(i)

        yaml_lst = []
        for stream in stream_list:

            fields = dict(stream.fields)

            # handle the next stream id
            if fields['next_stream_id'] == -1:
                del fields['next_stream_id']

            else:
                if not stream.get_id() in names:
                    raise STLError('broken dependencies in stream list')

                fields['next_stream'] = names[stream.get_id()]

            # add to list
            yaml_lst.append({'name': names[stream.get_id()], 'stream': fields})

        # write to file
        x = yaml.dump(yaml_lst, default_flow_style=False)
        if yaml_file:
            with open(yaml_file, 'w') as f:
                f.write(x)

        return x


    @staticmethod
    def load_from_yaml (yaml_file):

        with open(yaml_file, 'r') as f:
            yaml_str = f.read()


        # load YAML
        lst = yaml.load(yaml_str)

        # decode to streams
        streams = []
        for stream in lst:
            # for defaults
            defaults = STLStream()
            s = STLStream(packet = None,
                          mode = STLTXCont(1),
                          enabled = True,
                          self_start = True,
                          isg = 0.0,
                          rx_stats = None,
                          next_stream_id = -1,
                          stream_id = None
                          )

            streams.append(s)

        return streams

