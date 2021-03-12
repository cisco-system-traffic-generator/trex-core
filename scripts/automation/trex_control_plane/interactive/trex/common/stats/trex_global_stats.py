
from collections import deque, OrderedDict

from .trex_stats import AbstractStats
from ..trex_types import RpcCmdData

from ...utils import text_tables
from ...utils.common import calc_bps_L1, round_float
from ...utils.text_opts import format_text, format_threshold, format_num

############################   global stats  #############################
############################                 #############################
############################                 #############################

class GlobalStats(AbstractStats):

    def __init__(self, client):
        super(GlobalStats, self).__init__(RpcCmdData('get_global_stats', {}, ''))
        self.client = client
        #self.watched_cpu_util    = WatchedField('CPU util.', '%', 85, 60, events_handler)
        #self.watched_rx_cpu_util = WatchedField('RX core util.', '%', 85, 60, events_handler)


    def _pre_update(self, snapshot):

        # L1 bps
        bps = snapshot.get("m_tx_bps")
        pps = snapshot.get("m_tx_pps")

        snapshot['m_tx_bps_L1'] = calc_bps_L1(bps, pps)

        return snapshot
        #self.watched_cpu_util.update(snapshot.get('m_cpu_util'))
        #self.watched_rx_cpu_util.update(snapshot.get('m_rx_cpu_util'))


    def to_dict (self):
        stats = {}

        # absolute
        st = [
            "m_active_flows",
            "m_active_sockets",
            "m_bw_per_core",
            "m_cpu_util",
            "m_cpu_util_raw",
            "m_open_flows",
            "m_platform_factor",
            "m_rx_bps",
            "m_rx_core_pps",
            "m_rx_cpu_util",
            "m_rx_drop_bps",
            "m_rx_pps",
            "m_socket_util",
            "m_tx_expected_bps",
            "m_tx_expected_cps",
            "m_tx_expected_pps",
            "m_tx_pps",
            "m_tx_bps",
            "m_tx_cps",
            "m_total_servers",
            "m_total_clients",
            "m_total_alloc_error"]

        for obj in st:
            stats[obj[2:]]=self.get(obj)


        # relatives
        stats['queue_full'] = self.get_rel("m_total_queue_full")

        return stats



    def to_table (self):
        conn_info      = self.client.get_connection_info()
        server_info    = self.client.get_server_system_info()
        server_version = self.client.get_server_version()

        server_version_fmt = "{0} @ {1}".format(server_version.get('mode', 'N/A'), server_version.get('version', 'N/A'))

        stats_data_left = OrderedDict([

                             ("connection", "{host}, Port {port}".format(host = conn_info['server'], port = conn_info['sync_port'])),
                             ("version", "{ver}".format(ver = server_version_fmt)),

                             ("cpu_util.", "{0}% @ {2} cores ({3} per dual port) {1}".format( format_threshold(round_float(self.get("m_cpu_util")), [85, 100], [0, 85]),
                                                                                         self.get_trend_gui("m_cpu_util", use_raw = True),
                                                                                         server_info.get('dp_core_count'),
                                                                                         server_info.get('dp_core_count_per_port'),
                                                                                         )),

                             ("rx_cpu_util.", "{0}% / {1} {2}".format( format_threshold(round_float(self.get("m_rx_cpu_util")), [85, 100], [0, 85]),
                                                                       self.get("m_rx_core_pps", format = True, suffix = "pps"),
                                                                       self.get_trend_gui("m_rx_cpu_util", use_raw = True),)),

                             ("async_util.", "{0}% / {1}".format( format_threshold(round_float(self.client.conn.async_.monitor.get_cpu_util()), [85, 100], [0, 85]),
                                                                 format_num(self.client.conn.async_.monitor.get_bps(), suffix = "bps"))),
                             ("total_cps.", "{0} {1}".format( self.get("m_tx_cps", format=True, suffix="cps"),
                                                              self.get_trend_gui("m_tx_cps"))),

                            ])

        stats_data_right = OrderedDict([
                             ("total_tx_L2", "{0} {1}".format( self.get("m_tx_bps", format=True, suffix="bps"),
                                                                self.get_trend_gui("m_tx_bps"))),

                             ("total_tx_L1", "{0} {1}".format( self.get("m_tx_bps_L1", format=True, suffix="bps"),
                                                                self.get_trend_gui("m_tx_bps_L1"))),

                             ("total_rx", "{0} {1}".format( self.get("m_rx_bps", format=True, suffix="bps"),
                                                              self.get_trend_gui("m_rx_bps"))),

                             ("total_pps", "{0} {1}".format( self.get("m_tx_pps", format=True, suffix="pps"),
                                                              self.get_trend_gui("m_tx_pps"))),

                             ("drop_rate", "{0}".format( format_num(self.get("m_rx_drop_bps"),
                                                                    suffix = 'bps',
                                                                    opts = 'green' if (self.get("m_rx_drop_bps")== 0) else 'red'),
                                                            )),
                             ])

        queue_drop = self.get_rel("m_total_queue_drop")
        if queue_drop:
            stats_data_right['queue_drop'] = format_num(queue_drop, suffix = 'pkts', compact = False, opts = 'green' if queue_drop == 0 else 'red')
        else:
            queue_full = self.get_rel("m_total_queue_full")
            stats_data_right['queue_full'] = format_num(queue_full, suffix = 'pkts', compact = False, opts = 'green' if queue_full == 0 else 'red')

        # build table representation
        stats_table = text_tables.TRexTextInfo('global statistics')
        stats_table.set_cols_align(["l", "l"])
        stats_table.set_deco(0)
        stats_table.set_cols_width([55, 45])
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

        return stats_table



class UtilStats(AbstractStats):
    """
        CPU/MBUF utilization stats
    """

    get_number_of_bytes_cache = {}

    def __init__ (self):
        super(UtilStats, self).__init__(RpcCmdData('get_utilization', {}, ''), hlen = 1)
        self.mbuf_types_list = None


    @staticmethod
    def get_number_of_bytes(val):
        # get number of bytes: '64b'->64, '9kb'->9000 etc.
        if val not in UtilStats.get_number_of_bytes_cache:
            UtilStats.get_number_of_bytes_cache[val] = int(val[:-1].replace('k', '000'))
        return UtilStats.get_number_of_bytes_cache[val]


    def to_dict (self):
        return self.history[-1]


    def to_table (self, sect):
        if sect == 'cpu':
            return self.to_table_cpu()
        elif sect == 'mbuf':
            return self.to_table_mbuf()
        else:
            raise TRexError("invalid section type")


    def to_table_mbuf (self):

        stats_table = text_tables.TRexTextTable('Mbuf Util')
        util_stats = self.to_dict()

        if util_stats:
            if 'mbuf_stats' not in util_stats:
                raise Exception("Excepting 'mbuf_stats' section in stats %s" % util_stats)
            mbuf_stats = util_stats['mbuf_stats']
            for mbufs_per_socket in mbuf_stats.values():
                first_socket_mbufs = mbufs_per_socket
                break
            if not self.mbuf_types_list:
                mbuf_keys = list(first_socket_mbufs.keys())
                mbuf_keys.sort(key = self.get_number_of_bytes)
                self.mbuf_types_list = mbuf_keys
            types_len = len(self.mbuf_types_list)
            stats_table.set_cols_align(['l'] + ['r'] * (types_len + 1))
            stats_table.set_cols_width([10] + [7] * (types_len + 1))
            stats_table.set_cols_dtype(['t'] * (types_len + 2))
            stats_table.header([''] + self.mbuf_types_list + ['RAM(MB)'])
            total_list = []
            sum_totals = 0
            for mbuf_type in self.mbuf_types_list:
                sum_totals += first_socket_mbufs[mbuf_type][1] * self.get_number_of_bytes(mbuf_type) + 64
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
                for mbuf_type in self.mbuf_types_list:
                    used = mbufs[mbuf_type][1] - mbufs[mbuf_type][0]
                    sum_used += used * self.get_number_of_bytes(mbuf_type) + 64
                    used_list.append(used)
                    percentage_list.append('%s%%' % int(100 * used / mbufs[mbuf_type][1]))
                used_list.append(int(sum_used/1e6))
                stats_table.add_row([socket_show_name] + used_list)
                stats_table.add_row(['Percent:'] + percentage_list + [''])
        else:
            stats_table.add_row(['No Data.'])

        return stats_table


    def to_table_cpu (self):
        stats_table = text_tables.TRexTextTable('Cpu Util(%)')
        util_stats = self.to_dict()

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
                elif -1 not in ports:
                    interfaces = "({:},{:})".format(ports[0], ports[1])
                else:
                    interfaces = "({:})".format(ports[0] if ports[0] != -1 else ports[1])

                thread = "{:2} {:^7}".format(i, interfaces)
                stats_table.add_row([thread, avg] + history[:show_len])
        else:
            stats_table.add_row(['No Data.'])

        return stats_table


