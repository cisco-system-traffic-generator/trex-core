
import zmq
import json
from time import sleep

class RpcClient():

    def create_jsonrpc_v2 (self, method_name, params = {}, id = None):
        msg = {}
        msg["jsonrpc"] = "2.0"
        msg["method"]  = method_name

        msg["params"] = {}
        for key, value in params.iteritems():
            msg["params"][key] = value

        msg["id"] = id

        return json.dumps(msg)

    def invoke_rpc_method (self, method_name, params = {}, block = True):
        msg = self.create_jsonrpc_v2(method_name, params, id = 1)

        if self.verbose:
            print "\nSending Request To Server: " + str(msg) + "\n"

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

        if self.verbose:
            print "Server Response: " + str(response) + "\n"

        # decode
        response_json = json.loads(response)

        if (response_json.get("jsonrpc") != "2.0"):
            return False, "Malfromed Response ({0})".format(str(response))

        # error reported by server
        if ("error" in response_json):
            return False, response_json["error"]["message"]

        # if no error there should be a result
        if ("result" not in response_json):
            return False, "Malfromed Response ({0})".format(str(response))

        return True, response_json["result"]


    def ping_rpc_server (self):

        return self.invoke_rpc_method("rpc_ping", block = False)
        
    def get_rpc_server_status (self):
        return self.invoke_rpc_method("rpc_get_status")

    def query_rpc_server (self):
        return self.invoke_rpc_method("rpc_get_reg_cmds")

    def __init__ (self, port):
        self.context = zmq.Context()
        
        #  Socket to talk to server
        self.transport = "tcp://localhost:{0}".format(port)

        self.verbose = False

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
            raise Exception(err)

        #print "Connection Established !\n"
        print "[SUCCESS]\n"


    def __del__ (self):
        print "Shutting down RPC client\n"
        self.context.destroy(linger = 0)
        


