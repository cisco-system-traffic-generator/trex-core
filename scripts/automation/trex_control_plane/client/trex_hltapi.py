#!/router/bin/python

import trex_root_path
from client_utils.packet_builder import CTRexPktBuilder
from trex_stateless_client import CTRexStatelessClient
from common.trex_streams import *
from client_utils.general_utils import id_count_gen
import dpkt


class CTRexHltApi(object):

    def __init__(self):
        self.trex_client = None
        self.connected = False
        # self._stream_db = CStreamList()
        self._port_data = {}

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
        port_handle = {device: {port: port               # since only supporting single TRex at the moment, 1:1 map
                                for port in port_list}
                       }
        ret_dict.update({"status": 1,
                         "log": None,
                         "port_handle": port_handle,
                         "offline": 0})  # ports are online
        self.connected = True
        self._port_data_init(port_list)
        return ret_dict

    def cleanup_session(self, port_list, maintain_lock=False):
        ret_dict = {"status": 0}
        if not maintain_lock:
            # release taken ports
            if port_list == "all":
                port_list = self.trex_client.get_acquired_ports()
            else:
                port_list = self.parse_port_list(port_list)
            response = self.trex_client.release(port_list)
            res_ok, log = CTRexHltApi.process_response(port_list, response)
            print log
            if not res_ok:
                ret_dict.update({"log": log})
                return ret_dict
        ret_dict.update({"status": 1,
                         "log": None})
        self.trex_client.disconnect()
        self.trex_client = None
        self.connected = False
        return ret_dict

    def interface_config(self, port_handle, mode="config"):
        ALLOWED_MODES = ["config", "modify", "destroy"]
        if mode not in ALLOWED_MODES:
            raise ValueError("mode must be one of the following values: {modes}".format(modes=ALLOWED_MODES))
        # pass this function for now...
        return {"status": 1, "log": None}

    # ----- traffic functions ----- #
    def traffic_config(self, mode, port_handle,
                       l2_encap="ethernet_ii", mac_src="00:00:01:00:00:01", mac_dst="00:00:00:00:00:00",
                       l3_protocol="ipv4", ip_src_addr="0.0.0.0", ip_dst_addr="192.0.0.1", l3_length=110,
                       transmit_mode="continuous", rate_pps=100,
                       **kwargs):
        ALLOWED_MODES = ["create", "modify", "remove", "enable", "disable", "reset"]
        if mode not in ALLOWED_MODES:
            raise ValueError("mode must be one of the following values: {modes}".format(modes=ALLOWED_MODES))
        if mode == "create":
            # create a new stream with desired attributes, starting by creating packet
            try:
                packet = CTRexHltApi.generate_stream(l2_encap, mac_src, mac_dst,
                                                     l3_protocol, ip_src_addr, ip_dst_addr, l3_length)
                # set transmission attributes
                tx_mode = CTxMode(transmit_mode, rate_pps, **kwargs)
                # set rx_stats
                rx_stats = CRxStats()   # defaults with disabled
                # join the generated data into stream
                stream_obj = CStream()
                stream_obj_params = {"enabled": True,
                                     "self_start": True,
                                     "next_stream_id": -1,
                                     "isg": 0.0,
                                     "mode": tx_mode,
                                     "rx_stats": rx_stats,
                                     "packet": packet}  # vm is excluded from this list since CTRexPktBuilder obj is passed
                stream_obj.load_data(**stream_obj_params)
            except Exception as e:
                # some exception happened during the stream creation
                return {"status": 0, "log": str(e)}
            # try adding the stream, until free stream_id is found
            port_data = self._port_data.get(port_handle)
            id_candidate = None
            # TODO: change this to better implementation
            while True:
                 id_candidate = port_data["stream_id_gen"].next()
                 response = self.trex_client.add_stream(stream_id=id_candidate,
                                                        stream_obj=stream_obj,
                                                        port_id=port_handle)
                 res_ok, log = CTRexHltApi.process_response(port_handle, response)
                 if res_ok:
                     # found non-taken stream_id on server
                     # save it for modifying needs
                     port_data["streams"].update({id_candidate: stream_obj})
                     break
                 else:
                     # proceed to another iteration to use another id
                     continue
            return {"status": 1,
                    "stream_id": id_candidate,
                    "log": None}
        else:
            raise NotImplementedError("mode '{0}' is not supported yet on TRex".format(mode))

    def traffic_control(self, action, port_handle):
        ALLOWED_ACTIONS = ["clear_stats", "run", "stop", "sync_run"]
        if action not in ALLOWED_ACTIONS:
            raise ValueError("action must be one of the following values: {actions}".format(actions=ALLOWED_ACTIONS))
        # ret_dict = {"status": 0, "stopped": 1}
        port_list = self.parse_port_list(port_handle)
        if action == "run":
            response = self.trex_client.start_traffic(port_id=port_list)
            res_ok, log = CTRexHltApi.process_response(port_list, response)
            if res_ok:
                return {"status": 1,
                        "stopped": 0,
                        "log": None}
        elif action == "stop":
            response = self.trex_client.stop_traffic(port_id=port_list)
            res_ok, log = CTRexHltApi.process_response(port_list, response)
            if res_ok:
                return {"status": 1,
                        "stopped": 1,
                        "log": None}
        else:
            raise NotImplementedError("action '{0}' is not supported yet on TRex".format(action))

        # if we arrived here, this means that operation FAILED!
        return {"status": 0,
                "log": log}


    def traffic_stats(self, port_handle, mode):
        ALLOWED_MODES = ["aggregate", "streams", "all"]
        if mode not in ALLOWED_MODES:
            raise ValueError("mode must be one of the following values: {modes}".format(modes=ALLOWED_MODES))
        # pass this function for now...
        if mode == "aggregate":
            # create a new stream with desired attributes, starting by creating packet
            try:
                packet = CTRexHltApi.generate_stream(l2_encap, mac_src, mac_dst,
                                                     l3_protocol, ip_src_addr, ip_dst_addr, l3_length)
                # set transmission attributes
                tx_mode = CTxMode(transmit_mode, rate_pps, **kwargs)
                # set rx_stats
                rx_stats = CRxStats()   # defaults with disabled
                # join the generated data into stream
                stream_obj = CStream()
                stream_obj_params = {"enabled": True,
                                     "self_start": True,
                                     "next_stream_id": -1,
                                     "isg": 0.0,
                                     "mode": tx_mode,
                                     "rx_stats": rx_stats,
                                     "packet": packet}  # vm is excluded from this list since CTRexPktBuilder obj is passed
                stream_obj.load_data(**stream_obj_params)
            except Exception as e:
                # some exception happened during the stream creation
                return {"status": 0, "log": str(e)}
            # try adding the stream, until free stream_id is found
            port_data = self._port_data.get(port_handle)
            id_candidate = None
            # TODO: change this to better implementation
            while True:
                 id_candidate = port_data["stream_id_gen"].next()
                 response = self.trex_client.add_stream(stream_id=id_candidate,
                                                        stream_obj=stream_obj,
                                                        port_id=port_handle)
                 res_ok, log = CTRexHltApi.process_response(port_handle, response)
                 if res_ok:
                     # found non-taken stream_id on server
                     # save it for modifying needs
                     port_data["streams"].update({id_candidate: stream_obj})
                     break
                 else:
                     # proceed to another iteration to use another id
                     continue
            return {"status": 1,
                    "stream_id": id_candidate,
                    "log": None}
        else:
            raise NotImplementedError("mode '{0}' is not supported yet on TRex".format(mode))

    def get_aggregate_port_stats(self, port_handle):
        return self.traffic_stats(port_handle, mode="aggregate")

    def get_stream_stats(self, port_handle):
        return self.traffic_stats(port_handle, mode="streams")

    # ----- internal functions ----- #
    def _port_data_init(self, port_list):
        for port in port_list:
            self._port_data[port] = {"stream_id_gen": id_count_gen(),
                                     "streams": {}}

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

    @staticmethod
    def generate_stream(l2_encap, mac_src, mac_dst,
                       l3_protocol, ip_src_addr, ip_dst_addr, l3_length):
        ALLOWED_L3_PROTOCOL = {"ipv4": dpkt.ethernet.ETH_TYPE_IP,
                               "ipv6": dpkt.ethernet.ETH_TYPE_IP6,
                               "arp": dpkt.ethernet.ETH_TYPE_ARP}
        ALLOWED_L4_PROTOCOL = {"tcp": dpkt.ip.IP_PROTO_TCP,
                               "udp": dpkt.ip.IP_PROTO_UDP,
                               "icmp": dpkt.ip.IP_PROTO_ICMP,
                               "icmpv6": dpkt.ip.IP_PROTO_ICMP6,
                               "igmp": dpkt.ip.IP_PROTO_IGMP,
                               "rtp": dpkt.ip.IP_PROTO_IRTP,
                               "isis": dpkt.ip.IP_PROTO_ISIS,
                               "ospf": dpkt.ip.IP_PROTO_OSPF}

        pkt_bld = CTRexPktBuilder()
        if l2_encap == "ethernet_ii":
            pkt_bld.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
            # set Ethernet layer attributes
            pkt_bld.set_eth_layer_addr("l2", "src", mac_src)
            pkt_bld.set_eth_layer_addr("l2", "dst", mac_dst)
        else:
            raise NotImplementedError("l2_encap does not support the desired encapsulation '{0}'".format(l2_encap))
        # set l3 on l2
        if l3_protocol not in ALLOWED_L3_PROTOCOL:
            raise ValueError("l3_protocol must be one of the following: {0}".format(ALLOWED_L3_PROTOCOL))
        pkt_bld.set_layer_attr("l2", "type", ALLOWED_L3_PROTOCOL[l3_protocol])

        # set l3 attributes
        if l3_protocol == "ipv4":
            pkt_bld.add_pkt_layer("l3", dpkt.ip.IP())
            pkt_bld.set_ip_layer_addr("l3", "src", ip_src_addr)
            pkt_bld.set_ip_layer_addr("l3", "dst", ip_dst_addr)
            pkt_bld.set_layer_attr("l3", "len", l3_length)
        else:
            raise NotImplementedError("l3_protocol '{0}' is not supported by TRex yet.".format(l3_protocol))

        pkt_bld.dump_pkt_to_pcap("stream_test.pcap")
        return pkt_bld



if __name__ == "__main__":
    pass

