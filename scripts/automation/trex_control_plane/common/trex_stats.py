#!/router/bin/python
from collections import namedtuple, OrderedDict, deque
from client_utils import text_tables
from common.text_opts import format_text, format_threshold
from client.trex_async_client import CTRexAsyncStats
import copy
import datetime
import time
import re
import math

GLOBAL_STATS = 'g'
PORT_STATS = 'p'
PORT_STATUS = 'ps'
ALL_STATS_OPTS = {GLOBAL_STATS, PORT_STATS, PORT_STATUS}
COMPACT = {GLOBAL_STATS, PORT_STATS}

ExportableStats = namedtuple('ExportableStats', ['raw_data', 'text_table'])

# use to calculate diffs relative to the previous values
# for example, BW
def calculate_diff (samples):
    total = 0.0

    weight_step = 1.0 / sum(xrange(0, len(samples)))
    weight = weight_step

    for i in xrange(0, len(samples) - 1):
        current = samples[i] if samples[i] > 0 else 1
        next = samples[i + 1] if samples[i + 1] > 0 else 1

        s = 100 * ((float(next) / current) - 1.0)

        # block change by 100% 
        total  += (min(s, 100) * weight)
        weight += weight_step

    return total


# calculate by absolute values and not relatives (useful for CPU usage in % and etc.)
def calculate_diff_raw (samples):
    total = 0.0

    weight_step = 1.0 / sum(xrange(0, len(samples)))
    weight = weight_step

    for i in xrange(0, len(samples) - 1):
        current = samples[i]
        next = samples[i + 1]

        total  += ( (next - current) * weight )
        weight += weight_step

    return total


class CTRexInfoGenerator(object):
    """
    This object is responsible of generating stats and information from objects maintained at
    CTRexStatelessClient and the ports.
    """

    def __init__(self, global_stats_ref, ports_dict_ref):
        self._global_stats = global_stats_ref
        self._ports_dict = ports_dict_ref

    def generate_single_statistic(self, port_id_list, statistic_type):
        if statistic_type == GLOBAL_STATS:
            return self._generate_global_stats()
        elif statistic_type == PORT_STATS:
            return self._generate_port_stats(port_id_list)
            pass
        elif statistic_type == PORT_STATUS:
            return self._generate_port_status(port_id_list)
        else:
            # ignore by returning empty object
            return {}

    def generate_streams_info(self, port_id_list, stream_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_data = {}
        for port_obj in relevant_ports:
            streams_data = self._generate_single_port_streams_info(port_obj, stream_id_list)
            if not streams_data:
                continue
            hdr_key = "Port {port}: {yaml_file}".format(port= port_obj.port_id,
                                                        yaml_file= streams_data.raw_data.get('referring_file', ''))

            # TODO: test for other ports with same stream structure, and join them
            return_data[hdr_key] = streams_data
        return return_data

    def _generate_global_stats(self):
        # stats_obj = self._async_stats.get_general_stats()
        stats_data = self._global_stats.generate_stats()

        # build table representation
        stats_table = text_tables.TRexTextInfo()
        stats_table.set_cols_align(["l", "l"])
        stats_table.add_rows([[k.replace("_", " ").title(), v]
                              for k, v in stats_data.iteritems()],
                             header=False)

        return {"global_statistics": ExportableStats(stats_data, stats_table)}

    def _generate_port_stats(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_stats = OrderedDict([("owner", []),
                                       ("state", []),
                                       ("--", []),
                                       ("opackets", []),
                                       ("obytes", []),
                                       ("ipackets", []),
                                       ("ibytes", []),
                                       ("ierrors", []),
                                       ("oerrors", []),
                                       ("tx-bytes", []),
                                       ("rx-bytes", []),
                                       ("tx-pkts", []),
                                       ("rx-pkts", []),
                                       ("---", []),
                                       ("Tx bps", []),
                                       ("Rx bps", []),
                                       ("----", []),
                                       ("Tx pps", []),
                                       ("Rx pps", [])
                                      ]
                                      )

        for port_obj in relevant_ports:
            # fetch port data
            port_stats = port_obj.generate_port_stats()

            # populate to data structures
            return_stats_data[port_obj.port_id] = port_stats
            self.__update_per_field_dict(port_stats, per_field_stats)

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"]*len(relevant_ports))
        stats_table.set_cols_width([10] + [20] * len(relevant_ports))
        stats_table.set_cols_dtype(['t'] + ['t'] * len(relevant_ports))

        stats_table.add_rows([[k] + v
                              for k, v in per_field_stats.iteritems()],
                             header=False)
        stats_table.header(["port"] + [port.port_id
                                       for port in relevant_ports])

        return {"port_statistics": ExportableStats(return_stats_data, stats_table)}

    def _generate_port_status(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_status = OrderedDict([("port-type", []),
                                        ("maximum", []),
                                        ("port-status", [])
                                        ]
                                       )

        for port_obj in relevant_ports:
            # fetch port data
            # port_stats = self._async_stats.get_port_stats(port_obj.port_id)
            port_status = port_obj.generate_port_status()

            # populate to data structures
            return_stats_data[port_obj.port_id] = port_status

            self.__update_per_field_dict(port_status, per_field_status)

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["c"]*len(relevant_ports))
        stats_table.set_cols_width([10] + [20] * len(relevant_ports))

        stats_table.add_rows([[k] + v
                              for k, v in per_field_status.iteritems()],
                             header=False)
        stats_table.header(["port"] + [port.port_id
                                       for port in relevant_ports])

        return {"port_status": ExportableStats(return_stats_data, stats_table)}

    def _generate_single_port_streams_info(self, port_obj, stream_id_list):

        return_streams_data = port_obj.generate_loaded_streams_sum(stream_id_list)

        if not return_streams_data.get("streams"):
            # we got no streams available
            return None

        # FORMAT VALUES ON DEMAND
        for stream_id, stream_id_sum in return_streams_data['streams'].iteritems():
            stream_id_sum['rate_pps'] = CTRexStats.format_num(stream_id_sum['rate_pps'], suffix='pps')
            stream_id_sum['packet_type'] = self._trim_packet_headers(stream_id_sum['packet_type'], 20)

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["l"] + ["r"] + ["c"] + ["r"] + ["c"])
        info_table.set_cols_width([4]   + [20]  + [8]   + [16]  + [10]  + [12])

        info_table.add_rows([v.values()
                             for k, v in return_streams_data['streams'].iteritems()],
                             header=False)
        info_table.header(["ID", "packet type", "length", "mode", "rate", "next stream"])

        return ExportableStats(return_streams_data, info_table)


    def __get_relevant_ports(self, port_id_list):
        # fetch owned ports
        ports = [port_obj
                 for _, port_obj in self._ports_dict.iteritems()
                 if port_obj.port_id in port_id_list]
        
        # display only the first FOUR options, by design
        if len(ports) > 4:
            print format_text("[WARNING]: ", 'magenta', 'bold'), format_text("displaying up to 4 ports", 'magenta')
            ports = ports[:4]
        return ports

    def __update_per_field_dict(self, dict_src_data, dict_dest_ref):
        for key, val in dict_src_data.iteritems():
            if key in dict_dest_ref:
                dict_dest_ref[key].append(val)

    @staticmethod
    def _trim_packet_headers(headers_str, trim_limit):
        if len(headers_str) < trim_limit:
            # do nothing
            return headers_str
        else:
            return (headers_str[:trim_limit-3] + "...")



class CTRexStats(object):
    """ This is an abstract class to represent a stats object """

    def __init__(self):
        self.reference_stats = None
        self.latest_stats = {}
        self.last_update_ts = time.time()
        self.history = deque(maxlen = 10)

    def __getitem__(self, item):
        # override this to allow quick and clean access to fields
        if not item in self.latest_stats:
            return "N/A"

        # item must exist
        m = re.search('_(([a-z])ps)$', item)
        if m:
            # this is a non-relative item
            unit = m.group(2)
            if unit == "b":
                return self.get(item, format=True, suffix="b/sec")
            elif unit == "p":
                return self.get(item, format=True, suffix="pkt/sec")
            else:
                return self.get(item, format=True, suffix=m.group(1))

        m = re.search('^[i|o](a-z+)$', item)
        if m:
            # this is a non-relative item
            type = m.group(1)
            if type == "bytes":
                return self.get_rel(item, format=True, suffix="B")
            elif type == "packets":
                return self.get_rel(item, format=True, suffix="pkts")
            else:
                # do not format with suffix
                return self.get_rel(item, format=True)

        # can't match to any known pattern, return N/A
        return "N/A"

    @staticmethod
    def format_num(size, suffix = ""):
        if type(size) == str:
            return "N/A"

        for unit in ['','K','M','G','T','P']:
            if abs(size) < 1000.0:
                return "%3.2f %s%s" % (size, unit, suffix)
            size /= 1000.0
        return "NaN"

    def generate_stats(self):
        # must be implemented by designated classes (such as port/ global stats)
        raise NotImplementedError()

    def update(self, snapshot):
        # update
        self.latest_stats = snapshot
        self.history.append(snapshot)

        diff_time = time.time() - self.last_update_ts

        # 3 seconds is too much - this is the new reference
        if (self.reference_stats == None) or (diff_time > 3):
            self.reference_stats = self.latest_stats

        self.last_update_ts = time.time()

    def clear_stats(self):
        self.reference_stats = self.latest_stats

    def invalidate (self):
        self.latest_stats = {}

    def get(self, field, format=False, suffix=""):
        if not field in self.latest_stats:
            return "N/A"
        if not format:
            return self.latest_stats[field]
        else:
            return self.format_num(self.latest_stats[field], suffix)

    def get_rel(self, field, format=False, suffix=""):
        if not field in self.latest_stats:
            return "N/A"

        if not format:
            return (self.latest_stats[field] - self.reference_stats[field])
        else:
            return self.format_num(self.latest_stats[field] - self.reference_stats[field], suffix)

    # get trend for a field
    def get_trend (self, field, use_raw = False):
        if not field in self.latest_stats:
            return 0

        if len(self.history) < 5:
            return 0
        
        field_samples = [sample[field] for sample in self.history]

        if use_raw:
            return calculate_diff_raw(field_samples)
        else:
            return calculate_diff(field_samples)


    def get_trend_gui (self, field, show_value = True, use_raw = False, up_color = 'red', down_color = 'green'):
        v = self.get_trend(field, use_raw)

        value = abs(v)
        arrow = u'\u25b2' if v > 0 else u'\u25bc'
        color = up_color if v > 0 else down_color

        # change in 1% is not meaningful
        if value < 1:
            return ""

        elif value > 5:

            if show_value:
                return format_text(u"{0}{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text(u"{0}{0}{0}".format(arrow), color)

        elif value > 2:

            if show_value:
                return format_text(u"{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text(u"{0}{0}".format(arrow), color)

        else:
            if show_value:
                return format_text(u"{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text(u"{0}".format(arrow), color)



class CGlobalStats(CTRexStats):

    def __init__(self, connection_info, server_version, ports_dict_ref):
        super(CGlobalStats, self).__init__()
        self.connection_info = connection_info
        self.server_version = server_version
        self._ports_dict = ports_dict_ref

    def generate_stats(self):
        return OrderedDict([("connection", "{host}, Port {port}".format(host=self.connection_info.get("server"),
                                                                     port=self.connection_info.get("sync_port"))),
                             ("version", "{ver}, UUID: {uuid}".format(ver=self.server_version.get("version", "N/A"),
                                                                      uuid="N/A")),

                             ("cpu_util", u"{0}% {1}".format( format_threshold(self.get("m_cpu_util"), [85, 100], [0, 85]),
                                                              self.get_trend_gui("m_cpu_util", use_raw = True))),

                             ("total_tx", u"{0} {1}".format( self.get("m_tx_bps", format=True, suffix="b/sec"),
                                                              self.get_trend_gui("m_tx_bps"))),

                             ("total_rx", u"{0} {1}".format( self.get("m_rx_bps", format=True, suffix="b/sec"),
                                                              self.get_trend_gui("m_rx_bps"))),

                             ("total_pps", u"{0} {1}".format( self.get("m_tx_pps", format=True, suffix="pkt/sec"),
                                                              self.get_trend_gui("m_tx_pps"))),

                             ("total_streams", sum([len(port_obj.streams)
                                                    for _, port_obj in self._ports_dict.iteritems()])),
                             ("active_ports", sum([port_obj.is_active()
                                                   for _, port_obj in self._ports_dict.iteritems()]))
                             ]
                            )

class CPortStats(CTRexStats):

    def __init__(self, port_obj):
        super(CPortStats, self).__init__()
        self._port_obj = port_obj

    def generate_stats(self):
        return {"owner": self._port_obj.user,
                "state": self._port_obj.get_port_state_name(),
                "--": "",
                "opackets" : self.get_rel("opackets"),
                "obytes"   : self.get_rel("obytes"),
                "ipackets" : self.get_rel("ipackets"),
                "ibytes"   : self.get_rel("ibytes"),
                "ierrors"  : self.get_rel("ierrors"),
                "oerrors"  : self.get_rel("oerrors"),

                "tx-bytes": self.get_rel("obytes", format = True, suffix = "B"),
                "rx-bytes": self.get_rel("ibytes", format = True, suffix = "B"),
                "tx-pkts": self.get_rel("opackets", format = True, suffix = "pkts"),
                "rx-pkts": self.get_rel("ipackets", format = True, suffix = "pkts"),

                "---": "",
                "Tx bps": u"{0} {1}".format(self.get_trend_gui("m_total_tx_bps", show_value = False, up_color = None, down_color = None),
                                            self.get("m_total_tx_bps", format = True, suffix = "bps")),

                "Rx bps": u"{0} {1}".format(self.get_trend_gui("m_total_rx_bps", show_value = False, up_color = None, down_color = None),
                                            self.get("m_total_rx_bps", format = True, suffix = "bps")),

                "----": "",
                "Tx pps": u"{0} {1}".format(self.get_trend_gui("m_total_tx_pps", show_value = False, up_color = None, down_color = None),
                                            self.get("m_total_tx_pps", format = True, suffix = "pps")),

                "Rx pps": u"{0} {1}".format(self.get_trend_gui("m_total_rx_pps", show_value = False, up_color = None, down_color = None),
                                            self.get("m_total_rx_pps", format = True, suffix = "pps")),

                }



if __name__ == "__main__":
    pass
