#!/router/bin/python

import trex_root_path
from client_utils.packet_builder import CTRexPktBuilder
from trex_stateless_client import CTRexStatelessClient

print "done!"

class CTRexHltApi(object):

    def __init__(self):
        pass

    def connect(self, device, port_list, username, reset=False, break_locks=False):
        pass

    def interface_config(self, port_handle, mode="config"):
        pass

    def get_status(self):
        pass

    def get_port_handler(self):
        pass

    def traffic_config(self, mode, port_handle,
                       mac_src, mac_dst,
                       l3_protocol, ip_src_addr, ip_dst_addr, l3_length,
                       transmit_mode, rate_pps):
        pass

    def traffic_control(self, action, port_handle):
        pass

    def traffic_stats(self, port_handle, mode):
        pass

    def get_aggregate_port_stats(self, port_handle):
        return self.traffic_stats(port_handle, mode="aggregate")

    def get_stream_stats(self, port_handle):
        return self.traffic_stats(port_handle, mode="stream")





if __name__ == "__main__":
    pass

