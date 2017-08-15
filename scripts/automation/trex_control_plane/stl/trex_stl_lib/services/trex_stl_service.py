"""
Base API for services


Description:
  Base classes used to implement a service

Author:
  Itay Marom

"""

#########################         
#                       
# STL service filter    
#                       
#                       
#########################
class STLServiceFilter(object):
    '''
        Abstract class for service filtering
        each class of services should
        implement a filter
    '''
    
    def add (self, service):
        '''
            Adds a service to the filter
        '''
        raise NotImplementedError
        
        
    def lookup (self, pkt):
        '''
            Given a 'pkt' return a list
            of services that should get this packet
            
            can be an empty list
        '''
        raise NotImplementedError


    def get_bpf_filter (self):
        '''
            Each filter needs to describe a BPF filter
            any packets matching the BPF pattern will
            be forwarded to the filter
        '''
        raise NotImplementedError


#########################         
#                       
# STL service           
#                       
#                       
#########################
class STLService(object):
    '''
        Abstract class for implementing a service
    '''
    
    ERROR = 3
    WARN  = 2
    INFO  = 1

    def __init__ (self, verbose_level = ERROR):

        # by default, set the service verbose level to error
        self.verbose_level = verbose_level
        

######### implement-needed functions #########
    def get_filter_type (self):
        '''
            Returns a filter class type
            The filter will manage packet
            forwarding for the services
            in this group
        '''
        raise NotImplementedError


    def run (self, pipe):
        '''
            Executes the service in a run until completion
            model
        '''
        raise NotImplementedError

 
        
        
######### API          #########
 
    def err (self, msg):
        '''
            Genereate an error
        '''
        raise STLError(msg)


    def set_verbose (self, level):
        '''
            Sets verbose level
        '''
        self.verbose_level = level


    def log (self, msg, level = INFO):
        '''
            Log a message if the level
            is high enough
        '''
        if level >= self.verbose_level:
            print(msg)

            

