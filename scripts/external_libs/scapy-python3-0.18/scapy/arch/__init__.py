## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
Operating system specific functionality.
"""


import sys,os,socket
from scapy.error import *
import scapy.config

try:
    import matplotlib.pyplot as plt
    MATPLOTLIB = True
    if scapy.config.conf.interactive:
        plt.ion()
except ImportError:
    log_loading.info("Can't import matplotlib. Not critical, but won't be able to plot.")
    MATPLOTLIB = False

try:
    import networkx as nx
    NETWORKX = True
except ImportError:
    log_loading.info("Can't import networkx. Not criticial, but won't be able to draw network graphs.")
    NETWORKX = False

try:
    import pyx
    PYX=1
except ImportError:
    log_loading.info("Can't import PyX. Won't be able to use psdump() or pdfdump().")
    PYX=0


def str2mac(s):
    #return ("%02x:"*6)[:-1] % tuple(map(ord, s)) 
    return ("%02x:"*6)[:-1] % tuple(s)


    
def get_if_addr(iff):
    return socket.inet_ntoa(get_if_raw_addr(iff))
    
def get_if_hwaddr(iff):
    mac = get_if_raw_hwaddr(iff)
    return str2mac(mac)


LINUX=sys.platform.startswith("linux")
OPENBSD=sys.platform.startswith("openbsd")
FREEBSD=sys.platform.startswith("freebsd")
NETBSD = sys.platform.startswith("netbsd")
DARWIN=sys.platform.startswith("darwin")
SOLARIS=sys.platform.startswith("sunos")
WINDOWS=sys.platform.startswith("win32")

X86_64 = not WINDOWS and (os.uname()[4] == 'x86_64')

#if WINDOWS:
#  log_loading.warning("Windows support for scapy3k is currently in testing. Sniffing/sending/receiving packets should be working with WinPcap driver and Powershell. Create issues at https://github.com/phaethon/scapy")

# Next step is to import following architecture specific functions:
# def get_if_raw_hwaddr(iff)
# def get_if_raw_addr(iff):
# def get_if_list():
# def get_working_if():
# def attach_filter(s, filter):
# def set_promisc(s,iff,val=1):
# def read_routes():
# def get_if(iff,cmd):
# def get_if_index(iff):



if LINUX:
    from .linux import *
    if scapy.config.conf.use_winpcapy or scapy.config.conf.use_netifaces:
        from pcapdnet import *
elif OPENBSD or FREEBSD or NETBSD or DARWIN:
    from .bsd import *
elif SOLARIS:
    from .solaris import *
elif WINDOWS:
    pass;
    #from .windows import *

LOOPBACK_NAME="a"

if scapy.config.conf.iface is None:
    scapy.config.conf.iface = LOOPBACK_NAME

def get_if_raw_addr6(iff):
    """
    Returns the main global unicast address associated with provided 
    interface, in network format. If no global address is found, None 
    is returned. 
    """
    #r = filter(lambda x: x[2] == iff and x[1] == IPV6_ADDR_GLOBAL, in6_getifaddr())
    r = [ x for x in in6_getifaddr() if x[2] == iff and x[1] == IPV6_ADDR_GLOBAL]
    if len(r) == 0:
        return None
    else:
        r = r[0][0] 
    return inet_pton(socket.AF_INET6, r)
