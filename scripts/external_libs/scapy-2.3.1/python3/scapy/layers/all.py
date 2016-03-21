## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
All layers. Configurable with conf.load_layers.
"""

import importlib
from scapy.config import conf
from scapy.error import log_loading
import logging
log = logging.getLogger("scapy.loading")

#log_loading.info("Please, report issues to https://github.com/phaethon/scapy")

def _import_star(m):
    #mod = __import__("." + m, globals(), locals())
    mod = importlib.import_module("scapy.layers." + m)
    for k,v in mod.__dict__.items():
        globals()[k] = v


for _l in ['l2','inet','inet6']:
    log_loading.debug("Loading layer %s" % _l)
    #print "load  ",_l
    _import_star(_l)

#def _import_star(m):
    #mod = __import__("." + m, globals(), locals())
#    mod = importlib.import_module("scapy.layers." + m)
#    for k,v in mod.__dict__.items():
#        globals()[k] = v

#for _l in conf.load_layers:
#    log_loading.debug("Loading layer %s" % _l)
#    try:
#        _import_star(_l)
#    except Exception as e:
#        log.warning("can't import layer %s: %s" % (_l,e))




