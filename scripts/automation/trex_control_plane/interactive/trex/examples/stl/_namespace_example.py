import stl_path
from trex.stl.api import *
from trex.utils.text_opts import format_text
from trex.astf.arg_verify import ArgVerify
import pprint
import time
from trex.astf.trex_astf_exceptions import  ASTFErrorBadIp

# let's see that this configuration works 
def ns_json_to_string (cnt):
    cmd = {'method' : 'rpc_help',
           'params' : {'mac' :'00:01:02:03:04:05',
                       'ipv4' :'10.0.0.14',
                       'ipv4_dg' : '10.0.0.1' } }
    cmds =[]
    for i in range(cnt):
       cmds.append(cmd)
    s= json.dumps(cmds, separators=(',', ': '))
    return s


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
    namespace commands
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
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('add_node',mac=mac)

    def remove_node (self,mac):
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                   }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('remove_node',mac=mac)

    def set_vlan(self,mac,vlans):
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "vlans", 'arg': vlans, "t": list}
                     ]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_vlans',mac=mac,vlans=vlans)

    def set_ipv4(self,mac,ipv4,dg):
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"},
                     {"name": "ipv4", 'arg': ipv4, "t": "ip address"},
                     {"name": "dg", 'arg': dg, "t": "ip address"}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('set_ipv4',mac=mac,ipv4=ipv4,dg=dg)

    def clear_ipv4(self,mac):
        ver_args = {"types":
                    [{"name": "mac", 'arg': mac, "t": "mac"}]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('clear_ipv4',mac=mac)

    def set_ipv6(self,mac,enable,src_ipv6=None):
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
        self.add_cmd ('remove_all')

    def get_nodes(self):
        """
          get mac list for all objects
        """
        self.add_cmd ('get_nodes')

    def get_nodes_info (self,macs_list):
        """ provide list of macs return dict with object for each mac """
        ver_args = {"types":
                    [{"name": "macs_list", 'arg': macs_list, "t": list},
                     ]
                     }
        ArgVerify.verify(self.__class__.__name__, ver_args)
        self.add_cmd ('get_nodes_info',macs= macs_list)


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

    def __init__(self,data):
        self.data = data

    def is_any_error (self):
        for obj in self.data:
            if obj is not None and ('error' in obj):
                return True
        return False

    def errors(self):
        res=[]
        for obj in self.data:
            if obj is not None and ('error' in obj):
                res.append(obj['error'])
        return (res);



def simple (json_cmd_list):

    # create client
    #verbose_level = 'debug','error'
    c = STLClient(verbose_level = 'error',server='csi-trex-17')
    passed = True
    
    c.connect()

    my_ports=[0,1]
    c.reset(ports = my_ports)

    c.set_service_mode (ports = my_ports, enabled = True)
    # json to string
    cnt=0
    for cmd in json_cmd_list:
       json_rpc=cmd
       print("cmd {}".format(cnt))
       res=c.set_namespace_start( 0, json_rpc)
       res = c.wait_for_async_results(0);
       print(res);
       cnt +=1

    #while True:
      #if c.is_async_results_ready(0):
      #    print(c.wait_for_async_results(0))
      #    break;
      #else:
      #    print(" not ready !\n")

      #time.sleep(1)



    #while True:
    #  res = c.get_async_results(0)
    #  print(res);
    #  time.sleep(1);
    #  cnt += 1
    #  if cnt > 30:
    #      break

    #pprint.pprint(res);

    c.set_service_mode (ports = my_ports, enabled = False)

    c.disconnect()


def test1 ():
    cmds=NSCmds()
    cmds.add_node("00:01:02:03:04:05")
    #cmds.set_vlan("00:01:02:03:04:05",[123,123])
    #cmds.set_vlan("00:01:02:03:04:05",[1])
    cmds.set_ipv4("00:01:02:03:04:05","1.1.1.3","1.1.1.2")

    cmds.add_node("00:01:02:03:04:06")
    cmds.set_ipv4("00:01:02:03:04:06","10.0.0.2","10.0.0.4")
    cmds.set_ipv6("00:01:02:03:04:06",True,"::10.0.0.1")
    cmds.remove_all();
    cmds.get_nodes()
    cmds.get_nodes_info (["00:01:02:03:04:05","00:01:02:03:04:05"])

    pprint.pprint(cmds.get_json())
    pprint.pprint(cmds.get_json_str())


def test2 ():
    cmds=NSCmds()
    MAC="00:01:02:03:04:05"
    cmds.add_node(MAC)
    #cmds.set_vlan("00:01:02:03:04:05",[123,123])
    #cmds.set_vlan("00:01:02:03:04:05",[1])
    cmds.set_ipv4(MAC,"1.1.1.3","1.1.1.2")
    #cmds.set_vlan("00:01:02:03:04:05",[2,2])
    #cmds.set_ipv6("00:01:02:03:04:05",True)
    cmds.set_ipv6(MAC,True)

    #cmds.clear_ipv4(MAC)
    #cmds.set_ipv6(MAC,False)
    #cmds.remove_node (MAC)
    # remove the ipv4 and ipv6 
    #cmds.remove_all();
    cmds.get_nodes()
    cmds.get_nodes_info ([MAC])
    pprint.pprint(cmds.get_json())
    #pprint.pprint(cmds.get_json_str())
    return (cmds.get_json_str())

def test3 ():
    cmds=NSCmds()
    cmds.get_nodes()
    return (cmds.get_json_str())


def get_mac (prefix,index):
    mac="{}:{:02x}:{:02x}".format(prefix,(index>>8)&0xff,(index&0xff))
    return (mac)

def get_ipv4 (prefix,index):
    ipv4="{}.{:d}.{:d}".format(prefix,(index>>8)&0xff,(index&0xff))
    return(ipv4)




def build_network (size):
    cmds=NSCmds()
    MAC_PREFIX="00:01:02:03"
    IPV4_PREFIX="1.1"
    IPV4_DG ='1.1.1.2'
    for i in range(size):
        mac = get_mac (MAC_PREFIX,i+257+1)
        ipv4  =get_ipv4 (IPV4_PREFIX,259+i)
        cmds.add_node(mac)
        cmds.set_ipv4(mac,ipv4,IPV4_DG)
        cmds.set_ipv6(mac,True)

    pprint.pprint(cmds.get_json())
    return (cmds.get_json_str())



#test2 ()
# run the tests
#test2 ()
#simple(ns_json_to_string (10))
#test2 ()
#simple([test2 (),test3 ()])
#print(get_mac ('00:01:02:03',12))
#print("as")
#print(get_mac ('00:01:02:03',0x1212))

#print(build_network (200))

#simple([test2 ()])
#simple([build_network(1000)])
#simple([test3 ()])

simple([test2()])






