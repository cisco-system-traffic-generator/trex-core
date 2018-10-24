from trex.common.trex_types import TRexError
from trex.console.trex_tui import TrexTUI
from trex.utils.text_opts import format_num, red, green
from trex.utils import text_tables

import time

class CAstfLatencyStats(object):
    def __init__(self, rpc):
        self.rpc = rpc
        self.reset()


    @property
    def max_panel_size(self):
        if TrexTUI.has_instance():
            # 19 rows is for header, footer, spaces etc.
            return TrexTUI.MIN_ROWS-19
        return 0


    @property
    def latency_window_size(self):
        if self.max_panel_size:
            # 9 is other counters except window
            return self.max_panel_size - 9
        return 15


    def update_window(self, data, epoch):
        if epoch != self.window_epoch:
            self.history_of_max = []
            self.window_last_update_ts = 0
            self.window_last_rx_pkts = 0
            self.window_epoch = epoch

        cur_time = time.time()
        if cur_time >= self.window_last_update_ts + 1:
            cur_rx_pkts = sum([val['hist']['cnt'] for val in data.values()])
            if cur_rx_pkts > self.window_last_rx_pkts:
                max_val = 0
                port_count = len(data)
                self.history_of_max.insert(0, [''] * port_count)
                for col, port_id in enumerate(sorted(data.keys())):
                    val = int(data[port_id]['hist']['s_max'])
                    max_val = max(val, max_val)
                    self.history_of_max[0][col] = val
                self.window_last_update_ts = cur_time
                self.window_last_rx_pkts = cur_rx_pkts
                self.longest_key = max(self.longest_key, len('%s' % max_val))
        self.history_of_max = self.history_of_max[:self.latency_window_size]


    def reset(self):
        self.longest_key = 12
        self.window_epoch = None


    def get_stats(self):
        rc = self.rpc.transmit('get_latency_stats')
        if not rc:
            raise TRexError(rc.err())
        rc_data = rc.data()['data']

        data = {}
        for k, v in rc_data.items():
            if k.startswith('port-'):
                data[int(k[5:])] = v

        self.update_window(data, rc_data['epoch'])

        return data


    def _fit_to_panel(self, arr):
        if self.max_panel_size and len(arr) > self.max_panel_size:
            arr = arr[:self.max_panel_size]


    def to_table_main(self):
        data = self.get_stats()
        ports = sorted(data.keys())[:5]
        port_count = len(ports)
        rows = []

        def add_counter(name, *path):
            rows.append([name])
            for port_id in ports:
                sub = data[port_id]
                for key in path:
                    sub = sub[key]
                rows[-1].append(int(sub))

        def add_section(name):
            rows.append([name] + [''] * port_count)

        add_counter('TX pkts', 'stats', 'm_tx_pkt_ok')
        add_counter('RX pkts', 'stats', 'm_pkt_ok')
        add_counter('Max latency', 'hist', 'max_usec')
        add_counter('Avg latency', 'hist', 's_avg')
        add_section('-- Window --')

        for index in range(self.latency_window_size):
            if index == 0:
                rows.append(['Last max'])
            else:
                rows.append(['Last-%d' % index])
            if index < len(self.history_of_max):
                rows[-1] += [(val if val else '') for val in self.history_of_max[index]]
            else:
                rows[-1] += [''] * port_count

        add_section('---')
        add_counter('Jitter', 'stats', 'm_jitter')
        add_section('----')

        error_counters = [
            'm_unsup_prot',
            'm_no_magic',
            'm_no_id',
            'm_seq_error',
            'm_length_error',
            'm_no_ipv4_option',
            'm_tx_pkt_err',
            'm_l3_cs_err',
            'm_l4_cs_err']

        rows.append(['Errors'])
        for port_id in ports:
            errors = 0
            for error_counter in error_counters:
                errors += data[port_id]['stats'][error_counter]
            rows[-1].append(red(errors) if errors else green(errors))

        for row in rows:
            self.longest_key = max(self.longest_key, len(row[0]))

        # init table
        stats_table = text_tables.TRexTextTable('Latency Statistics')
        stats_table.set_cols_align(['l'] + ['r'] * port_count)
        stats_table.set_cols_width([self.longest_key] + [15] * port_count)
        stats_table.set_cols_dtype(['t'] * (1 + port_count))
        header = ['Port ID:'] + list(data.keys())
        stats_table.header(header)

        self._fit_to_panel(rows)
        for row in rows:
            stats_table.add_row(row)

        return stats_table


    def histogram_to_table(self):
        data = self.get_stats()
        ports = sorted(data.keys())[:5]
        port_count = len(ports)

        rows = {}
        for col, port_id in enumerate(ports):
            below_10 = data[port_id]['hist']['cnt'] - data[port_id]['hist']['high_cnt']
            if below_10:
                if 0 not in rows:
                    rows[0] = [''] * port_count
                rows[0][col] = below_10
            for elem in data[port_id]['hist']['histogram']:
                if elem['key'] not in rows:
                    rows[elem['key']] = [''] * port_count
                rows[elem['key']][col] = elem['val']

        for key in rows.keys():
            self.longest_key = max(self.longest_key, len('%s' % key))

        # init table
        stats_table = text_tables.TRexTextTable('Latency Histogram')
        stats_table.set_cols_align(['l'] + ['r'] * port_count)
        stats_table.set_cols_width([self.longest_key] + [15] * port_count)
        stats_table.set_cols_dtype(['t'] * (1 + port_count))
        header = ['Port ID:'] + list(data.keys())
        stats_table.header(header)

        keys = list(reversed(sorted(rows.keys())))
        self._fit_to_panel(keys)
        if self.max_panel_size:
            lack_for_full = self.max_panel_size - len(rows)
            if lack_for_full > 0:
                for _ in range(lack_for_full):
                    stats_table.add_row([''] * (port_count + 1))

        for k in keys:
            if k < 10:
                stats_table.add_row(['<10'] + rows[k])
            else:
                stats_table.add_row([k] + rows[k])

        return stats_table


    def counters_to_table(self):
        data = self.get_stats()
        ports = sorted(data.keys())[:5]
        port_count = len(ports)

        stat_rows = {}
        for col, port_id in enumerate(ports):
            for k, v in data[port_id]['stats'].items():
                if k not in stat_rows:
                    stat_rows[k] = [''] * port_count
                stat_rows[k][col] = v

        hist_rows = {}
        for col, port_id in enumerate(ports):
            for k, v in data[port_id]['hist'].items():
                if type(v) in (dict, list):
                    continue
                if k not in hist_rows:
                    hist_rows[k] = [''] * port_count
                hist_rows[k][col] = v

        rows = []
        for key in sorted(stat_rows.keys()):
            val = stat_rows[key]
            if any(val):
                rows.append([key] + val)
        rows.append(['--'] + [''] * port_count)
        for key in sorted(hist_rows.keys()):
            val = hist_rows[key]
            if any(val):
                rows.append([key] + val)

        for row in rows:
            self.longest_key = max(self.longest_key, len(row[0]))

        # init table
        stats_table = text_tables.TRexTextTable('Latency Counters')
        stats_table.set_cols_align(['l'] + ['r'] * port_count)
        stats_table.set_cols_width([self.longest_key] + [15] * port_count)
        stats_table.set_cols_dtype(['t'] * (1 + port_count))
        header = ['Port ID:'] + list(data.keys())
        stats_table.header(header)

        self._fit_to_panel(rows)
        if self.max_panel_size:
            lack_for_full = self.max_panel_size - len(rows)
            if lack_for_full > 0:
                for _ in range(lack_for_full):
                    stats_table.add_row([''] * (port_count + 1))

        for row in rows:
            stats_table.add_row(row)

        return stats_table


