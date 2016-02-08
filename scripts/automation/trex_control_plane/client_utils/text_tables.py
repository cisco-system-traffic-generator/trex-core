from texttable import Texttable
from trex_control_plane.common.text_opts import format_text

class TRexTextTable(Texttable):

    def __init__(self):
        Texttable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', '|', '-', '-'])
        self.set_deco(Texttable.HEADER | Texttable.VLINES)

class TRexTextInfo(Texttable):

    def __init__(self):
        Texttable.__init__(self)
        # set class attributes so that it'll be more like TRex standard output
        self.set_chars(['-', ':', '-', '-'])
        self.set_deco(Texttable.VLINES)

def generate_trex_stats_table():
    pass

def print_table_with_header(texttable_obj, header="", untouched_header=""):
    header = header.replace("_", " ").title() + untouched_header
    print format_text(header, 'cyan', 'underline') + "\n"

    print (texttable_obj.draw() + "\n").encode('utf-8')

if __name__ == "__main__":
    pass

