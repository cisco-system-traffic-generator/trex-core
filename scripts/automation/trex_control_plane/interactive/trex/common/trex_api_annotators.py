"""
    holds API decorators
"""

from functools import wraps
import time

from .trex_types import *
from .trex_ctx import TRexCtx
from .trex_exceptions import TRexError, TRexConsoleNoAction, TRexConsoleError

from ..utils.text_opts import format_time, format_text


# API decorator - double wrap because of argument
def client_api(api_type = 'getter', connected = True):
    """
        client API annotator
        
        api_type: str
            'getter', 'command', 'console'

        connected: bool
            if True enforce connection of the client

    """

    def wrap (f):
        @wraps(f)
        def wrap2(*args, **kwargs):
            client = args[0]

            func_name = f.__name__


            try:
                # before we enter the API, set the async thread to signal in case of connection lost
                client.conn.sigint_on_conn_lost_enable()

                # check connection
                if connected and not client.conn.is_connected():

                    if client.conn.is_marked_for_disconnect():
                        # connection state is marked for disconnect - something went wrong
                        raise TRexError("'{0}' - connection to the server had been lost: '{1}'".format(func_name, client.conn.get_disconnection_cause()))
                    else:
                        # simply was called while disconnected
                        raise TRexError("'{0}' - is not valid while disconnected".format(func_name))

                # call the API
                ret = f(*args, **kwargs)

            except KeyboardInterrupt as e:
                # SIGINT can be either from ctrl + c or from the async thread to interrupt the main thread
                if client.conn.is_marked_for_disconnect():
                    raise TRexError("'{0}' - connection to the server had been lost: '{1}'".format(func_name, client.conn.get_disconnection_cause()))
                else:
                    raise TRexError("'{0}' - interrupted by a keyboard signal (probably ctrl + c)".format(func_name))

            finally:
                # when we exit API context - disable SIGINT from the async thread
                client.conn.sigint_on_conn_lost_disable()


            return ret

        wrap2.api_type = api_type
        return wrap2

    return wrap



    def verify_connected(f):
        @wraps(f)
        def wrap(*args):
            inst = args[0]
            func_name = f.__name__
            if func_name.startswith("do_"):
                func_name = func_name[3:]

            if not inst.client.is_connected():
                print(format_text("\n'{0}' cannot be executed on offline mode\n".format(func_name), 'bold'))
                return

            ret = f(*args)
            return ret

        return wrap


def console_api (name, group, require_connect = True, preserve_history = False):
    """
        console decorator

        any function decorated will be exposed by the console

        name: str
            command name (used by the console)

        group: str
            group name for the console help

        require_connect: bool
            require client to be connected

        preserve_history: bool
            preserve readline history
    """

    def wrap (f):
        @wraps(f)
        def wrap2(*args, **kwargs):
            client = args[0]

            # check connection if needed
            if require_connect and not client.conn.is_connected():
                client.logger.error(format_text("\n'{0}' cannot be executed in offline mode\n".format(name), 'bold'))
                return

            time1 = time.time()

            try:
                rc = f(*args)

            except TRexConsoleNoAction:
                return RC_ERR("no action")

            except TRexConsoleError as e:
                # the argparser will handle the error
                return RC_ERR(e.brief())

            except TRexError as e:
                client.logger.debug('\nAction has failed with the following error:\n')
                client.logger.debug(e.get_tb())
                client.logger.error("\n%s - " % name + format_text(e.brief() + "\n", 'bold'))
                return RC_ERR(e.brief())

            # if got true - print time
            if rc:
                delta = time.time() - time1
                client.logger.error(format_time(delta) + "\n")

            return RC_OK()

        wrap2.api_type          = 'console'
        wrap2.name              = name
        wrap2.group             = group
        wrap2.preserve_history  = preserve_history

        return wrap2


    return wrap

