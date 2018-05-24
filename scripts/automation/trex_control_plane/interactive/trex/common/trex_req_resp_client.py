#!/router/bin/python

import zmq
import json
import re
from collections import namedtuple
import zlib
import struct
import pprint
import time
from threading import Lock

from .trex_types import RC, RC_OK, RC_ERR
from .trex_logger import Logger

from ..utils.common import random_id_gen
from ..utils.zipmsg import ZippedMsg
from ..utils.text_opts import bcolors


# sub class to describe a batch
class BatchMessage(object):
    def __init__ (self, rpc_client):
        self.rpc_client = rpc_client
        self.batch_list = []

    def add (self, method_name, params = None, api_h = None):

        id, msg = self.rpc_client.create_jsonrpc_v2(method_name, params, api_h, encode = False)
        self.batch_list.append(msg)

    def invoke(self, block = False, chunk_size = 500000, retry = 0):
        if not self.rpc_client.connected:
            return RC_ERR("Not connected to server")

        if chunk_size:
            response_batch = RC()
            size = 0
            new_batch = []
            for msg in self.batch_list:
                size += len(json.dumps(msg))
                new_batch.append(msg)
                if size > chunk_size:
                    batch_json = json.dumps(new_batch)
                    response_batch.add(self.rpc_client.send_msg(batch_json))
                    size = 0
                    new_batch = []
            if new_batch:
                batch_json = json.dumps(new_batch)
                response_batch.add(self.rpc_client.send_msg(batch_json))
            return response_batch
        else:
            batch_json = json.dumps(self.batch_list)
            return self.rpc_client.send_msg(batch_json, retry = retry)

# values from JSON-RPC RFC
class ErrNo:
    MethodNotSupported = -32601
    # custom err
    JSONRPC_V2_ERR_TRY_AGAIN          = -32001
    JSONRPC_V2_ERR_WIP                = -32002
    JSONRPC_V2_ERR_NO_RESULTS         = -32003


# JSON RPC v2.0 client
class JsonRpcClient(object):

    def __init__ (self, ctx):

        self.ctx = ctx

        self.connected = False

        # default values
        self.port   = self.ctx.sync_port
        self.server = self.ctx.server

        self.id_gen = random_id_gen()
        self.zipper = ZippedMsg()

        self.lock = Lock()

        # API handler provided by the server
        self.api_h = None
        
            
    def get_connection_details (self):
        rc = {}
        rc['server'] = self.server
        rc['port']   = self.port

        return rc


    def get_server (self):
        return self.server


    def get_port (self):
        return self.port


    def set_api_h(self, api_h):
        self.api_h = api_h


    # pretty print for JSON
    def pretty_json (self, json_str, use_colors = True):
        pretty_str = json.dumps(json.loads(json_str), indent = 4, separators=(',', ': '), sort_keys = True)

        if not use_colors:
            return pretty_str

        try:
            # int numbers
            pretty_str = re.sub(r'([ ]*:[ ]+)(\-?[1-9][0-9]*[^.])',r'\1{0}\2{1}'.format(bcolors.BLUE, bcolors.ENDC), pretty_str)
            # float
            pretty_str = re.sub(r'([ ]*:[ ]+)(\-?[1-9][0-9]*\.[0-9]+)',r'\1{0}\2{1}'.format(bcolors.MAGENTA, bcolors.ENDC), pretty_str)
            # strings

            pretty_str = re.sub(r'([ ]*:[ ]+)("[^"]*")',r'\1{0}\2{1}'.format(bcolors.RED, bcolors.ENDC), pretty_str)
            pretty_str = re.sub(r"('[^']*')", r'{0}\1{1}'.format(bcolors.MAGENTA, bcolors.RED), pretty_str)
        except :
            pass

        return pretty_str

    def verbose_msg (self, msg):
        self.ctx.logger.debug("\n\n[verbose] " + msg)


    # batch messages
    def create_batch (self):
        return BatchMessage(self)

    def create_jsonrpc_v2 (self, method_name, params = None, api_h = None, encode = True):
        msg = {}
        msg["jsonrpc"] = "2.0"
        msg["method"]  = method_name
        msg["id"] = next(self.id_gen)

        msg["params"] = params if params is not None else {}

        # if api_h is provided, add it
        if api_h:
            msg["params"]["api_h"] = api_h
        

        if encode:
            return id, json.dumps(msg)
        else:
            return id, msg


    def invoke_rpc_method (self, method_name, params = None, retry = 0):
        if not self.connected:
            return RC_ERR("Not connected to server")

        id, msg = self.create_jsonrpc_v2(method_name, params, self.api_h)

        return self.send_msg(msg, retry = retry)


    def handle_async_transmit (self, method_name, params, retry, rc):
        sleep_sec    = 0.3
        timeout_sec  = 3
        poll_tries   = int(timeout_sec / sleep_sec)

        while not rc and rc.errno() == ErrNo.JSONRPC_V2_ERR_TRY_AGAIN:
            if poll_tries == 0:
                return RC_ERR('Server was busy within %s sec, try again later' % timeout_sec)
            poll_tries -= 1
            time.sleep(sleep_sec)
            rc = self.invoke_rpc_method(method_name, params, self.api_h, retry = retry)
        while not rc and rc.errno() == ErrNo.JSONRPC_V2_ERR_WIP:
            try:
                params = {'ticket_id': int(rc.err())}
                if poll_tries == 0:
                    self.invoke_rpc_method('cancel_async_task', params)
                    return RC_ERR('Timeout on processing async command, server did not finish within %s second' % timeout_sec)
                poll_tries -= 1
                time.sleep(sleep_sec)
                rc = self.invoke_rpc_method('get_async_results', params, retry = retry)
            except KeyboardInterrupt:
                self.invoke_rpc_method('cancel_async_task', params)
                raise
        return rc


    def transmit(self, method_name, params = None, retry = 0):

        rc = self.invoke_rpc_method(method_name, params, retry)

        # handle async/work in progress
        if not rc and rc.errno() in (ErrNo.JSONRPC_V2_ERR_TRY_AGAIN, ErrNo.JSONRPC_V2_ERR_WIP):
            return self.handle_async_transmit(method_name, params, retry, rc)

        return rc


    # transmit a batch list
    def transmit_batch(self, batch_list, retry = 0):

        batch = self.create_batch()

        for command in batch_list:
            batch.add(command.method, command.params, self.api_h)
            
        # invoke the batch
        return batch.invoke(retry = retry)


   
    def send_msg (self, msg, retry = 0):
        # REQ/RESP pattern in ZMQ requires no interrupts during the send
        with self.lock:
            return self.__send_msg(msg, retry)
        
        
    def __send_msg (self, msg, retry = 0):

        # print before
        self.verbose_msg("Sending Request To Server:\n\n" + self.pretty_json(msg) + "\n")

        # encode string to buffer
        buffer = msg.encode()

        if self.zipper.check_threshold(buffer):
            response = self.send_raw_msg(self.zipper.compress(buffer), retry = retry)
        else:
            response = self.send_raw_msg(buffer, retry = retry)

        if not response:
            return response
        elif self.zipper.is_compressed(response):
            response = self.zipper.decompress(response)

        # bytes -> string -> load as JSON
        try:
            response = response.decode()
            response_json = json.loads(response)
        except (UnicodeDecodeError, TypeError, ValueError):
            pprint.pprint(response)
            return RC_ERR('*** [RPC] - Failed to decode response from server')
    
        # print after
        self.verbose_msg("Server Response:\n\n" + self.pretty_json(response) + "\n")
    
        # process response (batch and regular)
        if isinstance(response_json, list):
            return self.process_batch_response(response_json)
        else:
            return self.process_single_response(response_json)


    # low level send of string message
    def send_raw_msg (self, msg, retry = 0):
        try:
            return self._send_raw_msg_safe(msg, retry)
        except KeyboardInterrupt as e:
            # must restore the socket to a sane state
            self.reconnect()
            raise e
            
            
    def _send_raw_msg_safe (self, msg, retry):

        retry_left = retry
        while True:
            try:
                self.socket.send(msg)
                break
            except zmq.Again:
                retry_left -= 1
                if retry_left < 0:
                    self.disconnect()
                    return RC_ERR("*** [RPC] - Failed to send message to server")


        retry_left = retry
        while True:
            try:
                response = self.socket.recv()
                break
            except zmq.Again:
                retry_left -= 1
                if retry_left < 0:
                    self.disconnect()
                    return RC_ERR("*** [RPC] - Failed to get server response from {0}".format(self.transport))

        return response
       
     

    # processs a single response from server
    def process_single_response (self, response_json):

        if (response_json.get("jsonrpc") != "2.0"):
            return RC_ERR("Malformed Response ({0})".format(str(response_json)))

        # error reported by server
        if ("error" in response_json):
            if "specific_err" in response_json["error"]:
                return RC_ERR(response_json["error"]["specific_err"], errno = response_json['error']['code'])
            else:
                return RC_ERR(response_json["error"]["message"], errno = response_json['error']['code'])

        
        # if no error there should be a result
        if ("result" not in response_json):
            return RC_ERR("Malformed Response ({0})".format(str(response_json)))

        return RC_OK(response_json["result"])

  

    # process a batch response
    def process_batch_response (self, response_json):
        rc_batch = RC()

        for single_response in response_json:
            rc = self.process_single_response(single_response)
            rc_batch.add(rc)

        return rc_batch


    def disconnect (self):

        # invalidate the server API handler
        self.api_h = None

        if self.connected:
            self.socket.close(linger = 0)
            self.context.destroy(linger = 0)
            self.connected = False
            return RC_OK()
        else:
            return RC_ERR("Not connected to server")


    def connect(self, server = None, port = None):
        if self.connected:
            self.disconnect()

        self.context = zmq.Context()

        self.server = (server if server else self.server)
        self.port = (port if port else self.port)

        #  Socket to talk to server
        self.transport = "tcp://{0}:{1}".format(self.server, self.port)

        self.socket = self.context.socket(zmq.REQ)
        try:
            self.socket.connect(self.transport)
        except zmq.error.ZMQError as e:
            return RC_ERR("ZMQ Error: Bad server or port name: " + str(e))

        self.socket.setsockopt(zmq.SNDTIMEO, 10000)
        self.socket.setsockopt(zmq.RCVTIMEO, 10000)

        self.connected = True

        return RC_OK()


    def reconnect(self):
        # connect using current values
        return self.connect()


    def is_connected(self):
        return self.connected

    def __del__(self):
        self.ctx.logger.info("Shutting down RPC client\n")
        if hasattr(self, "context"):
            self.context.destroy(linger=0)

