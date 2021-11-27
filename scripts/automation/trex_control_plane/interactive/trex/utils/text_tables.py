from __future__ import print_function

import sys
import copy

from texttable import Texttable
from .text_opts import format_text


class Tableable(object):
    '''
        A class implementing this object
        provides to_table method
    '''
    def to_table(self):
        raise NotImplementedError()


class TRexTextTable(Texttable):

    def __init__(self, title = None):
        Texttable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', '|', '+', '-'])
        self.set_deco(Texttable.HEADER | Texttable.VLINES)
        self.title = title


    @staticmethod
    def merge (tables, title = None, row_filter = lambda row: True):

        # init rows (keys)
        rows = [row[:1] for row in tables[0]._rows]

        # generate the rows
        for table in tables:

            col_values = [row[1:] for row in table._rows]
            rows = [row + col for row, col in zip(rows, col_values)]

        # filter any rows if needed
        rows = [row for row in rows if row_filter(row[1:])]

        output = TRexTextTable(tables[0].title)
        if not rows:
            return output

        cols = len(rows[0])

        output.set_cols_align([tables[0]._align[0]] + [tables[0]._align[1]] * (cols - 1))
        output.set_cols_width([tables[0]._width[0]] + [tables[0]._width[1]] * (cols - 1))
        output.set_cols_dtype([tables[0]._dtype[0]] + [tables[0]._dtype[1]] * (cols - 1))

        output.add_rows(rows, header = False)
        header_title  = tables[0]._header[0]
        header_values = [table._header[1] for table in tables]
        output.header([header_title] + header_values)

        return output


class TRexTextInfo(Texttable):

    def __init__(self, title = None):
        Texttable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', ':', '-', '-'])
        self.set_deco(Texttable.VLINES)
        self.title = title


def generate_trex_stats_table():
    pass


def print_table_with_header(texttable_obj, header="", untouched_header="", buffer=sys.stdout, color = 'cyan'):
    header = header.replace("_", " ").title() + untouched_header
    print(format_text(header, color, 'underline') + "\n", file=buffer)

    drawn_table = texttable_obj.draw()
    if drawn_table:
        print((drawn_table + "\n"), file=buffer)


def print_colored_line(text, color, buffer = sys.stdout):
    print(format_text(text, color, 'bold') + "\n", file=buffer)


def print_table_by_keys(data, keys_to_headers, title = None, empty_msg = 'empty'):

    def _iter_dict(d, keys, max_lens):
        row_data = []
        for j, key in enumerate(keys):
            val = str(d.get(key, '-'))
            row_data.append(val)
            max_lens[j] = max(max_lens[j], len(val))
        return row_data

    if len(data) == 0:
        print_colored_line(empty_msg, 'yellow', buffer = sys.stdout)
        return

    table = TRexTextTable(title)
    headers = [e.get('header') for e in keys_to_headers]
    keys = [e.get('key') for e in keys_to_headers]

    max_lens = [len(h) for h in headers]
    table.header(headers)

    if type(data) is list:
        for one_record in data:
            row_data = _iter_dict(one_record, keys, max_lens)
            table.add_row(row_data)
    elif type(data) is dict:
            row_data = _iter_dict(data, keys, max_lens)
            table.add_row(row_data)

    # fix table look
    table.set_cols_align(["c"] * len(headers))
    table.set_cols_width(max_lens)
    table.set_cols_dtype(['a'] * len(headers))

    print_table_with_header(table, table.title, buffer = sys.stdout)


if __name__ == "__main__":
    pass

