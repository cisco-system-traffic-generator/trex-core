#!/router/bin/python

import external_packages
from client_utils.packet_builder import CTRexPktBuilder
from collections import OrderedDict
from client_utils.yaml_utils import *
import dpkt
import struct


class CStreamList(object):

    def __init__(self):
        self.streams_list = {OrderedDict()}
        self.yaml_loader = CTRexYAMLLoader("rpc_defaults.yaml")
        self._stream_id = 0
        # self._stream_by_name = {}

    def append_stream(self, name, stream_obj):
        assert isinstance(stream_obj, CStream)
        if name in self.streams_list:
            raise NameError("A stream with this name already exists on this list.")
        self.streams_list[name]=stream_obj
        return

    def remove_stream(self, name):
        return self.streams_list.pop(name)

    def rearrange_streams(self, streams_names_list, new_streams_dict={}):
        tmp_list = OrderedDict()
        for stream in streams_names_list:
            if stream in self.streams_list:
                tmp_list[stream] = self.streams_list.get(stream)
            elif stream in new_streams_dict:
                new_stream_obj = new_streams_dict.get(stream)
                assert isinstance(new_stream_obj, CStream)
                tmp_list[stream] = new_stream_obj
            else:
                raise NameError("Given stream named '{0}' cannot be found in existing stream list or and wasn't"
                                "provided with the new_stream_dict parameter.".format(stream))
        self.streams_list = tmp_list

    def export_to_yaml(self, file_path):
        raise NotImplementedError("export_to_yaml method is not implemented, yet")

    def load_yaml(self, file_path, multiplier_dict={}):
        # clear all existing streams linked to this object
        self.streams_list.clear()
        streams_data = load_yaml_to_obj(file_path)
        assert isinstance(streams_data, list)
        raw_streams = {}
        for stream in streams_data:
            stream_name = stream.get("name")
            raw_stream = stream.get("stream")
            if not stream_name or not raw_stream:
                raise ValueError("Provided stream is not according to convention."
                                 "Each stream must be provided as two keys: 'name' and 'stream'. "
                                 "Provided item was:\n {stream}".format(stream))
            new_stream_data = self.yaml_loader.validate_yaml(raw_stream,
                                                             "stream",
                                                             multiplier= multiplier_dict.get(stream_name, 1))
            new_stream_obj = CStream()
            new_stream_obj.load_data(**new_stream_data)
            self.append_stream(stream_name, new_stream_obj)

    def compile_streams(self):
        stream_ids = {}

        pass


class CRxStats(object):

    FIELDS = ["seq_enabled", "latency_enabled"]
    def __init__(self, enabled=False, **kwargs):
        self.enabled = bool(enabled)
        for field in CRxStats.FIELDS:
            setattr(self, field, kwargs.get(field, False))

    def dump(self):
        if self.enabled:
            dump = {"enabled": True}
            dump.update({k: getattr(self, k)
                         for k in CRxStats.FIELDS
                         if getattr(self, k)
                         })
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

    FIELDS = ["enabled", "self_start", "next_stream", "isg", "mode", "rx_stats", "packet", "vm"]

    def __init__(self):
        self.is_loaded = False
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
                        else:
                            raise ValueError("When providing packet object with a CTRexPktBuilder, vm parameter "
                                             "should not be supplied")
                    else:
                        binary = kwargs[k]["binary"]
                        if isinstance(binary, list):
                            setattr(self, k, kwargs[k])
                        elif isinstance(binary, str) and binary.endswith(".pcap"):
                            self.load_packet_from_pcap(binary, kwargs[k]["meta"])
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






if __name__ == "__main__":
    pass
