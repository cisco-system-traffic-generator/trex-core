import stl_path
from trex.stl.api import *
from trex.utils.text_opts import format_text
import argparse

import pprint
import time

def my_cb(obj):
    print(obj);
    prog = 100.0*( float(obj['exec_cmds']) / float(obj['total_cmds']))
    err = obj['errs_cmds'] 
    print("progress {:3.0f}% errors : {}".format(prog,err ))

def run(c, json_cmd_list):
    c.namespace_remove_all()
    c.set_namespace(0,method='clear_counters')
    pprint.pprint(c.set_namespace(0,method='counters_get_meta'))
    pprint.pprint(c.set_namespace(0,method='counters_get_values',zeros=True))


    # json to string
    for cmd in json_cmd_list:
        res=c.set_namespace_start( 0, cmd)
        res = c.wait_for_async_results(0,cb=my_cb);
        print(res);

    print(c.set_namespace(0,method='get_nodes'))
    pprint.pprint(c.set_namespace(0,method='counters_get_values',zeros=True))


    #while True:
        #if c.is_async_results_ready(0):
        #    print(c.wait_for_async_results(0))
        #    break;
        #else:
        #    print(" not ready !\n")


def simple(server, json_cmd_list):

    # create client
    #verbose_level = 'debug','error'
    c = STLClient(verbose_level = 'error', server=server)
    c.connect()

    my_ports=[0,1]
    c.acquire(my_ports)

    try:
        with c.service_mode(my_ports):
            run(c, json_cmd_list)

    finally:
        c.release()


def test2 ():
    cmds=NSCmds()
    MAC="00:01:02:03:04:05"
    cmds.add_node(MAC)
    #cmds.set_vlan("00:01:02:03:04:05", [123,123], [0x8100, 0x8100])
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


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-s', dest = 'server',
            type=str, default = 'localhost',
            help = 'TRex server address',)
    return parser.parse_args()

args = parse_args()
#simple(args.server, [test2 ()])
#simple(args.server, [build_network(1000)])
#simple(args.server, [test3 ()])

simple(args.server, [test2()])
#simple(args.server, [build_network(20)])


#test_5 ()





