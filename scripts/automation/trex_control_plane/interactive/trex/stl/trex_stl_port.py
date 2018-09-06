from datetime import datetime, timedelta
from collections import OrderedDict

from ..utils.common import list_difference, list_intersect
from ..utils.text_opts import limit_string
from ..utils import text_tables

from ..common.trex_types import listify, RpcCmdData, RC, RC_OK
from ..common.trex_port import Port, owned, writeable, up

from .trex_stl_streams import STLStream


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



class STLPort(Port):

    MASK_ALL = ((1 << 64) - 1)

    def __init__ (self, ctx, port_id, rpc, info):
        Port.__init__(self, ctx, port_id, rpc, info)

        self.has_rx_streams = False
        self.streams = {}
        self.profile = None
        self.next_available_id = 1


    # sync all the streams with the server
    def sync_streams (self):

        self.streams = {}
        
        params = {"port_id": self.port_id}

        rc = self.transmit("get_all_streams", params)
        if rc.bad():
            return self.err(rc.err())

        for k, v in rc.data()['streams'].items():
            self.streams[int(k)] = STLStream.from_json(v)
            
        return self.ok()


    @owned
    def pause_streams(self, stream_ids):

        if self.state == self.STATE_PCAP_TX:
            return self.err('pause is not supported during PCAP TX')

        if self.state != self.STATE_TX and self.state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'stream_ids': stream_ids or []}

        rc  = self.transmit('pause_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    @owned
    def resume_streams(self, stream_ids):

        if self.state == self.STATE_PCAP_TX:
            return self.err('resume is not supported during PCAP TX')

        if self.state != self.STATE_TX and self.state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'stream_ids': stream_ids or []}

        rc = self.transmit('resume_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    
    @owned
    def update_streams(self, mul, force, stream_ids):

        if self.state == self.STATE_PCAP_TX:
            return self.err('update is not supported during PCAP TX')

        if self.state != self.STATE_TX and self.state != self.STATE_PAUSE:
            return self.err('port should be either paused or transmitting')

        params = {'handler':    self.handler,
                  'port_id':    self.port_id,
                  'mul':        mul,
                  'force':      force,
                  'stream_ids': stream_ids or []}

        rc = self.transmit('update_streams', params)
        if rc.bad():
            return self.err(rc.err())

        return self.ok()


    def _allocate_stream_id (self):
        id = self.next_available_id
        self.next_available_id += 1
        return id


    # add streams
    @writeable
    def add_streams (self, streams_list):

        # listify
        streams_list = listify(streams_list)
        
        lookup = {}

        # allocate IDs
        for stream in streams_list:

            # allocate stream id
            stream_id = stream.get_id() if stream.get_id() is not None else self._allocate_stream_id()
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
                if next not in lookup:
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
            if single_rc:
                stream_id = batch[i].params['stream_id']
                self.streams[stream_id] = streams_list[i].clone()
                
                ret.add(RC_OK(data = stream_id))

                self.has_rx_streams = self.has_rx_streams or streams_list[i].has_flow_stats()

            else:
                ret.add(single_rc)

        self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE

        return ret if ret else self.err(str(ret))



    # remove stream from port
    @writeable
    def remove_streams (self, stream_id_list):

        # single element to list
        stream_id_list = listify(stream_id_list)

        # verify existance
        not_found = list_difference(stream_id_list, self.streams)
        found     = list_intersect(stream_id_list, self.streams)

        batch = []
        
        for stream_id in found:
            params = {"handler": self.handler,
                      "port_id": self.port_id,
                      "stream_id": stream_id}

            cmd = RpcCmdData('remove_stream', params, 'core')
            batch.append(cmd)


        if batch:
            rc = self.transmit_batch(batch)
            for i, single_rc in enumerate(rc):
                if single_rc:
                    id = batch[i].params['stream_id']
                    del self.streams[id]
    
            self.state = self.STATE_STREAMS if (len(self.streams) > 0) else self.STATE_IDLE
    
            # recheck if any RX stats streams present on the port
            self.has_rx_streams = any([stream.has_flow_stats() for stream in self.streams.values()])

            # did the batch send fail ?
            if not rc:
                return self.err(rc.err())
            
                
        # partially succeeded ?
        return self.err("stream(s) {0} do not exist".format(not_found)) if not_found else self.ok()


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


    @owned
    def set_service_mode (self, enabled):
        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "enabled": enabled}

        rc = self.transmit("service", params)
        if rc.bad():
            return self.err(rc.err())

        self.service_mode = enabled
        return self.ok()
        

    def is_service_mode_on (self):
        if not self.is_acquired(): # update lazy
            self.sync()
        return self.service_mode
        
                
    @writeable
    def start (self, mul, duration, force, mask, start_at_ts = 0):

        if self.state == self.STATE_IDLE:
            return self.err("unable to start traffic - no streams attached to port")

        params = {"handler":     self.handler,
                  "port_id":     self.port_id,
                  "mul":         mul,
                  "duration":    duration,
                  "force":       force,
                  "core_mask":   mask if mask is not None else self.MASK_ALL,
                  'start_at_ts': start_at_ts}
   
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

        if self.state != self.STATE_PAUSE:
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


    @writeable
    def push_remote (self, pcap_filename, ipg_usec, speedup, count, duration, is_dual, slave_handler, min_ipg_usec):

        params = {"handler": self.handler,
                  "port_id": self.port_id,
                  "pcap_filename": pcap_filename,
                  "ipg_usec": ipg_usec if ipg_usec is not None else -1,
                  "speedup": speedup,
                  "count": count,
                  "duration": duration,
                  "is_dual": is_dual,
                  "slave_handler": slave_handler,
                  "min_ipg_usec": min_ipg_usec if min_ipg_usec else 0}

        rc = self.transmit("push_remote", params, retry = 4)
        if rc.bad():
            return self.err(rc.err())

        self.state = self.STATE_PCAP_TX
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


    def get_profile (self):
        return self.profile


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


    
    ################# stream printout ######################
    def generate_loaded_streams_sum(self, stream_id_list, table_format = True):
        if self.state == self.STATE_DOWN:
            return {}

        self.sync_streams()

        if stream_id_list:
            stream_ids = list_intersect(stream_id_list, self.streams.keys())
        else:
            stream_ids = self.streams.keys()

        data = OrderedDict([(k, self.streams[k]) for k in sorted(self.streams.keys()) if k in stream_ids])

        if not data or not table_format:
            return data

        pkt_types = {}
        p_type_field_len = 1
        for stream_id, stream in data.items():
            pkt_types[stream_id] = limit_string(stream.get_pkt_type(), 30)
            p_type_field_len = max(p_type_field_len, len(pkt_types[stream_id]))

        info_table = text_tables.TRexTextTable()
        info_table.set_cols_align(["c"] + ["c"] + ["c"] + ["r"] + ["c"] + ["c"] + ["c"] + ["c"])
        info_table.set_cols_width([10]  + [15]  + [p_type_field_len]  + [8] + [16]  + [15] + [12] + [12])
        info_table.set_cols_dtype(["t"] * 8)
        info_table.header(["ID", "name", "packet type", "length", "mode", "rate", "PG ID", "next"])

        for stream_id, stream in data.items():
            if stream.has_flow_stats():
                pg_id = '{0}: {1}'.format(stream.get_flow_stats_type(), stream.get_pg_id())
            else:
                pg_id = '-'

            info_table.add_row([
                stream_id,
                stream.get_name() or '-',
                pkt_types[stream_id],
                len(stream.get_pkt())+ 4,
                stream.get_mode(),
                stream.get_rate(),
                pg_id,
                stream.get_next() or '-',
                ])

        return info_table


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

