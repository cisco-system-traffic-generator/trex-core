from collections import OrderedDict

from ..common.trex_types import StatNotAvailable, is_integer
from ..utils.text_opts import format_num
from ..utils.common import try_int
from ..utils import text_tables

# TRex PG stats - (needs a refactor)

class CPgIdStats(object):
    def __init__(self, rpc):
        self.rpc = rpc
        self.latency_window_size = 14
        self.max_histogram_size = 17
        self.reset()

    def reset(self):
        # sample when clear was last called. Values we return are last - ref
        self.ref =  {'flow_stats': {}, 'latency': {}}
        self.max_hist = {}
        self.max_hist_index = 0

    def _get (self, src, field, default = None):
        if isinstance(field, list):
            # deep
            value = src
            for level in field:
                if level not in value:
                    return default
                value = value[level]
        else:
            # flat
            if field not in src:
                return default
            value = src[field]

        return value

    def get(self, src, field, format=False, suffix="", opts = None):
        value = self._get(src, field)
        if type(value) is StatNotAvailable:
            return 'N/A'
        if value == None:
            return 'N/A'

        return value if not format else format_num(value, suffix = suffix, opts = opts)

    def get_rel(self, src, field, format=False, suffix="", opts = None):
        value = self._get(src, field)
        if type(value) is StatNotAvailable:
            return 'N/A'
        if value == None:
            return 'N/A'

        base = self._get(self.ref, field, default=0)
        return (value - base) if not format else format_num(value - base, suffix = suffix, opts = opts)

    def clear_stats(self, clear_flow_stats = True, clear_latency_stats = True):
        if not (clear_flow_stats or clear_latency_stats):
            return

        stats = self.get_stats(relative = False)

        if 'latency' in stats and clear_latency_stats:
            for pg_id in stats['latency']:
                if 'latency' in stats['latency'][pg_id]:
                    stats['latency'][pg_id]['latency']['total_max'] = 0

        for key in stats.keys():
            if key == 'flow_stats' and not clear_flow_stats:
                continue
            if key == 'latency' and not clear_latency_stats:
                continue
            self.ref[key] = stats[key]

    def get_stats(self, pgid_list = [], relative = True):

        # Should not exceed MAX_ALLOWED_PGID_LIST_LEN from common/stl/trex_stl_fs.cpp
        max_pgid_in_query = 1024 + 128
        pgid_list_len = len(pgid_list)
        index = 0
        ans_dict = {}

        while index <= pgid_list_len:
            curr_pgid_list = pgid_list[index : index + max_pgid_in_query]
            index += max_pgid_in_query
            rc = self.rpc.transmit('get_pgid_stats', params = {'pgids': curr_pgid_list})

            if not rc:
                raise STLError(rc)

            for key, val in rc.data().items():
                if key in ans_dict:
                    try:
                        ans_dict[key].update(val)
                    except:
                        pass
                else:
                    ans_dict[key] = val

        # translation from json values to python API names
        json_keys_latency = {'jit': 'jitter', 'average': 'average', 'total_max': 'total_max', 'last_max': 'last_max'}
        json_keys_err = {'drp': 'dropped', 'ooo': 'out_of_order', 'dup': 'dup', 'sth': 'seq_too_high', 'stl': 'seq_too_low'}
        json_keys_global = {'old_flow': 'old_flow', 'bad_hdr': 'bad_hdr'}
        json_keys_flow_stat = {'rp': 'rx_pkts', 'rb': 'rx_bytes', 'tp': 'tx_pkts', 'tb': 'tx_bytes',
                               'rbs': 'rx_bps', 'rps': 'rx_pps', 'tbs': 'tx_bps', 'tps': 'tx_pps'}
        json_keys_global_err = {'rx_err': 'rx_err', 'tx_err': 'tx_err'}

        # translate json 'latency' to python API 'latency'
        stats = {}
        if 'ver_id' in ans_dict and ans_dict['ver_id'] is not None:
            stats['ver_id'] = ans_dict['ver_id']
        else:
            stats['ver_id'] = {}

        if 'latency' in ans_dict and ans_dict['latency'] is not None:
            stats['latency'] = {}
            stats['latency']['global'] = {}
            for val in json_keys_global.values():
                stats['latency']['global'][val] = 0
            for pg_id in ans_dict['latency']:
                # 'g' value is not a number
                try:
                    int_pg_id = int(pg_id)
                except:
                    continue
                lat = stats['latency'][int_pg_id] = {}
                lat['err_cntrs'] = {}
                if 'er' in ans_dict['latency'][pg_id]:
                    for key, val in json_keys_err.items():
                        if key in ans_dict['latency'][pg_id]['er']:
                            lat['err_cntrs'][val] = ans_dict['latency'][pg_id]['er'][key]
                        else:
                            lat['err_cntrs'][val] = 0
                else:
                    for val in json_keys_err.values():
                        lat['err_cntrs'][val] = 0

                lat['latency'] = {}
                for field, val in json_keys_latency.items():
                    if field in ans_dict['latency'][pg_id]['lat']:
                        lat['latency'][val] = ans_dict['latency'][pg_id]['lat'][field]
                    else:
                        lat['latency'][val] = StatNotAvailable(field)

                if 'histogram' in ans_dict['latency'][pg_id]['lat']:
                    #translate histogram numbers from string to integers
                    lat['latency']['histogram'] = {
                                        int(elem): ans_dict['latency'][pg_id]['lat']['histogram'][elem]
                                         for elem in ans_dict['latency'][pg_id]['lat']['histogram']
                    }
                    min_val = min(lat['latency']['histogram'])
                    if min_val == 0:
                        min_val = 2
                    lat['latency']['total_min'] = min_val
                else:
                    lat['latency']['total_min'] = StatNotAvailable('total_min')
                    lat['latency']['histogram'] = {}

        # translate json 'flow_stats' to python API 'flow_stats'
        if 'flow_stats' in ans_dict and ans_dict['flow_stats'] is not None:
            stats['flow_stats'] = {}
            stats['flow_stats']['global'] = {}

            all_ports = []
            for pg_id in ans_dict['flow_stats']:
                # 'g' value is not a number
                try:
                    int_pg_id = int(pg_id)
                except:
                    continue

                # do this only once
                if all_ports == []:
                    # if field does not exist, we don't know which ports we have. We assume 'tp' will always exist
                    for port in ans_dict['flow_stats'][pg_id]['tp']:
                        all_ports.append(int(port))

                fs = stats['flow_stats'][int_pg_id] = {}
                for field, val in json_keys_flow_stat.items():
                    fs[val] = {}
                    #translate ports to integers
                    total = 0
                    if field in ans_dict['flow_stats'][pg_id]:
                        for port in ans_dict['flow_stats'][pg_id][field]:
                            fs[val][int(port)] = ans_dict['flow_stats'][pg_id][field][port]
                            total += fs[val][int(port)]
                        fs[val]['total'] = total
                    else:
                        for port in all_ports:
                            fs[val][int(port)] = StatNotAvailable(val)
                        fs[val]['total'] = StatNotAvailable('total')
                fs['rx_bps_l1'] = {}
                fs['tx_bps_l1'] = {}
                for port in fs['rx_pkts']:
                    # L1 overhead is 20 bytes per packet
                    fs['rx_bps_l1'][port] = float(fs['rx_bps'][port]) + float(fs['rx_pps'][port]) * 20 * 8
                    fs['tx_bps_l1'][port] = float(fs['tx_bps'][port]) + float(fs['tx_pps'][port]) * 20 * 8

            if 'g' in ans_dict['flow_stats']:
                for field, val in json_keys_global_err.items():
                    if field in ans_dict['flow_stats']['g']:
                        stats['flow_stats']['global'][val] = ans_dict['flow_stats']['g'][field]
                    else:
                        stats['flow_stats']['global'][val] = {}
                        for port in all_ports:
                            stats['flow_stats']['global'][val][int(port)] = 0
            else:
                for val in json_keys_global_err.values():
                    stats['flow_stats']['global'][val] = {}
                    for port in all_ports:
                        stats['flow_stats']['global'][val][int(port)] = 0

        # if pgid appears with different ver_id, delete it from the saved reference
        if 'ver_id' in self.ref:
            for pg_id in self.ref['ver_id']:
                if pg_id in stats['ver_id'] and self.ref['ver_id'][pg_id] != stats['ver_id'][pg_id]:
                    int_pg_id = int(pg_id)
                    if int_pg_id in self.ref['flow_stats']:
                        del self.ref['flow_stats'][int_pg_id]
                    if int_pg_id in self.ref['latency']:
                        del self.ref['latency'][int(pg_id)]
        else:
            self.ref['ver_id'] = {}

        self.ref['ver_id'].update(stats['ver_id'])

        if not relative:
            return stats

        # code below makes the stats relative

        # update total_max
        if 'latency' in self.ref and 'latency' in stats:
            for pg_id in self.ref['latency']:
                if 'latency' in self.ref['latency'][pg_id] and pg_id in stats['latency']:
                    if 'total_max' in self.ref['latency'][pg_id]['latency']:
                        if stats['latency'][pg_id]['latency']['last_max'] > self.ref['latency'][pg_id]['latency']['total_max']:
                            self.ref['latency'][pg_id]['latency']['total_max'] = stats['latency'][pg_id]['latency']['last_max']
                    else:
                        self.ref['latency'][pg_id]['latency']['total_max'] = self._get(stats, ['latency', pg_id, 'latency', 'total_max'], default=0)
                    stats['latency'][pg_id]['latency']['total_max'] = self.ref['latency'][pg_id]['latency']['total_max']

        if 'flow_stats' not in stats:
            return stats

        flow_stat_fields = ['rx_pkts', 'tx_pkts', 'rx_bytes', 'tx_bytes']

        for pg_id in stats['flow_stats']:
            for field in flow_stat_fields:
                if pg_id in self.ref['flow_stats'] and field in self.ref['flow_stats'][pg_id]:
                    for port in stats['flow_stats'][pg_id][field]:
                        if port in self.ref['flow_stats'][pg_id][field]:
                            # might be StatNotAvailable
                            try:
                                stats['flow_stats'][pg_id][field][port] -= self.ref['flow_stats'][pg_id][field][port]
                            except:
                                pass

        if 'latency' not in stats:
            return stats

        for pg_id in stats['latency']:
            if pg_id not in self.ref['latency']:
                continue
            if 'latency' in stats['latency'][pg_id]:
                to_delete = []
                for key in stats['latency'][pg_id]['latency']['histogram']:
                    if pg_id in self.ref['latency'] and key in self.ref['latency'][pg_id]['latency']['histogram']:
                        stats['latency'][pg_id]['latency']['histogram'][key] -= self.ref['latency'][pg_id]['latency']['histogram'][key]
                        if stats['latency'][pg_id]['latency']['histogram'][key] == 0:
                            to_delete.append(key)
                # cleaning values in histogram which are 0 (after decreasing ref from last)
                for del_key in to_delete:
                    del stats['latency'][pg_id]['latency']['histogram'][del_key]
                # special handling for 'total_max' field. We want to take it at first, from server reported 'total_max'
                # After running clear_stats, we zero it, and start calculating ourselves by looking at 'last_max' in each sampling
                if 'total_max' in self.ref['latency'][pg_id]['latency']:
                    stats['latency'][pg_id]['latency']['total_max'] = self.ref['latency'][pg_id]['latency']['total_max']
                if 'err_cntrs' in stats['latency'][pg_id] and 'err_cntrs' in self.ref['latency'][pg_id]:
                    for key in stats['latency'][pg_id]['err_cntrs']:
                         if key in self.ref['latency'][pg_id]['err_cntrs']:
                            stats['latency'][pg_id]['err_cntrs'][key] -= self.ref['latency'][pg_id]['err_cntrs'][key]

        return stats

    def streams_stats_to_table(self, pg_ids):
        stream_count = len(pg_ids)
        data = self.get_stats(pg_ids)

        # init table
        stats_table = text_tables.TRexTextTable('Streams Statistics')
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([10] + [17]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)
        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)

        # fill table
        def _add_vals(disp_key, internal_key, **k):
            vals = [self.get(data, ['flow_stats', pg_id, internal_key, 'total'], **k) for pg_id in pg_ids]
            stats_table.add_row([disp_key] + vals)

        def _add_sep(sep):
            stats_table.add_row([sep] + [''] * stream_count)

        _add_vals('Tx pps', 'tx_pps', format = True, suffix = 'pps')
        _add_vals('Tx bps L2', 'tx_bps', format = True, suffix = 'bps')
        _add_vals('Tx bps L1', 'tx_bps_l1', format = True, suffix = 'bps')
        _add_sep('---')
        _add_vals('Rx pps', 'rx_pps', format = True, suffix = 'pps')
        _add_vals('Rx bps', 'rx_bps', format = True, suffix = 'bps')
        _add_sep('----')
        _add_vals('opackets', 'tx_pkts')
        _add_vals('ipackets', 'rx_pkts')
        _add_vals('obytes', 'tx_bytes')
        _add_vals('ibytes', 'rx_bytes')
        _add_sep('-----')
        _add_vals('opackets', 'tx_pkts', format = True, suffix = 'pkts')
        _add_vals('ipackets', 'rx_pkts', format = True, suffix = 'pkts')
        _add_vals('obytes', 'tx_bytes', format = True, suffix = 'B')
        _add_vals('ibytes', 'rx_bytes', format = True, suffix = 'B')

        return stats_table


    def latency_stats_to_table(self, pg_ids):
        # Display data for at most 5 pgids.
        to_delete = []
        for id in self.max_hist.keys():
            if id not in pg_ids:
                to_delete.append(id)

        for id in to_delete:
            del self.max_hist[id]

        for id in pg_ids:
            if id not in self.max_hist.keys():
                self.max_hist[id] = [-1] * self.latency_window_size

        stream_count = len(pg_ids)
        data = self.get_stats(pg_ids)

        # init table
        stats_table = text_tables.TRexTextTable('Latency Statistics')
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([12] + [14]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)
        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)

        # fill table
        lstats_data = OrderedDict([('TX pkts',       []),
                                   ('RX pkts',       []),
                                   ('Max latency',   []),
                                   ('Avg latency',   []),
                                   ('-- Window --', [''] * stream_count),
                                   ('Last max',     []),
                                  ] + [('Last-%s' % i, []) for i in range(1, self.latency_window_size)] + [
                                   ('---', [''] * stream_count),
                                   ('Jitter',        []),
                                   ('----', [''] * stream_count),
                                   ('Errors',        []),
                                  ])

        for pg_id in pg_ids:
            last_max = self.get(data, ['latency', pg_id, 'latency', 'last_max'])
            lstats_data['TX pkts'].append(self.get(data, ['flow_stats', pg_id, 'tx_pkts', 'total']))
            lstats_data['RX pkts'].append(self.get(data, ['flow_stats', pg_id, 'rx_pkts', 'total']))
            lstats_data['Avg latency'].append(try_int(self.get(data, ['latency', pg_id, 'latency', 'average'])))
            lstats_data['Max latency'].append(try_int(self.get(data, ['latency', pg_id, 'latency', 'total_max'])))
            lstats_data['Last max'].append(last_max)
            self.max_hist[pg_id][self.max_hist_index] = last_max
            for i in range(1, self.latency_window_size):
                val = self.max_hist[pg_id][(self.max_hist_index - i) % self.latency_window_size]
                if val != -1:
                    lstats_data['Last-%s' % i].append(val)
                else:
                    lstats_data['Last-%s' % i].append(" ")
            lstats_data['Jitter'].append(self.get(data, ['latency', pg_id, 'latency', 'jitter']))
            errors = 0
            seq_too_low = self.get(data, ['latency', pg_id, 'err_cntrs', 'seq_too_low'])
            if is_integer(seq_too_low):
                errors += seq_too_low
            seq_too_high = self.get(data, ['latency', pg_id, 'err_cntrs', 'seq_too_high'])
            if is_integer(seq_too_high):
                errors += seq_too_high
            lstats_data['Errors'].append(format_num(errors,
                                            opts = 'green' if errors == 0 else 'red'))
        self.max_hist_index += 1
        self.max_hist_index %= self.latency_window_size


        stats_table.add_rows([[k] + v
                              for k, v in lstats_data.items()],
                              header=False)

        return stats_table


    def latency_histogram_to_table(self, pg_ids):
        stream_count = len(pg_ids)
        data = self.get_stats(pg_ids)
        lat_stats = data.get('latency')

        merged_histogram = {}
        for pg_id in pg_ids:
            merged_histogram.update(lat_stats[pg_id]['latency']['histogram'])
        histogram_size = min(self.max_histogram_size, len(merged_histogram))

        # init table
        stats_table = text_tables.TRexTextTable('Latency Histogram')
        stats_table.set_cols_align(["l"] + ["r"] * stream_count)
        stats_table.set_cols_width([12] + [14]   * stream_count)
        stats_table.set_cols_dtype(['t'] + ['t'] * stream_count)
        header = ["PG ID"] + [key for key in pg_ids]
        stats_table.header(header)

        # fill table
        for i in range(self.max_histogram_size - histogram_size):
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

        return stats_table
