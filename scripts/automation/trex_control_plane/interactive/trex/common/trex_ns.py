"""
Handles Namespace batch API

Author:
  hhaim

"""

import json
from ..astf.arg_verify import ArgVerify
from ..astf.trex_astf_exceptions import ASTFErrorBadIp
from ..common.trex_exceptions import TRexError
from ..common.trex_types import validate_type

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
    def add_node(self,mac):
        ''' add new namespace 

            :parameters:

            mac: string
                MAC address in the format of xx:xx:xx:xx:xx:xx

        '''

        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('add_node',mac=mac)

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
                     {"name": "vlans", 'arg': vlans, "t": list}]
                     }

        if tpids is not None:
            validate_type('tpids', tpids, list)
            if len(tpids) != len(vlans):
                raise TRexError('Size of vlan tags %s must match tpids %s' % (vlans, tpids))

        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_vlans', mac=mac, vlans=vlans, tpids=tpids)

    def set_ipv4(self,mac,ipv4,dg):
        ''' set or change ipv4 configuration 

            :parameters:

            mac: string
                Key to the already created namespace in format xx:xx:xx:xx:xx:xx

            ipv4: string
                IPv4 self address

            dg: string
                Default gateway

        '''

        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "ipv4", 'arg': ipv4, "t": "ip address"},
                     {"name": "dg", 'arg': dg, "t": "ip address"}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_ipv4',mac=mac,ipv4=ipv4,dg=dg)

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

    def set_ipv6(self,mac,enable,src_ipv6=None):
        ''' set ns ipv6 

            :parameters:
                enable : bool 
                    enable ipv6 

                src_ipv6: None for auto, or ipv6 addr 

        '''
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "enable", 'arg': enable, "t": bool}
                     ]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        if src_ipv6 == None:
            src_ipv6 = ""
        else:
            if not ArgVerify.verify_ipv6(src_ipv6):
                raise ASTFErrorBadIp('set_ipv6', '', src_ipv6)

        self.add_cmd ('set_ipv6',mac=mac,enable=enable,src_ipv6=src_ipv6)

    def remove_all(self):
        ''' remove all namespace nodes '''
        self.add_cmd ('remove_all')

    def get_nodes(self):
        '''
          get all nodes macs (keys)
        '''
        self.add_cmd ('get_nodes')

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




