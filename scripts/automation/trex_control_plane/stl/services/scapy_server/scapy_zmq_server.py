
import time
import sys
import os

python2_zmq_path = os.path.abspath(os.path.join(os.pardir,os.pardir,os.pardir,os.pardir,
                                                os.pardir,'external_libs','pyzmq-14.5.0','python2','fedora18','64bit'))
stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
sys.path.append(stl_pathname)
sys.path.append(python2_zmq_path)
import zmq
import inspect
from scapy_service import *
from argparse import *
import socket


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


    def error_handler(self,exception_obj,req_id):
        try:
            self.metaraise(exception_obj)
        except ParseException as e:
            response = self.create_error_response(-32700,'Parse error ',req_id)
        except InvalidRequest as e:
            response = self.create_error_response(-32600,'Invalid Request',req_id)
        except MethodNotFound as e:
            response = self.create_error_response(-32601,'Method not found',req_id)
        except InvalidParams as e:
            response = self.create_error_response(-32603,'Invalid params',req_id)
        except SyntaxError as e:
            response = self.create_error_response(-32097,'SyntaxError',req_id)
        except BaseException as e:
            response = self.create_error_response(-32098,'Scapy Server: '+str(e.message),req_id)
        except:
            response = self.create_error_response(-32096,'Scapy Server: Unknown Error',req_id)
        finally:
            return response



parser = ArgumentParser(description=' Runs Scapy Server ')
parser.add_argument('-s','--scapy-port',type=int, default = 4507, dest='scapy_port',
                    help='Select port to which Scapy Server will listen to.\n default is 4507\n',action='store')
args = parser.parse_args()

port = args.scapy_port

class Scapy_server():
    def __init__(self, port=4507):
        self.scapy_wrapper = Scapy_wrapper()
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind("tcp://*:"+str(port))
        self.IP_address = socket.gethostbyname(socket.gethostname())

    def activate(self):
        print '***Scapy Server Started***\nListening on port: %d' % self.port
        print 'Server IP address: %s' % self.IP_address
        try:
            while True:
                message = self.socket.recv()
                try:
                    method,params,req_id = self.scapy_wrapper.parse_req_msg(message)
                    result = self.scapy_wrapper.execute(method,params)
                    response = self.scapy_wrapper.create_success_response(result,req_id)
                except Exception as e:
                    exception_details = self.scapy_wrapper.get_exception()
                    response = self.scapy_wrapper.error_handler(exception_details,req_id)
                finally:
                    json_response = json.dumps(response)
                #  Send reply back to client
                    self.socket.send(json_response)
        except KeyboardInterrupt:
                print('Terminated By Ctrl+C')


#s = Scapy_server(port)
#s.activate()

#arg1 is port number for the server to listen to
def main(arg1=4507):
    s = Scapy_server(arg1)
    s.activate()

if __name__=='__main__':
    if len(sys.argv)>1:
        sys.exit(main(int(sys.argv[2])))
    else:
        sys.exit(main())
