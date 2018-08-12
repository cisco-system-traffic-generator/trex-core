
import sys
import threading
from .trex_exceptions import TRexError
from ..utils.text_opts import format_text
from collections import OrderedDict


class Logger(object):
    """
        TRex Logger
    """


    """
        verbose levels for the log
        with LEVEL_NONE no messages will be shown
    """
    __LEVEL_NONE       = 0
    __LEVEL_CRITICAL   = 1
    __LEVEL_ERROR      = 2
    __LEVEL_WARNING    = 3
    __LEVEL_INFO       = 4
    __LEVEL_DEBUG      = 5

    VERBOSES = OrderedDict([
        ('none',       __LEVEL_NONE),
        ('critical',   __LEVEL_CRITICAL),
        ('error',      __LEVEL_ERROR),
        ('warning',    __LEVEL_WARNING),
        ('info',       __LEVEL_INFO),
        ('debug',      __LEVEL_DEBUG)])


    def __init__ (self, verbose = 'error'):
        """
            TRex logger

            verbose - either 'none', 'critical', 'error', 'warning', 'info' or 'debug'

        """

        self.set_verbose(verbose)
        self.write_lock = threading.RLock()



    def set_verbose (self, verbose):
        """
            set verbose level (str)

            see __init__
        """
        
        if verbose not in Logger.VERBOSES.keys():
            raise TRexError("set_verbose: valid values by level of verbosity are: '{0}'".format("', '".join(Logger.VERBOSES.keys())))

        self.level = Logger.VERBOSES[verbose]
        self.verbose = verbose

    
    def get_verbose (self):
        """
            return the verbose level of the logger (str)

        """
        return self.verbose


    def critical (self, msg, newline = True, flush = False):
        """
            Logs a message with 'critical' level
        """
        self.__log(msg, level = Logger.__LEVEL_CRITICAL, newline = newline, flush = flush)


    def error (self, msg, newline = True, flush = False):
        """
            Logs a message with 'error' level
        """
        self.__log(msg, level = Logger.__LEVEL_ERROR, newline = newline, flush = flush)


    def warning (self, msg, newline = True, flush = False):
        """
            Logs a message with 'warning' level
        """
        self.__log(msg, level = Logger.__LEVEL_WARNING, newline = newline, flush = flush)


    def info (self, msg, newline = True, flush = False):
        """
            Logs a message with 'info' level
        """
        self.__log(msg, level = Logger.__LEVEL_INFO, newline = newline, flush = flush)


    def debug (self, msg, newline = True, flush = False):
        """
            Logs a message with 'debug' level
        """
        self.__log(msg, level = Logger.__LEVEL_DEBUG, newline = newline, flush = flush)


    # logging that comes from async event
    def async_log (self, msg, level = __LEVEL_INFO, newline = True):
        self.__log(msg, level, newline)


    # urgent async logging
    def urgent_async_log (self, msg, newline = True):
        self.__log(msg, level = Logger.__LEVEL_CRITICAL, newline = newline, flush = True)


    def pre_cmd (self, desc):
        """
            logs a prefix for command execution
        """
        self.info(format_text('\n{:<60}'.format(desc), 'bold'), newline = False, flush = True)


    def post_cmd (self, rc):
        """
            logs the result of a command
        """
        if rc:
            self.info(format_text("[SUCCESS]\n", 'green', 'bold'))
        else:
            self.info(format_text("[FAILED]\n", 'red', 'bold'))


    def log_cmd (self, desc):
        """
            full command log
        """
        self.pre_cmd(desc)
        self.post_cmd(True)


    def suppress (self, verbose = "warning"):
        """
            context-aware for suppressing commands

            verbose - under the suppression, which level should pass
                      by default, none shall pass
        """
        class Suppress(object):
            def __init__ (self, logger, verbose):
                self.logger  = logger
                self.verbose = verbose

            def __enter__ (self):
                self.saved_verbose = self.logger.get_verbose()
                self.logger.set_verbose(self.verbose)


            def __exit__ (self, type, value, traceback):
                self.logger.set_verbose(self.saved_verbose)


        return Suppress(self, verbose)



    # typo - backward compatible
    supress = suppress



    def __check_level (self, level):
        return (self.level >= level)


    def __log (self, msg, level, newline = True, flush = False):

        if not self.__check_level(level):
            return

        with self.write_lock:
            self._write(msg, newline)
            if flush:
                self._flush()


    # implemented by specific logger
    def _write(self, msg, newline):
        raise NotImplementedError("write")

    # implemented by specific logger
    def _flush(self):
        raise NotImplementedError("flush")





# stdout based impementation of the logger
class ScreenLogger(Logger):
    """
        A stdout based logger implementation
    """

    def __init__ (self, verbose = "error"):
        super(ScreenLogger, self).__init__(verbose)

    def _write (self, msg, newline):
        if newline:
            print(msg)
        else:
            print (msg),

    def _flush (self):
        sys.stdout.flush()

