#!/router/bin/python

from utils import text_tables
from utils.text_opts import format_text, format_threshold, format_num

from trex_stl_async_client import CTRexAsyncStats

from collections import namedtuple, OrderedDict, deque
import copy
import datetime
import time
import re
import math
import copy
import threading
import pprint

GLOBAL_STATS = 'g'
PORT_STATS = 'p'
PORT_STATUS = 'ps'
STREAMS_STATS = 's'

ALL_STATS_OPTS = {GLOBAL_STATS, PORT_STATS, PORT_STATUS, STREAMS_STATS}
COMPACT = {GLOBAL_STATS, PORT_STATS}
SS_COMPAT = {GLOBAL_STATS, STREAMS_STATS}

ExportableStats = namedtuple('ExportableStats', ['raw_data', 'text_table'])

# deep mrege of dicts dst = src + dst
def deep_merge_dicts (dst, src):
    for k, v in src.iteritems():
        # if not exists - deep copy it
        if not k in dst:
            dst[k] = copy.deepcopy(v)
        else:
            if isinstance(v, dict):
                deep_merge_dicts(dst[k], v)

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
    STLClient and the ports.
    """

    def __init__(self, global_stats_ref, ports_dict_ref, rx_stats_ref):
        self._global_stats = global_stats_ref
        self._ports_dict = ports_dict_ref
        self._rx_stats_ref = rx_stats_ref

    def generate_single_statistic(self, port_id_list, statistic_type):
        if statistic_type == GLOBAL_STATS:
            return self._generate_global_stats()

        elif statistic_type == PORT_STATS:
            return self._generate_port_stats(port_id_list)

        elif statistic_type == PORT_STATUS:
            return self._generate_port_status(port_id_list)

        elif statistic_type == STREAMS_STATS:
            return self._generate_streams_stats()
        else:
            # ignore by returning empty object
            return {}

    def generate_streams_info(self, port_id_list, stream_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)
        return_data = OrderedDict()

        for port_obj in relevant_ports:
            streams_data = self._generate_single_port_streams_info(port_obj, stream_id_list)
            if not streams_data:
                continue
            hdr_key = "Port {port}:".format(port= port_obj.port_id)

            # TODO: test for other ports with same stream structure, and join them
            return_data[hdr_key] = streams_data

        return return_data

    def _generate_global_stats(self):
        stats_data = self._global_stats.generate_stats()

        # build table representation
        stats_table = text_tables.TRexTextInfo()
        stats_table.set_cols_align(["l", "l"])

        stats_table.add_rows([[k.replace("_", " ").title(), v]
                              for k, v in stats_data.iteritems()],
                             header=False)

        return {"global_statistics": ExportableStats(stats_data, stats_table)}

    def _generate_streams_stats (self):
      
        sstats_data = self._rx_stats_ref.generate_stats()
        streams_keys = self._rx_stats_ref.get_streams_keys()
        stream_count = len(streams_keys)


        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([10] + [17]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)

        stats_table.add_rows([[k] + v
                              for k, v in sstats_data.iteritems()],
                              header=False)

        header = ["PG ID"] + [key for key in streams_keys]
        stats_table.header(header)

        return {"streams_statistics": ExportableStats(sstats_data, stats_table)}

        

        per_stream_stats = OrderedDict([("owner", []),
                                       ("state", []),
                                       ("--", []),
                                       ("Tx bps L2", []),
                                       ("Tx bps L1", []),
                                       ("Tx pps", []),
                                       ("Line Util.", []),

                                       ("---", []),
                                       ("Rx bps", []),
                                       ("Rx pps", []),

                                       ("----", []),
                                       ("opackets", []),
                                       ("ipackets", []),
                                       ("obytes", []),
                                       ("ibytes", []),
                                       ("tx-bytes", []),
                                       ("rx-bytes", []),
                                       ("tx-pkts", []),
                                       ("rx-pkts", []),

                                       ("-----", []),
                                       ("oerrors", []),
                                       ("ierrors", []),

                                      ]
                                      )

        total_stats = CPortStats(None)

        for port_obj in relevant_ports:
            # fetch port data
            port_stats = port_obj.generate_port_stats()

            total_stats += port_obj.port_stats

            # populate to data structures
            return_stats_data[port_obj.port_id] = port_stats
            self.__update_per_field_dict(port_stats, per_field_stats)

        total_cols = len(relevant_ports)
        header = ["port"] + [port.port_id for port in relevant_ports]

        if (total_cols > 1):
            self.__update_per_field_dict(total_stats.generate_stats(), per_field_stats)
            header += ['total']
            total_cols += 1

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * total_cols)
        stats_table.set_cols_width([10] + [17]   * total_cols)
        stats_table.set_cols_dtype(['t'] + ['t'] * total_cols)

        stats_table.add_rows([[k] + v
                              for k, v in per_field_stats.iteritems()],
                              header=False)

        stats_table.header(header)

        return {"streams_statistics": ExportableStats(return_stats_data, stats_table)}


    def _generate_port_stats(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_stats = OrderedDict([("owner", []),
                                       ("state", []),
                                       ("--", []),
                                       ("Tx bps L2", []),
                                       ("Tx bps L1", []),
                                       ("Tx pps", []),
                                       ("Line Util.", []),

                                       ("---", []),
                                       ("Rx bps", []),
                                       ("Rx pps", []),

                                       ("----", []),
                                       ("opackets", []),
                                       ("ipackets", []),
                                       ("obytes", []),
                                       ("ibytes", []),
                                       ("tx-bytes", []),
                                       ("rx-bytes", []),
                                       ("tx-pkts", []),
                                       ("rx-pkts", []),

                                       ("-----", []),
                                       ("oerrors", []),
                                       ("ierrors", []),

                                      ]
                                      )

        total_stats = CPortStats(None)

        for port_obj in relevant_ports:
            # fetch port data
            port_stats = port_obj.generate_port_stats()

            total_stats += port_obj.port_stats

            # populate to data structures
            return_stats_data[port_obj.port_id] = port_stats
            self.__update_per_field_dict(port_stats, per_field_stats)

        total_cols = len(relevant_ports)
        header = ["port"] + [port.port_id for port in relevant_ports]

        if (total_cols > 1):
            self.__update_per_field_dict(total_stats.generate_stats(), per_field_stats)
            header += ['total']
            total_cols += 1

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * total_cols)
        stats_table.set_cols_width([10] + [17]   * total_cols)
        stats_table.set_cols_dtype(['t'] + ['t'] * total_cols)

        stats_table.add_rows([[k] + v
                              for k, v in per_field_stats.iteritems()],
                              header=False)

        stats_table.header(header)

        return {"port_statistics": ExportableStats(return_stats_data, stats_table)}

    def _generate_port_status(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_status = OrderedDict([("driver", []),
                                        ("maximum", []),
                                        ("status", []),
                                        ("promiscuous", []),
                                        ("--", []),
                                         ("HW src mac", []),
                                        ("SW src mac", []),
                                        ("SW dst mac", []),
                                        ("---", []),
                                        ("PCI Address", []),
                                        ("NUMA Node", []),
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
        stats_table.set_cols_width([15] + [20] * len(relevant_ports))

        stats_table.add_rows([[k] + v
                              for k, v in per_field_status.iteritems()],
                             header=False)
        stats_table.header(["port"] + [port.port_id
                                       for port in relevant_ports])

        return {"port_status": ExportableStats(return_stats_data, stats_table)}

    def _generate_single_port_streams_info(self, port_obj, stream_id_list):

        return_streams_data = port_obj.generate_loaded_streams_sum()

        if not return_streams_data.get("streams"):
            # we got no streams available
            return None

        # FORMAT VALUES ON DEMAND

        # because we mutate this - deep copy before
        return_streams_data = copy.deepcopy(return_streams_data)

        p_type_field_len = 0

        for stream_id, stream_id_sum in return_streams_data['streams'].iteritems():
            stream_id_sum['packet_type'] = self._trim_packet_headers(stream_id_sum['packet_type'], 30)
            p_type_field_len = max(p_type_field_len, len(stream_id_sum['packet_type']))

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["l"] + ["r"] + ["c"] + ["r"] + ["c"])
        info_table.set_cols_width([10]   + [p_type_field_len]  + [8]   + [16]  + [15]  + [12])
        info_table.set_cols_dtype(["t"] + ["t"] + ["t"] + ["t"] + ["t"] + ["t"])

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
        self.reference_stats = {}
        self.latest_stats = {}
        self.last_update_ts = time.time()
        self.history = deque(maxlen = 10)
        self.lock = threading.Lock()
        self.has_baseline = False

    ######## abstract methods ##########

    # get stats for user / API
    def get_stats (self):
        raise NotImplementedError()

    # generate format stats (for TUI)
    def generate_stats(self):
        raise NotImplementedError()

    # called when a snapshot arrives - add more fields
    def _update (self, snapshot, baseline):
        raise NotImplementedError()


    ######## END abstract methods ##########

    def update(self, snapshot, baseline):

        if not self.has_baseline and not baseline:
            return

        rc = self._update(snapshot)
        if not rc:
            return

        # sync one time
        if not self.has_baseline and baseline:
            self.reference_stats = copy.deepcopy(self.latest_stats)
            self.has_baseline = True

        # save history
        with self.lock:
            self.history.append(self.latest_stats)


    def clear_stats(self):
        self.reference_stats = copy.deepcopy(self.latest_stats)


    def invalidate (self):
        self.latest_stats = {}


    def _get (self, src, field, default = None):
        if isinstance(field, list):
            # deep
            value = src
            for level in field:
                if not level in value:
                    return default
                value = value[level]
        else:
            # flat
            if not field in src:
                return default
            value = src[field]

        return value

    def get(self, field, format=False, suffix=""):
        value = self._get(self.latest_stats, field)
        if value == None:
            return "N/A"

        return value if not format else format_num(value, suffix)


    def get_rel(self, field, format=False, suffix=""):
        
        ref_value = self._get(self.reference_stats, field, default = 0)
        latest_value = self._get(self.latest_stats, field)

        # latest value is an aggregation - must contain the value
        if latest_value == None:
            return "N/A"


        value = latest_value - ref_value

        return value if not format else format_num(value, suffix)


    # get trend for a field
    def get_trend (self, field, use_raw = False, percision = 10.0):
        if not field in self.latest_stats:
            return 0

        # not enough history - no trend
        if len(self.history) < 5:
            return 0

        # absolute value is too low 0 considered noise
        if self.latest_stats[field] < percision:
            return 0
        
        # must lock, deque is not thread-safe for iteration
        with self.lock:
            field_samples = [sample[field] for sample in self.history]

        if use_raw:
            return calculate_diff_raw(field_samples)
        else:
            return calculate_diff(field_samples)


    def get_trend_gui (self, field, show_value = False, use_raw = False, up_color = 'red', down_color = 'green'):
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

    def get_stats (self):
        stats = {}

        # absolute
        stats['cpu_util'] = self.get("m_cpu_util")
        stats['tx_bps'] = self.get("m_tx_bps")
        stats['tx_pps'] = self.get("m_tx_pps")

        stats['rx_bps'] = self.get("m_rx_bps")
        stats['rx_pps'] = self.get("m_rx_pps")
        stats['rx_drop_bps'] = self.get("m_rx_drop_bps")

        # relatives
        stats['queue_full'] = self.get_rel("m_total_queue_full")

        return stats


    def pre_update (self, snapshot):
        # L1 bps
        bps = snapshot.get("m_tx_bps")
        pps = snapshot.get("m_tx_pps")

        if pps > 0:
            avg_pkt_size = bps / (pps * 8.0)
            bps_L1  = bps * ( (avg_pkt_size + 20.0) / avg_pkt_size )
        else:
            bps_L1 = 0.0

        snapshot['m_tx_bps_L1'] = bps_L1


    def _update(self, snapshot):

        self.pre_update(snapshot)

        # simple...
        self.latest_stats = snapshot

        return True


    def generate_stats(self):
        return OrderedDict([("connection", "{host}, Port {port}".format(host=self.connection_info.get("server"),
                                                                     port=self.connection_info.get("sync_port"))),
                             ("version", "{ver}, UUID: {uuid}".format(ver=self.server_version.get("version", "N/A"),
                                                                      uuid="N/A")),

                             ("cpu_util", u"{0}% {1}".format( format_threshold(self.get("m_cpu_util"), [85, 100], [0, 85]),
                                                              self.get_trend_gui("m_cpu_util", use_raw = True))),

                             (" ", ""),

                             ("total_tx_L2", u"{0} {1}".format( self.get("m_tx_bps", format=True, suffix="b/sec"),
                                                                self.get_trend_gui("m_tx_bps"))),

                            ("total_tx_L1", u"{0} {1}".format( self.get("m_tx_bps_L1", format=True, suffix="b/sec"),
                                                                self.get_trend_gui("m_tx_bps_L1"))),

                             ("total_rx", u"{0} {1}".format( self.get("m_rx_bps", format=True, suffix="b/sec"),
                                                              self.get_trend_gui("m_rx_bps"))),

                             ("total_pps", u"{0} {1}".format( self.get("m_tx_pps", format=True, suffix="pkt/sec"),
                                                              self.get_trend_gui("m_tx_pps"))),

                             ("  ", ""),

                             ("drop_rate", "{0}".format( format_num(self.get("m_rx_drop_bps"),
                                                                    suffix = 'b/sec',
                                                                    opts = 'green' if (self.get("m_rx_drop_bps")== 0) else 'red'))),

                             ("queue_full", "{0}".format( format_num(self.get_rel("m_total_queue_full"),
                                                                     suffix = 'pkts',
                                                                     compact = False,
                                                                     opts = 'green' if (self.get_rel("m_total_queue_full")== 0) else 'red'))),

                             ]
                            )

class CPortStats(CTRexStats):

    def __init__(self, port_obj):
        super(CPortStats, self).__init__()
        self._port_obj = port_obj

    @staticmethod
    def __merge_dicts (target, src):
        for k, v in src.iteritems():
            if k in target:
                target[k] += v
            else:
                target[k] = v


    def __add__ (self, x):
        if not isinstance(x, CPortStats):
            raise TypeError("cannot add non stats object to stats")

        # main stats
        if not self.latest_stats:
            self.latest_stats = {}

        self.__merge_dicts(self.latest_stats, x.latest_stats)

        # reference stats
        if x.reference_stats:
            if not self.reference_stats:
                self.reference_stats = x.reference_stats.copy()
            else:
                self.__merge_dicts(self.reference_stats, x.reference_stats)

        # history
        if not self.history:
            self.history = copy.deepcopy(x.history)
        else:
            for h1, h2 in zip(self.history, x.history):
                self.__merge_dicts(h1, h2)

        return self

    # for port we need to do something smarter
    def get_stats (self):
        stats = {}

        stats['opackets'] = self.get_rel("opackets")
        stats['ipackets'] = self.get_rel("ipackets")
        stats['obytes']   = self.get_rel("obytes")
        stats['ibytes']   = self.get_rel("ibytes")
        stats['oerrors']  = self.get_rel("oerrors")
        stats['ierrors']  = self.get_rel("ierrors")
        stats['tx_bps']   = self.get("m_total_tx_bps")
        stats['tx_pps']   = self.get("m_total_tx_pps")
        stats['rx_bps']   = self.get("m_total_rx_bps")
        stats['rx_pps']   = self.get("m_total_rx_pps")

        return stats


    def pre_update (self, snapshot):
        # L1 bps
        bps = snapshot.get("m_total_tx_bps")
        pps = snapshot.get("m_total_tx_pps")

        if pps > 0:
            avg_pkt_size = bps / (pps * 8.0)
            bps_L1  = bps * ( (avg_pkt_size + 20.0) / avg_pkt_size )
        else:
            bps_L1 = 0.0

        snapshot['m_total_tx_bps_L1'] = bps_L1
        snapshot['m_percentage'] = (bps_L1 / self._port_obj.get_speed_bps()) * 100


    def _update(self, snapshot):

        self.pre_update(snapshot)

        # simple...
        self.latest_stats = snapshot

        return True


    def generate_stats(self):

        state = self._port_obj.get_port_state_name() if self._port_obj else "" 
        if state == "ACTIVE":
            state = format_text(state, 'green', 'bold')
        elif state == "PAUSE":
            state = format_text(state, 'magenta', 'bold')
        else:
            state = format_text(state, 'bold')


        return {"owner": self._port_obj.user if self._port_obj else "",
                "state": "{0}".format(state),

                "--": " ",
                "---": " ",
                "----": " ",
                "-----": " ",

                "Tx bps L1": u"{0} {1}".format(self.get_trend_gui("m_total_tx_bps_L1", show_value = False),
                                               self.get("m_total_tx_bps_L1", format = True, suffix = "bps")),

                "Tx bps L2": u"{0} {1}".format(self.get_trend_gui("m_total_tx_bps", show_value = False),
                                               self.get("m_total_tx_bps", format = True, suffix = "bps")),

                "Line Util.": u"{0} {1}".format(self.get_trend_gui("m_percentage", show_value = False),
                                                format_text(
                                                    self.get("m_percentage", format = True, suffix = "%") if self._port_obj else "",
                                                    'bold')),

                "Rx bps": u"{0} {1}".format(self.get_trend_gui("m_total_rx_bps", show_value = False),
                                            self.get("m_total_rx_bps", format = True, suffix = "bps")),
                  
                "Tx pps": u"{0} {1}".format(self.get_trend_gui("m_total_tx_pps", show_value = False),
                                            self.get("m_total_tx_pps", format = True, suffix = "pps")),

                "Rx pps": u"{0} {1}".format(self.get_trend_gui("m_total_rx_pps", show_value = False),
                                            self.get("m_total_rx_pps", format = True, suffix = "pps")),

                 "opackets" : self.get_rel("opackets"),
                 "ipackets" : self.get_rel("ipackets"),
                 "obytes"   : self.get_rel("obytes"),
                 "ibytes"   : self.get_rel("ibytes"),

                 "tx-bytes": self.get_rel("obytes", format = True, suffix = "B"),
                 "rx-bytes": self.get_rel("ibytes", format = True, suffix = "B"),
                 "tx-pkts": self.get_rel("opackets", format = True, suffix = "pkts"),
                 "rx-pkts": self.get_rel("ipackets", format = True, suffix = "pkts"),

                 "oerrors"  : format_num(self.get_rel("oerrors"),
                                         compact = False,
                                         opts = 'green' if (self.get_rel("oerrors")== 0) else 'red'),

                 "ierrors"  : format_num(self.get_rel("ierrors"),
                                         compact = False,
                                         opts = 'green' if (self.get_rel("ierrors")== 0) else 'red'),

                }




# RX stats objects - COMPLEX :-(
class CRxStats(CTRexStats):
    def __init__(self):
        super(CRxStats, self).__init__()

    def bps_L1 (self, bps, pps):
        if pps == 0:
            return 0

        factor = bps / (pps * 8.0)
        return bps * ( 1 + (20 / factor) )

    def calculate_diff_sec (self, current, prev):
        if not 'ts' in current:
            raise ValueError("INTERNAL ERROR: RX stats snapshot MUST contain 'ts' field")

        if prev:
            prev_ts   = prev['ts']
            now_ts    = current['ts']
            diff_sec  = (now_ts['value'] - prev_ts['value']) / float(now_ts['freq'])
        else:
            diff_sec = 0.0

        return diff_sec


    def process_single_pg (self, current_pg, prev_pg):

        # start with the previous PG
        output = copy.deepcopy(prev_pg)

        for field in ['tx_pkts', 'tx_bytes', 'rx_pkts']:
            if not field in output:
                output[field] = {}

            if field in current_pg:
                for port, pv in current_pg[field].iteritems():
                    if not self.is_intable(port):
                        continue

                    output[field][port] = pv

            # sum up
            total = 0
            for port, pv in output[field].iteritems():
                if not self.is_intable(port):
                    continue
                total += pv

            output[field]['total'] = total

        return output
            

    def is_intable (self, value):
        try:
            int(value)
            return True
        except ValueError:
            return False

    def process_snapshot (self, current, prev):

        # timestamp
        diff_sec = self.calculate_diff_sec(current, prev)

        # final output
        output = {}

        # copy timestamp field
        output['ts'] = current['ts']

        pg_ids = set(prev.keys() + current.keys())

        for pg_id in pg_ids:
            if not self.is_intable(pg_id):
                continue

            current_pg = current.get(pg_id, {})
            prev_pg = prev.get(pg_id, {})
            
            if current_pg.get('first_time'):
                # new value - ignore history
                output[pg_id] = self.process_single_pg(current_pg, {})
                self.reference_stats[pg_id] = {}
                self.calculate_bw_for_pg(output[pg_id], prev_pg, 0)
            else:
                # aggregate
                output[pg_id] = self.process_single_pg(current_pg, prev_pg)

                self.calculate_bw_for_pg(output[pg_id], prev_pg, diff_sec)


        return output



    def calculate_bw_for_pg (self, pg_current, pg_prev, diff_sec):

        if diff_sec == 0:
            pg_current['tx_pps'] = 0.0
            pg_current['tx_bps'] = 0.0
            pg_current['tx_bps_L1'] = 0.0
            pg_current['rx_pps'] = 0.0
            pg_current['rx_bps'] = 0.0
            return


        # prev
        prev_tx_pkts  = pg_prev['tx_pkts']['total']
        prev_tx_bytes = pg_prev['tx_bytes']['total']
        prev_tx_pps   = pg_prev['tx_pps']
        prev_tx_bps   = pg_prev['tx_bps']

        # now
        now_tx_pkts  = pg_current['tx_pkts']['total']
        now_tx_bytes = pg_current['tx_bytes']['total']

        if not (now_tx_pkts >= prev_tx_pkts):
            print "CURRENT:\n"
            pprint.pprint(pg_current)
            print "PREV:\n"
            pprint.pprint(pg_prev)
            assert(now_tx_pkts > prev_tx_pkts)

        pg_current['tx_pps'] = (0.5 * prev_tx_pps) + (0.5 * ( (now_tx_pkts - prev_tx_pkts) / diff_sec) )
        pg_current['tx_bps'] = (0.5 * prev_tx_bps) + (0.5 * ( (now_tx_bytes - prev_tx_bytes) * 8 / diff_sec) )

        pg_current['rx_pps'] = 0.0
        pg_current['rx_bps'] = 0.0

        pg_current['tx_bps_L1'] = self.bps_L1(pg_current['tx_bps'], pg_current['tx_pps'])




    def _update (self, snapshot):

        # generate a new snapshot
        new_snapshot = self.process_snapshot(snapshot, self.latest_stats)

        #print new_snapshot
        # advance
        self.latest_stats = new_snapshot


        return True
      


    # for API
    def get_stats (self):
        stats = {}

        for pg_id, value in self.latest_stats.iteritems():
            # skip non ints
            if not self.is_intable(pg_id):
                continue

            stats[int(pg_id)] = {}
            for field in ['tx_pkts', 'tx_bytes', 'rx_pkts']:
                stats[int(pg_id)][field] = {'total': self.get_rel([pg_id, field, 'total'])}

                for port, pv in value[field].iteritems():
                    try:
                        int(port)
                    except ValueError:
                        continue
                    stats[int(pg_id)][field][int(port)] = self.get_rel([pg_id, field, port])

        return stats



    def get_streams_keys (self):
        keys = []
        for key in self.latest_stats.keys():
            # ignore non user ID keys
            try:
                int(key)
                keys.append(key)
            except ValueError:
                continue

        return keys


    def generate_stats (self):

        pg_ids = self.get_streams_keys()
        cnt = len(pg_ids)

        formatted_stats = OrderedDict([ ('Tx pps',  []),
                                        ('Tx bps L2',      []),
                                        ('Tx bps L1',      []),
                                        ('---', [''] * cnt),
                                        ('Rx pps',      []),
                                        ('Rx bps',      []),
                                        ('----', [''] * cnt),
                                        ('opackets',    []),
                                        ('ipackets',    []),
                                        ('obytes',      []),
                                        ('ibytes',      []),
                                        ('-----', [''] * cnt),
                                        ('tx_pkts',     []),
                                        ('rx_pkts',     []),
                                        ('tx_bytes',    []),
                                        ('rx_bytes',    [])
                                      ])



        # maximum 4
        for pg_id in pg_ids:

            formatted_stats['Tx pps'].append(self.get([pg_id, 'tx_pps'], format = True, suffix = "pps"))
            formatted_stats['Tx bps L2'].append(self.get([pg_id, 'tx_bps'], format = True, suffix = "bps"))

            formatted_stats['Tx bps L1'].append(self.get([pg_id, 'tx_bps_L1'], format = True, suffix = "bps"))

            formatted_stats['Rx pps'].append(self.get([pg_id, 'rx_pps'], format = True, suffix = "pps"))
            formatted_stats['Rx bps'].append(self.get([pg_id, 'rx_bps'], format = True, suffix = "bps"))
            
            formatted_stats['opackets'].append(self.get_rel([pg_id, 'tx_pkts', 'total']))
            formatted_stats['ipackets'].append(self.get_rel([pg_id, 'rx_pkts', 'total']))
            formatted_stats['obytes'].append(self.get_rel([pg_id, 'tx_bytes', 'total']))
            formatted_stats['ibytes'].append(self.get_rel([pg_id, 'rx_bytes', 'total']))
            formatted_stats['tx_bytes'].append(self.get_rel([pg_id, 'tx_bytes', 'total'], format = True, suffix = "B"))
            formatted_stats['rx_bytes'].append(self.get_rel([pg_id, 'rx_bytes', 'total'], format = True, suffix = "B"))
            formatted_stats['tx_pkts'].append(self.get_rel([pg_id, 'tx_pkts', 'total'], format = True, suffix = "pkts"))
            formatted_stats['rx_pkts'].append(self.get_rel([pg_id, 'rx_pkts', 'total'], format = True, suffix = "pkts"))

      

        return formatted_stats

if __name__ == "__main__":
    pass

