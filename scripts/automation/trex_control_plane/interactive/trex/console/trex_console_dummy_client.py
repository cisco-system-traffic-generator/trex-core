import inspect
from ..utils.common import get_current_user
from ..common.trex_api_annotators import client_api
from ..common.trex_client import TRexClient
from ..common.trex_ctx import TRexCtx
from ..common.trex_logger import ScreenLogger


class DummyConnection():
    """
        A dummy connection for compatability only.
    """
    DISCONNECTED          = 1
    CONNECTED             = 2
    MARK_FOR_DISCONNECT   = 3

    def __init__(self):
        self.state = (self.DISCONNECTED, None)

    def connect(self):
        """
            Connect
        """
        self.state = (self.CONNECTED, None)

    def disconnect(self):
        """
            Disconnect
        """
        self.state = (self.DISCONNECTED, None)

    def is_connected(self):
        """
            Is Connected?

            :returns:
                bool: Is connected?
        """
        return self.state[0] == self.CONNECTED

    is_any_connected = is_connected

    def is_marked_for_disconnect (self):
        """
            Is marked for disconnect?

            :returns:
                bool: Is marked for disconnect?
        """
        return self.state[0] == self.MARK_FOR_DISCONNECT

    def get_disconnection_cause(self):
        """
            Get disconnection cause.

            :returns:
                string: Disconnection cause.
        """
        return self.state[1]

    def mark_for_disconnect(self, cause):
        """
            A multithread safe call
            any thread can mark the current connection
            as not valid
            and will require the main thread to reconnect
        """
        self.state = (self.MARKED_FOR_DISCONNECT, cause)

    # as long as it is connected, it is alive.
    is_alive = is_connected

    def sigint_on_conn_lost_enable(self):
        """
            when enabled, if connection
            is lost a SIGINT will be sent
            to the main thread.
            Declared for compatibility only.
        """
        pass

    def sigint_on_conn_lost_disable(self):
        """
            disable SIGINT dispatching
            on case of connection lost.
            Declared for compatibility on.y
        """
        pass


class ConsoleDummyClient(TRexClient):
    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 verbose_level = "error",
                 logger = None,
                ):
        """
        TRex Dummy Client for Console purposes only.
        We use this client to be able to load the console without having to start
        a TRex instance. The capabilities of this client are very limited, by design.

        :parameters:
             username : string
                the user name, for example bdollma

              server : string
                the server name or ip

              verbose_level: str
                one of "none", "critical", "error", "info", "debug"

              logger: instance of AbstractLogger
                if None, will use ScreenLogger
        """

        api_ver = {'name': 'Dummy', 'major': 0, 'minor': 1}

        # logger
        logger = logger if logger is not None else ScreenLogger()
        logger.set_verbose(verbose_level)

        # first create a TRex context
        self.ctx = TRexCtx(api_ver,
                           username,
                           server,
                           None,
                           None,
                           logger,
                           None,
                           None)

        self.conn = DummyConnection()

        self.ctx.server_version = self.probe_server()
        self.ctx.system_info = self.get_system_info()

        self.supported_cmds = [] # Server supported cmds, no server -> no supported commands.
        self.ports = {} # No ports.

    def get_mode(self):
        """
            Returns running mode of TRex.
        """
        return "Dummy"

    def _register_events(self):
        # overload register event so that classic port events aren't registered.
        pass

    def get_system_info(self):
        """
            Get System Info returns some system information for the Console to show upon
            introduction.
        """
        return {
            'hostname': 'N/A',
            'uptime': 'N/A',
            'dp_core_count': 'N/A',
            'dp_core_count_per_port': 'N/A',
            'core_type': 'N/A',
            'is_multiqueue_mode': False,
            'advanced_per_stream_stats': False,
            'port_count': 'N/A',
            'ports': []
        }

    def get_console_methods(self):
        """
            Get Console Methods decides which methods are shown in the console help section.
            The parent function decides that each function that has @console_api decorator
            is shown.
            Here we override that, since all those functions are not relevant in this mode.
        """
        def predicate (x):
            return False

        return {cmd[1].name : cmd[1] for cmd in inspect.getmembers(self, predicate = predicate)}

    #######################################################
    #            Overriding TRexClient Getters            #
    #######################################################
    @client_api('getter', False)
    def probe_server(self):
        """
        Probe the server for the version / mode

        Can be used to determine mode.

        :parameters:
          None

        :return:
          dictionary describing server version and configuration

        :raises:
          None

        """
        return {'version': "v{}.{}".format(self.ctx.api_ver["major"], self.ctx.api_ver["minor"]),
                'mode': 'Dummy',
                "build_date": 'N/A',
                "build_time": 'N/A',
                "build_by": self.ctx.username}

    #######################################################
    #          Overriding TRexClient Commands             #
    #######################################################
    @client_api('command', False)
    def connect(self):
        """

            Connects to the TRex server

            :parameters:
                None

        """
        # Nothing to do here.
        self.conn.connect()

    @client_api('command', False)
    def disconnect (self, stop_traffic = True, release_ports = True):
        """
            Disconnects from the server

            :parameters:
                stop_traffic : bool
                    Attempts to stop traffic before disconnecting.
                release_ports : bool
                    Attempts to release all the acquired ports.

        """
        # no ports, nothing to do here.
        self.conn.disconnect()

    @client_api('command', True)
    def acquire(self, ports = None, force = False, sync_streams = True):
        """
            Acquires ports for executing commands

            :parameters:
                ports : list
                    Ports on which to execute the command

                force : bool
                    Force acquire the ports.

                sync_streams: bool
                    sync with the server about the configured streams

        """
        # All ports are acquired by default.
        pass
