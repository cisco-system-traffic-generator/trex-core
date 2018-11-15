

from collections import deque, OrderedDict
import copy

from .trex_stats import AbstractStats
from ..trex_types import RpcCmdData, TRexError

from ...utils import text_tables
from ...utils.common import calc_bps_L1, round_float
from ...utils.text_opts import format_text, format_threshold, format_num

############################   port stats   #############################
############################                #############################
############################                #############################

class PortStats(AbstractStats):

    def __init__(self, port_obj):
        super(PortStats, self).__init__(RpcCmdData('get_port_stats', {'port_id': port_obj.port_id}, ''))
        self._port_obj = port_obj


    def _pre_update(self, snapshot):
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

        return snapshot


    # for port we need to do something smarter
    def to_dict (self):
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



    def to_table (self):

        # default rate format modifiers
        rate_format = {'bpsl1': None, 'bps': None, 'pps': None, 'percentage': 'bold'}

        if self._port_obj:
            port_str = self._port_obj.port_id
            port_state = self._port_obj.get_port_state_name() if self._port_obj else "" 
            if port_state == "TRANSMITTING":
                port_state = format_text(port_state, 'green', 'bold')
            elif port_state == "PAUSE":
                port_state = format_text(port_state, 'magenta', 'bold')
            else:
                port_state = format_text(port_state, 'bold')

            link_state = 'UP' if self._port_obj.is_up() else format_text('DOWN', 'red', 'bold')

            # mark owned ports by color
            owner = self._port_obj.get_owner()
            rate_format[self._port_obj.last_factor_type] = ('blue', 'bold')
            if self._port_obj.is_acquired():
                owner = format_text(owner, 'green')
            speed = format_num(self._port_obj.get_speed_bps(), suffix = 'b/s')
            cpu_util = "{0} {1}%".format(self.get_trend_gui("m_cpu_util", use_raw = True),
                                         format_threshold(round_float(self.get("m_cpu_util")), [85, 100], [0, 85]))
            line_util = "{0} {1}".format(self.get_trend_gui("m_tx_util", show_value = False),
                                         self.get("m_tx_util", format = True, suffix = "%", opts = rate_format['percentage']))

        else:
            port_str = 'total'
            port_state = ''
            link_state = ''
            owner = ''
            speed = ''
            cpu_util = ''
            line_util = ''

        stats = OrderedDict([
                ("owner", owner),
                ("link",  link_state),
                ("state", "{0}".format(port_state)),
                ("speed",  speed),
                ("CPU util.", cpu_util),

                ("--", ''),

                ("Tx bps L2", "{0} {1}".format(self.get_trend_gui("m_total_tx_bps", show_value = False),
                                               self.get("m_total_tx_bps", format = True, suffix = "bps", opts = rate_format['bps']))),

                ("Tx bps L1", "{0} {1}".format(self.get_trend_gui("m_total_tx_bps_L1", show_value = False),
                                               self.get("m_total_tx_bps_L1", format = True, suffix = "bps", opts = rate_format['bpsl1']))),

                ("Tx pps", "{0} {1}".format(self.get_trend_gui("m_total_tx_pps", show_value = False),
                                            self.get("m_total_tx_pps", format = True, suffix = "pps", opts = rate_format['pps']))),

                ("Line Util.", line_util),

                ("---", ''),

                ("Rx bps", "{0} {1}".format(self.get_trend_gui("m_total_rx_bps", show_value = False),
                                            self.get("m_total_rx_bps", format = True, suffix = "bps"))),

                ("Rx pps", "{0} {1}".format(self.get_trend_gui("m_total_rx_pps", show_value = False),
                                            self.get("m_total_rx_pps", format = True, suffix = "pps"))),

                ("----", ''),

                ("opackets", self.get_rel("opackets")),
                ("ipackets", self.get_rel("ipackets")),
                ("obytes",   self.get_rel("obytes")),
                ("ibytes",   self.get_rel("ibytes")),
                ("tx-pkts",  self.get_rel("opackets", format = True, suffix = "pkts"),),
                ("rx-pkts",  self.get_rel("ipackets", format = True, suffix = "pkts")),
                ("tx-bytes", self.get_rel("obytes", format = True, suffix = "B"),),
                ("rx-bytes", self.get_rel("ibytes", format = True, suffix = "B"),),

                ("-----", ''),

                ("oerrors", format_num(self.get_rel("oerrors"),
                                       compact = False,
                                       opts = 'green' if (self.get_rel("oerrors")== 0) else 'red')),

                ("ierrors", format_num(self.get_rel("ierrors"),
                                       compact = False,
                                       opts = 'green' if (self.get_rel("ierrors")== 0) else 'red')),
                ])


        total_cols = 1
        stats_table = text_tables.TRexTextTable('Port Statistics')
        stats_table.set_cols_align(["l"] + ["r"] * total_cols)
        stats_table.set_cols_width([10] + [17]   * total_cols)
        stats_table.set_cols_dtype(['t'] + ['t'] * total_cols)

        #disp_str = {'tx-pkts':"opackets ", 'rx-pkts':"ipackets ", 'rx-bytes':"ibytes ", 'tx-bytes':"obytes "}
        #disp_data = OrderedDict([(disp_str[k], v) if k in disp_str else (k, v) for k, v in per_field_stats.items()])

        stats_table.add_rows([[k] + [v]
                              for k, v in stats.items()],
                              header=False)

        stats_table.header(["port", port_str])

        return stats_table



############################   port xstats  #############################
############################                #############################
############################                #############################
class PortXStats(AbstractStats):
    names = []
    def __init__(self, port_obj):
        super(PortXStats, self).__init__(RpcCmdData('get_port_xstats_values', {'port_id': port_obj.port_id}, ''))

        self.port_id = port_obj.port_id


    @classmethod
    def get_names(cls, port_obj):
        rc = port_obj.rpc.transmit('get_port_xstats_names', params = {'port_id': port_obj.port_id})
        if not rc:
            raise TRexError('Error getting xstat names, Error: %s' % rc.err())
        cls.names = rc.data()['xstats_names']


    def _pre_update(self, snapshot):
        values = snapshot['xstats_values']

        if len(values) != len(self.names):
            raise TRexError('Length of get_xstats_names: %s and get_port_xstats_values: %s' % (len(self.names), len(values)))

        snapshot = OrderedDict([(key, val) for key, val in zip(self.names, values)])

        return snapshot


    def to_dict(self, relative = True):

        stats = OrderedDict()
        for key, val in self.latest_stats.items():
            if relative:
                stats[key] = self.get_rel(key)
            else:
                stats[key] = self.get(key)

        return stats


    def to_table (self, include_zero_lines = False):

        # get the data on relevant ports
        xstats_data = self.to_dict(relative = True)

        # put into table
        stats_table = text_tables.TRexTextTable('xstats')

        stats_table.header(['Name:'] + ['Port %s:' % self.port_id])
        stats_table.set_cols_align(['l'] + ['r'])
        stats_table.set_cols_width([30] + [15])
        stats_table.set_cols_dtype(['t'] * 2)

        for key, value in xstats_data.items():
            if include_zero_lines or value:
                key = key[:28]
                stats_table.add_row([key] + [value])

        return stats_table

    


# sum of port stats
class PortStatsSum(PortStats):
    def __init__ (self):
        super(PortStatsSum, self).__init__(None)


    @staticmethod
    def __merge_dicts (target, src):
        for k, v in src.items():
            if k in target:
                target[k] += v
            else:
                target[k] = v


    def __radd__(self, other):
        if (other == None) or (other == 0):
            return self
        else:
            return self.__add__(other)


    def __add__ (self, x):
        assert isinstance(x, (PortStats, PortXStats))

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

