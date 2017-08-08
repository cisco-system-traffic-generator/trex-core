from trex_stl_lib.api import *
from trex_stl_lib.utils import parsing_opts, text_tables
from trex_stl_lib.utils.parsing_opts import is_valid_file, check_mac_addr, check_ipv4_addr


class ConsolePlugin(object):
    def plugin_description(self):
        raise NotImplementedError('Should implement plugin_description() function')

    def plugin_load(self):
        '''called upon loading of plugin'''
        pass

    def plugin_unload(self):
        '''called upon removing plugin'''
        pass

    # define argparse argument for do_* functions
    def add_argument(self, *a, **k):
        p = parsing_opts.CCmdArgParser(add_help = False)
        p.add_argument(*a, **k)
        assert(len(p._actions) == 1)
        name = p._actions[0].dest
        if name in self._args:
            raise Exception('Duplicate argument dest ("%s"), please use unique names' % name)
        self._args[name] = {'a': a, 'k': k}


