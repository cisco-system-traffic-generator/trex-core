from trex.common.trex_types import TRexError, listify
from trex.common.trex_types import DEFAULT_PROFILE_ID
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
        self._ref = {}
        self.tg_names_dict = {}

        self._ref_global = {}
        self._epoch_global = 0
        self.is_init = False
        self._counter_desc = None


    def _init_desc_and_ref(self, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        if is_sum:
            if self._ref_global:
                return
        else:
            if pid_input in self._ref.keys():
                return

        if not self._counter_desc:
            params = {'profile_id': pid_input}
            rc = self.rpc.transmit('get_counter_desc', params = params)
            if not rc:
                raise TRexError(rc.err())

            self._counter_desc = rc.data()['data']

        data = self._counter_desc

        if is_sum:
            for section in self.sections:
                self._ref_global[section] = [0] * len(data)
        else:
            self._ref[pid_input] = {}
            for section in self.sections:
                self._ref[pid_input][section] = [0] * len(data)

        if not self.is_init:
            self._desc = [0] * len(data)
            self._err_desc = {}
            self._max_desc_name_len = 0
            for item in data:
                self._desc[item['id']] = item
                self._max_desc_name_len = max(self._max_desc_name_len, len(item['name']))
                if item['info'] == 'error':
                    self._err_desc[item['name']] = item
            self.is_init = True


    def _clear_tg_name(self, pid_input = DEFAULT_PROFILE_ID):
        if pid_input in self.tg_names_dict.keys():
            self.tg_names_dict.pop(pid_input)

    def _remove_stats(self, pid_input = DEFAULT_PROFILE_ID):
        if pid_input in self._ref.keys():
            self._ref.pop(pid_input)
        self._clear_tg_name(pid_input)


    def _epoch_changed(self, new_epoch, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        if is_sum:
            self._ref_global.clear()
            self._epoch_global = new_epoch
        else:
            if pid_input in self._ref.keys():
                self._ref.pop(pid_input)
            if pid_input in self.tg_names_dict.keys():
                self.tg_names_dict.pop(pid_input)
            tg_info = {'epoch' : new_epoch, 'is_init' : False}
            self.tg_names_dict[pid_input] = tg_info


    def _translate_names_to_ids(self, tg_names, pid_input = DEFAULT_PROFILE_ID):
        list_of_tg_names = listify(tg_names)
        tg_ids = []
        if not list_of_tg_names:
            raise ASTFErrorBadTG("List of tg_names can't be empty")
        for tg_name in list_of_tg_names:
            if tg_name not in self.tg_names_dict[pid_input]['tg_names']:
                raise ASTFErrorBadTG("Template name %s  isn't defined in this profile" % tg_name)
            else:
                tg_ids.append(self.tg_names_dict[pid_input]['tg_names_dic'][tg_name])
        return tg_ids


    def _build_dict_vals_without_zero(self, section_list, skip_zero):
        def should_skip(skip_zero, k, v):
            return (not skip_zero) or (skip_zero and (not k['zero']) and  (v>0)) or k['zero']
        return dict([(k['name'], v) for k, v in zip(self._desc, section_list) if should_skip(skip_zero, k, v)])


    def _process_stats(self, stats, skip_zero, pid_input = DEFAULT_PROFILE_ID):
        processed_stats = {}
        
        del stats['epoch']
        # Translate template group ids to names
        for tg_id, tg_id_data in stats.items():
            tg_id_int = int(tg_id)
            del tg_id_data['epoch']
            del tg_id_data['name']
            assert tg_id_int != 0
            processed_stats[self.tg_names_dict[pid_input]['tg_names'][tg_id_int-1]] = tg_id_data
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


    def _get_tg_names(self, pid_input = DEFAULT_PROFILE_ID):

        tg_info = {'is_init': False, 'epoch' : -1}
        profile_id = pid_input

        if profile_id in list(self.tg_names_dict.keys()):
            tg_info = self.tg_names_dict[profile_id]

        pid_epoch = tg_info['epoch']

        params = {}
        params['profile_id'] = profile_id

        if pid_epoch >= 0 and tg_info['is_init'] == True:
            params['initialized'] = True
            params['epoch'] = pid_epoch
        else:
            params['initialized'] = False

        rc = self.rpc.transmit('get_tg_names', params=params)
        if not rc:
            raise TRexError(rc.err())

        server_epoch = rc.data()['epoch']

        # Update template group name and id
        if pid_epoch != server_epoch or tg_info['is_init'] == False:

            self._epoch_changed(server_epoch, pid_input = profile_id)
            tg_info = {}
            tg_names_dic = {}

            tg_info['epoch'] = server_epoch
            tg_info['is_init'] = True
            tg_names = rc.data()['tg_names']
            tg_info['tg_names'] = tg_names
            for tgid, name in enumerate(tg_names):
                tg_names_dic[name] = tgid+1
            tg_info['tg_names_dic'] = tg_names_dic

        self.tg_names_dict[profile_id] = tg_info

    def _get_traffic_tg_stats(self, tg_ids, pid_input = DEFAULT_PROFILE_ID):

        pid_epoch = self.tg_names_dict[pid_input]['epoch'] if pid_input in self.tg_names_dict.keys() else -1 
        assert pid_epoch >= 0
        stats = {}
        while tg_ids:
            size = min(len(tg_ids),self.MAX_TGIDS_ALLOWED_AT_ONCE)
            params = {'tg_ids': tg_ids[:size], 'epoch': pid_epoch, 'profile_id': pid_input}
            rc = self.rpc.transmit('get_tg_id_stats', params=params)
           
            if not rc:
                raise TRexError(rc.err())
            server_epoch = rc.data()['epoch']
            if pid_epoch != server_epoch:
                self._epoch_changed(server_epoch, pid_input = pid_input)
                return False, {}
            del tg_ids[:size]
            stats.update(rc.data())
        return True, stats


    def _get_stats_values(self, relative = True, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        self._init_desc_and_ref(pid_input, is_sum)
        params = {'profile_id' : pid_input}
        if is_sum:
            rc = self.rpc.transmit('get_total_counter_values', params = params)
            ref_epoch = self._epoch_global
        else:
            rc = self.rpc.transmit('get_counter_values', params = params)
            ref_epoch = self.tg_names_dict[pid_input]['epoch'] if pid_input in self.tg_names_dict.keys() else -1

        if not rc:
            raise TRexError(rc.err())

        data_epoch = rc.data()['epoch']
        if data_epoch != ref_epoch:
            self._epoch_changed(data_epoch, pid_input = pid_input, is_sum = is_sum)
        data = {'epoch': data_epoch}
        for section in self.sections:
            section_list = [0] * len(self._desc)
            for k, v in rc.data()[section].items():
                section_list[int(k)] = v
            if relative:
                for desc in self._desc:
                    id = desc['id']
                    if is_sum:
                        if self._ref_global and not desc['abs']: # return relative
                            section_list[id] -= self._ref_global[section][id]
                    else:
                        if pid_input in self._ref.keys() and not desc['abs']: # return relative
                            section_list[id] -= self._ref[pid_input][section][id]
            data[section] = section_list
        return data


    def get_tg_names(self, pid_input = DEFAULT_PROFILE_ID):
        self._get_tg_names(pid_input)
        return self.tg_names_dict[pid_input]['tg_names']


    def _get_num_of_tgids(self, pid_input = DEFAULT_PROFILE_ID):
        self._get_tg_names(pid_input)
        return len(self.tg_names_dict[pid_input]['tg_names'])


    def get_traffic_tg_stats(self, tg_names, skip_zero=True, for_table=False, pid_input = DEFAULT_PROFILE_ID):
        self._init_desc_and_ref(pid_input)
        self._get_tg_names(pid_input)
        pid_epoch = self.tg_names_dict[pid_input]['epoch'] if pid_input in self.tg_names_dict.keys() else -1
        assert pid_epoch >= 0
        tg_ids = self._translate_names_to_ids(tg_names, pid_input)
        success, traffic_stats = self._get_traffic_tg_stats(tg_ids, pid_input = pid_input)
        while not success:
            self._get_tg_names(pid_input)
            tg_ids = self._translate_names_to_ids(tg_names, pid_input)
            success, traffic_stats = self._get_traffic_tg_stats(tg_ids, pid_input = pid_input)
        if for_table:
            return self._process_stats_for_table(traffic_stats)
        return self._process_stats(traffic_stats, skip_zero, pid_input = pid_input)


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


    def get_stats(self,skip_zero = True, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        vals = self._get_stats_values(pid_input = pid_input, is_sum = is_sum)
        data = {}
        for section in self.sections:
            data[section] = self._build_dict_vals_without_zero(vals[section], skip_zero)
        return data


    def clear_stats(self, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        data = self._get_stats_values(relative = False, pid_input = pid_input, is_sum = is_sum)
        if is_sum:
            for section in self.sections:
                self._ref_global[section] = data[section]
        else:
            if pid_input in self._ref.keys():
                for section in self.sections:
                    self._ref[pid_input][section] = data[section]


    def to_table(self, with_zeroes = False, tgid = 0, pid_input = DEFAULT_PROFILE_ID, is_sum = False):
        self._get_tg_names(pid_input)
        num_of_tgids = len(self.tg_names_dict[pid_input]['tg_names'])
        title = ""
        data = {}
        if tgid == 0:
            data = self._get_stats_values(pid_input = pid_input, is_sum = is_sum)
            if is_sum:
                title = 'Traffic stats summary.'
            else:
                title = 'Traffic stats of Profile ID : ' + pid_input + '. Number of template groups = ' + str(num_of_tgids)
        else:
            if not 1 <= tgid <= num_of_tgids:
                raise ASTFErrorBadTG('Invalid tgid in to_table')
            else:
                name = self.tg_names_dict[pid_input]['tg_names'][tgid-1]
                title = 'Profile ID : ' + pid_input + '. Template Group Name: ' + name + '. Number of template groups = ' + str(num_of_tgids)
                data = self.get_traffic_tg_stats(tg_names = name, for_table=True, pid_input = pid_input)
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


    def _show_tg_names(self, start, amount, pid_input = DEFAULT_PROFILE_ID):
        tg_names = self.get_tg_names(pid_input)
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
