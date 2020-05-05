"""
Handles Namespace batch API

Author:
  hhaim

"""

import json
from ..astf.arg_verify import ArgVerify
from ..astf.trex_astf_exceptions import ASTFErrorBadIp
from ..common.trex_exceptions import TRexError
from ..common.trex_types import validate_type, listify

class NSCmd(object):
    def __init__(self):
        self.method=None
        self.parames =None

    def get_json(self):
        cmd = {'method' : self.method,
               'params' : self.parames }
        return cmd

    def get_json_str (self):
        s= json.dumps(self.get_json(), separators=(',', ': '))
        return (s);


class NSCmds(object):
    """
    namespace commands  builder
    """
    def __init__(self):
        self.cmds =[]

    def add_cmd_obj (self,cmd):
        self.cmds.append(cmd)

    def add_cmd (self,method,**arg):
        cmd = NSCmd()
        cmd.method = method
        cmd.parames = arg
        self.cmds.append(cmd)

    def clone (self):
        return copy.deepcopy(self)

    # add commands
    def add_node(self,mac, is_bird = False, shared_ns = None):
        ''' add new virtual interface and it's namespace, in shared_ns and bird cases only virtual interface will be created

            :parameters:

            mac: string
                MAC address in the format of xx:xx:xx:xx:xx:xx

            is_bird: bool
                True if the new node will be a bird node. Notice it's mutually exclusive with shared_ns

            shared_ns: string
                The name of the shared namespace to paired with. Notice it's mutually exclusive with is_bird
        '''

        ver_args = {"types":
                    [{'name': "mac", 'arg': mac, 't': "mac"},
                    {'name': "is_bird", 'arg': is_bird, 't': bool}]
                   }
        args = {'mac': mac, 'is_bird': is_bird}
        if shared_ns is not None:
            ver_args['types'].append({'name': "shared_ns", 'arg': shared_ns, 't': str})
            args['shared_ns'] = shared_ns
        ArgVerify.verify(self.__class__.__name__, ver_args)
        
        if is_bird and shared_ns is not None:
            raise TRexError("Cannot add node with bird option and shared namespace!")

        self.add_cmd ('add_node', **args)

    def add_shared_ns(self):
        '''
            simply adding new namespace to TRex with no veth.
            return the name of the new namespace.
        '''
        self.add_cmd ('add_shared_ns')

    def remove_node (self,mac):
        ''' remove namespace 

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx


        '''

        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('remove_node',mac=mac)

    def remove_shared_ns(self, shared_ns):
        '''
        remove shared namespace.

        parameters:

            shared_ns: string
                The name of the namespace to delete

        '''
        ver_args = {"types":
                    [{"name": "shared_ns", 'arg': shared_ns, "t": str}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('remove_shared_ns', shared_ns = shared_ns)

    # set commands
    def set_dg(self, shared_ns, dg):
        '''
        set or change default gateway for the whole namespace. works only on shared_ns nodes.

        parameters:

            shared_ns: string
                The name of the pre-created namespace

            dg: string
                The new default gateway to set
        '''
        ver_args = {"types":
                    [{"name": "shared_ns", 'arg': shared_ns, "t": str},
                    {"name": "dg", 'arg': dg, "t": "ip address"}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_dg', shared_ns = shared_ns, dg = dg)

    def set_vlan(self, mac, vlans, tpids = None):
        ''' add/remove QinQ and Dot1Q. could be up to 2 tags

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx

            vlans: list
                Array of up to 2 uint16 tags. In case of empty remove the vlans

            tpids: list
                | Array of tpids that correspond to vlans.
                | Default is [0x8100] in case of single VLAN and [0x88a8, 0x8100] in case of QinQ

        '''

        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "vlans", 'arg': vlans, "t": int, 'allow_list': True},
                     {"name": "tpids", 'arg': tpids, "t": int, 'allow_list': True, 'must': False}
                     ]}
        vlans = listify(vlans)
        if tpids is not None:
            tpids = listify(tpids)
            if len(tpids) != len(vlans):
                raise TRexError('Size of vlan tags %s must match tpids %s' % (vlans, tpids))

        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_vlans', mac=mac, vlans=vlans, tpids=tpids)

    def set_ipv4(self, mac, ipv4, dg = None, subnet = None, shared_ns = False):
        ''' set or change ipv4 configuration 

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx

            ipv4: string
                IPv4 self address

            dg: string
                Default gateway. Using only when shared_ns is set to False.

            subnet: int
                Subnet mask of the ipv4 address. Using only when shared_ns is set to True. 

            shared_ns: bool
                True or False if the node it's bird node or shared_ns node. Notice shared ns nodes need to supply 
                subnet instead of default gateway.
        '''

        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "ipv4", 'arg': ipv4, "t": "ip address"},
                     {"name": "shared_ns", 'arg': shared_ns, "t": bool}]
                     }
        cmd_args = {'mac': mac, 'ipv4': ipv4, 'shared_ns': shared_ns}
  
        if shared_ns:
            if subnet is None:
                raise TRexError('Must specify subnet for shared namespace!')
            ver_args['types'].append({"name": "subnet", 'arg': subnet, "t": int})
            cmd_args['subnet'] = subnet
            cmd_args['shared_ns'] = True
        else:
            if dg is None:
                raise TRexError('Must specify default gateway!')
            if subnet is not None:
                raise TRexError('Must NOT specify subnet for non shared namespace!')
            ver_args['types'].append({"name": "dg", 'arg': dg, "t": "ip address"})
            cmd_args['dg'] = dg
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd('set_ipv4', **cmd_args)

    def set_vlan_filter(self, mac, is_no_tcp_udp = True, is_tcp_udp = False):
        ''' 
            Set vlan filter mask according to "trex_vlan_filter.h" file.

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx

            is_no_tcp_udp: bool
                True if the new filter should pass no_tcp_udp traffic i.e ICMP.
            
            is_tcp_udp: bool
                True if the new filter should pass tcp_udp traffic i.e BGP.                
        '''
        ver_args = {"types":
                    [{'name': "mac", 'arg': mac, 't': 'mac'},
                     {'name': "is_no_tcp_udp", 'arg': is_no_tcp_udp, 't': bool},
                     {'name': "is_no_tcp_udp", 'arg': is_tcp_udp, 't': bool}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        
        vlan_filter_mask = 1 if is_no_tcp_udp else 0
        vlan_filter_mask = vlan_filter_mask | 2 if is_tcp_udp else vlan_filter_mask

        cmd_args = {'mac': mac, 'filter_mask': vlan_filter_mask}
        self.add_cmd('set_vlan_filter', **cmd_args)
    
    def set_filter(self, mac, bpf_filter):
        ''' 
            set or change bpf filter. Warning - bad filter might crash TRex!

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx

            bpf_filter: string
                Valid bfp filter
        '''
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "filter", 'arg': bpf_filter, "t": str}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        cmd_args = {'mac': mac, 'filter': bpf_filter}
        self.add_cmd('set_filter', **cmd_args)

    def set_ipv6(self, mac, enable, src_ipv6 = None, subnet = None, shared_ns = False):
        ''' set ns ipv6 

            :parameters:

                mac: string
                    Key to the already created namespace in format xx:xx:xx:xx:xx:xx

                enable : bool 
                    Enable ipv6.

                src_ipv6: None for auto, or ipv6 addr 

                subnet: int 
                    Subnet mask of the ipv6 address. Using only when shared_ns is set to True. 

                shared_ns: bool
                    True or False if the node it's bird node or shared_ns node. Notice shared ns nodes need to supply 
                    subnet.                
        '''
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "enable", 'arg': enable, "t": bool},
                     {"name": "shared_ns", 'arg': shared_ns, "t": bool}]
                     }
        cmd_params = {"mac": mac, "enable": enable}

        if shared_ns:
            if not (( src_ipv6 is None and subnet is None ) or ( src_ipv6 is not None and subnet is not None )):
                raise TRexError('Must specify ipv6 address & subnet or none of them for shared ns node!')
            else:
                ver_args['types'].append({"name": "subnet", 'arg': subnet, 'must': False, "t": int})
                ver_args['types'].append({"name": "src_ipv6", 'arg': src_ipv6, 'must': False, "t": "ipv6_addr"})
                if subnet is not None:
                    cmd_params["subnet"] = subnet
                cmd_params['shared_ns'] = True


        ArgVerify.verify(self.__class__.__name__, ver_args)
        if src_ipv6 is None:
            src_ipv6 = ""
        else:
            if not ArgVerify.verify_ipv6(src_ipv6):
                raise ASTFErrorBadIp('set_ipv6', '', src_ipv6)
        cmd_params["src_ipv6"] = src_ipv6
        self.add_cmd ('set_ipv6', **cmd_params)

    def set_mtu(self, mac, mtu):
        ''' Set mtu for a given mac

            :parameters:
                
                mac: string
                    Key to the already created namespace in format xx:xx:xx:xx:xx:xx

                mtu: int
                    The new mtu to set for the node with that mac
        '''
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                    {"name": "mtu", 'arg': mtu, "t": int}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_mtu', mac = mac, mtu = mtu)     

    def clear_ipv4(self,mac):
        ''' remove ipv4 configuration from the ns

            :parameters:
               None

        '''
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('clear_ipv4',mac=mac)
    
    def remove_all(self):
        ''' remove all namespace nodes '''
        self.add_cmd('remove_all')

    # get commands
    def get_nodes(self, only_bird = False):
        '''
          get all nodes macs (keys)
        '''
        self.add_cmd ('get_nodes', only_bird = only_bird)

    def get_nodes_info (self,macs_list):
        """ provide list of macs return alist of objects with each namepace information"""
        ver_args = {"types":
                    [{"name": "macs_list", 'arg': macs_list, "t": list},
                     ]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('get_nodes_info',macs= macs_list)

    def clear_counters(self):
        ''' clear debug counters. these counters are *global* to all users in the system. '''
        self.add_cmd ('counters_clear')

    def counters_get_meta(self):
        ''' get the counters description as dict  '''
        self.add_cmd ('counters_get_meta')

    def counters_get_values(self,zeros=False):
        ''' get the values of the counters, zeros: in case of false skip counters with zero value  '''
        ver_args = {"types":
                    [{"name": "zeros", 'arg': zeros, "t": bool},
                     ]
                     }

        self.add_cmd ('counters_get_value',zeros=zeros)

    def __rpc_help (self):
        """ this is for debug """
        self.add_cmd ('rpc_help',mac='00:01:02:03:04:05',
                                ipv4='10.0.0.14',ipv4_dg='10.0.0.1')

    def get_commands_list (self):
        """ return the list of the commands """
        self.add_cmd ('get_commands')

    def get_json (self):
        l=[]
        for obj in self.cmds:
            l.append(obj.get_json());
        return (l)

    def get_json_str (self):
        s= json.dumps(self.get_json(), separators=(',', ': '))
        return s


class NSCmdResult(object):
    '''

    namespace batch commands results helper object. 

    res = c.wait_for_async_results(0);

    if res.is_any_error():
        handle error 

    print(res.data)

    '''
    def __init__(self,data):
        self.data = data

    def is_any_error (self):
        """ do we have any error in batch response  """
        for obj in self.data:
            if obj is not None and ('error' in obj):
                return True
        return False

    def errors(self):
        """ in case we have an error, get list of all errors   """
        res=[]
        for obj in self.data:
            if obj is not None and ('error' in obj):
                res.append(obj['error'])
        return (res);




