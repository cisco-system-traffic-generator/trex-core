

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
              'underline': {'start': '\x1b[4m',
                            'end': '\x1b[24m'}}


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


def underline(text):
    return text_attribute(text, 'underline')


def text_attribute(text, attribute):
    return "{start}{txt}{stop}".format(start=TEXT_CODES[attribute]['start'],
                                       txt=text,
                                       stop=TEXT_CODES[attribute]['end'])


FUNC_DICT = {'blue': blue,
             'bold': bold,
             'green': green,
             'cyan': cyan,
             'magenta': magenta,
             'underline': underline,
             'red': red}


def format_text(text, *args):
    return_string = text
    for i in args:
        func = FUNC_DICT.get(i)
        if func:
            return_string = func(return_string)
    return return_string


if __name__ == "__main__":
    pass
