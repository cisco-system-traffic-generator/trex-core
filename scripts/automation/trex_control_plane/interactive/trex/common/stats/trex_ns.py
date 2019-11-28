from ..trex_types import TRexError
from ...utils.text_opts import format_num, red, green
from ...utils import text_tables
import sys

class CNsStats(object):
    def __init__(self):
        self.reset()

    def reset(self):
        self.is_init = False

    def set_meta_values (self,meta,values):
        if meta is not None and values is not None:
           self.is_init = True
           self.meta = meta
           self.values = values
           self._init_desc_and_ref()
        else:
            self.is_init = False

    def _init_desc_and_ref(self):
        self.items ={}
        self._max_desc_name_len = 0
        for obj in self.meta:
            self.items[str(obj['id'])]=obj
            self._max_desc_name_len = max(self._max_desc_name_len, len(obj['name']))


    def get_values_stats(self):
        data ={}
        for key in self.values:
            name=self.items[key]['name'].replace('"',"")
            data[name]=self.values[key]
        return data

    def dump_stats(self):
        stats_table = text_tables.TRexTextTable('ns stats')
        stats_table.set_cols_align(["l","c",'l'] )
        stats_table.set_cols_width([self._max_desc_name_len,17,self._max_desc_name_len] )
        stats_table.set_cols_dtype(['t','t','t'])
        stats_table.header(['name','value','help'])

        for key in self.values:
            help =self.items[key]['help'].replace('"',"")
            name =self.items[key]['name'].replace('"',"")
            val  = self.values[key]
            stats_table.add_row([name,val,help])

        text_tables.print_table_with_header(stats_table, untouched_header = stats_table.title, buffer = sys.stdout)
        




