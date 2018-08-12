#!/router/bin/python

import json
import threading
import time
import datetime
import zmq
import re
import random
import os
import signal
import traceback
import sys

from .trex_types import RC_OK, RC_ERR
#from .trex_stats import *

from ..utils.text_opts import format_num
from ..utils.zipmsg import ZippedMsg


# basic async stats class
class CTRexAsyncStats(object):
    def __init__ (self):
        self.ref_point = None
        self.current = {}
        self.last_update_ts = datetime.datetime.now()

    def update (self, snapshot):

        #update
        self.last_update_ts = datetime.datetime.now()

        self.current = snapshot

        if self.ref_point == None:
            self.ref_point = self.current

    def clear(self):
        self.ref_point = self.current
        

    def get(self, field, format=False, suffix=""):

        if field not in self.current:
            return "N/A"

        if not format:
            return self.current[field]
        else:
            return format_num(self.current[field], suffix)

    def get_rel (self, field, format=False, suffix=""):
        if field not in self.current:
            return "N/A"

        if not format:
            return (self.current[field] - self.ref_point[field])
        else:
            return format_num(self.current[field] - self.ref_point[field], suffix)


    # return true if new data has arrived in the past 2 seconds
    def is_online (self):
        delta_ms = (datetime.datetime.now() - self.last_update_ts).total_seconds() * 1000
        return (delta_ms < 2000)

# describes the general stats provided by TRex
class CTRexAsyncStatsGeneral(CTRexAsyncStats):
    def __init__ (self):
        super(CTRexAsyncStatsGeneral, self).__init__()


# per port stats
class CTRexAsyncStatsPort(CTRexAsyncStats):
    def __init__ (self):
        super(CTRexAsyncStatsPort, self).__init__()

    def get_stream_stats (self, stream_id):
        return None

# stats manager
class CTRexAsyncStatsManager():
    def __init__ (self):

        self.general_stats = CTRexAsyncStatsGeneral()
        self.port_stats = {}


    def get_general_stats(self):
        return self.general_stats

    def get_port_stats (self, port_id):

        if str(port_id) not in self.port_stats:
            return None

        return self.port_stats[str(port_id)]

   
    def update(self, data):
        self.__handle_snapshot(data)

    def __handle_snapshot(self, snapshot):

        general_stats = {}
        port_stats = {}

        # filter the values per port and general
        for key, value in snapshot.items():
            
            # match a pattern of ports
            m = re.search('(.*)\-([0-8])', key)
            if m:

                port_id = m.group(2)
                field_name = m.group(1)

                if port_id not in port_stats:
                    port_stats[port_id] = {}

                port_stats[port_id][field_name] = value

            else:
                # no port match - general stats
                general_stats[key] = value

        # update the general object with the snapshot
        self.general_stats.update(general_stats)

        # update all ports
        for port_id, data in port_stats.items():

            if port_id not in self.port_stats:
                self.port_stats[port_id] = CTRexAsyncStatsPort()

            self.port_stats[port_id].update(data)



class ServerEventsIDs(object):
    """
        server event IDs
        (in sync with the server IDs)
    """
    EVENT_PORT_STARTED   = 0
    EVENT_PORT_STOPPED   = 1
    EVENT_PORT_PAUSED    = 2
    EVENT_PORT_RESUMED   = 3
    EVENT_PORT_JOB_DONE  = 4
    EVENT_PORT_ACQUIRED  = 5
    EVENT_PORT_RELEASED  = 6
    EVENT_PORT_ERROR     = 7
    EVENT_PORT_ATTR_CHG  = 8
    
    EVENT_SERVER_STOPPED = 100




class TRexSubscriber():
    THREAD_STATE_ACTIVE = 1
    THREAD_STATE_ZOMBIE = 2
    THREAD_STATE_DEAD   = 3
    
    def __init__ (self, ctx, rpc):

        self.ctx = ctx

        self.port   = ctx.async_port
        self.server = ctx.server

        self.rpc = rpc

        self.event_handler = ctx.event_handler

        self.raw_snapshot = {}

        self.stats = CTRexAsyncStatsManager()

        self.last_data_recv_ts = 0
        self.async_barrier     = None

        self.monitor = AsyncUtil()

        self.connected = False

        self.zipped = ZippedMsg()
        
        self.t_state = self.THREAD_STATE_DEAD
        
        
    # connects the async channel
    def connect (self):

        if self.connected:
            self.disconnect()

        self.tr = "tcp://{0}:{1}".format(self.server, self.port)

        #  Socket to talk to server
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)


        # before running the thread - mark as active    
        self.t_state = self.THREAD_STATE_ACTIVE
        self.t = threading.Thread(target = self._run_safe)

        # kill this thread on exit and don't add it to the join list
        self.t.setDaemon(True)
        self.t.start()

        self.connected = True

        # first barrier - make sure async thread is up
        rc = self.barrier()
        if not rc:
            self.disconnect()
            return rc

  
        return RC_OK()

    
    # disconnect
    def disconnect (self):
        if not self.connected:
            return

        # mark for join
        self.t_state = self.THREAD_STATE_DEAD
        self.context.term()
        self.t.join()


        # done
        self.connected = False


    # set the thread as a zombie (in case of server death)
    def set_as_zombie (self):
        self.last_data_recv_ts = None
        self.t_state = self.THREAD_STATE_ZOMBIE
        
        
    # return the timeout in seconds for the ZMQ subscriber thread
    def get_timeout_sec (self):
        return 3
        
        
    def _run_safe (self):
        
        # socket must be created on the same thread 
        self.socket.setsockopt(zmq.SUBSCRIBE, b'')
        self.socket.setsockopt(zmq.RCVTIMEO, self.get_timeout_sec() * 1000)
        self.socket.connect(self.tr)
        
        try:
            self._run()
        except Exception as e:
            self.ctx.event_handler.on_event("subscriber crashed", e)
            
        finally:
            # closing of socket must be from the same thread
            self.socket.close(linger = 0)
        
            
    # thread function
    def _run (self):

        got_data = False

        self.monitor.reset()

        while self.t_state != self.THREAD_STATE_DEAD:
            try:
                with self.monitor:
                    line = self.socket.recv()
                
                # last data recv.
                self.last_data_recv_ts = time.time()
                
                # if thread was marked as zombie - it does nothing besides fetching messages
                if self.t_state == self.THREAD_STATE_ZOMBIE:
                    continue

                self.monitor.on_recv_msg(line)

                # try to decomrpess
                unzipped = self.zipped.decompress(line)
                if unzipped:
                    line = unzipped

                line = line.decode()
                
                # signal once
                if not got_data:
                    self.ctx.event_handler.on_event("subscriber resumed")
                    got_data = True
                

            # got a timeout - mark as not alive and retry
            except zmq.Again:
                # signal once
                if got_data:
                    self.ctx.event_handler.on_event("subscriber timeout", self.get_timeout_sec())
                    got_data = False

                continue

            except zmq.ContextTerminated:
                # outside thread signaled us to exit
                assert(self.t_state != self.THREAD_STATE_ACTIVE)
                break

            msg = json.loads(line)

            name     = msg['name']
            data     = msg['data']
            msg_type = msg['type']
            baseline = msg.get('baseline', False)

            self.raw_snapshot[name] = data

            self.__dispatch(name, msg_type, data, baseline)



                                  
        
    def get_stats (self):
        return self.stats

    def get_raw_snapshot (self):
        return self.raw_snapshot

    # dispatch the message to the right place
    def __dispatch (self, name, type, data, baseline):

        # stats
        if name == "trex-global":
            self.handle_global_stats_update(data, baseline)
            
        elif name == "flow_stats":
            self.handle_flow_stats_update(data, baseline)


        elif name == "latency_stats":
            self.handle_latency_stats_update(data, baseline)

        # events
        elif name == "trex-event":
            self.handle_event(type, data)

        # barriers
        elif name == "trex-barrier":
            self.handle_async_barrier(type, data)


        else:
            pass



    def handle_global_stats_update (self, data, baseline):
        self.ctx.event_handler.on_event("global stats update", data, baseline)


    def handle_flow_stats_update (self, data, baseline):
        self.ctx.event_handler.on_event("flow stats update", data, baseline)


    def handle_flow_stats_update (self, data, baseline):
        self.ctx.event_handler.on_event("latency stats update", data, baseline)


    def handle_event (self, event_id, data):


        if (event_id == ServerEventsIDs.EVENT_PORT_STARTED):
            port_id = int(data['port_id'])
            self.ctx.event_handler.on_event("port started", port_id)


        # port stopped
        elif (event_id == ServerEventsIDs.EVENT_PORT_STOPPED):
            port_id = int(data['port_id'])
            self.ctx.event_handler.on_event("port stopped", port_id)


        # port paused
        elif (event_id == ServerEventsIDs.EVENT_PORT_PAUSED):
            port_id = int(data['port_id'])
            self.ctx.event_handler.on_event("port paused", port_id)


        # port resumed
        elif (event_id == ServerEventsIDs.EVENT_PORT_RESUMED):
            port_id = int(data['port_id'])
            self.ctx.event_handler.on_event("port resumed", port_id)


        # port finished traffic
        elif (event_id == ServerEventsIDs.EVENT_PORT_JOB_DONE):
            port_id = int(data['port_id'])
            self.ctx.event_handler.on_event("port job done", port_id)



        # port was acquired - maybe stolen...
        elif (event_id == ServerEventsIDs.EVENT_PORT_ACQUIRED):
            session_id = data['session_id']

            port_id = int(data['port_id'])
            who     = data['who']
            force   = data['force']

            self.ctx.event_handler.on_event("port acquired", port_id, who, session_id, force)



        # port was released
        elif (event_id == ServerEventsIDs.EVENT_PORT_RELEASED):
            port_id     = int(data['port_id'])
            who         = data['who']
            session_id  = data['session_id']

            self.ctx.event_handler.on_event("port released", port_id, who, session_id)


        # port error
        elif (event_id == ServerEventsIDs.EVENT_PORT_ERROR):
            port_id = int(data['port_id'])

            self.ctx.event_handler.on_event("port error", port_id)


        # port attr changed
        elif (event_id == ServerEventsIDs.EVENT_PORT_ATTR_CHG):

            port_id = int(data['port_id'])
            attr    = data['attr']

            self.ctx.event_handler.on_event("port attr chg", port_id, attr)


        # server stopped
        elif (event_id == ServerEventsIDs.EVENT_SERVER_STOPPED):
            cause = data['cause']

            self.ctx.event_handler.on_event("server stopped", cause)




    # async barrier handling routine
    def handle_async_barrier (self, type, data):
        if self.async_barrier['key'] == type:
            self.async_barrier['ack'] = True


    # block on barrier for async channel
    def barrier(self, timeout = 5, baseline = False):

        # set a random key
        key = random.getrandbits(32)
        self.async_barrier = {'key': key, 'ack': False}

        # expr time
        expr = time.time() + timeout

        while not self.async_barrier['ack']:

            # inject
            rc = self.rpc.transmit("publish_now", params = {'key' : key, 'baseline': baseline})
            if not rc:
                return rc

            # fast loop
            for i in range(0, 100):
                if self.async_barrier['ack']:
                    break
                time.sleep(0.001)

            if time.time() > expr:
                return RC_ERR("*** [subscriber] - timeout - no data flow from server at : " + self.tr)

        return RC_OK()


# a class to measure util. of async subscriber thread
class AsyncUtil(object):

    STATE_SLEEP = 1
    STATE_AWAKE = 2

    def __init__ (self):
        self.reset()


    def reset (self):
        self.state = self.STATE_AWAKE
        self.clock = time.time()

        # reset the current interval
        self.interval = {'ts': time.time(), 'total_sleep': 0, 'total_bits': 0}

        # global counters
        self.cpu_util = 0
        self.bps      = 0


    def on_recv_msg (self, message):
        self.interval['total_bits'] += len(message) * 8.0

        self._tick()


    def __enter__ (self):
        assert(self.state == self.STATE_AWAKE)
        self.state = self.STATE_SLEEP

        self.sleep_start_ts = time.time()
        

    def __exit__(self, exc_type, exc_val, exc_tb):
        assert(self.state == self.STATE_SLEEP)
        self.state = self.STATE_AWAKE

        # measure total sleep time for interval
        self.interval['total_sleep'] += time.time() - self.sleep_start_ts

        self._tick()
                
    def _tick (self):
        # how much time did the current interval lasted
        ts = time.time() - self.interval['ts']
        if ts < 1:
            return

        # if tick is in the middle of sleep - add the interval and reset
        if self.state == self.STATE_SLEEP:
            self.interval['total_sleep'] += time.time() - self.sleep_start_ts
            self.sleep_start_ts = time.time()

        # add the interval
        if self.interval['total_sleep'] > 0:
            # calculate
            self.cpu_util = self.cpu_util * 0.75 + (float(ts - self.interval['total_sleep']) / ts) * 0.25
            self.interval['total_sleep'] = 0


        if self.interval['total_bits'] > 0:
            # calculate
            self.bps = self.bps * 0.75 + ( self.interval['total_bits'] / ts ) * 0.25
            self.interval['total_bits'] = 0

        # reset the interval's clock
        self.interval['ts'] = time.time()


    def get_cpu_util (self):
        self._tick()
        return (self.cpu_util * 100)

    def get_bps (self):
        self._tick()
        return (self.bps)

