#!/router/bin/python

#from rpc_exceptions import RPCExceptionHandler, WrappedRPCError

from jsonrpclib import Fault, ProtocolError, AppError

class RPCError(Exception):
    """
    This is the general RPC error exception class from which :exc:`trex_exceptions.TRexException` inherits. 

    Every exception in this class has as error format according to JSON-RPC convention convention: code, message and data.

    """
    def __init__(self, code, message, remote_data = None):
        self.code   = code
        self.msg    = message or self._default_message
        self.data   = remote_data
        self.args   = (code, self.msg, remote_data)

    def __str__(self):
        return self.__repr__()

    def __repr__(self):
        if self.args[2] is not None:
            return u"[errcode:%s] %s. Extended data: %s" % self.args
        else:
            return u"[errcode:%s] %s" % self.args[:2]

class TRexException(RPCError):
    """ 
    This is the most general TRex exception.
    
    All exceptions inherits from this class has an error code and a default message which describes the most common use case of the error.

    This exception isn't used by default and will only when an unrelated to ProtocolError will occur, and it can't be resolved to any of the deriviate exceptions.

    """
    code = -10
    _default_message = 'TRex encountered an unexpected error. please contact TRex dev team.'
    # api_name = 'TRex'

class TRexError(TRexException):
    """ 
    This is the most general TRex exception.

    This exception isn't used by default and will only when an unrelated to ProtocolError will occur, and it can't be resolved to any of the deriviate exceptions.
    """
    code = -11
    _default_message = 'TRex run failed due to wrong input parameters, or due to reachability issues.'

class TRexWarning(TRexException):
    """ Indicates a warning from TRex server. When this exception raises it normally used to indicate required data isn't ready yet """
    code = -12
    _default_message = 'TRex is starting (data is not available yet).'

class TRexRequestDenied(TRexException):
    """ Indicates the desired reques was denied by the server """
    code = -33
    _default_message = 'TRex desired request denied because the requested resource is already taken. Try again once TRex is back in IDLE state.'

class TRexInUseError(TRexException):
    """
    Indicates that TRex is currently in use

    """
    code = -13
    _default_message = 'TRex is already being used by another user or process. Try again once TRex is back in IDLE state.'

class TRexRunFailedError(TRexException):
    """ Indicates that TRex has failed due to some reason. This Exception is used when TRex process itself terminates due to unknown reason """
    code = -14
    _default_message = ''

class TRexIncompleteRunError(TRexException):
    """ 
    Indicates that TRex has failed due to some reason.
    This Exception is used when TRex process itself terminated with error fault or it has been terminated by an external intervention in the OS.

    """
    code = -15
    _default_message = 'TRex run was terminated unexpectedly by outer process or by the hosting OS'

EXCEPTIONS = [TRexException, TRexError, TRexWarning, TRexInUseError, TRexRequestDenied, TRexRunFailedError, TRexIncompleteRunError]

class CExceptionHandler(object):
    """ 
    CExceptionHandler is responsible for generating TRex API related exceptions in client side.
    """
    def __init__(self, exceptions):
        """ 
        Instatiate a CExceptionHandler object

        :parameters:

         exceptions : list
            a list of all TRex acceptable exception objects.
            
            default list:
               - :exc:`trex_exceptions.TRexException`
               - :exc:`trex_exceptions.TRexError`
               - :exc:`trex_exceptions.TRexWarning`
               - :exc:`trex_exceptions.TRexInUseError`
               - :exc:`trex_exceptions.TRexRequestDenied`
               - :exc:`trex_exceptions.TRexRunFailedError`
               - :exc:`trex_exceptions.TRexIncompleteRunError`

        """
        if isinstance(exceptions, type):
            exceptions = [ exceptions, ]
        self.exceptions = exceptions
        self.exceptions_dict = dict((e.code, e) for e in self.exceptions)

    def gen_exception (self, err):
        """
        Generates an exception based on a general ProtocolError exception object `err`. 

        When TRex is reserved, no other user can start new TRex runs.

                
        :parameters:
        
         err : exception
            a ProtocolError exception raised by :class:`trex_client.CTRexClient` class

        :return: 
         A TRex exception from the exception list defined in class creation.

         If such exception wasn't found, returns a TRexException exception

        """
        code, message, data = err
        try:
            exp = self.exceptions_dict[code]
            return exp(exp.code, message, data)
        except KeyError:
            # revert to TRexException when unknown error application raised
             return TRexException(err)


exception_handler = CExceptionHandler( EXCEPTIONS )

