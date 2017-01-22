#!/router/bin/python

import zmq
import json
import re
from collections import namedtuple
import zlib
import struct

from .trex_stl_types import *
from .utils.common import random_id_gen
from .utils.zipmsg import ZippedMsg

class bcolors:
    BLUE = '\033[94m'
    GREEN = '\033[32m'
    YELLOW = '\033[93m'
    RED = '\033[31m'
    MAGENTA = '\033[35m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

# sub class to describe a batch
class BatchMessage(object):
    def __init__ (self, rpc_client):
        self.rpc_client = rpc_client
        self.batch_list = []

    def add (self, method_name, params = None, api_class = 'core'):

        id, msg = self.rpc_client.create_jsonrpc_v2(method_name, params, api_class, encode = False)
        self.batch_list.append(msg)

    def invoke(self, block = False, chunk_size = 500000):
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
            return self.rpc_client.send_msg(batch_json)


# JSON RPC v2.0 client
class JsonRpcClient(object):

    def __init__ (self, default_server, default_port, client):
        self.client_api = client.api_h
        self.logger = client.logger
        self.connected = False

        # default values
        self.port   = default_port
        self.server = default_server

        self.id_gen = random_id_gen()
        self.zipper = ZippedMsg()

    def get_connection_details (self):
        rc = {}
        rc['server'] = self.server
        rc['port']   = self.port

        return rc

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
        self.logger.log("\n\n[verbose] " + msg, level = self.logger.VERBOSE_HIGH)


    # batch messages
    def create_batch (self):
        return BatchMessage(self)

    def create_jsonrpc_v2 (self, method_name, params = None, api_class = 'core', encode = True):
        msg = {}
        msg["jsonrpc"] = "2.0"
        msg["method"]  = method_name
        msg["id"] = next(self.id_gen)

        msg["params"] = params if params is not None else {}

        # if this RPC has an API class - add it's handler
        if api_class:
            msg["params"]["api_h"] = self.client_api[api_class]
        

        if encode:
            return id, json.dumps(msg)
        else:
            return id, msg


    def invoke_rpc_method (self, method_name, params = None, api_class = 'core'):
        if not self.connected:
            return RC_ERR("Not connected to server")

        id, msg = self.create_jsonrpc_v2(method_name, params, api_class)

        return self.send_msg(msg)

   
    def send_msg (self, msg):
        # print before
        if self.logger.check_verbose(self.logger.VERBOSE_HIGH):
            self.verbose_msg("Sending Request To Server:\n\n" + self.pretty_json(msg) + "\n")

        # encode string to buffer
        buffer = msg.encode()

        if self.zipper.check_threshold(buffer):
            response = self.send_raw_msg(self.zipper.compress(buffer))
        else:
            response = self.send_raw_msg(buffer)

        if not response:
            return response
        elif self.zipper.is_compressed(response):
            response = self.zipper.decompress(response)

        # return to string
        response = response.decode()

        # print after
        if self.logger.check_verbose(self.logger.VERBOSE_HIGH):
            self.verbose_msg("Server Response:\n\n" + self.pretty_json(response) + "\n")

        # process response (batch and regular)
        try:       
            response_json = json.loads(response)
        except (TypeError, ValueError):
            return RC_ERR("*** [RPC] - Failed to decode response from server")

        if isinstance(response_json, list):
            return self.process_batch_response(response_json)
        else:
            return self.process_single_response(response_json)



    # low level send of string message
    def send_raw_msg (self, msg):

        tries = 0
        while True:
            try:
                self.socket.send(msg)
                break
            except zmq.Again:
                tries += 1
                if tries > 0:
                    self.disconnect()
                    return RC_ERR("*** [RPC] - Failed to send message to server")

            except KeyboardInterrupt as e:
                # must restore the socket to a sane state
                self.reconnect()
                raise e

        tries = 0
        while True:
            try:
                response = self.socket.recv()
                break
            except zmq.Again:
                tries += 1
                if tries > 0:
                    self.disconnect()
                    return RC_ERR("*** [RPC] - Failed to get server response from {0}".format(self.transport))

            except KeyboardInterrupt as e:
                # must restore the socket to a sane state
                self.reconnect()
                raise e

        return response
       
     

    # processs a single response from server
    def process_single_response (self, response_json):

        if (response_json.get("jsonrpc") != "2.0"):
            return RC_ERR("Malformed Response ({0})".format(str(response_json)))

        # error reported by server
        if ("error" in response_json):
            if "specific_err" in response_json["error"]:
                return RC_ERR(response_json["error"]["specific_err"])
            else:
                return RC_ERR(response_json["error"]["message"])

        
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
        self.logger.log("Shutting down RPC client\n")
        if hasattr(self, "context"):
            self.context.destroy(linger=0)

