#
#   Hello World server in Python
#   Binds REP socket to tcp://*:5555
#   Expects b"Hello" from client, replies with b"World"
#

import time
import sys
sys.path.append(sys.path.append('/auto/srg-sce-swinfra-usr/emb/users/itraviv/trex/trex-core/scripts/external_libs/pyzmq-14.5.0/python2/fedora18/64bit'))
sys.path.append(sys.path.append('/auto/srg-sce-swinfra-usr/emb/users/itraviv/trex/trex-core/scripts/automation/trex_control_plane/stl/'))
import zmq
import inspect
from scapy_server import *

server_file_name = 'zmq_server_hello_world_server'

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:5555")


class ParseException(Exception): pass
class InvalidRequest(Exception): pass
class MethodNotFound(Exception): pass
class InvalidParams(Exception): pass
#def error_response()

class Scapy_wrapper:
    def __init__(self):
        self.scapy_master = scapy_service()

    def parse_req_msg(self,JSON_req):
        try:
            req = json.loads(JSON_req)
            req_id='null'
            if (type(req)!= type({})):
                raise ParseException(req_id)
            json_rpc_keys = ['jsonrpc','id','method']
            if ((set(req.keys())!=set(json_rpc_keys)) and (set(req.keys())!=set(json_rpc_keys+['params']))) :
                if 'id' in req.keys():
                    req_id = req['id']
                raise InvalidRequest(req_id)
            req_id = req['id']
            if not (self.scapy_master.supported_methods(req['method'])):
                raise MethodNotFound(req_id)
            scapy_method = eval("self.scapy_master."+req['method'])
            arg_num_for_method = len(inspect.getargspec(scapy_method)[0])
            if (arg_num_for_method>1) :
                if not ('params' in req.keys()):
                    raise InvalidRequest(req_id)
                params_len = len(req['params'])+1 # +1 because "self" is considered parameter in args for method
                if not (params_len==arg_num_for_method):
                    raise InvalidParams(req_id)
                return req['method'],req['params'],req_id
            else:
                return req['method'],[],req_id
        except ValueError:
            raise ParseException(req_id)

    def create_error_response(self,error_code,error_msg,req_id='null'):
        return {"jsonrpc": "2.0", "error": {"code": error_code, "message:": error_msg}, "id": req_id}
        
    def create_success_response(self,result,req_id='null'):
        return {"jsonrpc": "2.0", "result": result, "id": req_id }
    
    def getException(self):
        return sys.exc_info()

    def metaraise(self,exc_info):
        raise exc_info[0], exc_info[1], exc_info[2]


    def error_handler(self,exception_obj):
        try:
            self.metaraise(exception_obj)
        except ParseException as e:
            response = self.create_error_response(-32700,'Parse error',e.message)
        except InvalidRequest as e:
            response = self.create_error_response(-32600,'Invalid Request',e.message)
        except MethodNotFound as e:
            response = self.create_error_response(-32601,'Method not found',e.message)
        except InvalidParams as e:
            response = self.create_error_response(-32603,'Invalid params',e.message)
        except SyntaxError as e:
            response = self.create_error_response(-32097,'SyntaxError')
        except Exception as e:
            response = self.create_error_response(-32098,'Scapy Server: '+str(e.message),req_id)
        finally:
            return response
        

scapy_wrapper = Scapy_wrapper()
try:
    while True:
        #  Wait for next request from client
        message = socket.recv()
#       print("Received request: %s" % message)
#       print ("message type is: %s" % type(message))
#       print("message is now: %s" % message)
        try:
            method,params,req_id = scapy_wrapper.parse_req_msg(message)
            if len(params)>0:
                result = eval('scapy_wrapper.scapy_master.'+method+'(*'+str(params)+')')
            else:
                result = eval('scapy_wrapper.scapy_master.'+method+'()')
            response = scapy_wrapper.create_success_response(result,req_id)
        except:
            print 'got exception'
            e = scapy_wrapper.getException()
            response = scapy_wrapper.error_handler(e)
        finally:
            json_response = json.dumps(response)
            time.sleep(1)

        #  Send reply back to client
            socket.send(json_response)
except KeyboardInterrupt:
        print('Ctrl+C pressed')
