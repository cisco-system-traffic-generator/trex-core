from ..common.trex_types import StatNotAvailable

# TRex PG stats - (needs a refactor)

class CPgIdStats(object):
    def __init__(self, rpc):
        self.rpc = rpc
        self.reset()

    def reset(self):
        # sample when clear was last called. Values we return are last - ref
        self.ref =  {'flow_stats': {}, 'latency': {}}

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
                    try:
                        del self.ref['flow_stats'][int(del_id)]
                    except:
                        pass
                    try:
                        del self.ref['latency'][int(del_id)]
                    except:
                        pass
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


