#!/router/bin/python

import external_packages
from client_utils.packet_builder_interface import CTrexPktBuilderInterface
from client_utils.packet_builder import CTRexPktBuilder
from collections import OrderedDict, namedtuple
from client_utils.yaml_utils import *
import trex_stl_exceptions
import dpkt
import struct
import copy
import os
import random
import yaml
import base64

StreamPack = namedtuple('StreamPack', ['stream_id', 'stream'])
LoadedStreamList = namedtuple('LoadedStreamList', ['name', 'loaded', 'compiled'])

class CStreamList(object):

    def __init__(self):
        self.streams_list = OrderedDict()
        self.yaml_loader = CTRexYAMLLoader(os.path.join(os.path.dirname(os.path.realpath(__file__)), 
                                                        "rpc_defaults.yaml"))

    def generate_numbered_name (self, name):
        prefix = name.rstrip('01234567890')
        suffix = name[len(prefix):]
        if suffix == "":
            n = "_1"
        else:
            n = int(suffix) + 1
        return prefix + str(n)

    def append_stream(self, name, stream_obj):
        assert isinstance(stream_obj, CStream)

        # if name exists simply add numbered suffix to it
        while name in self.streams_list:
            name = self.generate_numbered_name(name)

        self.streams_list[name]=stream_obj
        return name

    def remove_stream(self, name):
        popped = self.streams_list.pop(name)
        if popped:
            for stream_name, stream in self.streams_list.items():
                if stream.next_stream_id == name:
                    stream.next_stream_id = -1
                try:
                    rx_stats_stream = getattr(stream.rx_stats, "stream_id")
                    if rx_stats_stream == name:
                        # if a referenced stream of rx_stats object deleted, revert to rx stats of current stream 
                        setattr(stream.rx_stats, "stream_id", stream_name)
                except AttributeError as e:
                    continue    # 
        return popped

    def export_to_yaml(self, file_path):
        raise NotImplementedError("export_to_yaml method is not implemented, yet")

    def load_yaml(self, file_path, multiplier=1):
        # clear all existing streams linked to this object
        self.streams_list.clear()
        streams_data = load_yaml_to_obj(file_path)
        assert isinstance(streams_data, list)
        new_streams_data = []
        for stream in streams_data:
            stream_name = stream.get("name")
            raw_stream = stream.get("stream")
            if not stream_name or not raw_stream:
                raise ValueError("Provided stream is not according to convention."
                                 "Each stream must be provided as two keys: 'name' and 'stream'. "
                                 "Provided item was:\n {stream}".format(stream))
            new_stream_data = self.yaml_loader.validate_yaml(raw_stream,
                                                             "stream",
                                                             multiplier= multiplier)
            new_streams_data.append(new_stream_data)
            new_stream_obj = CStream()
            new_stream_obj.load_data(**new_stream_data)
            self.append_stream(stream_name, new_stream_obj)
        return new_streams_data

    def compile_streams(self):
        # first, assign an id to each stream
        stream_ids = {}
        for idx, stream_name in enumerate(self.streams_list):
            stream_ids[stream_name] = idx

        # next, iterate over the streams and transform them from working with names to ids.
        # with that build a new dict with old stream_name as the key, and StreamPack as the stored value 
        compiled_streams = {}
        for stream_name, stream in self.streams_list.items():
            tmp_stream = CStreamList._compile_single_stream(stream_name, stream, stream_ids)
            compiled_streams[stream_name] = StreamPack(stream_ids.get(stream_name),
                                                       tmp_stream)
        return compiled_streams

    @staticmethod
    def _compile_single_stream(stream_name, stream, id_dict):
        # copy the old stream to temporary one, no change to class attributes
        tmp_stream = copy.copy(stream)
        next_stream_id = id_dict.get(getattr(tmp_stream, "next_stream_id"), -1)
        try:
            rx_stats_stream_id = id_dict.get(getattr(tmp_stream.rx_stats, "stream_id"),
                                             id_dict.get(stream_name))
        except AttributeError as e:
            rx_stats_stream_id = id_dict.get(stream_name)
        # assign resolved values to stream object
        tmp_stream.next_stream_id = next_stream_id
        tmp_stream.rx_stats.stream_id = rx_stats_stream_id
        return tmp_stream


class CRxStats(object):

    FIELDS = ["seq_enabled", "latency_enabled", "stream_id"]
    def __init__(self, enabled=False, **kwargs):
        self.enabled = bool(enabled)
        for field in CRxStats.FIELDS:
            setattr(self, field, kwargs.get(field, False))

    def dump(self):
        if self.enabled:
            dump = {"enabled": True}
            dump.update({k: getattr(self, k)
                         for k in CRxStats.FIELDS}
                        )
            return dump
        else:
            return {"enabled": False}



class CTxMode(object):
    """docstring for CTxMode"""
    GENERAL_FIELDS = ["type", "pps"]
    FIELDS = {"continuous": [],
              "single_burst": ["total_pkts"],
              "multi_burst": ["pkts_per_burst", "ibg", "count"]}

    def __init__(self, type, pps=0, **kwargs):
        self._MODES = CTxMode.FIELDS.keys()
        self.type = type
        self.pps = pps
        for field in CTxMode.FIELDS.get(self.type):
            setattr(self, field, kwargs.get(field, 0))

    @property
    def type(self):
        return self._type

    @type.setter
    def type(self, type):
        if type not in self._MODES:
            raise ValueError("Unknown TX mode ('{0}')has been initialized.".format(type))
        self._type = type
        self._reset_fields()

    def dump(self):
        dump = ({k: getattr(self, k)
                 for k in CTxMode.GENERAL_FIELDS
                 })
        dump.update({k: getattr(self, k)
                     for k in CTxMode.FIELDS.get(self.type)
                     })
        return dump

    def _reset_fields(self):
        for field in CTxMode.FIELDS.get(self.type):
            setattr(self, field, 0)


class CStream(object):
    """docstring for CStream"""

    FIELDS = ["enabled", "self_start", "next_stream_id", "isg", "mode", "rx_stats", "packet", "vm"]

    def __init__(self):
        self.is_loaded = False
        self._is_compiled = False
        self._pkt_bld_obj = CTRexPktBuilder()
        for field in CStream.FIELDS:
            setattr(self, field, None)


    def load_data(self, **kwargs):
        try:
            for k in CStream.FIELDS:
                if k == "rx_stats":
                    rx_stats_data = kwargs[k]
                    if isinstance(rx_stats_data, dict):
                        setattr(self, k, CRxStats(**rx_stats_data))
                    elif isinstance(rx_stats_data, CRxStats):
                        setattr(self, k, rx_stats_data)
                elif k == "mode":
                    tx_mode = kwargs[k]
                    if isinstance(tx_mode, dict):
                        setattr(self, k, CTxMode(**tx_mode))
                    elif isinstance(tx_mode, CTxMode):
                        setattr(self, k, tx_mode)
                elif k == "packet":
                    if isinstance(kwargs[k], CTRexPktBuilder):
                        if "vm" not in kwargs:
                            self.load_packet_obj(kwargs[k])
                            break # vm field check is skipped
                        else:
                            raise ValueError("When providing packet object with a CTRexPktBuilder, vm parameter "
                                             "should not be supplied")
                    else:
                        binary = kwargs[k]["binary"]
                        if isinstance(binary, str):

                            # TODO: load to _pkt_bld_obj also when passed as byte array!
                            if binary.endswith(".pcap"):
                                self._pkt_bld_obj.load_packet_from_pcap(binary)
                                self._pkt_bld_obj.metadata = kwargs[k]["meta"]
                                self.packet = self._pkt_bld_obj.dump_pkt()
                            else:
                                 self.packet = {}
                                 self.packet['binary'] = binary
                                 self.packet['meta'] = ""

                        else:
                            raise ValueError("Packet binary attribute has been loaded with unsupported value."
                                             "Supported values are reference to pcap file with SINGLE packet, "
                                             "or a list of unsigned-byte integers")
                else:
                    setattr(self, k, kwargs[k])
            self.is_loaded = True
        except KeyError as e:
            cause = e.args[0]
            raise KeyError("The attribute '{0}' is missing as a field of the CStream object.\n"
                           "Loaded data must contain all of the following fields: {1}".format(cause, CStream.FIELDS))

    def load_packet_obj(self, packet_obj):
        assert isinstance(packet_obj, CTRexPktBuilder)
        self.packet = packet_obj.dump_pkt()
        self.vm = packet_obj.get_vm_data()

    def load_packet_from_pcap(self, pcap_path, metadata=''):
        with open(pcap_path, 'r') as f:
            pcap = dpkt.pcap.Reader(f)
            first_packet = True
            for _, buf in pcap:
                # this is an iterator, can't evaluate the number of files in advance
                if first_packet:
                    self.packet = {"binary": [struct.unpack('B', buf[i:i+1])[0] # represent data as list of 0-255 ints
                                              for i in range(0, len(buf))],
                                   "meta": metadata}    # meta data continues without a change.
                    first_packet = False
                else:
                    raise ValueError("Provided pcap file contains more than single packet.")
        # arrive here ONLY if pcap contained SINGLE packet
        return


    def dump(self):
        if self.is_loaded:
            dump = {}
            for key in CStream.FIELDS:
                try:
                    dump[key] = getattr(self, key).dump()  # use dump() method of compound object, such TxMode
                except AttributeError:
                    dump[key] = getattr(self, key)
            return dump
        else:
            raise RuntimeError("CStream object isn't loaded with data. Use 'load_data' method.")

    def get_stream_layers(self, depth_limit=Ellipsis):
        stream_layers = self._pkt_bld_obj.get_packet_layers(depth_limit)
        return "/".join(stream_layers)



# describes a stream DB
class CStreamsDB(object):

    def __init__(self):
        self.stream_packs = {}

    def load_yaml_file(self, filename):

        stream_pack_name = filename
        if stream_pack_name in self.get_loaded_streams_names():
            self.remove_stream_packs(stream_pack_name)

        stream_list = CStreamList()
        loaded_obj = stream_list.load_yaml(filename)

        try:
            compiled_streams = stream_list.compile_streams()
            rc = self.load_streams(LoadedStreamList(stream_pack_name,
                                                    loaded_obj,
                                                    [StreamPack(v.stream_id, v.stream.dump())
                                                     for k, v in compiled_streams.items()]))
        except Exception as e:
            return None


        return self.get_stream_pack(stream_pack_name)

    def load_streams(self, LoadedStreamList_obj):
        if LoadedStreamList_obj.name in self.stream_packs:
            return False
        else:
            self.stream_packs[LoadedStreamList_obj.name] = LoadedStreamList_obj
            return True

    def remove_stream_packs(self, *names):
        removed_streams = []
        for name in names:
            removed = self.stream_packs.pop(name)
            if removed:
                removed_streams.append(name)
        return removed_streams

    def clear(self):
        self.stream_packs.clear()

    def get_loaded_streams_names(self):
        return self.stream_packs.keys()

    def stream_pack_exists (self, name):
        return name in self.get_loaded_streams_names()

    def get_stream_pack(self, name):
        if not self.stream_pack_exists(name):
            return None
        else:
            return self.stream_packs.get(name)


########################### Simple Streams ###########################
from trex_stl_exceptions import *

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
                  packet,
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

        if not isinstance(packet, CTrexPktBuilderInterface):
            raise STLArgumentError('packet', packet)

        if not isinstance(enabled, bool):
            raise STLArgumentError('enabled', enabled)

        if not isinstance(self_start, bool):
            raise STLArgumentError('self_start', self_start)

        if not isinstance(isg, (int, float)):
            raise STLArgumentError('isg', isg)

        if (type(mode) == STLTXCont) and (next_stream_id != -1):
            raise STLError("continuous stream cannot have a next stream ID")

        # use a random 31 bit for ID
        self.stream_id = stream_id if stream_id is not None else random.getrandbits(31)

        self.fields = {}

        # basic fields
        self.fields['enabled'] = enabled
        self.fields['self_start'] = self_start
        self.fields['isg'] = isg

        self.fields['next_stream_id'] = next_stream_id

        # mode
        self.fields['mode'] = mode.to_json()

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
        return self.stream_id

    @staticmethod
    def dump_to_yaml (yaml_file, stream_list):

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
        with open(yaml_file, 'w') as f:
            f.write(x)
        return x


# REMOVE ME when can - convert from stream pack to a simple stream
class HACKSTLStream(STLStream):
    def __init__ (self, stream_pack):
        if not isinstance(stream_pack, StreamPack):
            raise Exception("internal error")

        packet = CTRexPktBuilder()
        packet.load_from_stream_obj(stream_pack.stream)
        super(HACKSTLStream, self).__init__(packet, stream_id = stream_pack.stream_id)

        self.fields = stream_pack.stream
