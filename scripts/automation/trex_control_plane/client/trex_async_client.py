#!/router/bin/python

try:
    # support import for Python 2
    import outer_packages
except ImportError:
    # support import for Python 3
    import client.outer_packages
from client_utils.jsonrpc_client import JsonRpcClient, BatchMessage

import json
import threading
import time
import datetime
import zmq
import re

from common.trex_stats import *
from common.trex_streams import *

# basic async stats class
class TrexAsyncStats(object):
    def __init__ (self):
        self.ref_point = None
        self.current = {}
        self.last_update_ts = datetime.datetime.now()

    def __format_num (self, size, suffix = ""):

        for unit in ['','K','M','G','T','P']:
            if abs(size) < 1000.0:
                return "%3.2f %s%s" % (size, unit, suffix)
            size /= 1000.0

        return "NaN"

    def update (self, snapshot):

        #update
        self.last_update_ts = datetime.datetime.now()

        self.current = snapshot

        if self.ref_point == None:
            self.ref_point = self.current
        

    def get (self, field, format = False, suffix = ""):

        if not field in self.current:
            return "N/A"

        if not format:
            return self.current[field]
        else:
            return self.__format_num(self.current[field], suffix)


    def get_rel (self, field, format = False, suffix = ""):
        if not field in self.current:
            return "N/A"

        if not format:
            return (self.current[field] - self.ref_point[field])
        else:
            return self.__format_num(self.current[field] - self.ref_point[field], suffix)


    # return true if new data has arrived in the past 2 seconds
    def is_online (self):
        delta_ms = (datetime.datetime.now() - self.last_update_ts).total_seconds() * 1000
        return (delta_ms < 2000)

# describes the general stats provided by TRex
class TrexAsyncStatsGeneral(TrexAsyncStats):
    def __init__ (self):
        super(TrexAsyncStatsGeneral, self).__init__()


# per port stats
class TrexAsyncStatsPort(TrexAsyncStats):
    def __init__ (self):
        super(TrexAsyncStatsPort, self).__init__()

    def get_stream_stats (self, stream_id):
        return None

# stats manager
class TrexAsyncStatsManager():
    def __init__ (self):

        self.general_stats = TrexAsyncStatsGeneral()
        self.port_stats = {}


    def get_general_stats (self):
        return self.general_stats

    def get_port_stats (self, port_id):

        if not str(port_id) in self.port_stats:
            return None

        return self.port_stats[str(port_id)]

    def update (self, snapshot):

        if snapshot['name'] == 'trex-global':
            self.__handle_snapshot(snapshot['data'])
        else:
            # for now ignore the rest
            return

    def __handle_snapshot (self, snapshot):

        general_stats = {}
        port_stats = {}

        # filter the values per port and general
        for key, value in snapshot.iteritems():
            
            # match a pattern of ports
            m = re.search('(.*)\-([0-8])', key)
            if m:

                port_id = m.group(2)
                field_name = m.group(1)

                if not port_id in port_stats:
                    port_stats[port_id] = {}

                port_stats[port_id][field_name] = value

            else:
                # no port match - general stats
                general_stats[key] = value

        # update the general object with the snapshot
        self.general_stats.update(general_stats)

        # update all ports
        for port_id, data in port_stats.iteritems():

            if not port_id in self.port_stats:
                self.port_stats[port_id] = TrexAsyncStatsPort()

            self.port_stats[port_id].update(data)





class CTRexAsyncClient():
    def __init__ (self, port):

        self.port = port

        self.raw_snapshot = {}

        self.stats = TrexAsyncStatsManager()


        self.tr = "tcp://localhost:{0}".format(self.port)
        print "\nConnecting To ZMQ Publisher At {0}".format(self.tr)

        self.active = True
        self.t = threading.Thread(target = self._run)

        # kill this thread on exit and don't add it to the join list
        self.t.setDaemon(True)
        self.t.start()


    def _run (self):

        #  Socket to talk to server
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)

        self.socket.connect(self.tr)
        self.socket.setsockopt(zmq.SUBSCRIBE, '')

        while self.active:
            line = self.socket.recv_string();
            msg = json.loads(line)

            key = msg['name']
            self.raw_snapshot[key] = msg['data']

            self.stats.update(msg)


    def get_stats (self):
        return self.stats

    def get_raw_snapshot (self):
        #return str(self.stats.global_stats.get('m_total_tx_bytes')) + " / " + str(self.stats.global_stats.get_rel('m_total_tx_bytes'))
        return self.raw_snapshot


    def stop (self):
        self.active = False
        self.t.join()

