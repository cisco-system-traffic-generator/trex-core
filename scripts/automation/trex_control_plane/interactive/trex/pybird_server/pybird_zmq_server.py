import signal
from functools import partial
import json
import logging.handlers
import logging
import socket
import hashlib
from argparse import *
import errno
import traceback
import sys
import random as rand

import zmq
from trex.common.trex_types import LRU_cache
from trex.pybird_server.pybird import PyBird


class ParseException(Exception): pass
class InvalidRequest(Exception): pass
class MethodNotFound(Exception): pass
class InvalidParams(Exception): pass
class DifferentVersions(Exception): pass
class InvalidHandler(Exception): pass

class BirdWrapper:
    def __init__(self):
        self.pybird = PyBird()
        self.prev_md5s = LRU_cache(maxlen=5)        # store previous configs like: {'md5_key': bird_cfg_string }
        self._config_fragments = {'md5': None, 'frags': []}
        self.bird_methods = {   'connect': {'method_ptr': self.connect, 'is_command': False},
                                'get_config': {'method_ptr': self.get_config, 'is_command': False},
                                'set_config': {'method_ptr': self.set_config, 'is_command': True},
                                'set_empty_config': {'method_ptr': self.set_empty_config, 'is_command': True},
                                'get_protocols_info': {'method_ptr': self.get_protocols_info, 'is_command': False},
                                'disconnect': {'method_ptr': self.disconnect, 'is_command': False},
                                }
        try:
            current_cfg = self.pybird.get_config().encode('utf-8')
            self._current_md5 = hashlib.md5(current_cfg).hexdigest()
            self.prev_md5s[self._current_md5] = current_cfg
        except Exception as e:
            print("Error occurred while reading from bird current cfg in the first time: %s" % e)
            self._current_md5 = -1  # signify no md5 loaded

    def __del__(self):
        self.logger.info('***IN BIRD WRAPPER DTOR***')
        self.execute('disconnect', [])

    def parse_req_msg(self, JSON_req):
        try:
            req = json.loads(JSON_req)
            req_id = 'null'
            if (type(req) != type({})):
                raise ParseException("Expected dictionary got: '%s' " % type(req))
            json_rpc_keys = {'jsonrpc', 'id', 'method'}
            if set(req.keys()) != json_rpc_keys and set(req.keys()) != json_rpc_keys.union({'params'}):
                if 'id' in req.keys():
                    req_id = req['id']
                raise InvalidRequest(req_id)
            req_id = req['id']
            if 'params' in req.keys():
                return req['method'], req['params'], req_id
            else:
                return req['method'], [], req_id
        except ValueError:
            raise ParseException(req_id)

    def connect(self, params):
        return self.pybird.connect()

    def disconnect(self, params):
        return self.pybird.disconnect()

    def get_config(self, params):
        return self.pybird.get_config()

    def get_protocols_info(self, params):
        return self.pybird.get_protocols_info()

    def set_config(self ,params):
        first, last = params.get('frag_first'), params.get('frag_last')
        fragment = params['fragment']
        md5 = params.get('md5')
        
        self._config_fragments['frags'].append(fragment)

        if first and last:
            return self._set_new_config(fragment)
        elif first:
            self._config_fragments['md5'] = md5
        elif last:
            all_cfg = ''.join(self._config_fragments['frags'])
            md5 = self._config_fragments['md5']
            return self._set_new_config(all_cfg, md5)
        return 'send_another_frag'  # middle fragment

    def set_empty_config(self, params):
        return self.pybird.set_empty_config()

    def _set_new_config(self, config, md5=None):
        if md5 is None:
            if type(config) is str:
                config = config.encode('utf-8')
            md5 = hashlib.md5(config).hexdigest()
        
        self._config_fragments['frags'] = []       # reset fragments config
        self._config_fragments['md5'] = None
        cached = self._is_md5_cached(md5)
        
        if 'Already configured' in cached:
            return cached
        elif 'Found configuration in cache' in cached:
            self._current_md5 = md5
            return cached + self.pybird.set_config(self.prev_md5s[md5])
        else:
            res = self.pybird.set_config(config)  # return a string
            if 'Configured successfully' in res:
                self._current_md5 = md5
                self.prev_md5s[md5] = config           # new md5, cache it
            return res

    def _is_md5_cached(self, md5):
        if md5 == self._current_md5:
            return 'Already configured'
        if md5 in self.prev_md5s.keys():
            return 'Found configuration in cache. '
        return ''

    def create_error_response(self, error_code, error_msg, req_id):
        return {"jsonrpc": "2.0", "error": {"code": error_code, "message": error_msg}, "id": req_id}

    def create_success_response(self, result, req_id):
        return {"jsonrpc": "2.0", "result": result, "id": req_id}

    def get_exception(self):
        return sys.exc_info()

    def has_method(self, method):
        return method in self.bird_methods.keys()

    def execute(self, method, params):
        return self.bird_methods[method]['method_ptr'](params) 

    def is_command_method(self, method):
        return self.bird_methods[method]['is_command']

    def error_handler(self, e, req_id):
        response = {"jsonrpc": "2.0", "error": {"code": -32097, "message": str(e)}, "id": req_id}
        try:
            raise e
        except ParseException as e:
            response = self.create_error_response(-32700,
                                                  'Parse error ', req_id)
        except InvalidRequest as e:
            response = self.create_error_response(
                -32600, 'Invalid Request', req_id)
        except MethodNotFound as e:
            response = self.create_error_response(
                -32601, 'Method not found', req_id)
        except InvalidParams as e:
            response = self.create_error_response(
                -32603, 'Invalid params', req_id)
        except SyntaxError as e:
            response = self.create_error_response(-32097,
                                                  'SyntaxError', req_id)
        except Exception as e:
            if hasattr(e, 'strerror'):
                response = self.create_error_response(
                    -32099, 'Bird Server: '+str(e.strerror), req_id)  # TODO maybe other code?
            elif hasattr(e, 'message'):
                response = self.create_error_response(
                    -32098, 'Bird Server: '+str(e.message), req_id)
            elif (is_python(3)):
                response = self.create_error_response(
                    -32096, 'Bird Server: '+str(e), req_id)
            else:
                response = self.create_error_response(
                    -32096, 'Bird Server: Unknown Error', req_id)
        finally:
            return response

def rand_32_bit():
    return rand.randint(0, 0xFFFFFFFF)

class BirdServer():

    SERVER_VERSION = "1.0"  # need to sync with bird zmq client

    def __init__(self, args, port = 4509):
        self._build_logger()
        self.logger.info('***Bird Server is starting***')
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        
        self.socket.bind("tcp://*:"+str(port))
        self.logger.info('Listening on port: %d' % self.port)
        
        self.connection_methods    = {'shut_down': self.shut_down,
                                   'connect': self.connect_with_client,
                                   'acquire': self.acquire_client,
                                   'release': self.release_client,
                                   'disconnect': self.disconnect_client
                                   }
        self.is_connected          = False
        self.current_handler       = None  # store random 32bit number for current client
        self.connected_clients     = 0
        self.bird_wrapper          = BirdWrapper()

        try:
            self.IP_address = socket.gethostbyname(socket.gethostname())
        except:
            self.IP_address = '0.0.0.0'
        self.logger.info('Server IP address: %s' % self.IP_address)
        
    def __del__(self):
        try:
            self.logger.info('***IN BirdServer DTOR***')
            self._close_conn()
            self.logger.info('***DONE DTOR***')
        except Exception as e:
            self.logger.warning(e)

    def _close_conn(self):
        if self.socket is not None:
            self.logger.info('Closing socket!')
            self.socket.close()
        if self.context is not None:
            self.logger.info('Destroying context!')
            self.context.destroy()

    def _build_logger(self):
        self.logger = logging.getLogger('pybird_server_logger')
        self.logger.setLevel(logging.INFO)
        console_h = logging.StreamHandler(sys.__stdout__)
        formatter = logging.Formatter(
            fmt='%(asctime)s %(message)s', datefmt='%d-%m-%Y %H:%M:%S')
        if args.log:
            logfile_h = logging.FileHandler('/var/log/pybird_server.log')
            logfile_h.setLevel(logging.INFO)
            logfile_h.setFormatter(formatter)
            self.logger.addHandler(logfile_h)
        if args.verbose:
            console_h.setLevel(logging.INFO)
        else:
            console_h.setLevel(logging.WARNING)
        console_h.setFormatter(formatter)
        self.logger.addHandler(console_h)

    def shut_down(self, params):
        self.logger.info('Shut down by remote user')
        return 'Server shut down command received - server had shut down'

    def connect_with_client(self, params):
        if params[0] == BirdServer.SERVER_VERSION:
            self.bird_wrapper.execute('connect', [])
            self.connected_clients += 1
            self.is_connected = True
            return 'Connection established'
        else:
            raise DifferentVersions('Server version: "%s", Client version: "%s"' % (
                BirdServer.SERVER_VERSION, params[0]))

    def acquire_client(self, params):
        if self.current_handler:
            if params[0]:
                self.logger.info("Warning: Forced acquire by client")
                self.current_handler = rand_32_bit()  # forced acquire
            else:
                raise InvalidHandler("Cannot handle client, server is already acquired")
        else:
            self.logger.info("Acquired new client")
            self.current_handler = rand_32_bit()
        return self.current_handler

    def release_client(self, params):
        if self.current_handler is not None:
            if self.current_handler == params[0]:
                self.current_handler = None
                return "Client released successfully"
            else:
                raise InvalidHandler('Cannot release client with handler: "%s", working now with: "%s"' % (params[0], self.current_handler))
        else:
            raise InvalidHandler("Cannot release client, server is not acquired to client")


    def disconnect_client(self, params):
        if self.is_connected:
            self.connected_clients -= 1
            if self.connected_clients == 0:
                self.bird_wrapper.execute('disconnect', [])
                self.is_connected = False
        else:
            raise InvalidRequest("Cannot disconnect, client is not connected")

    def check_handler(self, params):
        if params['handler'] != self.current_handler:
            raise InvalidHandler('Cannot make a change while another client is handled')

    def start(self):
        self.logger.info('***Bird Server Started***')
        try:
            while True:
                try:
                    message = self.socket.recv()
                    message = message.decode()

                    self.logger.info('Received Message: %s' % message)
                except zmq.ZMQError as e:
                    if e.errno != errno.EINTR:
                        raise e
                    continue

                try:
                    params = []
                    method = ''
                    req_id = 'null'
                    method, params, req_id = self.bird_wrapper.parse_req_msg(message)
                    if method in self.connection_methods.keys():
                        result = self.connection_methods[method](params)
                    elif self.bird_wrapper.has_method(method):
                        if self.bird_wrapper.is_command_method(method):
                            self.check_handler(params)  # handler check for set commands
                        result = self.bird_wrapper.execute(method, params)
                    else:
                        raise MethodNotFound(req_id)
                    response = self.bird_wrapper.create_success_response(result, req_id)
                except Exception as e:
                    response = self.bird_wrapper.error_handler(e, req_id)
                    self.logger.info('ERROR %s: %s', response['error']['code'], response['error']['message'])
                    self.logger.info('Exception info: %s' % traceback.format_exc())
                finally:
                    try:
                        json_response = json.dumps(response)
                        self.logger.info('Sending Message: %s' % json_response)
                    except Exception as e:
                        # rare case when json can not be serialized due to encoding issues
                        # object is not JSON serialized
                        self.logger.error('Unexpected Error: %s' % traceback.format_exc())
                        json_response = json.dumps(self.bird_wrapper.error_handler(e, req_id))

                #  Send reply back to client
                    self.socket.send(json_response.encode('utf-8'))
                    if (method == 'shut_down'):
                        break

        except KeyboardInterrupt:
            self.logger.info(b'Terminated By local user')

        finally:
            self.socket.close()
            self.context.destroy()


# arg1 is port number for the server to listen to
def main(args, port):
    server = BirdServer(args, port)

    def cleanup(server, signal, frame):
        server.logger.info('Got signal, starting cleanup')
        server._close_conn()
        server.logger.info('END of cleanup')
        sys.exit(0)

    signal.signal(signal.SIGINT, partial(cleanup, server))
    signal.signal(signal.SIGTERM, partial(cleanup, server))

    server.start()


if __name__ == '__main__':

    parser = ArgumentParser(description=' Runs Bird Server ')
    parser.add_argument('-p', '--bird-port', type=int, default=4509, dest='bird_port',
                        help='Select port to which Bird Server will listen to.\n default is 4509.', action='store')
    parser.add_argument('-v', '--verbose', help='Print Client-Server Request-Reply information to console.',
                        action='store_true', default=False)
    parser.add_argument('-l', '--log', help='Log every activity of the server to the log file pybird_server .The log does not discard older entries, the file is not limited by size.',
                        action='store_true', default=False)
    args = parser.parse_args()
    port = args.bird_port
    main(args, port)
