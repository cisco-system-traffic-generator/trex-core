
import external_packages
from texttable import Texttable
from common.text_opts import format_text

class ExtendedTextTable(Texttable):
    def __init__(self):
        Texttable.__init__(self)

    def add_row(self, array, idx=None):
        """Add a row in the rows stack
        - cells can contain newlines and tabs
        """

        self._check_row_size(array)

        if not hasattr(self, "_dtype"):
            self._dtype = ["a"] * self._row_size

        cells = []
        for i, x in enumerate(array):
            cells.append(self._str(i, x))
        if idx is None:
            self._rows.append(cells)
        else:
            assert isinstance(idx, int)
            self._rows.insert(idx, cells)

    def add_div_rows(self, div_char, *rows):
        while rows:
            cur_row_idx = rows[0]
            self.add_row(self._gen_div_row(div_char), cur_row_idx)
            rows = map(lambda x: x+1 if x > cur_row_idx else x, rows)[1:]

    def _gen_div_row(self, div_char):
        return [div_char * n for n in self._width]


class TRexTextTable(ExtendedTextTable):

    def __init__(self):
        ExtendedTextTable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', '|', '-', '-'])
        self.set_deco(ExtendedTextTable.HEADER | ExtendedTextTable.VLINES)


class TRexTextInfo(ExtendedTextTable):

    def __init__(self):
        Texttable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', ':', '-', '-'])
        self.set_deco(ExtendedTextTable.VLINES)


def generate_trex_stats_table():
    pass

def print_table_with_header(texttable_obj, header="", untouched_header=""):
    header = header.replace("_", " ").title() + untouched_header
    print format_text(header, 'cyan', 'underline') + "\n"

    print (texttable_obj.draw() + "\n").encode('utf-8')

if __name__ == "__main__":
    pass

