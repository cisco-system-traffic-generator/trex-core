#!/router/bin/python

import external_packages
from client_utils.packet_builder import CTRexPktBuilder
from collections import OrderedDict


class CStreamList(object):

    def __init__(self):
        self.streams_list = OrderedDict()
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

    def export_to_yaml(self, file_path):
        pass

    def load_yaml(self, file_path):
        # clear all existing streams linked to this object
        self.streams_list.clear()
        # self._stream_id = 0
        # load from YAML file the streams one by one
        try:
            with open(file_path, 'r') as f:
                loaded_streams = yaml.load(f)

                # assume at this point that YAML file is according to rules and correct


        except yaml.YAMLError as e:
            print "Error in YAML configuration file:", e
            print "Aborting YAML loading, no changes made to stream list"
            return


        pass




class CStream(object):
    """docstring for CStream"""
    DEFAULTS = {"rx_stats": CRxStats,
                "mode": CTxMode,
                "isg": 5.0,
                "next_stream": -1,
                "self_start": True,
                "enabled": True}

    def __init__(self, **kwargs):
        super(CStream, self).__init__()
        for k, v in kwargs.items():
            setattr(self, k, v)
        # set default values to unset attributes, according to DEFAULTS dict
        set_keys = set(kwargs.keys())
        keys_to_set = [x
                       for x in self.DEFAULTS
                       if x not in set_keys]
        for key in keys_to_set:
            default = self.DEFAULTS.get(key)
            if type(default) == type:
                setattr(self, key, default())
            else:
                setattr(self, key, default)

    @property
    def packet(self):
        return self._packet

    @packet.setter
    def packet(self, packet_obj):
        assert isinstance(packet_obj, CTRexPktBuilder)
        self._packet = packet_obj

    @property
    def enabled(self):
        return self._enabled

    @enabled.setter
    def enabled(self, bool_value):
        self._enabled = bool(bool_value)

    @property
    def self_start(self):
        return self._self_start

    @self_start.setter
    def self_start(self, bool_value):
        self._self_start = bool(bool_value)

    @property
    def next_stream(self):
        return self._next_stream

    @next_stream.setter
    def next_stream(self, value):
        self._next_stream = int(value)

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

if __name__ == "__main__":
    pass
