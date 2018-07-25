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


def print_table_with_header(texttable_obj, header="", untouched_header="", buffer=sys.stdout):
    header = header.replace("_", " ").title() + untouched_header
    print(format_text(header, 'cyan', 'underline') + "\n", file=buffer)

    drawn_table = texttable_obj.draw()
    if drawn_table:
        print((drawn_table + "\n"), file=buffer)

if __name__ == "__main__":
    pass

