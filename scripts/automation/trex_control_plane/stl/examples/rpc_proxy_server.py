#!/usr/bin/python

import argparse
import traceback
import logging
import sys
import os
import json
import socket
from functools import partial
logging.basicConfig(level = logging.FATAL) # keep quiet

import stl_path
from trex_stl_lib.api import *
from trex_stl_lib.trex_stl_hltapi import CTRexHltApi, HLT_OK, HLT_ERR

# ext libs
ext_libs = os.path.join(os.pardir, os.pardir, os.pardir, os.pardir, 'external_libs') # usual package path
if not os.path.exists(ext_libs):
    ext_libs = os.path.join(os.pardir, os.pardir, 'external_libs') # client package path
sys.path.append(os.path.join(ext_libs, 'jsonrpclib-pelix-0.2.5'))
from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import yaml

# TODO: refactor this to class

native_client = None
hltapi_client = None

def OK(res = True):
    return[True, res]

def ERR(res = 'Unknown error'):
    return [False, res]

def deunicode_json(data):
    return yaml.safe_load(json.dumps(data))


### Server functions ###

def add(a, b): # for sanity checks
    try:
        return OK(a + b)
    except:
        return ERR(traceback.format_exc())

def check_connectivity():
    return OK()

def native_proxy_init(force = False, *args, **kwargs):
    global native_client
    if native_client and not force:
        return ERR('Native Client is already initiated')
    try:
        native_client = STLClient(*args, **kwargs)
        return OK('Native Client initiated')
    except:
        return ERR(traceback.format_exc())

def native_proxy_del():
    global native_client
    native_client = None
    return OK()

def hltapi_proxy_init(force = False, *args, **kwargs):
    global hltapi_client
    if hltapi_client and not force:
        return HLT_ERR('HLTAPI Client is already initiated')
    try:
        hltapi_client = CTRexHltApi(*args, **kwargs)
        return HLT_OK()
    except:
        return HLT_ERR(traceback.format_exc())

def hltapi_proxy_del():
    global hltapi_client
    hltapi_client = None
    return HLT_OK()

# any method not listed above can be called with passing its name here
def native_method(func_name, *args, **kwargs):
    try:
        func = getattr(native_client, func_name)
        return OK(func(*deunicode_json(args), **deunicode_json(kwargs)))
    except:
        return ERR(traceback.format_exc())

# any HLTAPI method can be called with passing its name here
def hltapi_method(func_name, *args, **kwargs):
    try:
        func = getattr(hltapi_client, func_name)
        return func(*deunicode_json(args), **deunicode_json(kwargs))
    except:
        return HLT_ERR(traceback.format_exc())

### /Server functions ###


def run_server(port = 8095):
    native_methods = [
                      'acquire',
                      'connect',
                      'disconnect',
                      'get_stats',
                      'get_warnings',
                      'push_remote',
                      'reset',
                      'wait_on_traffic',
                     ]
    hltapi_methods = [
                      'connect',
                      'cleanup_session',
                      'interface_config',
                      'traffic_config',
                      'traffic_control',
                      'traffic_stats',
                     ]

    try:
        server = SimpleJSONRPCServer(('0.0.0.0', port))
        server.register_function(add)
        server.register_function(check_connectivity)
        server.register_function(native_proxy_init)
        server.register_function(native_proxy_del)
        server.register_function(hltapi_proxy_init)
        server.register_function(hltapi_proxy_del)
        server.register_function(native_method)
        server.register_function(hltapi_method)

        for method in native_methods:
            server.register_function(partial(native_method, method), method)
        for method in hltapi_methods:
            if method in native_methods: # collision in names
                method_hlt_name = 'hlt_%s' % method
            else:
                method_hlt_name = method
            server.register_function(partial(hltapi_method, method), method_hlt_name)
        server.register_function(server.funcs.keys, 'get_methods') # should be last
        print('Started Stateless RPC proxy at port %s' % port)
        server.serve_forever()
    except KeyboardInterrupt:
        print('Done')

### Main ###



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Runs TRex Stateless RPC proxy for usage with any language client.')
    parser.add_argument('-p', '--port', type=int, default = 8095, dest='port', action = 'store',
                        help = 'Select port on which the stl rpc proxy will run.\nDefault is 8095.')
    kwargs = vars(parser.parse_args())
    run_server(**kwargs)

