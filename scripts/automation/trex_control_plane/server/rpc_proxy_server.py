#!/usr/bin/python

import argparse
import traceback
import logging
import json
from functools import partial
logging.basicConfig(level = logging.FATAL) # keep quiet

import outer_packages # This allows access to trex packages
from trex.stl.api import *
from trex.emu.api import *
from trex.stl.trex_stl_hltapi import CTRexHltApi, HLT_OK, HLT_ERR

from jsonrpclib.SimpleJSONRPCServer import SimpleJSONRPCServer
import yaml


native_client = None
emu_client = None
hltapi_client = None


def OK(res = True):
    return[True, res]


def ERR(res = 'Unknown error'):
    return [False, res]


def deunicode_json(data):
    return yaml.safe_load(json.dumps(data))


def decode_jsonpickle(data):
    """Receives a JSON decoded object. Will encode it back to string and decode using jsonpickle.

    Note: jsonpickle uses `__new__` to create a new instance of the object based on the `py/object` value.
    It doesn't use `__init__` at any stage. Hence be careful with the data. We can't provide part of
    the attributes and assume the other part will use default values.

    For example:

    .. highlight:: python
    .. code-block:: python

        {'ns_key':
            [
                {'vport': 1, 'tpid': [0], 'py/object': 'trex.emu.trex_emu_profile.EMUNamespaceKey', 'tci': [5]},
                {'vport': 1, 'tpid': [0], 'py/object': 'trex.emu.trex_emu_profile.EMUNamespaceKey', 'tci': [6]}
            ]
        }

    More examples of common types:

    .. highlight:: python
    .. code-block:: python

        {
            "mac": {"num_val": 7340033, "py/object": "trex.emu.trex_emu_conversions.Mac", "s_val": null, "v_val": null},
            "client_key": { "mac": {"num_val": 7340033, "py/object": "trex.emu.trex_emu_conversions.Mac", "s_val": null, "v_val": null},
                            "ns_key": {"py/object": "trex.emu.trex_emu_profile.EMUNamespaceKey", "tci": [0, 0], "tpid": [0, 0], "vport": 0},
                            "py/object": "trex.emu.trex_emu_profile.EMUClientKey"},
            "client_obj": {"fields": {"ipv4": {"mc": false, "num_val": 16843009, "py/object": "trex.emu.trex_emu_conversions.Ipv4", "s_val": null, "v_val": null},
                                      "mac": {"num_val": 7340033, "py/object": "trex.emu.trex_emu_conversions.Mac", "s_val": null, "v_val": null}}, 
                           "py/object": "trex.emu.trex_emu_profile.EMUClientObj"},
        }

    Args:
        data (Any): Any data that is correctly decoded by json.loads()

    Returns:
        Any: It creates new Python objects based on the `py/object` value.

        For example:

        .. highlight:: python
        .. code-block:: python

            {'ns_key':
                [
                    <trex.emu.trex_emu_profile.EMUNamespaceKey object at 0x7fd59150dba8>,
                    <trex.emu.trex_emu_profile.EMUNamespaceKey object at 0x7fd59150d8d0>
                ]
            }
    """
    import jsonpickle # Import happens only once :)
    return jsonpickle.decode(json.dumps(data))


### Server functions ###
def add(a, b): # for sanity checks
    try:
        return OK(a + b)
    except:
        return ERR(traceback.format_exc())


def check_connectivity():
    return OK()


def native_proxy_init(force = False, *args, **kwargs):
    """Initialize an STL client that will work as a proxy.

    Args:
        force (bool, optional): Create a Native STL Client Proxy even if one is already initiated. Defaults to False.

    Returns:
        Tuple(bool, response): True if there was no exception, False if there was. Second entry is the response/exception. 
    """
    global native_client
    if native_client and not force:
        return ERR('Native Client is already initiated')
    try:
        native_client = STLClient(*args, **kwargs)
        return OK('Native Client initiated')
    except:
        return ERR(traceback.format_exc())


def native_proxy_del():
    """Delete the Native STL Client Proxy.

    Returns:
        Tuple(bool, response): [True, None]
    """
    global native_client
    native_client = None
    return OK()


def emu_proxy_init(force = False, *args, **kwargs):
    """Initialize an Emu client that will work as a proxy.

    Args:
        force (bool, optional): Create a Native Emu Client Proxy even if one is already initiated. Defaults to False.

    Returns:
        Tuple(bool, response): True if there was no exception, False if there was. Second entry is the response/exception. 
    """
    global emu_client
    if emu_client and not force:
        return ERR('Emu Proxy Client is already initiated')
    try:
        emu_client = EMUClient(*args, **kwargs)
        return OK('Emu Proxy Client initiated')
    except:
        return ERR(traceback.format_exc())


def emu_proxy_del():
    """Delete the Emu Proxy Client.

    Returns:
        Tuple(bool, response): [True, None]
    """
    global emu_client
    emu_client = None
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


# any STLClient method not listed above can be called with passing its name here
def native_method(func_name, *args, **kwargs):
    """Call a STL Client Native method.

    Args:
        func_name (string): STL Client Method to call.

    Returns:
        Tuple(bool, response): True if there was no exception, False if there was. Second entry is the response/exception. 
    """
    try:
        func = getattr(native_client, func_name)
        res = func(*deunicode_json(args), **deunicode_json(kwargs))
        if isinstance(res, RC):
            return OK(res.data()) if res else ERR(res.err())
        return OK(res)
    except:
        return ERR(traceback.format_exc())


# any EMUClient method called with passing its name here
def emu_method(func_name, *args, **kwargs):
    """Call a EMUClient method.

    Args:
        func_name (string): EMUClient Method to call.

    Returns:
        Tuple (bool, response): True if there was no exception, False if there was. Second entry is the response/exception.
    """
    try:
        func = getattr(emu_client, func_name)
        res = func(*decode_jsonpickle(args), **decode_jsonpickle(kwargs))
        if isinstance(res, RC):
            return OK(res.data()) if res else ERR(res.err())
        return OK(res)
    except:
        return ERR(traceback.format_exc())


def emu_plugin_method(plugin_name, func_name, *args, **kwargs):
    """Call a EMU Plugin Method.

    Args:
        plugin_name (str): Name of the plugin.
        func_name (str): Name of the method.

    Returns:
        Tuple (bool, response): True if there was no exception, False if there was. Second entry is the response/exception. 
    """
    try:
        plugin = getattr(emu_client, plugin_name)
        func = getattr(plugin, func_name)
        res = func(*decode_jsonpickle(args), **decode_jsonpickle(kwargs))
        if isinstance(res, RC):
            return OK(res.data()) if res else ERR(res.err())
        return OK(res)
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
def run_server(port = 8095, emu = False):
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
    emu_methods = [
                    'connect',
                    'is_connected',
                    'disconnect',
                    'load_profile',
                    'remove_profile',
                    'add_ns',
                    'remove_ns',
                    'add_clients',
                    'remove_clients',
                    'get_counters',
                    'get_server_version'
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
        server.register_function(native_method)
        if emu:
            server.register_function(emu_proxy_init)
            server.register_function(emu_proxy_del)
            server.register_function(emu_method)
            server.register_function(emu_plugin_method)
        server.register_function(hltapi_proxy_init)
        server.register_function(hltapi_proxy_del)
        server.register_function(hltapi_method)

        for method in native_methods:
            server.register_function(partial(native_method, method), method)
        if emu:
            for method in emu_methods:
                server.register_function(partial(emu_method, method), "emu_{}".format(method))
        for method in hltapi_methods:
            if method in native_methods: # collision in names
                method_hlt_name = 'hlt_%s' % method
            else:
                method_hlt_name = method
            server.register_function(partial(hltapi_method, method), method_hlt_name)
        server.register_function(server.funcs.keys, 'get_methods') # should be last
        print('Started Stateless RPC proxy at port {}'.format(port))
        if emu:
            print('Started Emu RPC proxy at port {}'.format(port))
        server.serve_forever()
    except KeyboardInterrupt:
        print('Done')


### Main ###
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Runs TRex Stateless/Emu RPC proxy for usage with any language client.')
    parser.add_argument('-p', '--port', type=int, default = 8095, dest='port', action = 'store',
                        help = 'Select port on which the STL/Emu RPC proxy will run.\nDefault is 8095.')
    parser.add_argument('--emu', action = 'store_true', help = 'Create an Emu RPC proxy also. Warning: This can execute arbitrary Python code. Use at your own risk!!!')
    kwargs = vars(parser.parse_args())
    run_server(**kwargs)
