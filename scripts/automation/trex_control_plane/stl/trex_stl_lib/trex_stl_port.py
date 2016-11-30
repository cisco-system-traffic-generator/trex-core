
from collections import namedtuple, OrderedDict

from .trex_stl_packet_builder_scapy import STLPktBuilder
from .trex_stl_streams import STLStream
from .trex_stl_types import *
from .trex_stl_rx_features import ARPResolver, PingResolver
from . import trex_stl_stats
from .utils.constants import FLOW_CTRL_DICT_REVERSED

import base64
import copy
from datetime import datetime, timedelta
import threading

StreamOnPort = namedtuple('StreamOnPort', ['compiled_stream', 'metadata'])

########## utlity ############
def mult_to_factor (mult, max_bps_l2, max_pps, line_util):
    if mult['type'] == 'raw':
        return mult['value']

    if mult['type'] == 'bps':
        return mult['value'] / max_bps_l2

    if mult['type'] == 'pps':
        return mult['value'] / max_pps

    if mult['type'] == 'percentage':
        return mult['value'] / line_util


# describes a single port
class Port(object):
    STATE_DOWN         = 0
    STATE_IDLE         = 1
    STATE_STREAMS      = 2
    STATE_TX           = 3
    STATE_PAUSE        = 4
    STATE_PCAP_TX      = 5

    MASK_ALL = ((1 << 64) - 1)

    PortState = namedtuple('PortState', ['state_id', 'state_name'])
    STATES_MAP = {STATE_DOWN: "DOWN",
                  STATE_IDLE: "IDLE",
                  STATE_STREAMS: "IDLE",
                  STATE_TX: "TRANSMITTING",
                  STATE_PAUSE: "PAUSE",
                  STATE_PCAP_TX : "TRANSMITTING"}


    def __init__ (self, port_id, user, comm_link, session_id, info):
        self.port_id = port_id
        
        self.state = self.STATE_IDLE
        
        self.handler = None
        self.comm_link = comm_link
        self.transmit = comm_link.transmit
        self.transmit_batch = comm_link.transmit_batch
        self.user = user

        self.info = dict(info)

        self.streams = {}
        self.profile = None
        self.session_id = session_id
        self.status = {}

        self.port_stats = trex_stl_stats.CPortStats(self)

        self.next_available_id = 1
        self.tx_stopped_ts = None
        self.has_rx_streams = False

        self.owner = ''
        self.last_factor_type = None
        
        self.__attr = {}
        self.attr_lock = threading.Lock()
        
    # decorator to verify port is up
    def up(func):
        def func_wrapper(*args, **kwargs):
            port = args[0]

            if not port.is_up():
                return port.err("{0} - port is down".format(func.__name__))

            return func(*args, **kwargs)

        return func_wrapper

    # owned
    def owned(func):
        def func_wrapper(*args, **kwargs):
            port = args[0]

            if not port.is_acquired():
                return port.err("{0} - port is not owned".format(func.__name__))

            return func(*args, **kwargs)

        return func_wrapper


    # decorator to check server is readable (port not down and etc.)
    def writeable(func):
        def func_wrapper(*args, **kwargs):
            port = args[0]

            if not port.is_acquired():
                return port.err("{0} - port is not owned".format(func.__name__))

            if not port.is_writeable():
                return port.err("{0} - port is not in a writeable state".format(func.__name__))

            return func(*args, **kwargs)

        return func_wrapper



    def err(self, msg):
        return RC_ERR("port {0} : {1}\n".format(self.port_id, msg))

    def ok(self, data = ""):
        return RC_OK(data)

    def get_speed_bps (self):
        return (self.__attr['speed'] * 1000 * 1000 * 1000)

    def get_formatted_speed (self):
        return "%g Gb/s" % (self.__attr['speed'] / 1000)

    def is_acquired(self):
        return (self.handler != None)

    def is_up (self):
        return self.__attr['link']['up']

    def is_active(self):
        return (self.state == self.STATE_TX ) or (self.state == self.STATE_PAUSE) or (self.state == self.STATE_PCAP_TX)

    def is_transmitting (self):
        return (self.state == self.STATE_TX) or (self.state == self.STATE_PCAP_TX)

    def is_paused (self):
        return (self.state == self.STATE_PAUSE)

    def is_writeable (self):
        # operations on port can be done on state idle or state streams
        return ((self.state == self.STATE_IDLE) or (self.state == self.STATE_STREAMS))

    def get_owner (self):
        if self.is_acquired():
            return self.user
        else:
            return self.owner

    def __allocate_stream_id (self):
        id = self.next_available_id
        self.next_available_id += 1
        return id


    # take the port
    def acquire(self, force = False, sync_streams = True):
        params = {"port_id":     self.port_id,
                  "user":        self.user,
                  "session_id":  self.session_id,
                  "force":       force}

        rc = self.transmit("acquire", params)
        if not rc:
            return self.err(rc.err())

        self.handler = rc.data()

        if sync_streams:
            return self.sync_streams()
        else:
            return self.ok()

      
    # sync all the streams with the server
    def sync_streams (self):
        params = {"port_id": self.port_id}

        rc = self.transmit("get_all_streams", params)
        if rc.bad():
            return self.err(rc.err())

        for k, v in rc.data()['streams'].items():
            self.streams[k] = {'next_id': v['next_stream_id'],
                               'pkt'    : base64.b64decode(v['packet']['binary']),
                               'mode'   : v['mode']['type'],
                               'rate'   : STLStream.get_rate_from_field(v['mode']['rate'])}
        return self.ok()

    # release the port
    def release(self):
        params = {"port_id": self.port_id,
                  "handler": self.handler}

        rc = self.transmit("release", params)
        
        if rc.good():

            self.handler = None
            self.owner = ''

            return self.ok()
        else:
            return self.err(rc.err())

 

    def sync(self):

        params = {"port_id": self.port_id}

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

        self.next_available_id = int(rc.data()['max_stream_id']) + 1

        self.status = rc.data()

        # replace the attributes in a thread safe manner
        self.set_ts_attr(rc.data()['attr'])

        return self.ok()



    # add streams
    @writeable
    def add_streams (self, streams_list):

        # listify
        streams_list = streams_list if isinstance(streams_list, list) else [streams_list]
        
        lookup = {}

        # allocate IDs
        for stream in streams_list:

            # allocate stream id
            stream_id = stream.get_id() if stream.get_id() is not None else self.__allocate_stream_id()
            if stream_id in self.streams:
                return self.err('Stream ID: {0} already exists'.format(stream_id))

            # name
            name = stream.get_name() if stream.get_name() is not None else id(stream)
            if name in lookup:
                return self.err("multiple streams with duplicate name: '{0}'".format(name))
            lookup[name] = stream_id

        batch = []
        for stream in streams_list:

            name = stream.get_name() if stream.get_name() is not None else id(stream)
            stream_id = lookup[name]
            next_id = -1

            next = stream.get_next()
            if next:
                if not next in lookup:
                    return self.err("stream dependency error - unable to find '{0}'".format(next))
                next_id = lookup[next]

            stream_json = stream.to_json()
            stream_json['next_stream_id'] = next_id

            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream_id,
                      "stream": stream_json}

            cmd = RpcCmdData('add_stream', params, 'core')
            batch.append(cmd)


        rc = self.transmit_batch(batch)

        ret = RC()
        for i, single_rc in enumerate(rc):
            if single_rc.rc:
                stream_id = batch[i].params['stream_id']
                next_id   = batch[i].params['stream']['next_stream_id']
                self.streams[stream_id] = {'next_id'        : next_id,
                                           'pkt'            : streams_list[i].get_pkt(),
                                           'mode'           : streams_list[i].get_mode(),
                                           'rate'           : streams_list[i].get_rate(),
                                           'has_flow_stats' : streams_list[i].has_flow_stats()}

                ret.add(RC_OK(data = stream_id))

                self.has_rx_streams = self.has_rx_streams or streams_list[i].has_flow_stats()

            else:
                ret.add(RC(*single_rc))

        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        return ret if ret else self.err(str(ret))



    # remove stream from port
    @writeable
    def remove_streams (self, stream_id_list):

        # single element to list
        stream_id_list = stream_id_list if isinstance(stream_id_list, list) else [stream_id_list]

        # verify existance
        if not all([stream_id in self.streams for stream_id in stream_id_list]):
            return self.err("stream {0} does not exists".format(stream_id))

        batch = []

        for stream_id in stream_id_list:
            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream_id}

            cmd = RpcCmdData('remove_stream', params, 'core')
            batch.append(cmd)


        rc = self.transmit_batch(batch)
        for i, single_rc in enumerate(rc):
            if single_rc:
                id = batch[i].params['stream_id']
                del self.streams[id]

        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        # recheck if any RX stats streams present on the port
        self.has_rx_streams = any([stream['has_flow_stats'] for stream in self.streams.values()])

        return self.ok() if rc else self.err(rc.err())


    # remove all the streams
    @writeable
    def remove_all_streams (self):

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("remove_all_streams", params)
        if not rc:
            return self.err(rc.err())

        self.streams = {}

        self.state = self.STATE_IDLE
        self.has_rx_streams = False

        return self.ok()


    # get a specific stream
    def get_stream (self, stream_id):
        if stream_id in self.streams:
            return self.streams[stream_id]
        else:
            return None

    def get_all_streams (self):
        return self.streams


    @writeable
    def start (self, mul, duration, force, mask):

        if self.state == self.STATE_IDLE:
            return self.err("unable to start traffic - no streams attached to port")

        params = {"handler":    self.handler,
                  "port_id":    self.port_id,
                  "mul":        mul,
                  "duration":   duration,
                  "force":      force,
                  "core_mask":  mask if mask is not None else self.MASK_ALL}
   
        # must set this before to avoid race with the async response
        last_state = self.state
        self.state = self.STATE_TX

        rc = self.transmit("start_traffic", params)

        if rc.bad():
            self.state = last_state
            return self.err(rc.err())

        # save this for TUI
        self.last_factor_type = mul['type']
        
        return rc


    # stop traffic
    # with force ignores the cached state and sends the command
    @owned
    def stop (self, force = False):

        # if not is not active and not force - go back
        if not self.is_active() and not force:
            return self.ok()

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("stop_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_STREAMS
        
        self.last_factor_type = None
        
        # timestamp for last tx
        self.tx_stopped_ts = datetime.now()
        
        return self.ok()


    # return True if port has any stream configured with RX stats
    def has_rx_enabled (self):
        return self.has_rx_streams


    # return true if rx_delay_ms has passed since the last port stop
    def has_rx_delay_expired (self, rx_delay_ms):
        assert(self.has_rx_enabled())

        # if active - it's not safe to remove RX filters
        if self.is_active():
            return False

        # either no timestamp present or time has already passed
        return not self.tx_stopped_ts or (datetime.now() - self.tx_stopped_ts) > timedelta(milliseconds = rx_delay_ms)


    @writeable
    def remove_rx_filters (self):
        assert(self.has_rx_enabled())

        if self.state == self.STATE_IDLE:
            return self.ok()


        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("remove_rx_filters", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()

    
    @owned
    def set_rx_sniffer (self, pcap_filename, limit):

        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "type":           "capture",
                  "enabled":        True,
                  "pcap_filename":  pcap_filename,
                  "limit":          limit}

        rc = self.transmit("set_rx_feature", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()

      
    @owned
    def remove_rx_sniffer (self):
        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "type":           "capture",
                  "enabled":        False}

        rc = self.transmit("set_rx_feature", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()
     
           
    @owned
    def set_arp_resolution (self, ipv4, mac):

        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "ipv4":           ipv4,
                  "mac":            mac}

        rc = self.transmit("set_arp_resolution", params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()
        
  


    @owned
    def set_rx_queue (self, size):

        params = {"handler":        self.handler,
                  "port_id":        self.port_id,
                  "type":           "queue",
                  "enabled":        True,
                  "size":          size}

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
    def pause (self):

        if (self.state == self.STATE_PCAP_TX) :
            return self.err("pause is not supported during PCAP TX")

        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc  = self.transmit("pause_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_PAUSE

        return self.ok()

    @owned
    def resume (self):

        if (self.state != self.STATE_PAUSE) :
            return self.err("port is not in pause mode")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        # only valid state after stop

        rc = self.transmit("resume_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_TX

        return self.ok()

    @owned
    def update (self, mul, force):

        if (self.state == self.STATE_PCAP_TX) :
            return self.err("update is not supported during PCAP TX")

        if (self.state != self.STATE_TX) :
            return self.err("port is not transmitting")

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "mul":     mul,
                  "force":   force}

        rc = self.transmit("update_traffic", params)
        if rc.bad():
            return self.err(rc.err())

        # save this for TUI
        self.last_factor_type = mul['type']

        return self.ok()

    @owned
    def validate (self):

        if (self.state == self.STATE_IDLE):
            return self.err("no streams attached to port")

        params = {"handler": self.handler,
                  "port_id": self.port_id}

        rc = self.transmit("validate", params)
        if rc.bad():
            return self.err(rc.err())

        self.profile = rc.data()

        return self.ok()


    @owned
    def set_attr (self, **kwargs):
        
        json_attr = {}
        
        if kwargs.get('promiscuous') is not None:
            json_attr['promiscuous'] = {'enabled': kwargs.get('promiscuous')}
        
        if kwargs.get('link_status') is not None:
            json_attr['link_status'] = {'up': kwargs.get('link_status')}
        
        if kwargs.get('led_status') is not None:
            json_attr['led_status'] = {'on': kwargs.get('led_status')}
        
        if kwargs.get('flow_ctrl_mode') is not None:
            json_attr['flow_ctrl_mode'] = {'on': kwargs.get('flow_ctrl_mode')}

        if kwargs.get('rx_filter_mode') is not None:
            json_attr['rx_filter_mode'] = {'mode': kwargs.get('rx_filter_mode')}

        if kwargs.get('ipv4') is not None:
            json_attr['ipv4'] = {'addr': kwargs.get('ipv4')}
        
        if kwargs.get('dest') is not None:
            json_attr['dest'] = {'addr': kwargs.get('dest')}
            

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "attr": json_attr}

        rc = self.transmit("set_port_attr", params)
        if rc.bad():
            return self.err(rc.err())

        # update the dictionary from the server explicitly
        return self.sync()

                
    @writeable
    def push_remote (self, pcap_filename, ipg_usec, speedup, count, duration, is_dual, slave_handler):

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "pcap_filename": pcap_filename,
                  "ipg_usec": ipg_usec if ipg_usec is not None else -1,
                  "speedup": speedup,
                  "count": count,
                  "duration": duration,
                  "is_dual": is_dual,
                  "slave_handler": slave_handler}

        rc = self.transmit("push_remote", params)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_PCAP_TX
        return self.ok()


    def get_profile (self):
        return self.profile

    # invalidates the current ARP
    def invalidate_arp (self):
        dest = self.__attr['dest']
        
        if dest['type'] != 'mac':
            return self.set_attr(dest = dest['ipv4'])
        else:
            return self.ok()
        
        
    def print_profile (self, mult, duration):
        if not self.get_profile():
            return

        rate = self.get_profile()['rate']
        graph = self.get_profile()['graph']

        print(format_text("Profile Map Per Port\n", 'underline', 'bold'))

        factor = mult_to_factor(mult, rate['max_bps_l2'], rate['max_pps'], rate['max_line_util'])

        print("Profile max BPS L2    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l2'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l2'] * factor, suffix = "bps")))

        print("Profile max BPS L1    (base / req):   {:^12} / {:^12}".format(format_num(rate['max_bps_l1'], suffix = "bps"),
                                                                             format_num(rate['max_bps_l1'] * factor, suffix = "bps")))

        print("Profile max PPS       (base / req):   {:^12} / {:^12}".format(format_num(rate['max_pps'], suffix = "pps"),
                                                                             format_num(rate['max_pps'] * factor, suffix = "pps"),))

        print("Profile line util.    (base / req):   {:^12} / {:^12}".format(format_percentage(rate['max_line_util']),
                                                                             format_percentage(rate['max_line_util'] * factor)))


        # duration
        exp_time_base_sec = graph['expected_duration'] / (1000 * 1000)
        exp_time_factor_sec = exp_time_base_sec / factor

        # user configured a duration
        if duration > 0:
            if exp_time_factor_sec > 0:
                exp_time_factor_sec = min(exp_time_factor_sec, duration)
            else:
                exp_time_factor_sec = duration


        print("Duration              (base / req):   {:^12} / {:^12}".format(format_time(exp_time_base_sec),
                                                                             format_time(exp_time_factor_sec)))
        print("\n")

    # generate formatted (console friendly) port info
    def get_formatted_info (self, sync = True):

        # sync the status
        if sync:
            self.sync()

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

        if 'description' not in info:
            info['description'] = "N/A"

        if 'is_fc_supported' in info:
            info['fc_supported'] = 'yes' if info['is_fc_supported'] else 'no'
        else:
            info['fc_supported'] = 'N/A'

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

        if 'speed' in attr:
            info['speed'] = self.get_formatted_speed()
        else:
            info['speed'] = 'N/A'
                                                  
        
        if 'rx_filter_mode' in attr:
            info['rx_filter_mode'] = 'hardware match' if attr['rx_filter_mode'] == 'hw' else 'fetch all'
        else:
            info['rx_filter_mode'] = 'N/A'

        # src MAC and IPv4
        info['src_mac'] = attr.get('src_mac', 'N/A')
            
        info['src_ipv4'] = attr.get('src_ipv4', 'N/A')
        if info['src_ipv4'] is None:
            info['src_ipv4'] = 'Not Configured'

        # dest
        dest = attr['dest']
        if dest['type'] == 'mac':
            info['dest'] = dest['mac']
            info['arp']  = '-'
            
        elif dest['type'] == 'ipv4':
            info['dest'] = dest['ipv4']
            info['arp']  = dest['arp']
            
        elif dest['type'] == 'ipv4_u':
            info['dest'] = dest['ipv4']
            info['arp']  = 'unresolved'
            
            
        # RX info
        rx_info = self.status['rx_info']

        # RX sniffer
        sniffer = rx_info['sniffer']
        info['rx_sniffer'] = '{0}\n[{1} / {2}]'.format(sniffer['pcap_filename'], sniffer['count'], sniffer['limit']) if sniffer['is_active'] else 'off'
        

        # RX queue
        queue = rx_info['queue']
        info['rx_queue'] = '[{0} / {1}]'.format(queue['count'], queue['size']) if queue['is_active'] else 'off'
        

        return info


    def get_port_state_name(self):
        return self.STATES_MAP.get(self.state, "Unknown")

    def get_src_addr (self):
        src_mac  = self.__attr['src_mac']
        src_ipv4 = self.__attr['src_ipv4']
            
        return {'mac': src_mac, 'ipv4': src_ipv4}
        
        
    def get_dst_addr (self):
        dest = self.__attr['dest']
        
        if dest['type'] == 'mac':
            return {'ipv4': None, 'mac': dest['mac']}
            
        elif dest['type'] == 'ipv4':
            return {'ipv4': dest['ipv4'], 'mac': dest['arp']}
            
        elif dest['type'] == 'ipv4_u':
            return {'ipv4': dest['ipv4'], 'mac': None}
            
        else:
            assert(0)
    
        
    # return True if the port is resolved
    def is_resolved (self):
        return (self.get_dst_addr()['mac'] is not None)
    
    # return True if the port is valid for resolve (has an IPv4 address as dest)
    def is_resolvable (self):
        return (self.get_dst_addr()['ipv4'] is not None)
        
    @writeable
    def arp_resolve (self, retries):
        return ARPResolver(self).resolve(retries)

    @writeable
    def ping (self, ping_ipv4, pkt_size):
        return PingResolver(self, ping_ipv4, pkt_size).resolve()

        
    ################# stats handler ######################
    def generate_port_stats(self):
        return self.port_stats.generate_stats()

    def generate_port_status(self):

        info = self.get_formatted_info()

        return {"driver":           info['driver'],
                "description":      info.get('description', 'N/A')[:18],
                "src MAC":          info['src_mac'],
                "src IPv4":         info['src_ipv4'],
                "Destination":      info['dest'],
                "ARP Resolution":   info['arp'],
                "PCI Address":      info['pci_addr'],
                "NUMA Node":        info['numa'],
                "--": "",
                "---": "",
                "----": "",
                "-----": "",
                "link speed": info['speed'],
                "port status": info['status'],
                "link status": info['link'],
                "promiscuous" : info['prom'],
                "flow ctrl" : info['fc'],

                "RX Filter Mode": info['rx_filter_mode'],
                "RX Queueing": info['rx_queue'],
                "RX sniffer": info['rx_sniffer'],

                }

    def clear_stats(self):
        return self.port_stats.clear_stats()


    def get_stats (self):
        return self.port_stats.get_stats()


    def invalidate_stats(self):
        return self.port_stats.invalidate()

    ################# stream printout ######################
    def generate_loaded_streams_sum(self):
        if self.state == self.STATE_DOWN:
            return {}

        data = {}
        for id, obj in self.streams.items():

            # lazy build scapy repr.
            if not 'pkt_type' in obj:
                obj['pkt_type'] = STLPktBuilder.pkt_layers_desc_from_buffer(obj['pkt'])
            
            data[id] = OrderedDict([ ('id',  id),
                                     ('packet_type',  obj['pkt_type']),
                                     ('L2 len',       len(obj['pkt']) + 4),
                                     ('mode',         obj['mode']),
                                     ('rate',         obj['rate']),
                                     ('next_stream',  obj['next_id'] if not '-1' else 'None')
                                    ])
    
        return {"streams" : OrderedDict(sorted(data.items())) }
    

    ######## attributes are a complex type (dict) that might be manipulated through the async thread #############
    
    # get in a thread safe manner a duplication of attributes
    def get_ts_attr (self):
        with self.attr_lock:
            return dict(self.__attr)
        
    # set in a thread safe manner a new dict of attributes
    def set_ts_attr (self, new_attr):
        with self.attr_lock:
            self.__attr = new_attr
    
        
  ################# events handler ######################
    def async_event_port_job_done (self):
        # until thread is locked - order is important
        self.tx_stopped_ts = datetime.now()
        self.state = self.STATE_STREAMS
        
        self.last_factor_type = None

    def async_event_port_attr_changed (self, new_attr):
        
        # get a thread safe duplicate
        cur_attr = self.get_ts_attr()
        
        # check if anything changed
        if new_attr == cur_attr:
            return None
            
        # generate before
        before = self.get_formatted_info(sync = False)
        
        # update
        self.set_ts_attr(new_attr)
        
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

    def async_event_acquired (self, who):
        self.handler = None
        self.owner = who

    def async_event_released (self):
        self.owner = ''


