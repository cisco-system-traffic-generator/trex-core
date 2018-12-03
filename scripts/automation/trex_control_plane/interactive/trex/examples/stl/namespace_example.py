import stl_path
from trex.stl.api import *
from trex.utils.text_opts import format_text

import pprint
import time

def my_cb(obj):
    print(obj);
    prog = 100.0*( float(obj['exec_cmds']) / float(obj['total_cmds']))
    err = obj['errs_cmds'] 
    print("progress {:3.0f}% errors : {}".format(prog,err ))

def simple (json_cmd_list):

    # create client
    #verbose_level = 'debug','error'
    c = STLClient(verbose_level = 'error',server='csi-trex-17')
    passed = True
    
    c.connect()

    my_ports=[0,1]
    c.reset(ports = my_ports)

    c.set_service_mode (ports = my_ports, enabled = True)
    c.namespace_remove_all()
    c.set_namespace(0,method='clear_counters')
    pprint.pprint(c.set_namespace(0,method='counters_get_meta'))
    pprint.pprint(c.set_namespace(0,method='counters_get_values',zeros=True))


    # json to string
    cnt=0
    for cmd in json_cmd_list:
       res=c.set_namespace_start( 0, cmd)
       res = c.wait_for_async_results(0,cb=my_cb);
       print(res);
       cnt +=1

    print(c.set_namespace(0,method='get_nodes'))
    pprint.pprint(c.set_namespace(0,method='counters_get_values',zeros=True))


    #while True:
      #if c.is_async_results_ready(0):
      #    print(c.wait_for_async_results(0))
      #    break;
      #else:
      #    print(" not ready !\n")




    c.set_service_mode (ports = my_ports, enabled = False)

    c.disconnect()



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
    return (cmds)

def test3 ():
    cmds=NSCmds()
    cmds.get_nodes()
    return (cmds)


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
    return (cmds)


def test4_args (method,**args):
    cmds = NSCmds()
    func = getattr(cmds, method)

    func(**args)
    pprint.pprint(cmds.get_json())

def test_5 ():
  MAC="00:01:02:03:04:05"
  test4_args ('add_node',mac=MAC)
  test4_args ('remove_all')
  test4_args ('set_ipv4',mac=MAC,ipv4="1.1.1.3",dg="1.1.1.2")
  test4_args ('add_node',mac=MAC)

  


#simple([test2 ()])
#simple([build_network(1000)])
#simple([test3 ()])

simple([test2()])
#simple([build_network(20)])


#test_5 ()





