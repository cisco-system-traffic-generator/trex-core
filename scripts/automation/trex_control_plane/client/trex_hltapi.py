#!/router/bin/python

import trex_root_path
from client_utils.packet_builder import CTRexPktBuilder
from trex_stateless_client import CTRexStatelessClient


class CTRexHltApi(object):

    def __init__(self):
        self.trex_client = None
        self.connected = False

        pass

    # ----- session functions ----- #

    def connect(self, device, port_list, username, port=5050, reset=False, break_locks=False):
        ret_dict = {"status": 0}
        self.trex_client = CTRexStatelessClient(username, device, port)
        res_ok, msg = self.trex_client.connect()
        if not res_ok:
            self.trex_client = None
            ret_dict.update({"log": msg})
            return ret_dict
        # arrived here, connection successfully created with server
        # next, try acquiring ports of TRex
        port_list = self.parse_port_list(port_list)
        response = self.trex_client.acquire(port_list, force=break_locks)
        res_ok, log = CTRexHltApi.process_response(port_list, response)
        if not res_ok:
            self.trex_client.disconnect()
            self.trex_client = None
            ret_dict.update({"log": log})
            # TODO: should revert taken ports?!
            return ret_dict
        # arrived here, all desired ports were successfully acquired
        print log
        if reset:
            # remove all port traffic configuration from TRex
            response = self.trex_client.remove_all_streams(port_list)
            res_ok, log = CTRexHltApi.process_response(port_list, response)
            if not res_ok:
                self.trex_client.disconnect()
                self.trex_client = None
                ret_dict.update({"log": log})
                return ret_dict
        print log
        ret_dict.update({"status": 1})
        self.trex_client.disconnect()

        pass

    def cleanup_session(self, port_list, maintain_lock=False):
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

    # ----- internal functions ----- #
    @staticmethod
    def process_response(port_list, response):
        if isinstance(port_list, list):
            res_ok, response = response
            log = CTRexHltApi.join_batch_response(response)
        else:
            res_ok = response.success
            log = str(response)
        return res_ok, log

    @staticmethod
    def parse_port_list(port_list):
        if isinstance(port_list, str):
            return [int(port)
                    for port in port_list.split()]
        elif isinstance(port_list, list):
            return [int(port)
                    for port in port_list]
        else:
            return port_list

    @staticmethod
    def join_batch_response(responses):
        return "\n".join([str(response)
                          for response in responses])


if __name__ == "__main__":
    pass

