#!/router/bin/python

from .utils import text_tables
from .utils.text_opts import format_text, format_threshold, format_num
from .trex_stl_types import StatNotAvailable, is_integer
from .trex_stl_exceptions import STLError

from collections import namedtuple, OrderedDict, deque
import sys
import copy
import datetime
import time
import re
import math
import threading
import pprint

GLOBAL_STATS = 'g'
PORT_STATS = 'p'
PORT_GRAPH = 'pg'
PORT_STATUS = 'ps'
STREAMS_STATS = 's'
LATENCY_STATS = 'ls'
LATENCY_HISTOGRAM = 'lh'
CPU_STATS = 'c'
MBUF_STATS = 'm'
EXTENDED_STATS = 'x'
EXTENDED_INC_ZERO_STATS = 'xz'

ALL_STATS_OPTS = [GLOBAL_STATS, PORT_STATS, PORT_STATUS, STREAMS_STATS, LATENCY_STATS, PORT_GRAPH, LATENCY_HISTOGRAM, CPU_STATS, MBUF_STATS, EXTENDED_STATS, EXTENDED_INC_ZERO_STATS]
COMPACT = [GLOBAL_STATS, PORT_STATS]
GRAPH_PORT_COMPACT = [GLOBAL_STATS, PORT_GRAPH]
SS_COMPAT = [GLOBAL_STATS, STREAMS_STATS] # stream stats
LS_COMPAT = [GLOBAL_STATS, LATENCY_STATS] # latency stats
LH_COMPAT = [GLOBAL_STATS, LATENCY_HISTOGRAM] # latency histogram
UT_COMPAT = [GLOBAL_STATS, CPU_STATS, MBUF_STATS] # utilization

ExportableStats = namedtuple('ExportableStats', ['raw_data', 'text_table'])

def round_float (f):
    return float("%.2f" % f) if type(f) is float else f

def try_int(i):
    try:
        return int(i)
    except:
        return i

# deep mrege of dicts dst = src + dst
def deep_merge_dicts (dst, src):
    for k, v in src.items():
        # if not exists - deep copy it
        if not k in dst:
            dst[k] = copy.deepcopy(v)
        else:
            if isinstance(v, dict):
                deep_merge_dicts(dst[k], v)

# BPS L1 from pps and BPS L2
def calc_bps_L1 (bps, pps):
    if (pps == 0) or (bps == 0):
        return 0

    factor = bps / (pps * 8.0)
    return bps * ( 1 + (20 / factor) )
#

def is_intable (value):
    try:
        int(value)
        return True
    except ValueError:
        return False

# use to calculate diffs relative to the previous values
# for example, BW
def calculate_diff (samples):
    total = 0.0

    weight_step = 1.0 / sum(range(0, len(samples)))
    weight = weight_step

    for i in range(0, len(samples) - 1):
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

    weight_step = 1.0 / sum(range(0, len(samples)))
    weight = weight_step

    for i in range(0, len(samples) - 1):
        current = samples[i]
        next = samples[i + 1]

        total  += ( (next - current) * weight )
        weight += weight_step

    return total

get_number_of_bytes_cache = {}
# get number of bytes: '64b'->64, '9kb'->9000 etc.
def get_number_of_bytes(val):
    if val not in get_number_of_bytes_cache:
        get_number_of_bytes_cache[val] = int(val[:-1].replace('k', '000'))
    return get_number_of_bytes_cache[val]

# a simple object to keep a watch over a field
class WatchedField(object):

    def __init__ (self, name, suffix, high_th, low_th, events_handler):
        self.name           = name
        self.suffix         = suffix
        self.high_th        = high_th
        self.low_th         = low_th
        self.events_handler = events_handler

        self.hot     = False
        self.current = None

    def update (self, value):
        if value is None:
            return

        if value > self.high_th and not self.hot:
            self.events_handler.log_warning("{0} is high: {1}{2}".format(self.name, value, self.suffix))
            self.hot = True

        if value < self.low_th and self.hot:
            self.hot = False

        self.current = value



class CTRexInfoGenerator(object):
    """
    This object is responsible of generating stats and information from objects maintained at
    STLClient and the ports.
    """

    def __init__(self, global_stats_ref, ports_dict_ref, rx_stats_ref, latency_stats_ref, util_stats_ref, xstats_ref, async_monitor):
        self._global_stats = global_stats_ref
        self._ports_dict = ports_dict_ref
        self._rx_stats_ref = rx_stats_ref
        self._latency_stats_ref = latency_stats_ref
        self._util_stats_ref = util_stats_ref
        self._xstats_ref = xstats_ref
        self._async_monitor = async_monitor

    def generate_single_statistic(self, port_id_list, statistic_type):
        if statistic_type == GLOBAL_STATS:
            return self._generate_global_stats()

        elif statistic_type == PORT_STATS:
            return self._generate_port_stats(port_id_list)

        elif statistic_type == PORT_GRAPH:
            return self._generate_port_graph(port_id_list)

        elif statistic_type == PORT_STATUS:
            return self._generate_port_status(port_id_list)

        elif statistic_type == STREAMS_STATS:
            return self._generate_streams_stats()

        elif statistic_type == LATENCY_STATS:
            return self._generate_latency_stats()

        elif statistic_type == LATENCY_HISTOGRAM:
            return self._generate_latency_histogram()

        elif statistic_type == CPU_STATS:
            return self._generate_cpu_util_stats()

        elif statistic_type == MBUF_STATS:
            return self._generate_mbuf_util_stats()

        elif statistic_type == EXTENDED_STATS:
            return self._generate_xstats(port_id_list, include_zero_lines = False)

        elif statistic_type == EXTENDED_INC_ZERO_STATS:
            return self._generate_xstats(port_id_list, include_zero_lines = True)

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
        global_stats = self._global_stats
     
        stats_data_left = OrderedDict([("connection", "{host}, Port {port}".format(host=global_stats.connection_info.get("server"),
                                                                     port=global_stats.connection_info.get("sync_port"))),
                             ("version", "{ver}, UUID: {uuid}".format(ver=global_stats.server_version.get("version", "N/A"),
                                                                      uuid="N/A")),

                             ("cpu_util.", "{0}% @ {2} cores ({3} per port) {1}".format( format_threshold(round_float(global_stats.get("m_cpu_util")), [85, 100], [0, 85]),
                                                                                         global_stats.get_trend_gui("m_cpu_util", use_raw = True),
                                                                                         global_stats.system_info.get('dp_core_count'),
                                                                                         global_stats.system_info.get('dp_core_count_per_port'),
                                                                                         )),

                             ("rx_cpu_util.", "{0}% {1}".format( format_threshold(round_float(global_stats.get("m_rx_cpu_util")), [85, 100], [0, 85]),
                                                                global_stats.get_trend_gui("m_rx_cpu_util", use_raw = True))),

                             ("async_util.", "{0}% / {1}".format( format_threshold(round_float(self._async_monitor.get_cpu_util()), [85, 100], [0, 85]),
                                                                 format_num(self._async_monitor.get_bps() / 8.0, suffix = "B/sec"))),
                            ])

        stats_data_right = OrderedDict([
                             ("total_tx_L2", "{0} {1}".format( global_stats.get("m_tx_bps", format=True, suffix="b/sec"),
                                                                global_stats.get_trend_gui("m_tx_bps"))),

                             ("total_tx_L1", "{0} {1}".format( global_stats.get("m_tx_bps_L1", format=True, suffix="b/sec"),
                                                                global_stats.get_trend_gui("m_tx_bps_L1"))),

                             ("total_rx", "{0} {1}".format( global_stats.get("m_rx_bps", format=True, suffix="b/sec"),
                                                              global_stats.get_trend_gui("m_rx_bps"))),

                             ("total_pps", "{0} {1}".format( global_stats.get("m_tx_pps", format=True, suffix="pkt/sec"),
                                                              global_stats.get_trend_gui("m_tx_pps"))),

                             ("drop_rate", "{0}".format( format_num(global_stats.get("m_rx_drop_bps"),
                                                                    suffix = 'b/sec',
                                                                    opts = 'green' if (global_stats.get("m_rx_drop_bps")== 0) else 'red'),
                                                            )),

                             ("queue_full", "{0}".format( format_num(global_stats.get_rel("m_total_queue_full"),
                                                                     suffix = 'pkts',
                                                                     compact = False,
                                                                     opts = 'green' if (global_stats.get_rel("m_total_queue_full")== 0) else 'red'))),
                             ])

        # build table representation
        stats_table = text_tables.TRexTextInfo()
        stats_table.set_cols_align(["l", "l"])
        stats_table.set_deco(0)
        stats_table.set_cols_width([50, 45])
        max_lines = max(len(stats_data_left), len(stats_data_right))
        for line_num in range(max_lines):
            row = []
            if line_num < len(stats_data_left):
                key = list(stats_data_left.keys())[line_num]
                row.append('{:<12} : {}'.format(key, stats_data_left[key]))
            else:
                row.append('')
            if line_num < len(stats_data_right):
                key = list(stats_data_right.keys())[line_num]
                row.append('{:<12} : {}'.format(key, stats_data_right[key]))
            else:
                row.append('')
            stats_table.add_row(row)

        return {"global_statistics": ExportableStats(None, stats_table)}

    def _generate_streams_stats (self):
        flow_stats = self._rx_stats_ref
        # for TUI - maximum 4 
        pg_ids = list(filter(is_intable, flow_stats.latest_stats.keys()))[:4]
        stream_count = len(pg_ids)

        sstats_data = OrderedDict([ ('Tx pps',  []),
                                        ('Tx bps L2',      []),
                                        ('Tx bps L1',      []),
                                        ('---', [''] * stream_count),
                                        ('Rx pps',      []),
                                        ('Rx bps',      []),
                                        ('----', [''] * stream_count),
                                        ('opackets',    []),
                                        ('ipackets',    []),
                                        ('obytes',      []),
                                        ('ibytes',      []),
                                        ('-----', [''] * stream_count),
                                        ('tx_pkts',     []),
                                        ('rx_pkts',     []),
                                        ('tx_bytes',    []),
                                        ('rx_bytes',    [])
                                      ])



        # maximum 4
        for pg_id in pg_ids:

            sstats_data['Tx pps'].append(flow_stats.get([pg_id, 'tx_pps_lpf', 'total'], format = True, suffix = "pps"))
            sstats_data['Tx bps L2'].append(flow_stats.get([pg_id, 'tx_bps_lpf', 'total'], format = True, suffix = "bps"))

            sstats_data['Tx bps L1'].append(flow_stats.get([pg_id, 'tx_bps_L1_lpf', 'total'], format = True, suffix = "bps"))

            sstats_data['Rx pps'].append(flow_stats.get([pg_id, 'rx_pps_lpf', 'total'], format = True, suffix = "pps"))
            sstats_data['Rx bps'].append(flow_stats.get([pg_id, 'rx_bps_lpf', 'total'], format = True, suffix = "bps"))
            
            sstats_data['opackets'].append(flow_stats.get_rel([pg_id, 'tx_pkts', 'total']))
            sstats_data['ipackets'].append(flow_stats.get_rel([pg_id, 'rx_pkts', 'total']))
            sstats_data['obytes'].append(flow_stats.get_rel([pg_id, 'tx_bytes', 'total']))
            sstats_data['ibytes'].append(flow_stats.get_rel([pg_id, 'rx_bytes', 'total']))
            sstats_data['tx_bytes'].append(flow_stats.get_rel([pg_id, 'tx_bytes', 'total'], format = True, suffix = "B"))
            sstats_data['rx_bytes'].append(flow_stats.get_rel([pg_id, 'rx_bytes', 'total'], format = True, suffix = "B"))
            sstats_data['tx_pkts'].append(flow_stats.get_rel([pg_id, 'tx_pkts', 'total'], format = True, suffix = "pkts"))
            sstats_data['rx_pkts'].append(flow_stats.get_rel([pg_id, 'rx_pkts', 'total'], format = True, suffix = "pkts"))


        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([10] + [17]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)

        stats_table.add_rows([[k] + v
                              for k, v in sstats_data.items()],
                              header=False)

        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)

        return {"streams_statistics": ExportableStats(sstats_data, stats_table)}

    def _generate_latency_stats(self):
        lat_stats = self._latency_stats_ref
        latency_window_size = 14

        # for TUI - maximum 5 
        pg_ids = list(filter(is_intable, lat_stats.latest_stats.keys()))[:5]
        stream_count = len(pg_ids)
        lstats_data = OrderedDict([('TX pkts',       []),
                                   ('RX pkts',       []),
                                   ('Max latency',   []),
                                   ('Avg latency',   []),
                                   ('-- Window --', [''] * stream_count),
                                   ('Last (max)',     []),
                                  ] + [('Last-%s' % i, []) for i in range(1, latency_window_size)] + [
                                   ('---', [''] * stream_count),
                                   ('Jitter',        []),
                                   ('----', [''] * stream_count),
                                   ('Errors',        []),
                                  ])

        with lat_stats.lock:
            history = [x for x in lat_stats.history]
        flow_stats = self._rx_stats_ref.get_stats()
        for pg_id in pg_ids:
            lstats_data['TX pkts'].append(flow_stats[pg_id]['tx_pkts']['total'] if pg_id in flow_stats else '')
            lstats_data['RX pkts'].append(flow_stats[pg_id]['rx_pkts']['total'] if pg_id in flow_stats else '')
            lstats_data['Avg latency'].append(try_int(lat_stats.get([pg_id, 'latency', 'average'])))
            lstats_data['Max latency'].append(try_int(lat_stats.get([pg_id, 'latency', 'total_max'])))
            lstats_data['Last (max)'].append(try_int(lat_stats.get([pg_id, 'latency', 'last_max'])))
            for i in range(1, latency_window_size):
                val = history[-i - 1].get(pg_id, {}).get('latency', {}).get('last_max', '') if len(history) > i else ''
                lstats_data['Last-%s' % i].append(try_int(val))
            lstats_data['Jitter'].append(try_int(lat_stats.get([pg_id, 'latency', 'jitter'])))
            errors = 0
            seq_too_low = lat_stats.get([pg_id, 'err_cntrs', 'seq_too_low'])
            if is_integer(seq_too_low):
                errors += seq_too_low
            seq_too_high = lat_stats.get([pg_id, 'err_cntrs', 'seq_too_high'])
            if is_integer(seq_too_high):
                errors += seq_too_high
            lstats_data['Errors'].append(format_num(errors,
                                            opts = 'green' if errors == 0 else 'red'))


        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([12] + [14]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)
        stats_table.add_rows([[k] + v
                              for k, v in lstats_data.items()],
                              header=False)

        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)

        return {"latency_statistics": ExportableStats(lstats_data, stats_table)}

    def _generate_latency_histogram(self):
        lat_stats = self._latency_stats_ref.latest_stats
        max_histogram_size = 17

        # for TUI - maximum 5 
        pg_ids = list(filter(is_intable, lat_stats.keys()))[:5]

        merged_histogram = {}
        for pg_id in pg_ids:
            merged_histogram.update(lat_stats[pg_id]['latency']['histogram'])
        histogram_size = min(max_histogram_size, len(merged_histogram))

        stream_count = len(pg_ids)
        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([12] + [14]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)

        for i in range(max_histogram_size - histogram_size):
            if i == 0 and not merged_histogram:
                stats_table.add_row(['  No Data'] + [' '] * stream_count)
            else:
                stats_table.add_row([' '] * (stream_count + 1))
        for key in list(reversed(sorted(merged_histogram.keys())))[:histogram_size]:
            hist_vals = []
            for pg_id in pg_ids:
                hist_vals.append(lat_stats[pg_id]['latency']['histogram'].get(key, ' '))
            stats_table.add_row([key] + hist_vals)

        stats_table.add_row(['- Counters -'] + [' '] * stream_count)
        err_cntrs_dict = OrderedDict()
        for pg_id in pg_ids:
            for err_cntr in sorted(lat_stats[pg_id]['err_cntrs'].keys()):
                if err_cntr not in err_cntrs_dict:
                    err_cntrs_dict[err_cntr] = [lat_stats[pg_id]['err_cntrs'][err_cntr]]
                else:
                    err_cntrs_dict[err_cntr].append(lat_stats[pg_id]['err_cntrs'][err_cntr])
        for err_cntr, val_list in err_cntrs_dict.items():
            stats_table.add_row([err_cntr] + val_list)
        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)
        return {"latency_histogram": ExportableStats(None, stats_table)}

    def _generate_cpu_util_stats(self):
        util_stats = self._util_stats_ref.get_stats(use_1sec_cache = True)
        
        stats_table = text_tables.TRexTextTable()
        if util_stats:
            if 'cpu' not in util_stats:
                raise Exception("Excepting 'cpu' section in stats %s" % util_stats)
            cpu_stats = util_stats['cpu']
            hist_len = len(cpu_stats[0]["history"])
            avg_len = min(5, hist_len)
            show_len = min(15, hist_len)
            stats_table.header(['Thread', 'Avg', 'Latest'] + list(range(-1, 0 - show_len, -1)))
            stats_table.set_cols_align(['l'] + ['r'] * (show_len + 1))
            stats_table.set_cols_width([10, 3, 6] + [3] * (show_len - 1))
            stats_table.set_cols_dtype(['t'] * (show_len + 2))

            for i in range(min(18, len(cpu_stats))):
                history = cpu_stats[i]["history"]
                ports = cpu_stats[i]["ports"]
                avg = int(round(sum(history[:avg_len]) / avg_len))

                # decode active ports for core
                if ports == [-1, -1]:
                    interfaces = "(IDLE)"
                elif not -1 in ports:
                    interfaces = "({:},{:})".format(ports[0], ports[1])
                else:
                    interfaces = "({:})".format(ports[0] if ports[0] != -1 else ports[1])

                thread = "{:2} {:^7}".format(i, interfaces)
                stats_table.add_row([thread, avg] + history[:show_len])
        else:
            stats_table.add_row(['No Data.'])
        return {'cpu_util(%)': ExportableStats(None, stats_table)}

    def _generate_mbuf_util_stats(self):
        util_stats = self._util_stats_ref.get_stats(use_1sec_cache = True)
        stats_table = text_tables.TRexTextTable()
        if util_stats:
            if 'mbuf_stats' not in util_stats:
                raise Exception("Excepting 'mbuf_stats' section in stats %s" % util_stats)
            mbuf_stats = util_stats['mbuf_stats']
            for mbufs_per_socket in mbuf_stats.values():
                first_socket_mbufs = mbufs_per_socket
                break
            if not self._util_stats_ref.mbuf_types_list:
                mbuf_keys = list(first_socket_mbufs.keys())
                mbuf_keys.sort(key = get_number_of_bytes)
                self._util_stats_ref.mbuf_types_list = mbuf_keys
            types_len = len(self._util_stats_ref.mbuf_types_list)
            stats_table.set_cols_align(['l'] + ['r'] * (types_len + 1))
            stats_table.set_cols_width([10] + [7] * (types_len + 1))
            stats_table.set_cols_dtype(['t'] * (types_len + 2))
            stats_table.header([''] + self._util_stats_ref.mbuf_types_list + ['RAM(MB)'])
            total_list = []
            sum_totals = 0
            for mbuf_type in self._util_stats_ref.mbuf_types_list:
                sum_totals += first_socket_mbufs[mbuf_type][1] * get_number_of_bytes(mbuf_type) + 64
                total_list.append(first_socket_mbufs[mbuf_type][1])
            sum_totals *= len(list(mbuf_stats.values()))
            total_list.append(int(sum_totals/1e6))
            stats_table.add_row(['Total:'] + total_list)
            stats_table.add_row(['Used:'] + [''] * (types_len + 1))
            for socket_name in sorted(list(mbuf_stats.keys())):
                mbufs = mbuf_stats[socket_name]
                socket_show_name = socket_name.replace('cpu-', '').replace('-', ' ').capitalize() + ':'
                sum_used = 0
                used_list = []
                percentage_list = []
                for mbuf_type in self._util_stats_ref.mbuf_types_list:
                    used = mbufs[mbuf_type][1] - mbufs[mbuf_type][0]
                    sum_used += used * get_number_of_bytes(mbuf_type) + 64
                    used_list.append(used)
                    percentage_list.append('%s%%' % int(100 * used / mbufs[mbuf_type][1]))
                used_list.append(int(sum_used/1e6))
                stats_table.add_row([socket_show_name] + used_list)
                stats_table.add_row(['Percent:'] + percentage_list + [''])
        else:
            stats_table.add_row(['No Data.'])
        return {'mbuf_util': ExportableStats(None, stats_table)}

    def _generate_xstats(self, port_id_list, include_zero_lines = False):
        relevant_ports = [port.port_id for port in self.__get_relevant_ports(port_id_list)]
        # get the data on relevant ports
        xstats_data = OrderedDict()
        for port_id in relevant_ports:
            for key, val in self._xstats_ref.get_stats(port_id).items():
                if key not in xstats_data:
                    xstats_data[key] = []
                xstats_data[key].append(val)

        # put into table
        stats_table = text_tables.TRexTextTable()
        stats_table.header(['Name:'] + ['Port %s:' % port_id for port_id in relevant_ports])
        stats_table.set_cols_align(['l'] + ['r'] * len(relevant_ports))
        stats_table.set_cols_width([30] + [15] * len(relevant_ports))
        stats_table.set_cols_dtype(['t'] * (len(relevant_ports) + 1))
        for key, arr in xstats_data.items():
            if include_zero_lines or list(filter(None, arr)):
                if len(key) > 28:
                    key = key[:28]
                stats_table.add_row([key] + arr)
        return {'xstats:': ExportableStats(None, stats_table)}

    @staticmethod
    def _get_rational_block_char(value, range_start, interval):
        # in Konsole, utf-8 is sometimes printed with artifacts, return ascii for now
        #return 'X' if value >= range_start + float(interval) / 2 else ' '

        if sys.__stdout__.encoding != 'UTF-8':
            return 'X' if value >= range_start + float(interval) / 2 else ' '

        value -= range_start
        ratio = float(value) / interval
        if ratio <= 0.0625:
            return u' '         # empty block
        if ratio <= 0.1875:
            return u'\u2581'    # 1/8
        if ratio <= 0.3125:
            return u'\u2582'    # 2/8
        if ratio <= 0.4375:
            return u'\u2583'    # 3/8
        if ratio <= 0.5625:
            return u'\u2584'    # 4/8
        if ratio <= 0.6875:
            return u'\u2585'    # 5/8
        if ratio <= 0.8125:
            return u'\u2586'    # 6/8
        if ratio <= 0.9375:
            return u'\u2587'    # 7/8
        return u'\u2588'        # full block

    def _generate_port_graph(self, port_id_list):
        relevant_port = self.__get_relevant_ports(port_id_list)[0]
        hist_len = len(relevant_port.port_stats.history)
        hist_maxlen = relevant_port.port_stats.history.maxlen
        util_tx_hist = [0] * (hist_maxlen - hist_len) + [round(relevant_port.port_stats.history[i]['tx_percentage']) for i in range(hist_len)]
        util_rx_hist = [0] * (hist_maxlen - hist_len) + [round(relevant_port.port_stats.history[i]['rx_percentage']) for i in range(hist_len)]


        stats_table = text_tables.TRexTextTable()
        stats_table.header([' Util(%)', 'TX', 'RX'])
        stats_table.set_cols_align(['c', 'c', 'c'])
        stats_table.set_cols_width([8, hist_maxlen, hist_maxlen])
        stats_table.set_cols_dtype(['t', 't', 't'])

        for y in range(95, -1, -5):
            stats_table.add_row([y, ''.join([self._get_rational_block_char(util_tx, y, 5) for util_tx in util_tx_hist]),
                                    ''.join([self._get_rational_block_char(util_rx, y, 5) for util_rx in util_rx_hist])])

        return {"port_graph": ExportableStats({}, stats_table)}

    def _generate_port_stats(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_stats = OrderedDict([("owner", []),
                                       ('link', []),
                                       ("state", []),
                                       ("speed", []),
                                       ("CPU util.", []),
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

                                      ])

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
                              for k, v in per_field_stats.items()],
                              header=False)

        stats_table.header(header)

        return {"port_statistics": ExportableStats(return_stats_data, stats_table)}

    def _generate_port_status(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_status = OrderedDict([("driver", []),
                                        ("description", []),
                                        ("link status", []),
                                        ("link speed", []),
                                        ("port status", []),
                                        ("promiscuous", []),
                                        ("flow ctrl", []),
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
                              for k, v in per_field_status.items()],
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

        for stream_id, stream_id_sum in return_streams_data['streams'].items():
            stream_id_sum['packet_type'] = self._trim_packet_headers(stream_id_sum['packet_type'], 30)
            p_type_field_len = max(p_type_field_len, len(stream_id_sum['packet_type']))

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["l"] + ["r"] + ["c"] + ["r"] + ["c"])
        info_table.set_cols_width([10]   + [p_type_field_len]  + [8]   + [16]  + [15]  + [12])
        info_table.set_cols_dtype(["t"] + ["t"] + ["t"] + ["t"] + ["t"] + ["t"])

        info_table.add_rows([v.values()
                             for k, v in return_streams_data['streams'].items()],
                             header=False)
        info_table.header(["ID", "packet type", "length", "mode", "rate", "next stream"])

        return ExportableStats(return_streams_data, info_table)


    def __get_relevant_ports(self, port_id_list):
        # fetch owned ports
        ports = [port_obj
                 for _, port_obj in self._ports_dict.items()
                 if port_obj.port_id in port_id_list]
        
        # display only the first FOUR options, by design
        if len(ports) > 4:
            #self.logger is not defined
            #self.logger.log(format_text("[WARNING]: ", 'magenta', 'bold'), format_text("displaying up to 4 ports", 'magenta'))
            ports = ports[:4]
        return ports

    def __update_per_field_dict(self, dict_src_data, dict_dest_ref):
        for key, val in dict_src_data.items():
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
        self.history = deque(maxlen = 47)
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

        # no update is valid before baseline
        if not self.has_baseline and not baseline:
            return

        # call the underlying method
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
        self.history.clear()


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

    def get(self, field, format=False, suffix="", opts = None):
        value = self._get(self.latest_stats, field)
        if value == None:
            return 'N/A'

        return value if not format else format_num(value, suffix = suffix, opts = opts)


    def get_rel(self, field, format=False, suffix=""):
        ref_value = self._get(self.reference_stats, field)
        latest_value = self._get(self.latest_stats, field)

        # latest value is an aggregation - must contain the value
        if latest_value == None:
            return 'N/A'

        if ref_value == None:
            ref_value = 0

        value = latest_value - ref_value

        return value if not format else format_num(value, suffix)


    # get trend for a field
    def get_trend (self, field, use_raw = False, percision = 10.0):
        if field not in self.latest_stats:
            return 0

        # not enough history - no trend
        if len(self.history) < 5:
            return 0

        # absolute value is too low 0 considered noise
        if self.latest_stats[field] < percision:
            return 0
        
        # must lock, deque is not thread-safe for iteration
        with self.lock:
            field_samples = [sample[field] for sample in list(self.history)[-5:]]

        if use_raw:
            return calculate_diff_raw(field_samples)
        else:
            return calculate_diff(field_samples)


    def get_trend_gui (self, field, show_value = False, use_raw = False, up_color = 'red', down_color = 'green'):
        v = self.get_trend(field, use_raw)

        value = abs(v)

        # use arrows if utf-8 is supported
        if sys.__stdout__.encoding == 'UTF-8':
            arrow = u'\u25b2' if v > 0 else u'\u25bc'
        else:
            arrow = ''

        if sys.version_info < (3,0):
            arrow = arrow.encode('utf-8')

        color = up_color if v > 0 else down_color

        # change in 1% is not meaningful
        if value < 1:
            return ""

        elif value > 5:

            if show_value:
                return format_text("{0}{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}{0}{0}".format(arrow), color)

        elif value > 2:

            if show_value:
                return format_text("{0}{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}{0}".format(arrow), color)

        else:
            if show_value:
                return format_text("{0} {1:.2f}%".format(arrow,v), color)
            else:
                return format_text("{0}".format(arrow), color)



class CGlobalStats(CTRexStats):

    def __init__(self, connection_info, server_version, ports_dict_ref, events_handler):
        super(CGlobalStats, self).__init__()

        self.connection_info = connection_info
        self.server_version  = server_version
        self._ports_dict     = ports_dict_ref
        self.events_handler  = events_handler

        self.watched_cpu_util    = WatchedField('CPU util.', '%', 85, 60, events_handler)
        self.watched_rx_cpu_util = WatchedField('RX core util.', '%', 85, 60, events_handler)

    def get_stats (self):
        stats = {}

        # absolute
        stats['cpu_util']    = self.get("m_cpu_util")
        stats['rx_cpu_util'] = self.get("m_rx_cpu_util")
        stats['bw_per_core'] = self.get("m_bw_per_core")

        stats['tx_bps'] = self.get("m_tx_bps")
        stats['tx_pps'] = self.get("m_tx_pps")

        stats['rx_bps'] = self.get("m_rx_bps")
        stats['rx_pps'] = self.get("m_rx_pps")
        stats['rx_drop_bps'] = self.get("m_rx_drop_bps")

        # relatives
        stats['queue_full'] = self.get_rel("m_total_queue_full")

        return stats



    def _update(self, snapshot):
        # L1 bps
        bps = snapshot.get("m_tx_bps")
        pps = snapshot.get("m_tx_pps")

        snapshot['m_tx_bps_L1'] = calc_bps_L1(bps, pps)


        # simple...
        self.latest_stats = snapshot

        self.watched_cpu_util.update(snapshot.get('m_cpu_util'))
        self.watched_rx_cpu_util.update(snapshot.get('m_rx_cpu_util'))

        return True


class CPortStats(CTRexStats):

    def __init__(self, port_obj):
        super(CPortStats, self).__init__()
        self._port_obj = port_obj

    @staticmethod
    def __merge_dicts (target, src):
        for k, v in src.items():
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

        # history - should be traverse with a lock
        with self.lock, x.lock:
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

        stats['tx_bps']     = self.get("m_total_tx_bps")
        stats['tx_pps']     = self.get("m_total_tx_pps")
        stats['tx_bps_L1']  = self.get("m_total_tx_bps_L1")
        stats['tx_util']    = self.get("m_tx_util") 

        stats['rx_bps']     = self.get("m_total_rx_bps")
        stats['rx_pps']     = self.get("m_total_rx_pps")
        stats['rx_bps_L1']  = self.get("m_total_rx_bps_L1")
        stats['rx_util']    = self.get("m_rx_util") 

        return stats



    def _update(self, snapshot):
        speed = self._port_obj.get_speed_bps()

        # L1 bps
        tx_bps  = snapshot.get("m_total_tx_bps")
        tx_pps  = snapshot.get("m_total_tx_pps")
        rx_bps  = snapshot.get("m_total_rx_bps")
        rx_pps  = snapshot.get("m_total_rx_pps")
        ts_diff = 0.5 # TODO: change this to real ts diff from server

        bps_tx_L1 = calc_bps_L1(tx_bps, tx_pps)
        bps_rx_L1 = calc_bps_L1(rx_bps, rx_pps)

        snapshot['m_total_tx_bps_L1'] = bps_tx_L1
        if speed:
            snapshot['m_tx_util'] = (bps_tx_L1 / speed) * 100.0
        else:
            snapshot['m_tx_util'] = 0

        snapshot['m_total_rx_bps_L1'] = bps_rx_L1
        if speed:
            snapshot['m_rx_util'] = (bps_rx_L1 / speed) * 100.0
        else:
            snapshot['m_rx_util'] = 0

        # TX line util not smoothed
        diff_tx_pkts = snapshot.get('opackets', 0) - self.latest_stats.get('opackets', 0)
        diff_tx_bytes = snapshot.get('obytes', 0) - self.latest_stats.get('obytes', 0)
        tx_bps_L1 = calc_bps_L1(8.0 * diff_tx_bytes / ts_diff, float(diff_tx_pkts) / ts_diff)
        if speed:
            snapshot['tx_percentage'] = 100.0 * tx_bps_L1 / speed
        else:
            snapshot['tx_percentage'] = 0

        # RX line util not smoothed
        diff_rx_pkts = snapshot.get('ipackets', 0) - self.latest_stats.get('ipackets', 0)
        diff_rx_bytes = snapshot.get('ibytes', 0) - self.latest_stats.get('ibytes', 0)
        rx_bps_L1 = calc_bps_L1(8.0 * diff_rx_bytes / ts_diff, float(diff_rx_pkts) / ts_diff)
        if speed:
            snapshot['rx_percentage'] = 100.0 * rx_bps_L1 / speed
        else:
            snapshot['rx_percentage'] = 0

        # simple...
        self.latest_stats = snapshot

        return True


    def generate_stats(self):

        port_state = self._port_obj.get_port_state_name() if self._port_obj else "" 
        if port_state == "TRANSMITTING":
            port_state = format_text(port_state, 'green', 'bold')
        elif port_state == "PAUSE":
            port_state = format_text(port_state, 'magenta', 'bold')
        else:
            port_state = format_text(port_state, 'bold')

        if self._port_obj:
            if 'link' in self._port_obj.attr:
                if self._port_obj.attr.get('link', {}).get('up') == False:
                    link_state = format_text('DOWN', 'red', 'bold')
                else:
                    link_state = 'UP'
            else:
                link_state = 'N/A'
        else:
            link_state = ''

        # default rate format modifiers
        rate_format = {'bpsl1': None, 'bps': None, 'pps': None, 'percentage': 'bold'}

        # mark owned ports by color
        if self._port_obj:
            owner = self._port_obj.get_owner()
            rate_format[self._port_obj.last_factor_type] = ('blue', 'bold')
            if self._port_obj.is_acquired():
                owner = format_text(owner, 'green')

        else:
            owner = ''


        return {"owner": owner,
                "state": "{0}".format(port_state),
                'link': link_state,
                "speed": self._port_obj.get_formatted_speed() if self._port_obj else '',
                "CPU util.": "{0} {1}%".format(self.get_trend_gui("m_cpu_util", use_raw = True),
                                               format_threshold(round_float(self.get("m_cpu_util")), [85, 100], [0, 85])) if self._port_obj else '' ,
                "--": " ",
                "---": " ",
                "----": " ",
                "-----": " ",

                "Tx bps L1": "{0} {1}".format(self.get_trend_gui("m_total_tx_bps_L1", show_value = False),
                                               self.get("m_total_tx_bps_L1", format = True, suffix = "bps", opts = rate_format['bpsl1'])),

                "Tx bps L2": "{0} {1}".format(self.get_trend_gui("m_total_tx_bps", show_value = False),
                                                self.get("m_total_tx_bps", format = True, suffix = "bps", opts = rate_format['bps'])),

                "Line Util.": "{0} {1}".format(self.get_trend_gui("m_tx_util", show_value = False) if self._port_obj else "",
                                               self.get("m_tx_util", format = True, suffix = "%", opts = rate_format['percentage']) if self._port_obj else ""),

                "Rx bps": "{0} {1}".format(self.get_trend_gui("m_total_rx_bps", show_value = False),
                                            self.get("m_total_rx_bps", format = True, suffix = "bps")),
                  
                "Tx pps": "{0} {1}".format(self.get_trend_gui("m_total_tx_pps", show_value = False),
                                            self.get("m_total_tx_pps", format = True, suffix = "pps", opts = rate_format['pps'])),

                "Rx pps": "{0} {1}".format(self.get_trend_gui("m_total_rx_pps", show_value = False),
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


class CLatencyStats(CTRexStats):
    def __init__(self, ports):
        super(CLatencyStats, self).__init__()


    # for API
    def get_stats (self):
        return copy.deepcopy(self.latest_stats)


    def _update(self, snapshot):
        if snapshot is None:
            snapshot = {}
        output = {}

        output['global'] = {}
        for field in ['bad_hdr', 'old_flow']:
            if 'global' in snapshot and field in snapshot['global']:
                output['global'][field] = snapshot['global'][field]
            else:
                output['global'][field] = 0

        # we care only about the current active keys
        pg_ids = list(filter(is_intable, snapshot.keys()))

        for pg_id in pg_ids:
            current_pg = snapshot.get(pg_id)
            int_pg_id = int(pg_id)
            output[int_pg_id] = {}
            output[int_pg_id]['err_cntrs'] = current_pg['err_cntrs']
            output[int_pg_id]['latency'] = {}

            if 'latency' in current_pg:
                for field in ['jitter', 'average', 'total_max', 'last_max']:
                    if field in current_pg['latency']:
                        output[int_pg_id]['latency'][field] = current_pg['latency'][field]
                    else:
                        output[int_pg_id]['latency'][field] = StatNotAvailable(field)

                if 'histogram' in current_pg['latency']:
                    output[int_pg_id]['latency']['histogram'] = {int(elem): current_pg['latency']['histogram'][elem]
                                                                 for elem in current_pg['latency']['histogram']}
                    min_val = min(output[int_pg_id]['latency']['histogram'].keys())
                    if min_val == 0:
                        min_val = 2
                    output[int_pg_id]['latency']['total_min'] = min_val
                else:
                    output[int_pg_id]['latency']['total_min'] = StatNotAvailable('total_min')
                    output[int_pg_id]['latency']['histogram'] = {}

        self.latest_stats = output
        return True


# RX stats objects - COMPLEX :-(
class CRxStats(CTRexStats):
    def __init__(self, ports):
        super(CRxStats, self).__init__()
        self.ports = ports


    # calculates a diff between previous snapshot
    # and current one
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


    # this is the heart of the complex
    def process_single_pg (self, current_pg, prev_pg):

        # start with the previous PG
        output = copy.deepcopy(prev_pg)

        for field in ['tx_pkts', 'tx_bytes', 'rx_pkts', 'rx_bytes']:
            # is in the first time ? (nothing in prev)
            if field not in output:
                output[field] = {}

            # does the current snapshot has this field ?
            if field in current_pg:
                for port, pv in current_pg[field].items():
                    if not is_intable(port):
                        continue

                    output[field][port] = pv

            # sum up
            total = None
            for port, pv in output[field].items():
                if not is_intable(port):
                    continue
                if total is None:
                    total = 0
                total += pv

            output[field]['total'] = total


        return output
        
            
    def process_snapshot (self, current, prev):

        # final output
        output = {}

        # copy timestamp field
        output['ts'] = current['ts']

        # global (not per pg_id) error counters
        output['global'] = {}
        for field in ['rx_err', 'tx_err']:
            output['global'][field] = {}
            if 'global' in current and field in current['global']:
                for port in current['global'][field]:
                    output['global'][field][int(port)] = current['global'][field][port]

        # we care only about the current active keys
        pg_ids = list(filter(is_intable, current.keys()))

        for pg_id in pg_ids:

            current_pg = current.get(pg_id, {})
            
            # first time - we do not care
            if current_pg.get('first_time'):
                # new value - ignore history
                output[pg_id] = self.process_single_pg(current_pg, {})
                self.reference_stats[pg_id] = {}

                # 'dry' B/W
                self.calculate_bw_for_pg(output[pg_id])

            else:
                # aggregate the two values
                prev_pg = prev.get(pg_id, {})
                output[pg_id] = self.process_single_pg(current_pg, prev_pg)

                # calculate B/W
                diff_sec = self.calculate_diff_sec(current, prev)
                self.calculate_bw_for_pg(output[pg_id], prev_pg, diff_sec)


        # cleanp old reference values - they are dead
        ref_pg_ids = list(filter(is_intable, self.reference_stats.keys()))

        deleted_pg_ids = set(ref_pg_ids).difference(pg_ids)
        for d_pg_id in deleted_pg_ids:
            del self.reference_stats[d_pg_id]

        return output



    def calculate_bw_for_pg (self, pg_current, pg_prev = None, diff_sec = 0.0):
        # no previous values
        if (not pg_prev) or not (diff_sec > 0):
            pg_current['tx_pps']        = {}
            pg_current['tx_bps']        = {}
            pg_current['tx_bps_L1']     = {}
            pg_current['tx_line_util']  = {}
            pg_current['rx_pps']        = {}
            pg_current['rx_bps']        = {}
            pg_current['rx_bps_L1']     = {}
            pg_current['rx_line_util']  = {}

            pg_current['tx_pps_lpf']    = {}
            pg_current['tx_bps_lpf']    = {}
            pg_current['tx_bps_L1_lpf'] = {}
            pg_current['rx_pps_lpf']    = {}
            pg_current['rx_bps_lpf']    = {}
            pg_current['rx_bps_L1_lpf'] = {}
            return

        # TX
        for port in pg_current['tx_pkts'].keys():

            prev_tx_pps   = pg_prev['tx_pps'].get(port)
            now_tx_pkts   = pg_current['tx_pkts'].get(port)
            prev_tx_pkts  = pg_prev['tx_pkts'].get(port)
            pg_current['tx_pps'][port], pg_current['tx_pps_lpf'][port] = self.calc_pps(prev_tx_pps, now_tx_pkts, prev_tx_pkts, diff_sec)

            prev_tx_bps   = pg_prev['tx_bps'].get(port)
            now_tx_bytes  = pg_current['tx_bytes'].get(port)
            prev_tx_bytes = pg_prev['tx_bytes'].get(port)

            pg_current['tx_bps'][port], pg_current['tx_bps_lpf'][port] = self.calc_bps(prev_tx_bps, now_tx_bytes, prev_tx_bytes, diff_sec)

            if pg_current['tx_bps'].get(port) != None and pg_current['tx_pps'].get(port) != None:
                pg_current['tx_bps_L1'][port] = calc_bps_L1(pg_current['tx_bps'][port], pg_current['tx_pps'][port])
                pg_current['tx_bps_L1_lpf'][port] = calc_bps_L1(pg_current['tx_bps_lpf'][port], pg_current['tx_pps_lpf'][port])
            else:
                pg_current['tx_bps_L1'][port] = None
                pg_current['tx_bps_L1_lpf'][port] = None


        # RX
        for port in pg_current['rx_pkts'].keys():

            prev_rx_pps   = pg_prev['rx_pps'].get(port)
            now_rx_pkts   = pg_current['rx_pkts'].get(port)
            prev_rx_pkts  = pg_prev['rx_pkts'].get(port)
            pg_current['rx_pps'][port], pg_current['rx_pps_lpf'][port] = self.calc_pps(prev_rx_pps, now_rx_pkts, prev_rx_pkts, diff_sec)
    
            prev_rx_bps   = pg_prev['rx_bps'].get(port)
            now_rx_bytes  = pg_current['rx_bytes'].get(port)
            prev_rx_bytes = pg_prev['rx_bytes'].get(port)
            pg_current['rx_bps'][port], pg_current['rx_bps_lpf'][port] = self.calc_bps(prev_rx_bps, now_rx_bytes, prev_rx_bytes, diff_sec)
            if pg_current['rx_bps'].get(port) != None and pg_current['rx_pps'].get(port) != None:
                pg_current['rx_bps_L1'][port] = calc_bps_L1(pg_current['rx_bps'][port], pg_current['rx_pps'][port])
                pg_current['rx_bps_L1_lpf'][port] = calc_bps_L1(pg_current['rx_bps_lpf'][port], pg_current['rx_pps_lpf'][port])
            else:
                pg_current['rx_bps_L1'][port] = None
                pg_current['rx_bps_L1_lpf'][port] = None


    def calc_pps (self, prev_bw, now, prev, diff_sec):
        return self.calc_bw(prev_bw, now, prev, diff_sec, False)


    def calc_bps (self, prev_bw, now, prev, diff_sec):
        return self.calc_bw(prev_bw, now, prev, diff_sec, True)

    # returns tuple - first value is real, second is low pass filtered
    def calc_bw (self, prev_bw, now, prev, diff_sec, is_bps):
        # B/W is not valid when the values are None
        if (now is None) or (prev is None):
            return (None, None)
        
        # calculate the B/W for current snapshot
        current_bw = (now - prev) / diff_sec
        if is_bps:
            current_bw *= 8

        # previous B/W is None ? ignore it
        if prev_bw is None:
            prev_bw = 0

        return (current_bw, 0.5 * prev_bw + 0.5 * current_bw)




    def _update (self, snapshot):
        #print(snapshot)
        # generate a new snapshot
        new_snapshot = self.process_snapshot(snapshot, self.latest_stats)

        #print new_snapshot
        # advance
        self.latest_stats = new_snapshot


        return True



    # for API
    def get_stats (self):
        stats = {}

        for pg_id, value in self.latest_stats.items():
            # skip non ints
            if not is_intable(pg_id):
                # 'global' stats are in the same level of the pg_ids. We do want them to go to the user
                if pg_id == 'global':
                    stats[pg_id] = value
                continue
            # bare counters
            stats[int(pg_id)] = {}
            for field in ['tx_pkts', 'tx_bytes', 'rx_pkts', 'rx_bytes']:
                val = self.get_rel([pg_id, field, 'total'])
                stats[int(pg_id)][field] = {'total': val if val != 'N/A' else StatNotAvailable(field)}
                for port in value[field].keys():
                    if is_intable(port):
                        val = self.get_rel([pg_id, field, port])
                        stats[int(pg_id)][field][int(port)] = val if val != 'N/A' else StatNotAvailable(field)

            # BW values
            for field in ['tx_pps', 'tx_bps', 'tx_bps_L1', 'rx_pps', 'rx_bps', 'rx_bps_L1']:
                val = self.get([pg_id, field, 'total'])
                stats[int(pg_id)][field] = {'total': val if val != 'N/A' else StatNotAvailable(field)}
                for port in value[field].keys():
                    if is_intable(port):
                        val = self.get([pg_id, field, port])
                        stats[int(pg_id)][field][int(port)] = val if val != 'N/A' else StatNotAvailable(field)

        return stats

class CUtilStats(CTRexStats):

    def __init__(self, client):
        super(CUtilStats, self).__init__()
        self.client = client
        self.history = deque(maxlen = 1)
        self.mbuf_types_list = None
        self.last_update_ts = -999

    def get_stats(self, use_1sec_cache = False):
        time_now = time.time()
        if self.last_update_ts + 1 < time_now or not self.history or not use_1sec_cache:
            if self.client.is_connected():
                rc = self.client._transmit('get_utilization')
                if not rc:
                    raise STLError(rc)
                self.last_update_ts = time_now
                self.history.append(rc.data())
            else:
                self.history.append({})

        return self.history[-1]

class CXStats(CTRexStats):

    def __init__(self, client):
        super(CXStats, self).__init__()
        self.client = client
        self.names = []
        self.last_update_ts = -999

    def clear_stats(self, port_id = None):
        if port_id == None:
            ports = self.client.get_all_ports()
        elif type(port_id) is list:
            ports = port_id
        else:
            ports = [port_id]

        for port_id in ports:
            self.reference_stats[port_id] = self.get_stats(port_id, relative = False)

    def get_stats(self, port_id, use_1sec_cache = False, relative = True):
        time_now = time.time()
        if self.last_update_ts + 1 < time_now or not self.latest_stats or not use_1sec_cache:
            if self.client.is_connected():
                rc = self.client._transmit('get_port_xstats_values', params = {'port_id': port_id})
                if not rc:
                    raise STLError(rc)
                self.last_update_ts = time_now
                values = rc.data().get('xstats_values', [])
                if len(values) != len(self.names): # need to update names ("keys")
                    rc = self.client._transmit('get_port_xstats_names', params = {'port_id': port_id})
                    if not rc:
                        raise STLError(rc)
                    self.names = rc.data().get('xstats_names', [])
                if len(values) != len(self.names):
                    raise STLError('Length of get_xstats_names: %s and get_port_xstats_values: %s' % (len(self.names), len(values)))
                self.latest_stats[port_id] = OrderedDict([(key, val) for key, val in zip(self.names, values)])

        stats = OrderedDict()
        for key, val in self.latest_stats[port_id].items():
            if relative:
                stats[key] = self.get_rel([port_id, key])
            else:
                stats[key] = self.get([port_id, key])
        return stats

if __name__ == "__main__":
    pass

