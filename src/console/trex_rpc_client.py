
import zmq
import json
from time import sleep
import random

class RpcClient():

    def __init__ (self, server, port):
        self.context = zmq.Context()
        
        self.port = port
        self.server = server
        #  Socket to talk to server
        self.transport = "tcp://{0}:{1}".format(server, port)

        self.verbose = False

    def get_connection_details (self):
        rc = {}
        rc['server'] = self.server
        rc['port']   = self.port

        return rc

    def pretty_json (self, json_str):
        return json.dumps(json.loads(json_str), indent = 4, separators=(',', ': '), sort_keys = True)

    def verbose_msg (self, msg):
        if not self.verbose:
            return

        print "[verbose] " + msg


    def create_jsonrpc_v2 (self, method_name, params = {}, id = None):
        msg = {}
        msg["jsonrpc"] = "2.0"
        msg["method"]  = method_name

        msg["params"] = params

        msg["id"] = id

        return json.dumps(msg)

    def invoke_rpc_method (self, method_name, params = {}, block = True):
        id = random.randint(1, 1000)
        msg = self.create_jsonrpc_v2(method_name, params, id = id)

        self.verbose_msg("Sending Request To Server:\n\n" + self.pretty_json(msg) + "\n")

        if block:
            self.socket.send(msg)
        else:
            try:
                self.socket.send(msg, flags = zmq.NOBLOCK)
            except zmq.error.ZMQError:
                return False, "Failed To Get Server Response"

        got_response = False

        if block:
            response = self.socket.recv()
            got_response = True
        else:
            for i in xrange(0 ,5):
                try:
                    response = self.socket.recv(flags = zmq.NOBLOCK)
                    got_response = True
                    break
                except zmq.error.Again:
                    sleep(0.1)

        if not got_response:
            return False, "Failed To Get Server Response"

        self.verbose_msg("Server Response:\n\n" + self.pretty_json(response) + "\n")

        # decode
        response_json = json.loads(response)

        if (response_json.get("jsonrpc") != "2.0"):
            return False, "Malfromed Response ({0})".format(str(response))

        if (response_json.get("id") != id):
            return False, "Server Replied With Bad ID ({0})".format(str(response))

        # error reported by server
        if ("error" in response_json):
            return False, response_json["error"]["message"]

        # if no error there should be a result
        if ("result" not in response_json):
            return False, "Malfromed Response ({0})".format(str(response))

        return True, response_json["result"]


    def ping_rpc_server (self):

        return self.invoke_rpc_method("ping", block = False)
        
    def get_rpc_server_status (self):
        return self.invoke_rpc_method("get_status")

    def query_rpc_server (self):
        return self.invoke_rpc_method("get_reg_cmds")


    def set_verbose (self, mode):
        self.verbose = mode

    def connect (self):

        print "\nConnecting To RPC Server On {0}".format(self.transport)

        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(self.transport)

        # ping the server
        rc, err = self.ping_rpc_server()
        if not rc:
            self.context.destroy(linger = 0)
            return False, err

        #print "Connection Established !\n"
        print "[SUCCESS]\n"
        return True, ""

    def __del__ (self):
        print "Shutting down RPC client\n"
        self.context.destroy(linger = 0)
        


