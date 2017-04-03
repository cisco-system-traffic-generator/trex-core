import json
import re
import sys

TEXT_CODES = {'bold': {'start': '\x1b[1m',
                       'end': '\x1b[22m'},
              'cyan': {'start': '\x1b[36m',
                       'end': '\x1b[39m'},
              'blue': {'start': '\x1b[34m',
                       'end': '\x1b[39m'},
              'red': {'start': '\x1b[31m',
                      'end': '\x1b[39m'},
              'magenta': {'start': '\x1b[35m',
                          'end': '\x1b[39m'},
              'green': {'start': '\x1b[32m',
                        'end': '\x1b[39m'},
              'yellow': {'start': '\x1b[33m',
                         'end': '\x1b[39m'},
              'underline': {'start': '\x1b[4m',
                            'end': '\x1b[24m'}}

class TextCodesStripper:
    keys = [re.escape(v['start']) for k,v in TEXT_CODES.items()]
    keys += [re.escape(v['end']) for k,v in TEXT_CODES.items()]
    pattern = re.compile("|".join(keys))

    @staticmethod
    def strip (s):
        return re.sub(TextCodesStripper.pattern, '', s)

def clear_formatting(s):
    return TextCodesStripper.strip(s)

def format_num (size, suffix = "", compact = True, opts = None):
    if opts is None:
        opts = ()

    txt = "NaN"

    if type(size) == str:
        return "N/A"

    u = ''

    if compact:
        for unit in ['','K','M','G','T','P']:
            if abs(size) < 1000.0:
                u = unit
                break
            size /= 1000.0

    if isinstance(size, float):
        txt = "%3.2f" % (size)
    else:
        txt = "{:,}".format(size)

    if u or suffix:
        txt += " {:}{:}".format(u, suffix)

    if isinstance(opts, tuple):
        return format_text(txt, *opts)
    else:
        return format_text(txt, (opts))



def format_time (t_sec):
    if t_sec < 0:
        return "infinite"

    if t_sec == 0:
        return "zero"

    if t_sec < 1:
        # low numbers
        for unit in ['ms', 'usec', 'ns']:
            t_sec *= 1000.0
            if t_sec >= 1.0:
                return '{:,.2f} [{:}]'.format(t_sec, unit)

        return "NaN"

    else:
        # seconds
        if t_sec < 60.0:
            return '{:,.2f} [{:}]'.format(t_sec, 'sec')

        # minutes
        t_sec /= 60.0
        if t_sec < 60.0:
            return '{:,.2f} [{:}]'.format(t_sec, 'minutes')

        # hours
        t_sec /= 60.0
        if t_sec < 24.0:
            return '{:,.2f} [{:}]'.format(t_sec, 'hours')

        # days
        t_sec /= 24.0
        return '{:,.2f} [{:}]'.format(t_sec, 'days')


def format_percentage (size):
    return "%0.2f %%" % (size)

def bold(text):
    return text_attribute(text, 'bold')


def cyan(text):
    return text_attribute(text, 'cyan')


def blue(text):
    return text_attribute(text, 'blue')


def red(text):
    return text_attribute(text, 'red')


def magenta(text):
    return text_attribute(text, 'magenta')


def green(text):
    return text_attribute(text, 'green')

def yellow(text):
    return text_attribute(text, 'yellow')

def underline(text):
    return text_attribute(text, 'underline')

# apply attribute on each non-empty line
def text_attribute(text, attribute):
    return '\n'.join(['{start}{txt}{end}'.format(
            start = TEXT_CODES[attribute]['start'],
            txt = line,
            end = TEXT_CODES[attribute]['end'])
                      if line else '' for line in ('%s' % text).split('\n')])


FUNC_DICT = {'blue': blue,
             'bold': bold,
             'green': green,
             'yellow': yellow,
             'cyan': cyan,
             'magenta': magenta,
             'underline': underline,
             'red': red}


def __format_text_tty(text, *args):
    return_string = text
    for i in args:
        func = FUNC_DICT.get(i)
        if func:
            return_string = func(return_string)

    return return_string

    
def __format_text_non_tty (text, *args):
    return text
   
     
# choose according to stdout type
format_text = __format_text_tty if sys.stdout.isatty() else __format_text_non_tty


def format_threshold (value, red_zone, green_zone):
    try:
        if value >= red_zone[0] and value <= red_zone[1]:
            return format_text("{0}".format(value), 'red')

        if value >= green_zone[0] and value <= green_zone[1]:
            return format_text("{0}".format(value), 'green')
    except TypeError:
        # if value is not comparable or not a number - skip this
        pass

    return "{0}".format(value)

# pretty print for JSON
def pretty_json (json_str, use_colors = True):
    pretty_str = json.dumps(json.loads(json_str), indent = 4, separators=(',', ': '), sort_keys = True)

    if not use_colors:
        return pretty_str

    try:
        # int numbers
        pretty_str = re.sub(r'([ ]*:[ ]+)(\-?[1-9][0-9]*[^.])',r'\1{0}'.format(blue(r'\2')), pretty_str)
        # float
        pretty_str = re.sub(r'([ ]*:[ ]+)(\-?[1-9][0-9]*\.[0-9]+)',r'\1{0}'.format(magenta(r'\2')), pretty_str)
        # # strings
        #
        pretty_str = re.sub(r'([ ]*:[ ]+)("[^"]*")',r'\1{0}'.format(red(r'\2')), pretty_str)
        pretty_str = re.sub(r"('[^']*')", r'{0}\1{1}'.format(TEXT_CODES['magenta']['start'],
                                                             TEXT_CODES['red']['start']), pretty_str)
    except :
        pass

    return pretty_str


if __name__ == "__main__":
    pass
