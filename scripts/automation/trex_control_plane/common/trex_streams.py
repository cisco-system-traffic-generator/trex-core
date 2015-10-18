#!/router/bin/python

import external_packages
from client_utils.packet_builder import CTRexPktBuilder
from collections import OrderedDict
from client_utils.yaml_utils import *
import dpkt


class CStreamList(object):

    def __init__(self):
        self.streams_list = OrderedDict()
        self.yaml_loader = CTRexYAMLLoader("rpc_exceptions.yaml")
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



        # start validating and reassembling clients input


        pass

class CRxStats(object):

    def __init__(self, enabled=False, seq_enabled=False, latency_enabled=False):
        self._rx_dict = {"enabled": enabled,
                         "seq_enabled": seq_enabled,
                         "latency_enabled": latency_enabled}

    @property
    def enabled(self):
        return self._rx_dict.get("enabled")

    @enabled.setter
    def enabled(self, bool_value):
        self._rx_dict['enabled'] = bool(bool_value)

    @property
    def seq_enabled(self):
        return self._rx_dict.get("seq_enabled")

    @seq_enabled.setter
    def seq_enabled(self, bool_value):
        self._rx_dict['seq_enabled'] = bool(bool_value)

    @property
    def latency_enabled(self):
        return self._rx_dict.get("latency_enabled")

    @latency_enabled.setter
    def latency_enabled(self, bool_value):
        self._rx_dict['latency_enabled'] = bool(bool_value)

    def dump(self):
        return {k: v
                for k, v in self._rx_dict.items()
                if v
                }

class CTxMode(object):
    """docstring for CTxMode"""
    def __init__(self, tx_mode, pps):
        super(CTxMode, self).__init__()
        if tx_mode not in ["continuous", "single_burst", "multi_burst"]:
            raise ValueError("Unknown TX mode ('{0}')has been initialized.".format(tx_mode))
        self._tx_mode = tx_mode
        self._fields = {'pps': float(pps)}
        if tx_mode == "single_burst":
            self._fields['total_pkts'] = 0
        elif tx_mode == "multi_burst":
            self._fields['pkts_per_burst'] = 0
            self._fields['ibg'] = 0.0
            self._fields['count'] = 0
        else:
            pass

    def set_tx_mode_attr(self, attr, val):
        if attr in self._fields:
            self._fields[attr] = type(self._fields.get(attr))(val)
        else:
            raise ValueError("The provided attribute ('{0}') is not a legal attribute in selected TX mode ('{1}')".
                             format(attr, self._tx_mode))

    def dump(self):
        dump = {"type": self._tx_mode}
        dump.update({k: v
                     for k, v in self._fields.items()
                     })
        return dump

class CStream(object):
    """docstring for CStream"""
    DEFAULTS = {"rx_stats": CRxStats,
                "mode": CTxMode,
                "isg": 5.0,
                "next_stream": -1,
                "self_start": True,
                "enabled": True}

    FIELDS = ["enabled", "self_start", "next_stream", "isg", "mode", "rx_stats", "packet", "vm"]

    def __init__(self):
        super(CStream, self).__init__()
        for field in CStream.FIELDS:
            setattr(self, field, None)

    def load_data(self, **kwargs):
        for k, v in kwargs.items():
            if k == "rx_stats":
                if isinstance(v, dict):
                    setattr(self, k, CRxStats(**v))
                elif isinstance(v, CRxStats):
                    setattr(self, k, v)
            elif k == "mode":
                if isinstance(v, dict):
                    setattr(self, k, CTxMode(v))
                elif isinstance(v, CTxMode):
                    setattr(self, k, v)
            else:
                setattr(self, k, v)



    # def __init__(self, enabled, self_start, next_stream, isg, mode, rx_stats, packet, vm):
    #     super(CStream, self).__init__()
    #     for k, v in kwargs.items():
    #         if k == "rx_stats":
    #             if isinstance(v, dict):
    #                 setattr(self, k, CRxStats(v))
    #             elif isinstance(v, CRxStats):
    #                 setattr(self, k, v)
    #         elif k == "mode":
    #             if isinstance(v, dict):
    #                 setattr(self, k, CTxMode(v))
    #             elif isinstance(v, CTxMode):
    #                 setattr(self, k, v)
    #         else:
    #             setattr(self, k, v)
    #     # set default values to unset attributes, according to DEFAULTS dict
    #     set_keys = set(kwargs.keys())
    #     keys_to_set = [x
    #                    for x in self.DEFAULTS
    #                    if x not in set_keys]
    #     for key in keys_to_set:
    #         default = self.DEFAULTS.get(key)
    #         if type(default) == type:
    #             setattr(self, key, default())
    #         else:
    #             setattr(self, key, default)

    # @property
    # def packet(self):
    #     return self._packet
    #
    # @packet.setter
    # def packet(self, packet_obj):
    #     assert isinstance(packet_obj, CTRexPktBuilder)
    #     self._packet = packet_obj
    #
    # @property
    # def enabled(self):
    #     return self._enabled
    #
    # @enabled.setter
    # def enabled(self, bool_value):
    #     self._enabled = bool(bool_value)
    #
    # @property
    # def self_start(self):
    #     return self._self_start
    #
    # @self_start.setter
    # def self_start(self, bool_value):
    #     self._self_start = bool(bool_value)
    #
    # @property
    # def next_stream(self):
    #     return self._next_stream
    #
    # @next_stream.setter
    # def next_stream(self, value):
    #     self._next_stream = int(value)

    def dump(self):
        pass
        return {"enabled": self.enabled,
                "self_start": self.self_start,
                "isg": self.isg,
                "next_stream": self.next_stream,
                "packet": self.packet.dump_pkt(),
                "mode": self.mode.dump(),
                "vm": self.packet.get_vm_data(),
                "rx_stats": self.rx_stats.dump()}






if __name__ == "__main__":
    pass
