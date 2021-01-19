from ..common.trex_exceptions import *
from scapy.utils import *
from trex.emu.trex_emu_conversions import *
from trex.common.trex_types import listify
from .trex_emu_validator import EMUValidator

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


class EMUNamespaceKey(object):

    def __init__(self, vport, tci = None, tpid = None, **kwargs):
        """
        Creates a namespace key, defines a namespace in emulation server. 

            .. code-block:: python

                # creating a namespace key with no vlans
                ns_key = EMUNamespaceKey(vport = 0)

                # creating a namespace key with vlan using default tpid(0x8100)
                ns_key = EMUNamespaceKey(vport = 0, tci = 1)

                # creating a namespace key with 2 vlans using tpid
                ns_key = EMUNamespaceKey(vport = 0, tci = [1, 2], tpid = [0x8100, 0x8100])
            
            :parameters:
                vport: int (Mandatory)
                    Port for the namespace.
                tci: int or list of 2 ints. (Optional)
                    Tag Control Information/s for the namespace (up to 2), defaults to None means no Vlan.
                tpid: int or 2 ints. (Optional)
                    Tag Protocol Identifier/s for the tci/s (up to 2), defaults to None means no vlan.
                    If tci supplied without tpid, it will set tpid to 0x8100 by default.

            :raises:
                + :exe:'TRexError': In any case of wrong parameters.
        """
        if tci is None:
            tci = [0, 0]
        tci = listify(tci)

        if tpid is None:
            tpid = [0 for _ in tci]
        tpid = listify(tpid)

        ver_args = [
            {'name': 'vport', 'arg': vport, 't': 'vport'},
            {'name': 'tci', 'arg': tci, 't': 'tci'},
            {'name': 'tpid', 'arg': tpid, 't': 'tpid'},
        ]
        EMUValidator.verify(ver_args)

        if tpid != [0, 0] and tci == [0, 0]:
            raise TRexError('Cannot supply tpid without tci to namespace!')

        if tpid != [0, 0] and tci != [0, 0]:
            if len(tpid) != len(tci):
                raise TRexError('tci & tpid length must be equal!')
            elif any([tc == 0 and tp !=0 for tc, tp in zip(tci, tpid)]):
                raise TRexError('Cannot supply tpid with non zero value to zero value vlan tag!')

        self.vport = vport
        self.tci   = tci
        self.tpid  = tpid

    def conv_to_dict(self, add_tunnel_key = False):
        """
        Convert ns key to dictionary. 
        
            :parameters:
                add_tunnel_key: bool
                    True will add the tunnel key (ready for sending over RPC), defaults to False.
        
            :returns:
                dict: dictionary with ns key parameters
        """
        if add_tunnel_key:
            return {'tun': {'vport': self.vport, 'tci': self.tci, 'tpid': self.tpid}}
        else:
            return {'vport': self.vport, 'tci': self.tci, 'tpid': self.tpid} 

class EMUClientKey(object):
    
    def __init__(self, ns_key, mac):
        """
        Creating client key for identification clients in emu. 
        
            .. code-block:: python

                mac    = Mac('00:00:00:70:00:01')       # creating a Mac obj
                ns_key = EMUNamespaceKey(vport = 0)     # creating ns_key with no vlans
                c_key  = EMUClientKey(ns_key, mac.V())  # creating client key, notice mac converted to list of bytes

            :parameters:
                ns_key: EMUNamespaceKey
                    Namespace key, see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
                
                mac: list of bytes
                    Mac address of the client. Use Mac object to convert from string to list of bytes see :class:`trex.emu.trex_emu_conversions.Mac`
        """
        ver_args = [
            {'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
            {'name': 'mac', 'arg': mac, 't': 'mac',},
        ]
        EMUValidator.verify(ver_args)
        self.ns_key = ns_key
        self.mac    = Mac(mac)

    def conv_to_dict(self, add_ns = False, to_bytes = False):
        """
        Convert the key to a dictionary. 
        
            :parameters:
                add_ns: bool
                    True will add the ns key as well to the dict, defaults to False.
                
                to_bytes: bool
                    True will convert values to bytes, defaults to False.
                
            :returns:
                dict: dictionary representing the client key
        """        
        if add_ns:
            res = self.ns_key.conv_to_dict(True)
        else:
            res = {}
        if to_bytes:
            res.update({'mac': self.mac.V()})
        else:
            res.update({'mac': self.mac.S()})
        return res


class EMUClientObj(object):

    def __init__(self, mac, **kwargs):
        """

        Create a new Emu client object. 

            .. code-block:: python

                # creating a simple client
                mac = Mac('00:00:00:70:00:01)  # creating Mac obj
                EMUClientObj(mac.V())          # converting mac to list of bytes

                # creating a client with parameters
                ipv4, ipv4_dg = Ipv4('1.1.1.3'), Ipv4('1.1.1.1')
                ipv6 = Ipv6('::1234')
                
                # kwargs keys must be identical to the ones in our docs, see kwargs description.
                kwargs = {
                    'ipv4': ipv4.V(),        # converting every field using .V()
                    'ipv4_dg': ipv4_dg.V(),
                    'ipv6': ipv6.V(),
                    'plugs': {'arp': {}}     # client's plugin
                }
                EMUClientObj(mac.V(), ipv4 = ipv4.V(), **kwargs)  
            
            :parameters:
                mac: list of bytes
                    Mac address for a client. see :class:`trex.emu.trex_emu_conversions.Mac`
                
                kwargs: dict
                    Additional parameters for client, besides the mac. Those may be found in emu docs under `CClientCmd struct <https://trex-tgn.cisco.com/trex/doc/trex_emu.html#_emu_client>`_
                    
                    Here are some of the useful ones:
                
                    +------------+--------------------+---------------------------------------------------------------------+
                    |    Key     |       Type         |                          Description                                |
                    +============+====================+=====================================================================+
                    | ipv4       | List of 4 bytes    | IPv4 address of client                                              |
                    +------------+--------------------+---------------------------------------------------------------------+
                    | ipv4_dg    | List of 4 bytes    | IPv4 address of the default gateway                                 |
                    +------------+--------------------+---------------------------------------------------------------------+
                    | ipv6       | List of 16 bytes   | IPv6 address of client                                              |
                    +------------+--------------------+---------------------------------------------------------------------+
                    | dg_ipv6    | List of 16 bytes   | IPv6 address of the default gateway                                 |
                    +------------+--------------------+---------------------------------------------------------------------+
                    | plugs      | Dictionary         | Dict with client's plugins. keys as plug names, value as plug info. |
                    +------------+--------------------+---------------------------------------------------------------------+

        """
        ver_args = [{'name': 'mac', 'arg': mac, 't': 'mac'},]
        EMUValidator.verify(ver_args)

        self.fields = {'mac': Mac(mac)}
        for k,v in kwargs.items():
            self.fields[k] = EMUTypeBuilder.build_type(k, v)

    def __hash__(self):
        return self.mac.num_val

    def __getattr__(self, name):
        try:
            return self.fields[name]
        except KeyError:
            raise AttributeError('EMUClientObj has no attribute: "%s"' % name)

    def to_json(self):
        """
        Convert client into a JSON format. 
        
            :returns:
                | dict: Client's data in a dictionary format.
                | i.e: {'mac': '00:00:00:70:00:01',
                |   'ipv4': '1.1.1.3',
                |   'ipv4_dg: '1.1.1.1'
                | }
        """
        return self.get_fields(to_bytes = False)

    def get_fields(self, to_bytes = False):
        """
        Get client's inner fields. 

            :parameters:
                to_bytes: bool
                    True will convert fields into list of bytes, False to strings. Defaults to False.
                
            :returns:
                | dict: Dictionary represents the client's fields.
                | i.e: client.get_fields(to_bytes = False) ->
                | { 'mac': '00:00:00:70:00:01',
                |   'ipv4': '1.1.1.3',
                |   'ipv4_dg: '1.1.1.1'
                | }
        """        
        res = {}
        for k,v in self.fields.items():
            if isinstance(v, EMUType):
                if to_bytes:
                    res[k] = v.V()
                else:
                    res[k] = v.S()
            else:
                res[k] = v
        return res

    @property
    def plugs(self):
        try:
            return self.fields['plugs']
        except KeyError:
            return None

class EMUNamespaceObj(object):

    def __init__(self, ns_key, clients = None, plugs = None, def_c_plugs = None):
        """
        Create Emu Namespace object.
            
            .. code-block:: python

                # creating a namespace key with no vlans
                ns_key = EMUNamespaceKey(vport = 0)

                # plugs for namespace
                plugs = {'igmp': {}}

                # default plugins for each client in the namespace
                def_c_plugs = {'arp': {}}

                # creating a simple client
                mac = Mac('00:00:00:70:00:01)   # creating Mac obj
                kwargs = {
                    'ipv4': [1, 1, 1, 3], 'ipv4_dg': [1, 1, 1, 1],
                    'plugs': {'icmp': {}}
                }
                client = EMUClientObj(mac.V(), **kwargs)  # converting mac to list of bytes

                # creating the namespace with 1 client
                ns = EMUNamespaceObj(ns_key = ns_key, clients = [client], plugs = plugs, def_c_plugs = def_c_plugs)

                # Summary: ns obj will have igmp, 1 client with icmp plugin and arp plugin he got by default from ns

            :parameters:
                ns_key: EMUNamespaceKey
                    Namespace key identification.
                
                clients: EMUClientObj or list of EMUClientObj
                    Client/s for namespace, defaults to None.
                
                plugs: dictionary
                    Plugins for namespace, keys as plug names values as plug indfo. Not to be confused with `def_c_plugs` plugs are namespace plugins, defaults to None.
                
                def_c_plugs: dictionary
                    Default client plugins, defaults to None.
                    Every client in the namespace gets those default plugins, If a client has his own plugs it will be merged with the default one
                    and overide duplicates giving favor to the client.
        """
        if def_c_plugs is  None:
            def_c_plugs = {}

        if plugs is  None:
            plugs = {}

        ver_args = [
                {'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey},
                {'name': 'clients', 'arg': clients, 't': EMUClientObj, 'allow_list': True, 'must': False},
                {'name': 'def_c_plugs', 'arg': def_c_plugs, 't': dict},
                {'name': 'plugs', 'arg': plugs, 't': dict},
        ]
        EMUValidator.verify(ver_args)

        self.key = ns_key
        self.fields = ns_key.conv_to_dict()
        self.fields['plugs'] = plugs
        self.fields['def_c_plugs'] = def_c_plugs
        self.c_map = {}

        if clients is not None:
            self.add_clients(clients)

    def __getattr__(self, name):
        try:
            return self.fields[name]
        except KeyError:
            raise AttributeError('EMUNamespaceObj has no attribute: "%s"' % name)

    def add_clients(self, clients):
        """
        Add clients to namespace. 
        
            :parameters:
                clients: EMUClientObj / list of EMUClientObj
                    Clients to add, may be a list or just one.
            
            :raises:
                + :exe:'TRexError': If client with the same mac address is already in namespace.

        """
        ver_args = [
            {'name': 'clients', 'arg': clients, 't': EMUClientObj, 'allow_list': True},
        ]
        EMUValidator.verify(ver_args)

        try:
            clients_it = iter(clients)
            for client in clients_it:
                self._add_one_client(client)
        except TypeError:
            # clients is not a list
            self._add_one_client(clients)

    def _add_one_client(self, client):
        if client in self.c_map:
            raise TRexError("Namespace already has a client with same key: %s" % client.key)
        
        if client.plugs is not None:
            for k in self.def_c_plugs.keys():
                # adding from default only plugins that aren't already in client
                if k not in client.plugs:
                    client.plugs[k] = self.def_c_plugs[k]
        self.c_map[client] = client

    def to_json(self):
        """
        Convert namespace to a json. 
                        
            :returns:
                | dict: Namespace data in dictionary format
                | i.e: {
                |     'def_c_plugs': {},
                |     'clients': [{'mac': '00:00:00:70:00:01',
                |                 'ipv4': '1.1.1.3',
                |                 'ipv4_dg: '1.1.1.1'
                |     }],
                |     'plugs': {'arp': {}},
                | }
        """
        res = {'def_c_plugs': self.def_c_plugs, 'clients': []}
        res.update(self.fields)
        for c in self.c_map.values():
            res['clients'].append(c.to_json())
        return res

    def get_fields(self):
        """
        Get namespace fields. 
        
            :returns:
                | dict: Dictionary with all namespace fields.
                | i.e: {
                |   'vport': 0, 'tci': [0, 0], 'tpid': [0, 0], 'plugs': {'igmp': {}} 
                | }
        """        
        return self.fields

class EMUProfile(object):

    def __init__(self, ns = None, def_ns_plugs = None):
        """
        Create an Emu profile. 

            .. code-block:: python

                # creating a namespace key with no vlans
                ns_key = EMUNamespaceKey(vport = 0)

                # plugs for namespace
                plugs = {'igmp': {}}

                # default plugins for each client in the namespace
                def_c_plugs = {'arp': {}}

                # creating a simple client
                mac = Mac('00:00:00:70:00:01)   # creating Mac obj
                kwargs = {
                    'ipv4': [1, 1, 1, 3], 'ipv4_dg': [1, 1, 1, 1],
                    'plugs': {'icmp': {}}
                }
                client = EMUClientObj(mac.V(), **kwargs)  # converting mac to list of bytes

                # creating the namespace with 1 client
                ns = EMUNamespaceObj(ns_key = ns_key, clients = [client], plugs = plugs, def_c_plugs = def_c_plugs)

                # every ns in the profile will have by default ipv6 plugin
                def_ns_plugs = {'ipv6': {}}

                # creating the profile and send it
                profile = EMUProfile(ns = ns, def_ns_plugs = def_ns_plugs)
                emu_client.load_profile(profile = profile)  # using EMUClient

            :parameters:
                ns: EmuNamespaceObj or list of EmuNamespaceObj
                    The namespace/s to use in profile, defaults to None.
                
                def_ns_plugs: dict
                    Dictionary with all the default namespace plugins, keys as plugs names and values as plugs info. If a namespace hasn't plugins it will take the default, defaults to None.
        
            :raises:
                + :exe:'TRexError': In any case of wrong parameters.
        
            :returns:
                EMUProfile: Emu profile object, ready to send.
        """
        ver_args = [
                {'name': 'ns', 'arg': ns, 't': EMUNamespaceObj, 'allow_list': True, 'must': False},
                {'name': 'def_ns_plugs', 'arg': def_ns_plugs, 't': dict, 'must': False},
        ]
        EMUValidator.verify(ver_args)

        self.ns_list = []
        if def_ns_plugs is not None:
            self.def_ns_plugs = def_ns_plugs
        else:
            self.def_ns_plugs = {}

        if ns is not None:
            self.add_ns(ns)

    def add_ns(self, ns):
        ver_args = [
            {'name': 'ns', 'arg': ns, 't': EMUNamespaceObj, 'allow_list': True},
        ]
        EMUValidator.verify(ver_args)
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

    def to_json(self):
        """
        Convert profile to a json. 
                
            :returns:
                | dict: Profile data in dictionary format.
                | i.e: {
                |    'def_ns_plugs': {'igmp': {}},
                |    'namespaces': [
                |     {
                |       'def_c_plugs': {'icmp': {}},
                |       'clients': [{'mac': '00:00:00:70:00:01',
                |                     'ipv4': '1.1.1.3',
                |                     'ipv4_dg: '1.1.1.1'
                |       }],
                |       'plugs': {'arp': {}},
                |     }]
                | }
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
        ver_args = [
            {'name': 'filename', 'arg': filename, 't': str},
            {'name': 'tunables', 'arg': tunables, 't': 'tunables'},
        ]
        EMUValidator.verify(ver_args)
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
