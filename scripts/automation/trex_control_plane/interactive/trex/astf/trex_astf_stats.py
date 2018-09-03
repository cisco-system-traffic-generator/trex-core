from ..common.trex_types import TRexError
from ..utils.text_opts import format_num, red, green
from ..utils import text_tables

class CAstfStats(object):
    def __init__(self, rpc):
        self.rpc = rpc
        self.sections = ['client', 'server']
        self.reset()

    def reset(self):
        self.is_init = False

    def _init_desc_and_ref(self):
        if self.is_init:
            return
        rc = self.rpc.transmit('get_counter_desc')
        if not rc:
            raise TRexError(rc.err())
        data = rc.data()['data']
        self._desc = [0] * len(data)
        self._ref = {'epoch': -1}
        for section in self.sections:
            self._ref[section] = [0] * len(data)
        self._max_desc_name_len = 0
        for item in data:
            self._desc[item['id']] = item
            self._max_desc_name_len = max(self._max_desc_name_len, len(item['name']))
        self.is_init = True

    def _get_stats_values(self, relative = True):
        self._init_desc_and_ref()
        rc = self.rpc.transmit('get_counter_values')
        if not rc:
            raise TRexError(rc.err())
        ref_epoch = self._ref['epoch']
        data_epoch = rc.data()['epoch']
        data = {'epoch': data_epoch}
        for section in self.sections:
            section_list = [0] * len(self._desc)
            for k, v in rc.data()[section].items():
                section_list[int(k)] = v
            if relative:
                for desc in self._desc:
                    id = desc['id']
                    if ref_epoch == data_epoch and not desc['abs']: # return relative
                        section_list[id] -= self._ref[section][id]
            data[section] = section_list
        return data

    def get_stats(self):
        vals = self._get_stats_values()
        data = {}
        for section in self.sections:
            data[section] = dict([(k['name'], v) for k, v in zip(self._desc, vals[section]) if k['real']])
        return data

    def clear_stats(self):
        data = self._get_stats_values(relative = False)
        for section in self.sections:
            self._ref[section] = data[section]
        self._ref['epoch'] = data['epoch']

    def to_table(self, with_zeroes = False):
        data = self._get_stats_values()
        sec_count = len(self.sections)

        # init table
        stats_table = text_tables.TRexTextTable('ASTF Stats')
        stats_table.set_cols_align(["r"] * (1 + sec_count) + ["l"])
        stats_table.set_cols_width([self._max_desc_name_len] + [17] * sec_count + [self._max_desc_name_len])
        stats_table.set_cols_dtype(['t'] * (2 + sec_count))
        header = [''] + self.sections + ['']
        stats_table.header(header)

        for desc in self._desc:
            if desc['real']:
                vals = [data[section][desc['id']] for section in self.sections]
                if not (with_zeroes or desc['zero'] or any(vals)):
                    continue
                if desc['info'] == 'error':
                    vals = [red(v) if v else green(v) for v in vals]
                if desc['units']:
                    vals = [format_num(v, suffix = desc['units']) for v in vals]

                stats_table.add_row([desc['name']] + vals + [desc['help']])
            else:
                stats_table.add_row([desc['name']] + [''] * (1 + sec_count))

        return stats_table
