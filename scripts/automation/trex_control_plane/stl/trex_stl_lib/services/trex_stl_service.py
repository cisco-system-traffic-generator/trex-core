"""
Base API for services


Description:
  Base classes used to implement a service

Author:
  Itay Marom

"""



##################           
#
# STL service
#
#
##################

class STLService(object):
    '''
        Abstract class for implementing a service
    '''
    
    ERROR = 1
    WARN  = 2
    INFO  = 3

    def __init__ (self, ctx):
        self.ctx = ctx
        ctx._register_service(self)
        
        
######### implement-needed functions #########
    def consume (self, scapy_pkt):
        ''' 
            Handles a packet if it belongs to this module.
             
            returns True if packet was consumed and False
            if not

            scapy_pkt - A scapy formatted packet
        '''
        raise NotImplementedError


    def run (self, ctx):
        '''
            Executes the service in a run until completion
            model
        '''
        raise NotImplementedError

        
######### API          #########
    def pipe (self):
        '''
            Create a two side pipe
        '''
        return self.ctx.pipe()

    def err (self, msg):
        '''
            Genereate an error
        '''
        self.ctx.err(msg)


    def set_verbose (self, level):
        '''
            Sets verbose level
        '''
        self.verbose_level = level


    def log (self, msg, level = INFO):
        if not hasattr(self, 'is_verbose'):
            self.is_verbose = self.ERROR

        if self.verbose_level >= level:
            print(msg)

            

