#
# TRex stateless API 
# provides a layer for communicating with TRex
# using Python API
#

import sys
import os
import time
import cStringIO

api_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(api_path, '../automation/trex_control_plane/client/'))

from trex_stateless_client import CTRexStatelessClient, LoggerApi
from common import trex_stats

# a basic test object
class BasicTestAPI(object):

    # exception class
    class Failure(Exception):
        def __init__ (self, rc):
            self.msg = str(rc)

        def __str__ (self):
            s = "\n\n******\n"
            s += "Test has failed.\n\n"
            s += "Error reported:\n\n  {0}\n".format(self.msg)
            return s


    # logger for test
    class Logger(LoggerApi):
        def __init__ (self):
            LoggerApi.__init__(self)
            self.buffer = cStringIO.StringIO()

        def write (self, msg, newline = True):
            line = str(msg) + ("\n" if newline else "")
            self.buffer.write(line)

            #print line

        def flush (self):
            pass


    # main object
    def __init__ (self, server = "localhost", sync_port = 4501, async_port = 4500):
        self.logger = BasicTestAPI.Logger()
        self.client = CTRexStatelessClient(logger = self.logger,
                                           sync_port = sync_port,
                                           async_port = async_port,
                                           verbose_level = LoggerApi.VERBOSE_REGULAR)


        self.__invalid_stats = True

    # connect to the stateless client
    def connect (self):
        rc = self.client.connect(mode = "RWF")
        self.__verify_rc(rc)


    # disconnect from the stateless client
    def disconnect (self):
        self.client.disconnect()


    def inject_profile (self, filename, rate = "1", duration = None):
        self.__invalid_stats = True

        cmd = "--total -f {0} -m {1}".format(filename, rate)
        if duration:
            cmd += " -d {0}".format(duration)

        rc = self.client.cmd_start_line(cmd)
        self.__verify_rc(rc)


    def wait_while_traffic_on (self, timeout = None):
        while self.client.get_active_ports():
            time.sleep(0.1)


    def get_stats (self):
        if self.__invalid_stats:
            # send a barrier
            rc = self.client.block_on_stats()
            self.__verify_rc(rc)
            self.__invalid_stats = False

        total_stats = trex_stats.CPortStats(None)

        for port in self.client.ports.values():
            total_stats += port.port_stats

        return total_stats


    # some internal methods

    # verify RC return value
    def __verify_rc (self, rc):
        if not rc:
            raise self.Failure(rc)

