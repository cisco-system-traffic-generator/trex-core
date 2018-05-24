import random
from .trex_logger import Logger
from .trex_events import EventsHandler


class TRexCtx(object):
    """
        Holds TRex context

        a slim object containing common objects
        for every object in the session
    """

    def __init__ (self, api_ver, username, server, sync_port, async_port, logger):

        self.api_ver        = api_ver
        self.username       = username
        self.session_id     = random.getrandbits(32)
        self.event_handler  = EventsHandler()
        self.server         = server
        self.sync_port      = sync_port
        self.async_port     = async_port
        self.logger         = logger
        self.server_version = None
        self.system_info    = None
