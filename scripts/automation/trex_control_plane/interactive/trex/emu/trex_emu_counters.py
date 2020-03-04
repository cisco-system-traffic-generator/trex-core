from ..astf.arg_verify import ArgVerify
from ..utils import text_tables
from ..utils.text_tables import *
from ..common.trex_exceptions import *
from ..common.trex_types import *
from trex_emu_conversions import *

import yaml
import json

class DataCounter(object):

    un_verbose_keys = ['name', 'value']
    verbose_keys = ['unit', 'zero', 'help']

    def __init__(self, conn, cmd):
        self.conn     = conn
        self.cmd      = cmd
        self.meta     = None
        self.add_data = None  # additional data, attached to every counters command

    # API #
    def get_counters(self, tables = None, cnt_filter = None, zero = True):
        """ 
            Get the wanted counters from server.

            :parameters:
                tables: list of strings
                    Names of all the wanted counters table. If not supplied, will get all of them.

                cnt_filter: list
                    List of counters type as strings. i.e: ['INFO', 'ERROR']. default is None means no filter

                verbose: bool
                    Show verbose version of each counter.
                
                with_zero: bool
                    Get zero values, default is True.
        """
        self._get_meta()
        self._update_meta_vals(tables)
        return self._filter_cnt(tables, cnt_filter, zero)

    def get_counters_headers(self):
        """ Simply print the counters headers names """
        self._get_meta()

        headers = [str(h) for h in list(self.meta.keys())]
        print('Current counters headers are:')

        for h in headers:
            print('\t%s' % h)

    def clear_counters(self):
        return self._get_counters(clear = True)

    def set_add_ns_data(self, port = None, vlan = None, tpid = None, clear = False):
        if clear:
            self.add_data = None
        else:
            self.add_data = conv_ns_for_tunnel(port, vlan, tpid)

    @staticmethod
    def print_counters(data, verbose = False, to_json = False, to_yaml = False):
        """
            Print tables for each ctx counter.

            :parameters:
                to_json: bool
                    if True prints a json version and exit.
        """
        if to_json:
            print(json.dumps(data, indent = 4))
            return
        
        if to_yaml:
            print(yaml.safe_dump(data, allow_unicode = True, default_flow_style = False))
            return

        if all(len(c) == 0 for c in data.values()):
            text_tables.print_colored_line('There is no information to show with current filter', 'yellow', buffer = sys.stdout)   
            return

        headers = list(DataCounter.un_verbose_keys)
        if verbose:
            headers += DataCounter.verbose_keys

        for table_name, counters in data.items():
            if len(counters):
                DataCounter._print_one_table(table_name, counters, headers)

    @staticmethod
    def _print_one_table(table_name, counters, headers):
        """
            Prints one ctx counter table, using the meta data values to reduce the zero value counters that doesn't send.  

            :parameters:
                table_name: str
                    Name of the counters table
                
                counters: list
                    List of dictionaries with data to print about table_name. Keys as counter names and values as counter value. 
                
                headers: list
                    List of all the headers in the table as strings.

                filters: list
                    List of counters type as strings. i.e: ['INFO', 'ERROR']

                verbose: bool
                    Show verbose version of counter tables.
        """
        def _get_info_postfix(info):
            postfixes = {'INFO': '', 'WARNING': '+', 'ERROR': '*'}
            info = info.upper()
            return postfixes.get(info, '')


        table = text_tables.TRexTextTable('%s counters' % table_name)
        table.header(headers)
        max_lens = [len(h) for h in headers]

        for cnt_info in counters:
            row_data = []
            for i, h in enumerate(headers):
                cnt_val = str(cnt_info.get(h, '-'))
                if h == 'name':
                    postfix = _get_info_postfix(cnt_info.get('info', ''))
                    cnt_val = cnt_info.get('name', '-') + postfix
                max_lens[i] = max(max_lens[i], len(cnt_val))
                row_data.append(cnt_val)
            table.add_row(row_data)

        # fix table look
        table.set_cols_align(['l'] + ['c'] * (len(headers) - 2) + ['l'])  # only middle headers are aligned to the middle
        table.set_cols_width(max_lens)
        table.set_cols_dtype(['a'] * len(headers))

        text_tables.print_table_with_header(table, table.title, buffer = sys.stdout)



    # Private functions #
    def _get_counters(self, meta = False, zero = False, mask = None, clear = False):
        """
            Gets counters from EMU server.

            :parameters:
                meta: bool
                    Get all the meta data.
                
                zero: bool
                    Bring zero values, default is False for optimizations.

                mask: list
                    list of string, get only specific counters blocks if it is empty get all.

                clear: bool
                    Clear all current counters.
            :return:
                dictionary describing counters of clients, fields that don't appear are treated as zero valued.
        """
        def _parse_info(info_code):
            info_dict = {
                0x12: 'INFO', 0x13: 'WARNING', 0x14: 'ERROR' 
            }
            return info_dict.get(info_code, 'UNKNOWN_TYPE')

        ver_args = {'types':
                [{'name': 'meta', 'arg': meta, 't': bool},
                {'name': 'zero', 'arg': zero, 't': bool},
                {'name': 'mask', 'arg': mask, 't': str, 'allow_list': True, 'must': False}]
                }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        if mask is not None:
            mask = listify(mask)
        
        params = {'meta': meta, 'zero': zero, 'mask': mask, 'clear': clear}
        if self.add_data is not None:
            params.update(self.add_data)

        rc = self._transmit(self.cmd, params)
        if not rc:
            raise TRexError(rc.err())

        if meta:
            # convert info values to string in meta mode
            for table_data in rc.data().values():
                table_cnts = table_data.get('meta', []) 
                for cnt in table_cnts:
                    cnt['info'] = _parse_info(cnt.get('info'))
        return rc.data()

    def _filter_cnt(self, tables, cnt_filter, zero):
        ''' Return a new dict with all the filtered counters '''
        
        def _pass_filter(cnt, cnt_filter, zero):
            if cnt_filter is not None and cnt.get('info') not in cnt_filter: 
                return False
            if not zero and cnt.get('value', 0) == 0:
                return False
            return True

        res = {}
        for table_name, table_data in self.meta.items():
            if tables is not None and table_name not in tables:
                continue

            new_cnt_list = [] 
            for cnt in table_data['meta']:
                if not _pass_filter(cnt, cnt_filter, zero):
                    continue

                new_cnt_list.append(cnt)

            if new_cnt_list:
                res[table_name] = new_cnt_list
        return res

    def _update_meta_vals(self, tables = None):
        ''' 
            Update meta counters with the current values.

            :parameters:

                tables: list of strings
                    Names of all the wanted counters table. If not supplied, will print all of them.   
        '''

        curr_cnts = self._get_counters(mask = tables)

        for table_name, table_data in self.meta.items():
            curr_table = curr_cnts.get(table_name, {})
            for cnt in table_data['meta']:
                cnt_name = cnt['name']
                cnt['value'] = curr_table.get(cnt_name, 0)
    
    def _get_meta(self):
        ''' Save meta data in object'''
        if self.meta is None:
            self.meta = self._get_counters(meta = True)

    def _transmit(self, method_name, params = None):
        ''' Using connection to transmit method name and parameters '''
        return self.conn.rpc.transmit(method_name, params)

