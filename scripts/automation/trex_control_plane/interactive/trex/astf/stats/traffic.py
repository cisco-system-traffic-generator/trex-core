from trex.common.trex_types import TRexError, listify
from trex.utils.text_opts import format_num, red, green, format_text
from trex.utils import text_tables
from trex.astf.trex_astf_exceptions import ASTFErrorBadTG

class CAstfTrafficStats(object):
    MAX_TGIDS_ALLOWED_AT_ONCE = 10
    def __init__(self, rpc):
        self.rpc = rpc
        self.sections = ['client', 'server']
        self.reset()


    def reset(self):
        self.is_init = False
        self.epoch = -1
        self.tg_names = []
        self._ref = {}
        self.tg_names_dict = {}


    def _init_desc_and_ref(self):
        if self.is_init:
            return
        rc = self.rpc.transmit('get_counter_desc')
        if not rc:
            raise TRexError(rc.err())
        data = rc.data()['data']
        self._desc = [0] * len(data)
        self._err_desc = {}
        for section in self.sections:
            self._ref[section] = [0] * len(data)
        self._max_desc_name_len = 0
        for item in data:
            self._desc[item['id']] = item
            self._max_desc_name_len = max(self._max_desc_name_len, len(item['name']))
            if item['info'] == 'error':
                self._err_desc[item['name']] = item
        self.is_init = True


    def _epoch_changed(self, new_epoch):
        self.epoch = new_epoch
        self._ref.clear()
        del self.tg_names[:]
        self.tg_names_dict.clear()


    def _translate_names_to_ids(self, tg_names):
        list_of_tg_names = listify(tg_names)
        tg_ids = []
        if not list_of_tg_names:
            raise ASTFErrorBadTG("List of tg_names can't be empty")
        for tg_name in list_of_tg_names:
            if tg_name not in self.tg_names_dict:
                raise ASTFErrorBadTG("Template name %s  isn't defined in this profile" % tg_name)
            else:
                tg_ids.append(self.tg_names_dict[tg_name])
        return tg_ids


    def _build_dict_vals_without_zero(self, section_list, skip_zero):
        def should_skip(skip_zero, k, v):
            return (not skip_zero) or (skip_zero and (not k['zero']) and  (v>0)) or k['zero']
        return dict([(k['name'], v) for k, v in zip(self._desc, section_list) if should_skip(skip_zero, k, v)])


    def _process_stats(self, stats, skip_zero):
        processed_stats = {}
        del stats['epoch']
        # Translate template group ids to names
        for tg_id, tg_id_data in stats.items():
            tg_id_int = int(tg_id)
            del tg_id_data['epoch']
            del tg_id_data['name']
            assert tg_id_int != 0
            processed_stats[self.tg_names[tg_id_int-1]] = tg_id_data
        # Translate counters ids to names
        for tg_name in processed_stats.keys():
            for section in self.sections:
                section_list = [0] * len(self._desc)
                for k, v in processed_stats[tg_name][section].items():
                    section_list[int(k)] = v
                processed_stats[tg_name][section] = self._build_dict_vals_without_zero(section_list, skip_zero)
        return processed_stats


    def _process_stats_for_table(self, stats):
        del stats['epoch']
        for processed_stats in stats.values():
            for section in self.sections:
                section_list = [0] * len(self._desc)
                for k, v in processed_stats[section].items():
                    section_list[int(k)] = v
                processed_stats[section] = section_list
        return processed_stats


    def _get_tg_names(self):
        params = {}
        if self.epoch >= 0:
            params['initialized'] = True
            params['epoch'] = self.epoch
        else:
            params['initialized'] = False
        rc = self.rpc.transmit('get_tg_names', params=params)
        if not rc:
            raise TRexError(rc.err())
        server_epoch = rc.data()['epoch']
        if self.epoch != server_epoch:
            self._epoch_changed(server_epoch)
            self.tg_names = rc.data()['tg_names']
            for tgid, name in enumerate(self.tg_names):
                self.tg_names_dict[name] = tgid+1


    def _get_traffic_tg_stats(self, tg_ids):
        assert self.epoch >= 0
        stats = {}
        while tg_ids:
            size = min(len(tg_ids),self.MAX_TGIDS_ALLOWED_AT_ONCE)
            params = {'tg_ids': tg_ids[:size], 'epoch': self.epoch}
            rc = self.rpc.transmit('get_tg_id_stats', params=params)
            if not rc:
                raise TRexError(rc.err())
            server_epoch = rc.data()['epoch']
            if self.epoch != server_epoch:
                self._epoch_changed(server_epoch)
                return False, {}
            del tg_ids[:size]
            stats.update(rc.data())
        return True, stats


    def _get_stats_values(self, relative = True):
        self._init_desc_and_ref()
        rc = self.rpc.transmit('get_counter_values')
        if not rc:
            raise TRexError(rc.err())
        ref_epoch = self.epoch
        data_epoch = rc.data()['epoch']
        if data_epoch != ref_epoch:
            self._epoch_changed(data_epoch)
        data = {'epoch': data_epoch}
        for section in self.sections:
            section_list = [0] * len(self._desc)
            for k, v in rc.data()[section].items():
                section_list[int(k)] = v
            if relative:
                for desc in self._desc:
                    id = desc['id']
                    if self._ref and not desc['abs']: # return relative
                        section_list[id] -= self._ref[section][id]
            data[section] = section_list
        return data


    def get_tg_names(self):
        self._get_tg_names()
        return self.tg_names


    def _get_num_of_tgids(self):
        self._get_tg_names()
        return len(self.tg_names)


    def get_traffic_tg_stats(self, tg_names, skip_zero=True, for_table=False):
        self._init_desc_and_ref()
        self._get_tg_names()
        assert self.epoch >= 0
        tg_ids = self._translate_names_to_ids(tg_names)
        success, traffic_stats = self._get_traffic_tg_stats(tg_ids)
        while not success:
            self._get_tg_names()
            tg_ids = self._translate_names_to_ids(tg_names)
            success, traffic_stats = self._get_traffic_tg_stats(tg_ids)
        if for_table:
            return self._process_stats_for_table(traffic_stats)
        return self._process_stats(traffic_stats, skip_zero)


    def is_traffic_stats_error(self, stats):
        data = {}
        errs = False

        for section in self.sections:
            s = stats[section]
            data[section] = {}
            for k, v in self._err_desc.items():
                if s.get(k, 0):
                    data[section][k] = v['help']
                    errs = True

        return (errs, data)


    def get_stats(self,skip_zero = True):
        vals = self._get_stats_values()
        data = {}
        for section in self.sections:
            data[section] = self._build_dict_vals_without_zero(vals[section], skip_zero)
        return data


    def clear_stats(self):
        data = self._get_stats_values(relative = False)
        for section in self.sections:
            self._ref[section] = data[section]


    def to_table(self, with_zeroes = False, tgid = 0):
        self._get_tg_names()
        num_of_tgids = len(self.tg_names)
        title = ""
        data = {}
        if tgid == 0:
            data = self._get_stats_values()
            title = 'Traffic stats summary. Number of template groups = ' + str(num_of_tgids)
        else:
            if not 1 <= tgid <= num_of_tgids:
                raise ASTFErrorBadTG('Invalid tgid in to_table')
            else:
                name = self.tg_names[tgid-1]
                title = 'Template Group Name: ' + name + '. Number of template groups = ' + str(num_of_tgids)
                data = self.get_traffic_tg_stats(tg_names = name, for_table=True)
        sec_count = len(self.sections)

        # init table
        stats_table = text_tables.TRexTextTable(title)
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


    def _show_tg_names(self, start, amount):
        tg_names = self.get_tg_names()
        names = tg_names[start : start+amount]
        if not tg_names:
            print(format_text('There are no template groups!', 'bold'))
        elif not names:
                print(format_text('Invalid parameter combination!', 'bold'))
        else:
            if len(names) != len(tg_names):
                print(format_text('Showing only %s names out of %s.' % (len(names), len(tg_names)), 'bold'))
            NUM_OF_COLUMNS = 5
            while names:
                print('  '.join(map(lambda x: '%-20s' % x, names[:NUM_OF_COLUMNS])))
                del names[:NUM_OF_COLUMNS]
