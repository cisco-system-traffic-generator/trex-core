#!/usr/bin/python

from console.plugins import *

'''
Example plugin
'''

class Hello_Plugin(ConsolePlugin):
    def plugin_description(self):
        return 'Simple example'


    # used to init stuff
    def plugin_load(self):
        # Adding arguments to be used at do_* functions
        self.add_argument(type = str,
                dest = 'username', # <----- variable name to be used
                help = 'Username to greet')


    # We build argparser from do_* functions, stripping the "do_" from name
    def do_greet(self, username): # <------ username was registered in plugin_load
        '''Greet some username'''
        self.trex_client.logger.log('Hello, %s!' % bold(username.capitalize())) # <--- trex_client is set implicitly

