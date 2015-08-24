#!/router/bin/python

import outer_packages
import zmq
import json
import general_utils
from time import sleep

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

    def invoke_rpc_method (self, method_name, params = {}, block = False):
        rc, msg = self._invoke_rpc_method(method_name, params, block)
        if not rc:
            self.disconnect()

        return rc, msg

    def _invoke_rpc_method (self, method_name, params = {}, block = False):
        if not self.connected:
            return False, "Not connected to server"

        id = self.id_gen.next()
        msg = self.create_jsonrpc_v2(method_name, params, id = id)

        self.verbose_msg("Sending Request To Server:\n\n" + self.pretty_json(msg) + "\n")

        if block:
            self.socket.send(msg)
        else:
            try:
                self.socket.send(msg, flags = zmq.NOBLOCK)
            except zmq.error.ZMQError as e:
                return False, "Failed To Get Send Message"

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
            return True, response_json["error"]["message"]

        # if no error there should be a result
        if ("result" not in response_json):
            return False, "Malfromed Response ({0})".format(str(response))

        return True, response_json["result"]


    def ping_rpc_server(self):

        return self.invoke_rpc_method("ping", block = False)

    def get_rpc_server_status (self):
        return self.invoke_rpc_method("get_status")

    def query_rpc_server(self):
        return self.invoke_rpc_method("get_reg_cmds")


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

    def connect(self, server = None, port = None):
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

        # ping the server
        rc, err = self.ping_rpc_server()
        if not rc:
            self.disconnect()
            return rc, err

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
        self.context.destroy(linger=0)

if __name__ == "__main__":
    pass
