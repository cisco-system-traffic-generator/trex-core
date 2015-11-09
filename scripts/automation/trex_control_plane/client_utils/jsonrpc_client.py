#!/router/bin/python

import external_packages
import zmq
import json
import general_utils
import re
from time import sleep
from collections import namedtuple

CmdResponse = namedtuple('CmdResponse', ['success', 'data'])

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

    def add (self, method_name, params={}):

        id, msg = self.rpc_client.create_jsonrpc_v2(method_name, params, encode = False)
        self.batch_list.append(msg)

    def invoke(self, block = False):
        if not self.rpc_client.connected:
            return False, "Not connected to server"

        msg = json.dumps(self.batch_list)

        rc, resp_list = self.rpc_client.send_raw_msg(msg, block = False)
        if len(self.batch_list) == 1:
            return CmdResponse(True, [CmdResponse(rc, resp_list)])
        else:
            return CmdResponse(rc, resp_list)


# JSON RPC v2.0 client
class JsonRpcClient(object):

    def __init__ (self, default_server, default_port):
        self.verbose = False
        self.connected = False

        # default values
        self.port   = default_port
        self.server = default_server
        self.id_gen = general_utils.random_id_gen()

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
        if not self.verbose:
            return

        print "[verbose] " + msg


    # batch messages
    def create_batch (self):
        return BatchMessage(self)

    def create_jsonrpc_v2 (self, method_name, params = {}, encode = True):
        msg = {}
        msg["jsonrpc"] = "2.0"
        msg["method"]  = method_name

        msg["params"] = params

        msg["id"] = self.id_gen.next()

        if encode:
            return id, json.dumps(msg)
        else:
            return id, msg


    def invoke_rpc_method (self, method_name, params = {}, block = False):
        if not self.connected:
            return False, "Not connected to server"

        id, msg = self.create_jsonrpc_v2(method_name, params)

        return self.send_raw_msg(msg, block)


    # low level send of string message
    def send_raw_msg (self, msg, block = False):
        self.verbose_msg("Sending Request To Server:\n\n" + self.pretty_json(msg) + "\n")

        if block:
            self.socket.send(msg)
        else:
            try:
                self.socket.send(msg, flags = zmq.NOBLOCK)
            except zmq.error.ZMQError as e:
                self.disconnect()
                return CmdResponse(False, "Failed To Get Send Message")

        got_response = False

        if block:
            response = self.socket.recv()
            got_response = True
        else:
            for i in xrange(0 ,10):
                try:
                    response = self.socket.recv(flags = zmq.NOBLOCK)
                    got_response = True
                    break
                except zmq.Again:
                    sleep(0.2)

        if not got_response:
            self.disconnect()
            return CmdResponse(False, "Failed To Get Server Response")

        self.verbose_msg("Server Response:\n\n" + self.pretty_json(response) + "\n")

        # decode

        # batch ?
        response_json = json.loads(response)

        if isinstance(response_json, list):
            rc_list = []

            for single_response in response_json:
                rc, msg = self.process_single_response(single_response)
                rc_list.append( CmdResponse(rc, msg) )

            return CmdResponse(True, rc_list)

        else:
            rc, msg = self.process_single_response(response_json)
            return CmdResponse(rc, msg)


    def process_single_response (self, response_json):

        if (response_json.get("jsonrpc") != "2.0"):
            return False, "Malfromed Response ({0})".format(str(response))

        # error reported by server
        if ("error" in response_json):
            if "specific_err" in response_json["error"]:
                return False, response_json["error"]["specific_err"]
            else:
                return False, response_json["error"]["message"]

        # if no error there should be a result
        if ("result" not in response_json):
            return False, "Malformed Response ({0})".format(str(response))

        return True, response_json["result"]


  
    def set_verbose(self, mode):
        self.verbose = mode

    def disconnect (self):
        if self.connected:
            self.socket.close(linger = 0)
            self.context.destroy(linger = 0)
            self.connected = False
            return True, ""
        else:
            return False, "Not connected to server"

    def connect(self, server=None, port=None):
        if self.connected:
            self.disconnect()

        self.context = zmq.Context()

        self.server = (server if server else self.server)
        self.port = (port if port else self.port)

        #  Socket to talk to server
        self.transport = "tcp://{0}:{1}".format(self.server, self.port)

        print "\nConnecting To RPC Server On {0}".format(self.transport)

        self.socket = self.context.socket(zmq.REQ)
        try:
            self.socket.connect(self.transport)
        except zmq.error.ZMQError as e:
            return False, "ZMQ Error: Bad server or port name: " + str(e)


        self.connected = True

        return True, ""


    def reconnect(self):
        # connect using current values
        return self.connect()

        if not self.connected:
            return False, "Not connected to server"

        # reconnect
        return self.connect(self.server, self.port)


    def is_connected(self):
        return self.connected

    def __del__(self):
        print "Shutting down RPC client\n"
        if hasattr(self, "context"):
            self.context.destroy(linger=0)

# MOVE THIS TO DAN'S FILE
class TrexStatelessClient(JsonRpcClient):

    def __init__ (self, server, port, user):

        super(TrexStatelessClient, self).__init__(server, port)

        self.user = user
        self.port_handlers = {}

        self.supported_cmds  = []
        self.system_info     = None
        self.server_version  = None
        

    def whoami (self):
        return self.user

    def ping_rpc_server(self):

        return self.invoke_rpc_method("ping", block = False)

    def get_rpc_server_version (self):
        return self.server_version

    def get_system_info (self):
        if not self.system_info:
            return {}

        return self.system_info

    def get_supported_cmds(self):
        if not self.supported_cmds:
            return {}

        return self.supported_cmds

    def get_port_count (self):
        if not self.system_info:
            return 0

        return self.system_info["port_count"]

    # sync the client with all the server required data
    def sync (self):

        # get server version
        rc, msg = self.invoke_rpc_method("get_version")
        if not rc:
            self.disconnect()
            return rc, msg

        self.server_version = msg

        # get supported commands
        rc, msg = self.invoke_rpc_method("get_supported_cmds")
        if not rc:
            self.disconnect()
            return rc, msg

        self.supported_cmds = [str(x) for x in msg if x]

        # get system info
        rc, msg = self.invoke_rpc_method("get_system_info")
        if not rc:
            self.disconnect()
            return rc, msg

        self.system_info = msg

        return True, ""

    def connect (self):
        rc, err = super(TrexStatelessClient, self).connect()
        if not rc:
            return rc, err

        return self.sync()


    # take ownership over ports
    def take_ownership (self, port_id_array, force = False):
        if not self.connected:
            return False, "Not connected to server"

        batch = self.create_batch()

        for port_id in port_id_array:
            batch.add("acquire", params = {"port_id":port_id, "user":self.user, "force":force})

        rc, resp_list = batch.invoke()
        if not rc:
            return rc, resp_list

        for i, rc in enumerate(resp_list):
            if rc[0]:
                self.port_handlers[port_id_array[i]] = rc[1]

        return True, resp_list


    def release_ports (self, port_id_array):
        batch = self.create_batch()

        for port_id in port_id_array:

            # let the server handle un-acquired errors
            if self.port_handlers.get(port_id):
                handler = self.port_handlers[port_id]
            else:
                handler = ""

            batch.add("release", params = {"port_id":port_id, "handler":handler})


        rc, resp_list = batch.invoke()
        if not rc:
            return rc, resp_list

        for i, rc in enumerate(resp_list):
            if rc[0]:
                self.port_handlers.pop(port_id_array[i])

        return True, resp_list

    def get_owned_ports (self):
        return self.port_handlers.keys()

    # fetch port stats
    def get_port_stats (self, port_id_array):
        if not self.connected:
            return False, "Not connected to server"

        batch = self.create_batch()

        # empty list means all
        if port_id_array == []:
            port_id_array = list([x for x in xrange(0, self.system_info["port_count"])])

        for port_id in port_id_array:

            # let the server handle un-acquired errors
            if self.port_handlers.get(port_id):
                handler = self.port_handlers[port_id]
            else:
                handler = ""

            batch.add("get_port_stats", params = {"port_id":port_id, "handler":handler})


        rc, resp_list = batch.invoke()

        return rc, resp_list

    # snapshot will take a snapshot of all your owned ports for streams and etc.
    def snapshot(self):
        

        if len(self.get_owned_ports()) == 0:
            return {}

        snap = {}

        batch = self.create_batch()
        
        for port_id in self.get_owned_ports():

            batch.add("get_port_stats", params = {"port_id": port_id, "handler": self.port_handlers[port_id]})
            batch.add("get_stream_list", params = {"port_id": port_id, "handler": self.port_handlers[port_id]})

        rc, resp_list = batch.invoke()
        if not rc:
            return rc, resp_list

        # split the list to 2s
        index = 0
        for port_id in self.get_owned_ports():
            if not resp_list[index] or not resp_list[index + 1]:
                snap[port_id] = None
                continue

            # fetch the first two
            stats = resp_list[index][1]
            stream_list = resp_list[index + 1][1]
           
            port = {}
            port['status'] = stats['status']
            port['stream_list'] = []

            # get all the streams
            if len(stream_list) > 0:
                batch = self.create_batch()
                for stream_id in stream_list:
                    batch.add("get_stream", params = {"port_id": port_id, "stream_id": stream_id, "handler": self.port_handlers[port_id]})

                rc, stream_resp_list = batch.invoke()
                if not rc:
                    port = {}

                port['streams'] = {}
                for i, resp in enumerate(stream_resp_list):
                    if resp[0]:
                        port['streams'][stream_list[i]] = resp[1]

            snap[port_id] = port

            # move to next one
            index += 2


        return snap

    # add stream
    # def add_stream (self, port_id, stream_id, isg, next_stream_id, packet, vm=[]):
    #     if not port_id in self.get_owned_ports():
    #         return False, "Port {0} is not owned... please take ownership before adding streams".format(port_id)
    #
    #     handler = self.port_handlers[port_id]
    #
    #     stream = {}
    #     stream['enabled'] = True
    #     stream['self_start'] = True
    #     stream['isg'] = isg
    #     stream['next_stream_id'] = next_stream_id
    #     stream['packet'] = {}
    #     stream['packet']['binary'] = packet
    #     stream['packet']['meta'] = ""
    #     stream['vm'] = vm
    #     stream['rx_stats'] = {}
    #     stream['rx_stats']['enabled'] = False
    #
    #     stream['mode'] = {}
    #     stream['mode']['type'] = 'continuous'
    #     stream['mode']['pps'] = 10.0
    #
    #     params = {}
    #     params['handler'] = handler
    #     params['stream'] = stream
    #     params['port_id'] = port_id
    #     params['stream_id'] = stream_id
    #
    #     print params
    #     return self.invoke_rpc_method('add_stream', params = params)

    def add_stream(self, port_id_array, stream_pack_list):
        batch = self.create_batch()

        for port_id in port_id_array:
            for stream_pack in stream_pack_list:
                params = {"port_id": port_id,
                          "handler": self.port_handlers[port_id],
                          "stream_id": stream_pack.stream_id,
                          "stream": stream_pack.stream}
                batch.add("add_stream", params=params)
        rc, resp_list = batch.invoke()
        if not rc:
            return rc, resp_list

        for i, rc in enumerate(resp_list):
            if rc[0]:
                print "Stream {0} - {1}".format(i, rc[1])
                # self.port_handlers[port_id_array[i]] = rc[1]

        return True, resp_list

        # return self.invoke_rpc_method('add_stream', params = params)

if __name__ == "__main__":
    pass