
from collections import namedtuple, OrderedDict
from datetime import datetime
import copy
import base64
import threading

from ..utils.constants import FLOW_CTRL_DICT_REVERSED
from ..utils.text_tables import Tableable, TRexTextTable
from ..utils.text_opts import *
from .trex_types import *
from .trex_exceptions import *
from .stats.trex_port_stats import PortStats, PortXStats


# state decorators


# decorator to verify port is up
def up(func):
    def func_wrapper(*args, **kwargs):
        port = args[0]

        if not port.is_up():
            return port.err("{0} - port is down".format(func.__name__))

        return func(*args, **kwargs)

    return func_wrapper


# decorator to check server is readable (port not down and etc.)
def writeable(func):
    def func_wrapper(*args, **kwargs):
        port = args[0]

        if not port.is_acquired():
            return port.err("{0} - port is not owned".format(func.__name__))

        if not port.is_writeable():
            return port.err("{0} - port is active, please stop the port before executing command".format(func.__name__))

        return func(*args, **kwargs)

    return func_wrapper


def owned(func):
    def func_wrapper(*args, **kwargs):
        port = args[0]

        if not port.is_acquired():
            return port.err("{0} - port is not owned".format(func.__name__))

        return func(*args, **kwargs)

    return func_wrapper

class PortAttr(object):
    def __init__(self):
        self.__attr = {}
        self.__lock = threading.RLock()

    def update(self, attr):
        with self.__lock:
            self.__attr.update(attr)

    def get(self):
        with self.__lock:
            return dict(self.__attr)

    def get_param(self, *path):
        with self.__lock:
            ret = self.__attr
            for key in path:
                if key not in ret:
                    raise TRexError('Port attribute with path "%s" does not exist!' % ', '.join(path))
                ret = ret[key]
            return copy.deepcopy(ret)

# describes a single port
class Port(object):
    STATE_DOWN         = 0
    STATE_IDLE         = 1
    STATE_STREAMS      = 2
    STATE_TX           = 3
    STATE_PAUSE        = 4
    STATE_PCAP_TX      = 5


    STATES_MAP = {STATE_DOWN:        "DOWN",
                  STATE_IDLE:        "IDLE",
                  STATE_STREAMS:     "IDLE",
                  STATE_TX:          "TRANSMITTING",
                  STATE_PAUSE:       "PAUSE",
                  STATE_PCAP_TX :    "TRANSMITTING"}


    def __init__ (self, ctx, port_id, rpc, info):
        self.ctx            = ctx
        self.port_id        = port_id

        self.state          = self.STATE_IDLE
        self.service_mode   = False

        self.handler        = ''
        self.rpc            = rpc
        self.transmit       = rpc.transmit
        self.transmit_batch = rpc.transmit_batch

        self.info = dict(info)

        self.status = {}

        self.stats  = PortStats(self)
        self.xstats = PortXStats(self)

        self.tx_stopped_ts = None

        self.owner = ''
        self.last_factor_type = None

        self.__attr = PortAttr()
        self.__is_sync = False

    def err(self, msg):
        return RC_ERR("Port {0} : *** {1}".format(self.port_id, msg))


    def ok(self, data = ""):
        return RC_OK(data)

    def is_sync(self):
        return self.__is_sync

    def get_speed_bps (self):
        return (self.get_speed_gbps() * 1000 * 1000 * 1000)


    def get_speed_gbps (self):
        return self.__attr.get_param('speed')


    def is_acquired(self):
        return (self.handler != '')


    def is_up (self):
        return self.__attr.get_param('link', 'up')


    def is_active(self):
        return (self.state == self.STATE_TX ) or (self.state == self.STATE_PAUSE) or (self.state == self.STATE_PCAP_TX)


    def is_transmitting (self):
        return (self.state == self.STATE_TX) or (self.state == self.STATE_PCAP_TX)


    def is_paused (self):
        return (self.state == self.STATE_PAUSE)


    def is_writeable (self):
        # operations on port can be done on state idle or state streams
        return ((self.state == self.STATE_IDLE) or (self.state == self.STATE_STREAMS))


    def is_virtual(self):
        return self.info.get('is_virtual')


    def get_owner (self):
        if self.is_acquired():
            return self.ctx.username
        else:
            return self.owner


    # take the port
    def acquire(self, force = False):
        params = {"port_id":     self.port_id,
                  "user":        self.ctx.username,
                  "session_id":  self.ctx.session_id,
                  "force":       force}

        rc = self.transmit("acquire", params)
        if not rc:
            return self.err(rc.err())

        self.handler = rc.data()

        return self.ok()


    # release the port
    def release(self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        rc = self.transmit("release", params)

        if rc.good():

            self.handler = ''
            self.owner = ''

            return self.ok()
        else:
            return self.err(rc.err())


    def sync(self):

        params = {"port_id": self.port_id, 'block': False}

        rc = self.transmit("get_port_status", params)
        if rc.bad():
            return self.err(rc.err())

        # sync the port
        port_state = rc.data()['state']

        if port_state == "DOWN":
            self.state = self.STATE_DOWN
        elif port_state == "IDLE":
            self.state = self.STATE_IDLE
        elif port_state == "STREAMS":
            self.state = self.STATE_STREAMS
        elif port_state == "TX":
            self.state = self.STATE_TX
        elif port_state == "PAUSE":
            self.state = self.STATE_PAUSE
        elif port_state == "PCAP_TX":
            self.state = self.STATE_PCAP_TX
        else:
            raise Exception("port {0}: bad state received from server '{1}'".format(self.port_id, port_state))

        self.owner = rc.data()['owner']
        
        # for stateless (hack)
        if 'max_stream_id' in rc.data():
            self.next_available_id = int(rc.data()['max_stream_id']) + 1

        self.status = rc.data()
        
        # replace the attributes in a thread safe manner
        self.update_ts_attr(rc.data()['attr'])
        
        self.service_mode = rc.data()['service']

        self.__is_sync = True
        return self.ok()

     
    @writeable
    def set_l2_mode (self, dst_mac):
        if not self.is_service_mode_on():
            return self.err('port service mode must be enabled for configuring L2 mode. Please enable service mode')
        
        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "dst_mac":        dst_mac,
                  "block"  :        False}

        rc = self.transmit("set_l2", params)
        if rc.bad():
            return self.err(rc.err())

        return self.sync()
        
        
    @writeable
    def set_l3_mode (self, src_addr, dst_addr, resolved_mac = None):
        if not self.is_service_mode_on():
            return self.err('port service mode must be enabled for configuring L3 mode. Please enable service mode')
        
        params = {"handler"  :      self.handler,
                  "port_id"  :      self.port_id,
                  "src_addr" :      src_addr,
                  "dst_addr" :      dst_addr,
                  "block"    :      False}

        if resolved_mac:
            params["resolved_mac"] = resolved_mac
            
        rc = self.transmit("set_l3", params)
        if rc.bad():
            return self.err(rc.err())

        return self.sync()

        
    @writeable
    def conf_ipv6(self, enabled, src_ipv6):
        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'enabled':    enabled,
                  'src_ipv6':   src_ipv6 if (enabled and src_ipv6) else '',
                  'block':      False}
        rc = self.transmit("conf_ipv6", params)
        if rc.bad():
            return self.err(rc.err())
        return self.sync()
        
                
    @writeable
    def set_vlan (self, vlan):
        if not self.is_service_mode_on():
            return self.err('port service mode must be enabled for configuring VLAN. Please enable service mode')
        
        params = {"handler" :       self.handler,
                  "port_id" :       self.port_id,
                  "vlan"    :       vlan.get_tags(),
                  "block"   :       False}
            
        rc = self.transmit("set_vlan", params)
        if rc.bad():
            return self.err(rc.err())

        return self.sync()
        
        
    @owned
    def set_rx_queue (self, size):

        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "type":           "queue",
                  "enabled":        True,
                  "size":           size}

        rc = self.transmit("set_rx_feature", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    @owned
    def remove_rx_queue (self):
        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "type":           "queue",
                  "enabled":        False}

        rc = self.transmit("set_rx_feature", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    @owned
    def get_rx_queue_pkts (self):
        params = {"handler":        self.handler,
                  "port_id":        self.port_id}

        rc = self.transmit("get_rx_queue_pkts", params)
        if rc.bad():
            return self.err(rc.err())

        pkts = rc.data()['pkts']
        
        # decode the packets from base64 to binary
        for i in range(len(pkts)):
            pkts[i]['binary'] = base64.b64decode(pkts[i]['binary'])
            
        return RC_OK(pkts)
        
        
 
    @owned
    def set_attr (self, **kwargs):
        
        json_attr = {}
        
        if kwargs.get('promiscuous') is not None:
            json_attr['promiscuous'] = {'enabled': kwargs.get('promiscuous')}

        if kwargs.get('multicast') is not None:
            json_attr['multicast'] = {'enabled': kwargs.get('multicast')}

        if kwargs.get('link_status') is not None:
            json_attr['link_status'] = {'up': kwargs.get('link_status')}
        
        if kwargs.get('led_status') is not None:
            json_attr['led_status'] = {'on': kwargs.get('led_status')}
        
        if kwargs.get('flow_ctrl_mode') is not None:
            json_attr['flow_ctrl_mode'] = {'mode': kwargs.get('flow_ctrl_mode')}

        if kwargs.get('rx_filter_mode') is not None:
            json_attr['rx_filter_mode'] = {'mode': kwargs.get('rx_filter_mode')}


        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "attr": json_attr}

        rc = self.transmit("set_port_attr", params)
        if rc.bad():
            return self.err(rc.err())

        # update the dictionary from the server explicitly
        return self.sync()

    
    def push_packets (self, pkts, force, ipg_usec):
        params = {'port_id'   : self.port_id,
                  'pkts'      : pkts,
                  'force'     : force,
                  'ipg_usec'  : ipg_usec}
        
        rc = self.transmit("push_pkts", params)
        if rc.bad():
            return self.err(rc.err())
            
        return rc
        

    def get_profile (self):
        return self.profile

        
    # invalidates the current ARP
    def invalidate_arp (self):
        if not self.is_l3_mode():
            return self.err('port is not configured with L3')
        
        layer_cfg = self.get_layer_cfg()
        
        # reconfigure server with unresolved IPv4 information
        return self.set_l3_mode(layer_cfg['ipv4']['src'], layer_cfg['ipv4']['dst'])
        
        
        
    # generate formatted (console friendly) port info
    def get_formatted_info (self, sync = True):

        # sync the status
        if sync:
            self.sync()
        elif not self.is_sync():
            return {}

        # get a copy of the current attribute set (safe against manipulation)
        attr = self.get_ts_attr()

        info = dict(self.info)

        info['status'] = self.get_port_state_name()

        if 'link' in attr:
            info['link'] = 'UP' if attr['link']['up'] else 'DOWN'
        else:
            info['link'] = 'N/A'

        if 'fc' in attr:
            info['fc'] = FLOW_CTRL_DICT_REVERSED.get(attr['fc']['mode'], 'N/A')
        else:
            info['fc'] = 'N/A'

        if 'promiscuous' in attr:
            info['prom'] = "on" if attr['promiscuous']['enabled'] else "off"
        else:
            info['prom'] = "N/A"

        if 'multicast' in attr:
            info['mult'] = "on" if attr['multicast']['enabled'] else "off"
        else:
            info['mult'] = "N/A"

        if 'description' not in info:
            info['description'] = "N/A"

        if 'is_fc_supported' in info:
            info['fc_supported'] = 'yes' if info['is_fc_supported'] else 'no'
        else:
            info['fc_supported'] = 'N/A'

        if 'is_prom_supported' in info:
            info['prom_supported'] = 'yes' if info['is_prom_supported'] else 'no'
        else:
            info['prom_supported'] = 'N/A'

        if 'is_led_supported' in info:
            info['led_change_supported'] = 'yes' if info['is_led_supported'] else 'no'
        else:
            info['led_change_supported'] = 'N/A'

        if 'is_link_supported' in info:
            info['link_change_supported'] = 'yes' if info['is_link_supported'] else 'no'
        else:
            info['link_change_supported'] = 'N/A'

        if 'is_virtual' in info:
            info['is_virtual'] = 'yes' if info['is_virtual'] else 'no'
        else:
            info['is_virtual'] = 'N/A'

        # speed
        info['speed'] = self.get_speed_gbps()
        
        # VLAN
        vlan = attr['vlan']
        tags = vlan['tags']
        
        if len(tags) == 0:
            # no VLAN
            info['vlan'] = '-'
            
        elif len(tags) == 1:
            # single VLAN
            info['vlan']  = tags[0]
            
        elif len(tags) == 2:
            # QinQ
            info['vlan']  = '{0}/{1} (QinQ)'.format(tags[0], tags[1])
            
        
        # RX filter mode
        info['rx_filter_mode'] = 'hardware match' if attr['rx_filter_mode'] == 'hw' else 'fetch all'

        # pretty show per mode
        ether = attr['layer_cfg']['ether']
        ipv4  = attr['layer_cfg']['ipv4']
        ipv6  = attr['layer_cfg'].get('ipv6')
        
        info['src_mac'] = ether['src']
        
        if ipv4['state'] == 'none':
            info['layer_mode'] = 'Ethernet'
            info['src_ipv4']   = '-'
            info['dest']       = ether['dst'] if ether['state'] == 'configured' else 'unconfigured'
            info['arp']        = '-'
            
        elif ipv4['state'] == 'unresolved':
            info['layer_mode'] = 'IPv4'
            info['src_ipv4']   = ipv4['src']
            info['dest']       = ipv4['dst']
            info['arp']        = 'unresolved'
            
        elif ipv4['state'] == 'resolved':
            info['layer_mode'] = 'IPv4'
            info['src_ipv4']   = ipv4['src']
            info['dest']       = ipv4['dst']
            info['arp']        = ether['dst']

        else:
            assert 0, ipv4['state']

        if ipv6 and ipv6['enabled']:
            if ipv6['src']:
                info['ipv6'] = ipv6['src']
            else:
                info['ipv6'] = 'auto'
        else:
            info['ipv6'] = 'off'

        # RX info
        rx_info = self.status['rx_info']

        # RX queue
        queue = rx_info['queue']
        info['rx_queue'] = '[{0} / {1}]'.format(queue['count'], queue['size']) if queue['is_active'] else 'off'
        
        # Grat ARP
        grat_arp = rx_info['grat_arp']
        if grat_arp['is_active']:
            info['grat_arp'] = "every {0} seconds".format(grat_arp['interval_sec'])
        else:
            info['grat_arp'] = "off"


        return info


    def get_port_state_name(self):
        return self.STATES_MAP.get(self.state, "Unknown")


    def get_layer_cfg(self):
        return self.__attr.get_param('layer_cfg')


    def get_vlan_cfg (self):
        return self.__attr.get_param('vlan', 'tags')


    def is_l3_mode (self):
        return self.get_layer_cfg()['ipv4']['state'] != 'none'
        

    def is_resolved (self):
        # for L3
        if self.is_l3_mode():
            return self.get_layer_cfg()['ipv4']['state'] != 'unresolved'
        # for L2
        else:
            return self.get_layer_cfg()['ether']['state'] != 'unconfigured'
            

    def is_link_change_supported (self):
        return self.info['is_link_supported']


    def is_prom_supported (self):
        return self.info['is_prom_supported']

    def is_prom_enabled(self):
        return self.__attr.get_param('promiscuous', 'enabled')

    def is_mult_enabled(self):
        return self.__attr.get_param('multicast', 'enabled')


    ################# stats handler ######################

    def get_port_stats (self):
        return self.stats

    def get_port_xstats (self):
        return self.xstats

    def get_port_status(self):

        info = self.get_formatted_info()

        data = OrderedDict([
                           ("driver",           info['driver']),
                           ("description",      info.get('description', 'N/A')[:18]),
                           ("link status",     info['link']),
                           ("link speed",      "%g Gb/s" % info['speed']),
                           ("port status",     info['status']),
                           ("promiscuous",     info['prom']),
                           ("multicast",       info['mult']),
                           ("flow ctrl",       info['fc']),
                           ("--", ""),

                           ("layer mode",      format_text(info['layer_mode'], 'green' if info['layer_mode'] == 'IPv4' else 'magenta')),
                           ("src IPv4",         info['src_ipv4']),
                           ("IPv6",             info['ipv6']),
                           ("src MAC",          info['src_mac']),
                           ("---", ""),

                           ("Destination",      format_text("{0}".format(info['dest']), 'bold', 'red' if info['dest'] == 'unconfigured' else None)),
                           ("ARP Resolution",   format_text("{0}".format(info['arp']), 'bold', 'red' if info['arp'] == 'unresolved' else None)),
                           ("----", ""),

                           ("VLAN",             format_text("{0}".format(info['vlan']), *('bold', 'magenta') if info['vlan'] != '-' else '')),
                           ("-----", ""),

                           ("PCI Address",      info['pci_addr']),
                           ("NUMA Node",        info['numa']),
                           ("RX Filter Mode",  info['rx_filter_mode']),
                           ("RX Queueing",     info['rx_queue']),
                           ("Grat ARP",        info['grat_arp']),

                           ("------", ""),
                           
                           ])


        table = TRexTextTable('Port Status')
        table.set_cols_align(["l"] + ["c"])
        table.set_cols_width([15] + [20])

        table.add_rows([[k] + [v] for k, v in data.items()],
                       header=False)
        table.header(["port"] + [self.port_id])

        return table


    def get_stats (self):
        return self.stats


    def get_xstats (self):
        return self.xstats


    ######## attributes are a complex type (dict) that might be manipulated through the async thread #############
    
    # get in a thread safe manner a duplication of attributes
    def get_ts_attr (self):
        return self.__attr.get()

    # update in a thread safe manner a dict of attributes
    def update_ts_attr (self, new_attr):
        self.__attr.update(new_attr)

  ################# events handler ######################
    def async_event_port_job_done (self):
        # until thread is locked - order is important
        self.tx_stopped_ts = datetime.now()
        self.state = self.STATE_STREAMS
        
        self.last_factor_type = None


    def async_event_port_attr_changed (self, new_attr):
        
        # get a thread safe duplicate
        cur_attr = self.get_ts_attr()

        if not cur_attr:
            return

        # check if anything changed
        if new_attr == cur_attr:
            return

        # generate before
        before = self.get_formatted_info(sync = False)
        
        # update
        self.update_ts_attr(new_attr)
        
        # generate after
        after = self.get_formatted_info(sync = False)
        
        # return diff
        diff = {}
        for key, new_value in after.items():
            old_value = before.get(key, 'N/A')
            if new_value != old_value:
                diff[key] = (old_value, new_value)
                
        return diff
        

    # rest of the events are used for TUI / read only sessions
    def async_event_port_stopped (self):
        if not self.is_acquired():
            self.state = self.STATE_STREAMS


    def async_event_port_paused (self):
        if not self.is_acquired():
            self.state = self.STATE_PAUSE


    def async_event_port_started (self):
        if not self.is_acquired():
            self.state = self.STATE_TX


    def async_event_port_resumed (self):
        if not self.is_acquired():
            self.state = self.STATE_TX


    def async_event_port_acquired (self, who):
        self.handler = ''
        self.owner = who


    def async_event_port_released (self):
        self.owner = ''


