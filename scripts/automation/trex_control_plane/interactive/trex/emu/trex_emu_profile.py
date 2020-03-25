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

            summary = "\nEmu profile loading failed! Traceback:\n%s" % x
            raise TRexError(summary)
    return pretty_exceptions_inner

class EMUClientObj(object):
    
    def __init__(self, mac, **kwargs):
        """
        Create a new Emu client object. 
        
            :parameters:
                mac: str
                    Mac Address for the client.
        
            :raises:
        """        
        ver_args = {'types':[
                {'name': 'mac', 'arg': mac, 't': 'mac'},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        self.fields = {'mac': mac}
        self.fields.update(kwargs)
    
    def __hash__(self):
        return hash(self.mac)

    def __getattr__(self, name):
        try:
            return self.fields[name]
        except KeyError:
            raise AttributeError('EMUClientObj has no attribute: "%s"' % name)

    def to_json(self):
        """
        Convert client into a JSON format. 
        
            :returns:
                dict: Client's data in a dictionary format.
        """        
        return self.fields

    @property
    def plugs(self):
        try:
            return self.fields['plugs']
        except KeyError:
            return None

class EMUNamespaceObj(object):

    def __init__(self, vport, tci = None, tpid = None, clients = None, plugs = None, def_c_plugs = None):
        """
        Create Emu Namespace object. 
        
            :parameters:
                vport: int
                    Port for the namespace.
                tci: int or list of 2 ints.
                    Tci/s for the namespace (up to 2), defaults to None means no Vlan.
                tpid: int or 2 ints.
                    Tpid/s for the tci/s (up to 2), defaults to None means no vlan.
                    If tci supplied without tpid, it will set tpid to 0x8100.
                clients: EMUClientObj or list of EMUClientObjs
                    Client/s for namespace, defaults to None.
                plugs: dictionary
                    Plugins for namespace, not to be confused with `def_c_plugs` plugs are namespace plugins, defaults to None.
                def_c_plugs: dictionary
                    Default client plugins, defaults to None.
                    Every client in the namespace gets those default plugins, If a client has his own plugs it will be merged with the default one
                    and overide duplicates giving favor to the client.
        
            :raises:
                + :exe:'TRexError': [description]
        """        
        ver_args = {'types':[
                {'name': 'vport', 'arg': vport, 't': int},
                {'name': 'tci', 'arg': tci, 't': int, 'allow_list': True, 'must': False},
                {'name': 'tpid', 'arg': tpid, 't': int, 'allow_list': True, 'must': False},
                {'name': 'clients', 'arg': clients, 't': EMUClientObj, 'allow_list': True, 'must': False},
                {'name': 'def_c_plugs', 'arg': def_c_plugs, 't': dict, 'must': False},
                ]}
        ArgVerify.verify(self.__class__.__name__, ver_args)

        if tpid is not None and tci is None:
            raise TRexError('Cannot supply tpid without tci to namespace!')

        if tpid is not None and tci is not None:
            if len(tpid) != len(tci):
                raise TRexError('tci & tpid length must be equal!')
            elif any([tc == 0 and tp !=0 for tc, tp in zip(tci, tpid)]):
                raise TRexError('Cannot supply tpid to zero vlan tag!')

        self.fields = {'vport': vport, 'tci': tci, 'tpid': tpid, 'plugs': plugs}
        self.mac_map = {}
        self.def_c_plugs = def_c_plugs

        if clients is not None:
            self.add_clients(clients)

    def __getattr__(self, name):
        try:
            return self.fields[name]
        except KeyError:
            raise AttributeError('EMUNamespaceObj has no attribute: "%s"' % name)


    def set_def_c_plugs(self, plugs):
        """
        Set default client plugins. 
        
            :parameters:
                plugs: dict
                    Dictonary with keys as plug names and values as plug info.
        """        
        self.def_c_plugs = plugs

    def add_clients(self, clients):
        """
        Add clients to namespace. 
        
            :parameters:
                clients: EMUClientObj / list of EMUClientObj
                    Clients to add, may be a list or just one.
        """        
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

    def to_json(self):
        """
        Convert namespace to a json. 
                        
            :returns:
                dict: Namespace data in dictionary format 
        """        
        res = {'def_c_plugs': self.def_c_plugs, 'clients': []}
        res.update(self.fields)
        for c in self.mac_map.values():
            res['clients'].append(c.to_json())
        return res

    def get_clients(self):
        return [c.fields for c in self.mac_map.values()]
    
    def conv_to_key(self):
        return self.fields

class EMUProfile(object):

    def __init__(self, ns = None, def_ns_plugs = None):
        """
        Create an Emu profile. 

            :parameters:
                ns: EmuNamespaceObj or list of EmuNamespaceObj
                    The namespace/s to use in profile, defaults to None.
                def_ns_plugs: dict
                    Dictionary with all the default namespace plugins. If a namespace hasn't plugins it will take the default, defaults to None.
        
            :raises:
                + :exe:'TRexError': [description]
        
            :returns:
                EMUProfile: Emu profile object.
        """
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
        """
        Set default namespace plugins (every namespace from this profile will have them by default). 
        
            :parameters:
                plugs: dict
                    Dictonary with keys as plug names and values as plug info.
        """        
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

    def to_json(self):
        """
        Convert profile to a json. 
                
            :returns:
                dict: Namespace data in dictionary format 
        """ 
        res = {'def_ns_plugs': self.def_ns_plugs, 'namespaces': []}
        for ns in self.ns_list:
            res['namespaces'].append(ns.to_json())
        return res

    @classmethod
    def load(cls, filename, tunables):
        """ Load emu profile by its type. Supported type for now is .py

           :Parameters:
              filename  : string as filename 
              tunables  : list of strings, list of tunables data. i.e: ['--clients', '10']
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
        """
        Loads EMU profile from file with tunables, returns None if called only for help.
        
            :parameters:
                filename: string
                    Path to a valid Python file describing emu profile.
                tunables: list of strings
                    List of tunables. i.e: ['--clients', '10']
        
            :raises:
                + :exe:'TRexError': In any case loading profile fails
        
            :returns:
                EMUProfile: EMUProfile object.
        """
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
