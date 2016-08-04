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

class ParseException(Exception): pass
class InvalidRequest(Exception): pass
class MethodNotFound(Exception): pass
class InvalidParams(Exception): pass

class Scapy_wrapper:
    def __init__(self):
        self.scapy_master = Scapy_service()

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
    
    def get_exception(self):
        return sys.exc_info()


    def execute(self,method,params):
        if len(params)>0:
            result = eval('self.scapy_master.'+method+'(*'+str(params)+')')
        else:
            result = eval('self.scapy_master.'+method+'()')
        return result


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
        except BaseException as e:
            response = self.create_error_response(-32098,'Scapy Server: '+str(e.message),req_id)
        finally:
            return response
        
class Scapy_server():
    def __init__(self,port):
        self.scapy_wrapper = Scapy_wrapper()
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind("tcp://*:"+str(port))

    def activate(self):
        try:
            while True:
                message = self.socket.recv()
                try:
                    method,params,req_id = self.scapy_wrapper.parse_req_msg(message)
                    result = self.scapy_wrapper.execute(method,params)
                    response = self.scapy_wrapper.create_success_response(result,req_id)
                except Exception as e:
                    exception_details = self.scapy_wrapper.get_exception()
                    response = self.scapy_wrapper.error_handler(exception_details)
                finally:
                    json_response = json.dumps(response)
                    time.sleep(1)

                #  Send reply back to client
                    self.socket.send(json_response)
        except KeyboardInterrupt:
                print('Terminated By Ctrl+C')


s = Scapy_server(5555)
s.activate()
