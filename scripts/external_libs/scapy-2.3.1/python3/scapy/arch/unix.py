## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
Common customizations for all Unix-like operating systems other than Linux
"""

import sys,os,struct,socket,time
from subprocess import check_output
from fcntl import ioctl
from scapy.error import warning
import scapy.config
import scapy.utils
import scapy.utils6
import scapy.arch

scapy.config.conf.use_winpcapy = True
scapy.config.conf.use_netifaces = True
scapy.config.conf.use_dnet = True
from .pcapdnet import *



##################
## Routes stuff ##
##################


def read_routes():
    if scapy.arch.SOLARIS:
        f=check_output(["netstat", "-rvn"], universal_newlines = True) # -f inet
    elif scapy.arch.FREEBSD:
        f=check_output(["netstat", "-rnW"], universal_newlines = True) # -W to handle long interface names
    else:
        f=check_output(["netstat", "-rn"], universal_newlines = True) # -f inet
    ok = False
    routes = []
    pending_if = []
    for l in f.split('\n'):
        l = l.strip()
        if l.find("----") >= 0: # a separation line
            continue
        if not ok:
            if_index = [ l.split().index(i) for i in ['Iface', 'Netif', 'Interface', 'Device'] if i in l.split()]
            if if_index:
                ok = True
                if_index = if_index[0]
            continue
        if not l:
            break
        if scapy.arch.SOLARIS:
            lspl = l.split()
            if len(lspl) == 10:
                dest,mask,gw,netif,mxfrg,rtt,ref,flg = lspl[:8]
            else: # missing interface
                dest,mask,gw,mxfrg,rtt,ref,flg = lspl[:7]
                netif=None
        else:
            rt = l.split()
            dest,gw,flg = rt[:3]
            netif = rt[if_index]
        if flg.find("Lc") >= 0:
            continue                
        if dest == "default":
            dest = 0
            netmask = 0
        else:
            if scapy.arch.SOLARIS:
                netmask = scapy.utils.atol(mask)
            elif "/" in dest:
                dest,netmask = dest.split("/")
                netmask = scapy.utils.itom(int(netmask))
            else:
                netmask = scapy.utils.itom((dest.count(".") + 1) * 8)
            dest += ".0"*(3-dest.count("."))
            dest = scapy.utils.atol(dest)
        if not "G" in flg:
            gw = '0.0.0.0'
        if netif is not None:
            ifaddr = scapy.arch.get_if_addr(netif)
            routes.append((dest,netmask,gw,netif,ifaddr))
        else:
            pending_if.append((dest,netmask,gw))

    # On Solaris, netstat does not provide output interfaces for some routes
    # We need to parse completely the routing table to route their gw and
    # know their output interface
    for dest,netmask,gw in pending_if:
        gw_l = scapy.utils.atol(gw)
        max_rtmask,gw_if,gw_if_addr, = 0,None,None
        for rtdst,rtmask,_,rtif,rtaddr in routes[:]:
            if gw_l & rtmask == rtdst:
                if rtmask >= max_rtmask:
                    max_rtmask = rtmask
                    gw_if = rtif
                    gw_if_addr = rtaddr
        if gw_if:
            routes.append((dest,netmask,gw,gw_if,gw_if_addr))
        else:
            warning("Did not find output interface to reach gateway %s" % gw)
            
    return routes

############
### IPv6 ###
############

def read_routes6():
    f = os.popen("netstat -rn -f inet6")
    ok = False
    mtu_present = False
    prio_present = False
    routes = []
    lifaddr = in6_getifaddr()
    for l in f.readlines():
        if not l:
            break
        l = l.strip()
        if not ok:
            if l.find("Destination") >= 0:
                ok = 1
                mtu_present = l.find("Mtu") >= 0
                prio_present = l.find("Prio") >= 0
            continue
        # gv 12/12/06: under debugging      
        if scapy.arch.NETBSD or scapy.arch.OPENBSD:
            lspl = l.split()
            d,nh,fl = lspl[:3]
            dev = lspl[5+mtu_present+prio_present]
            expire = None
        else:       # FREEBSD or DARWIN 
            d,nh,fl,dev = l.split()[:4]
        if [ x for x in lifaddr if x[2] == dev] == []:
            continue
        if 'L' in fl: # drop MAC addresses
            continue

        if 'link' in nh:
            nh = '::'

        cset = [] # candidate set (possible source addresses)
        dp = 128
        if d == 'default':
            d = '::'
            dp = 0
        if '/' in d:
            d,dp = d.split("/")
            dp = int(dp)
        if '%' in d:
            d,dev = d.split('%')
        if '%' in nh:
            nh,dev = nh.split('%')
        if scapy.arch.LOOPBACK_NAME in dev:
            if d == '::' and dp == 96:  #Do not use ::/96 deprecated IPV4 mapping address
              continue
            cset = ['::1']
            nh = '::'
        else:
            devaddrs = [ x for x in lifaddr if x[2] == dev ]
            cset = scapy.utils6.construct_source_candidate_set(d, dp, devaddrs, scapy.arch.LOOPBACK_NAME)

        if len(cset) != 0:
            routes.append((d, dp, nh, dev, cset))

    f.close()
    return routes
