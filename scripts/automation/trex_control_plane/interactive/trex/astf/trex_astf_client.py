
from ..utils.common import get_current_user
from ..utils import parsing_opts, text_tables

from ..common.trex_logger import Logger
from ..common.trex_exceptions import TRexError
from ..common.trex_client import TRexClient
from ..common.trex_types import RC_OK, RC_ERR, RC
from ..common.trex_api_annotators import client_api, console_api

from .trex_astf_port import ASTFPort

class ASTFClient(TRexClient):
    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = "error",
                 logger = None):

        """ 
        TRex advance stateful client

        :parameters:
             username : string 
                the user name, for example imarom

              server  : string 
                the server name or ip 

              sync_port : int 
                the RPC port 

              async_port : int 
                the ASYNC port (subscriber port)

              verbose_level: str
                one of "none", "critical", "error", "info", "debug"

              logger: instance of AbstractLogger
                if None, will use ScreenLogger
        """

        api_ver = {'name': 'ASTF', 'major': 1, 'minor': 0}

        TRexClient.__init__(self,
                            api_ver,
                            username,
                            server,
                            sync_port,
                            async_port,
                            verbose_level,
                            logger)



############################    called       #############################
############################    by base      #############################
############################    TRex Client  #############################   

    def _on_connect (self, system_info):
        """
            called when connecting to the server
            triggered by the common client object
        """

        # create ports
        self.ports.clear()
        for port_id in range(system_info["port_count"]):
            info = system_info['ports'][port_id]
            self.ports[port_id] = ASTFPort(self.ctx, port_id, self.conn.rpc, info)


        return RC_OK()



############################       ASTF     #############################
############################       API      #############################
############################                #############################

    @client_api('command', True)
    def reset(self, ports = None, restart = False):
        """
            Force acquire ports, stop the traffic, remove all streams and clear stats
    
            :parameters:
                ports : list
                   Ports on which to execute the command
    
                restart: bool
                   Restart the NICs (link down / up)
    
            :raises:
                + :exc:`TRexError`
    
        """
        ports = ports if ports is not None else self.get_all_ports()
    
        self.psv.validate('reset', ports)
    
        if restart:
            self.ctx.logger.pre_cmd("Hard resetting ports {0}:".format(ports))
        else:
            self.ctx.logger.pre_cmd("Resetting ports {0}:".format(ports))
    
    
        try:
            with self.ctx.logger.suppress():
            # force take the port and ignore any streams on it
                self.acquire(ports, force = True)
                self.stop(ports)
                self.clear_stats(ports)
                self.set_port_attr(ports,
                                   promiscuous = False,
                                   link_up = True if restart else None)
                self.remove_rx_queue(ports)
    
            self.ctx.logger.post_cmd(RC_OK())
    
    
        except TRexError as e:
            self.ctx.logger.post_cmd(False)
            raise e



    @client_api('command', True)
    def acquire (self, ports = None, force = False):
        """
            Acquires ports for executing commands

            :parameters:
                ports : list
                    Ports on which to execute the command

                force : bool
                    Force acquire the ports.

            :raises:
                + :exc:`STLError`

        """
        # call the base class acqurie
        self._acquire_common(ports, force)


    @client_api('command', True)
    def stop (self, ports = None, rx_delay_ms = None):
        pass


    # get stats
    @client_api('getter', True)
    def get_stats (self, ports = None, sync_now = True):

        # TBD: add ASTF specific stats here to ext_stats
        ext_stats = {}

        return self._get_stats_common(ports, sync_now, ext_stats = ext_stats)


     # clear stats
    @client_api('getter', True)
    def clear_stats (self,
                     ports = None,
                     clear_global = True,
                     clear_xstats = True):

        # TODO: create STL specific stats mapping
        ext_stats = []

        return self._clear_stats_common(ports, clear_global, clear_xstats)


############################   console   #############################
############################   commands  #############################
############################             #############################

    @console_api('stats', 'common', True)
    def show_stats_line (self, line):
        '''Show various statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.show_stats_line.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.ASTF_STATS)

        opts = parser.parse_args(line.split())
        if not opts:
            return

        # without paramters show only global and ports
        if not opts.stats:
            self._show_global_stats()
            self._show_port_stats(opts.ports)
            return

        # decode which stats to show
        if opts.stats == 'global':
            self._show_global_stats()

        elif opts.stats == 'ports':
            self._show_port_stats(opts.ports)

        elif opts.stats == 'xstats':
            self._show_port_xstats(opts.ports, False)

        elif opts.stats == 'xstats_inc_zero':
            self._show_port_xstats(opts.ports, True)

        elif opts.stats == 'status':
            self._show_port_status(opts.ports)

        elif opts.stats == 'cpu':
            self._show_cpu_util()

        elif opts.stats == 'mbuf':
            self._show_mbuf_util()



