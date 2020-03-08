from ..astf.arg_verify import ArgVerify
from ..common.trex_exceptions import *
from ..utils.common import mac_str_to_num, increase_mac, int2mac
from scapy.utils import *

import imp
import os
import sys
import traceback

def pretty_exceptions(func):
    def pretty_exceptions_inner(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            a, b, tb = sys.exc_info()
            x =''.join(traceback.format_list(traceback.extract_tb(tb)[1:])) + a.__name__ + ": " + str(b) + "\n"

            summary = "\nPython Traceback follows:\n\n" + x
            raise TRexError(summary)
    return pretty_exceptions_inner

class EMUClientObj(object):
    
    def __init__(self, mac, **kwargs):
        
        ver_args = {'types':[
                {'name': 'mac', 'arg': mac, 't': 'mac'},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {'mac': mac}
        self.fields.update(kwargs)
    
    def __hash__(self):
        return hash(self.mac)

    def __getattr__(self, name):
        if name in self.fields:
            return self.fields[name]
        else:
            raise AttributeError

class EMUNamespaceObj(object):

    def __init__(self, vport, tci = None, tpid = None, clients = None, plugs = None, def_c_plugs = None):
        ver_args = {'types':[
                {'name': 'vport', 'arg': vport, 't': int},
                {'name': 'tci', 'arg': tci, 't': int, 'allow_list': True, 'must': False},
                {'name': 'tpid', 'arg': tpid, 't': int, 'allow_list': True, 'must': False},
                {'name': 'def_c_plugs', 'arg': def_c_plugs, 't': dict, 'must': False},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)
        
        self.fields = {'vport': vport, 'tci': tci, 'tpid': tpid, 'plugs': plugs}
        self.mac_map = {}
        self.def_c_plugs = def_c_plugs

        if clients is not None:
            self.add_clients(clients)

    def __getattr__(self, name):
        if name in self.fields:
            return self.fields[name]
        else:
            raise AttributeError


    def set_def_c_plugs(self, plugs):
        self.def_c_plugs = plugs

    def add_clients(self, clients):
        ver_args = {'types':[
                {'name': 'clients', 'arg': clients, 't': EMUClientObj, 'allow_list': True},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        try:
            clients_it = iter(clients)
            for client in clients_it:
                self._add_one_client(client)
        except TypeError:
            # clients is not a list
            self._add_one_client(clients)

    def _add_one_client(self, client):
        c_mac = client.mac
        if c_mac in self.mac_map:
            raise TRexError("Namespace already has a client with mac: %s" % c_mac)
        
        if client.plugs is not None:
            for k in self.def_c_plugs.keys():
                # adding from default only plugins that aren't already in client
                if k not in client.plugs:  
                    client.plugs[k] = self.def_c_plugs[k]
        self.mac_map[c_mac] = client


    def get_clients(self):
        return [c.fields for c in self.mac_map.values()]
    
    def conv_to_key(self):
        return self.fields

class EMUProfile(object):

    def __init__(self, ns = None, def_ns_plugs = None):
        ver_args = {'types':[
                {'name': 'ns', 'arg': ns, 't': EMUNamespaceObj, 'allow_list': True, 'must': False},
                {'name': 'def_ns_plugs', 'arg': def_ns_plugs, 't': dict, 'must': False},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.ns_list = []
        self.def_ns_plugs = def_ns_plugs

        if ns is not None:
            self.add_ns(ns)

    def set_def_ns_plugs(self, plugs):
        self.def_ns_plugs = plugs

    def add_ns(self, ns):
        try:
            for one_ns in ns:
                self._add_one_ns(one_ns)
        except TypeError:
            self._add_one_ns(ns)

    def _add_one_ns(self, ns):
        # adding to each ns with plugins, the default ctx plugins
        if ns.plugs is not None:
            for k in self.def_ns_plugs:
                if k not in ns.plugs:
                    ns.plugs[k] = self.def_ns_plugs[k]
        
        self.ns_list.append(ns)

    def serialize_ns(self):
        """ Return a list with all the ns dictionaries serialized for transmition """
        return [ns.fields for ns in self.ns_list]

    @classmethod
    def load(cls, filename, tunables):
        """ Load emu profile by its type. Supported type for now is .py

           :Parameters:
              filename  : string as filename 
              kwargs    : forward those key-value pairs to the profile
        """

        x = os.path.basename(filename).split('.')
        suffix = x[1] if (len(x) == 2) else None

        if suffix == 'py':
            profile = cls.load_py(filename, tunables)
        else:
            raise TRexError("unknown profile file type: '{0}'".format(suffix))

        return profile
    
    @classmethod
    @pretty_exceptions
    def load_py(cls, filename, tunables):
        """ Loads EMU profile from file with tunables, returns None if called only for help """

        # check filename
        if not os.path.isfile(filename):
            raise TRexError("File '{0}' does not exist".format(filename))

        basedir = os.path.dirname(filename)
        sys.path.insert(0, basedir)

        try:
            file = os.path.basename(filename).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module)  # reload the update 

            try:
                profile = module.register().get_profile(tunables)
            except SystemExit:
                # called ".. -t --help", return None
                return None
            profile.meta = {'type': 'python',
                            'tunables': tunables}
            return profile
        finally:
            sys.path.remove(basedir)
