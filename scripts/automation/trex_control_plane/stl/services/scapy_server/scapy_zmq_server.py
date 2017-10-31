
import time
import sys
import os
import traceback
import errno

stl_pathname = os.path.abspath(os.path.join(os.pardir, os.pardir))
if stl_pathname not in sys.path:
    sys.path.append(stl_pathname)
from trex_stl_lib.api import *
import zmq
import inspect
from scapy_service import *
from argparse import *
import socket
import logging
import logging.handlers


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
            if (req['method']=='shut_down'):
                return 'shut_down',[],req_id
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

    def create_error_response(self,error_code,error_msg,req_id):
        return {"jsonrpc": "2.0", "error": {"code": error_code, "message": error_msg}, "id": req_id}
        
    def create_success_response(self,result,req_id):
        return {"jsonrpc": "2.0", "result": result, "id": req_id }
    
    def get_exception(self):
        return sys.exc_info()


    def execute(self,method,params):
        if len(params)>0:
            result = eval('self.scapy_master.'+method+'(*'+str(params)+')')
        else:
            result = eval('self.scapy_master.'+method+'()')
        return result


    def error_handler(self,e,req_id):
        response = []
        try:
            raise e
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
        except Exception as e:
            if hasattr(e,'message'):
                response = self.create_error_response(-32098,'Scapy Server: '+str(e.message),req_id)
            else:
                response = self.create_error_response(-32096,'Scapy Server: Unknown Error',req_id)            
        finally:
            return response

class Scapy_server():
    def __init__(self, args,port=4507):
        self.scapy_wrapper = Scapy_wrapper()
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind("tcp://*:"+str(port))
        try:
            self.IP_address = socket.gethostbyname(socket.gethostname())
        except:
            self.IP_address = '0.0.0.0'
        self.logger = logging.getLogger('scapy_logger')
        self.logger.setLevel(logging.INFO)
        console_h = logging.StreamHandler(sys.__stdout__)
        formatter = logging.Formatter(fmt='%(asctime)s %(message)s',datefmt='%d-%m-%Y %H:%M:%S')
        if args.log:
            logfile_h = logging.FileHandler('scapy_server.log')
            logfile_h.setLevel(logging.INFO)
            logfile_h.setFormatter(formatter)
            self.logger.addHandler(logfile_h)
        if args.verbose:
            console_h.setLevel(logging.INFO)
        else:
            console_h.setLevel(logging.WARNING)
        console_h.setFormatter(formatter)
        self.logger.addHandler(console_h)


    def activate(self):
        self.logger.info('***Scapy Server Started***')
        self.logger.info('Listening on port: %d' % self.port)
        self.logger.info('Server IP address: %s' % self.IP_address)
        try:
            while True:
                try:
                    message = self.socket.recv_string()
                    self.logger.info('Received Message: %s' % message)
                except zmq.ZMQError as e:
                    if e.errno != errno.EINTR:
                        raise e
                    continue

                try:
                    params = []
                    method=''
                    req_id = 'null'
                    method,params,req_id = self.scapy_wrapper.parse_req_msg(message)
                    if (method == 'shut_down'):
                        self.logger.info('Shut down by remote user')
                        result = 'Server shut down command received - server had shut down'
                    else:
                        result = self.scapy_wrapper.execute(method,params)
                    response = self.scapy_wrapper.create_success_response(result,req_id)
                except Exception as e:
                    response = self.scapy_wrapper.error_handler(e,req_id)
                    self.logger.info('ERROR %s: %s',response['error']['code'], response['error']['message'])
                    self.logger.info('Exception info: %s' % traceback.format_exc())
                finally:
                    try:
                        json_response = json.dumps(response)
                        self.logger.info('Sending Message: %s' % json_response)
                    except Exception as e:
                        # rare case when json can not be searialized due to encoding issues
                        # object is not JSON serializable
                        self.logger.error('Unexpected Error: %s' % traceback.format_exc())
                        json_response = json.dumps(self.scapy_wrapper.error_handler(e,req_id))

                #  Send reply back to client
                    self.socket.send_string(json_response)
                    if (method == 'shut_down'):
                        break

        except KeyboardInterrupt:
                self.logger.info(b'Terminated By local user')

        finally:
            self.socket.close()
            self.context.destroy()



#arg1 is port number for the server to listen to
def main(args,port):
    s = Scapy_server(args,port)
    s.activate()

if __name__=='__main__':
    
        parser = ArgumentParser(description=' Runs Scapy Server ')
        parser.add_argument('-s','--scapy-port',type=int, default = 4507, dest='scapy_port',
                            help='Select port to which Scapy Server will listen to.\n default is 4507.',action='store')
        parser.add_argument('-v','--verbose',help='Print Client-Server Request-Reply information to console.',action='store_true',default = False)
        parser.add_argument('-l','--log',help='Log every activity of the server to the log file scapy_server.log .The log does not discard older entries, the file is not limited by size.',
                            action='store_true',default = False)
        args = parser.parse_args()
        port = args.scapy_port
        sys.exit(main(args,port))

