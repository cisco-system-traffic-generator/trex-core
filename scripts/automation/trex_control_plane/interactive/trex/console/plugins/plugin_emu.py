#!/usr/bin/python
import pprint
import argparse
import inspect

from trex.utils import parsing_opts
from trex.emu.trex_emu_client import EMUClient
from trex.console.plugins import *
from trex.common.trex_exceptions import TRexError

'''
EMU plugin
'''

class Emu_Plugin(ConsolePlugin):

    EMU_PREFIX = 'emu_'  # prefix for every registered method in trex_console

    def plugin_description(self):
        return 'Emu plugin is used in order to communicate with emulation server, i.e loading emu profile'

    def __init__(self):
        super(Emu_Plugin, self).__init__()
        self.console = None

    def plugin_load(self):
        
        if self.console is None:
            raise TRexError("Trex console must be provided in order to load emu plugin")
        
        client = self.console.client
        verbose = client.logger.get_verbose()
        
        # taking parameters from original 
        self.c = EMUClient(server = self.console.emu_server, verbose_level = verbose, logger = client.logger)
        self.c.connect()

        self.console.load_client_plugin_functions(self.c, func_prefix = Emu_Plugin.EMU_PREFIX)
        self.console.emu_client = self.c  # adding a ref to EMUClient in console

    def plugin_unload(self):
        if self.console is None:
            raise TRexError("Trex console must be provided in order to unload emu plugin")
        
        self.console.unload_client_plugin_functions(func_prefix = Emu_Plugin.EMU_PREFIX)
        self.c.disconnect()

    def set_plugin_console(self, trex_console):
        self.console = trex_console
