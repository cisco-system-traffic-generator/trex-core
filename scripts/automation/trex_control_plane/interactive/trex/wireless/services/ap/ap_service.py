import simpy
import threading
import time
import struct

from wireless.services.trex_wireless_device_service import WirelessDeviceService
from wireless.utils.utils import SynchronizedStore
from scapy.contrib.capwap import CAPWAP_PKTS, capwap_result_codes

class APService(WirelessDeviceService):
    """An APService is a WirelessService for an AP."""
    def __init__(self, device, env, tx_conn, topics_to_subs, done_event=None, max_concurrent=float('inf'), **kw):
        super().__init__(device=device, env=env, tx_conn=tx_conn, topics_to_subs=topics_to_subs, done_event=done_event, max_concurrent=max_concurrent, **kw)
        self.ap = device

    def control_round_trip(self, capw_ctrl_pkt, expected_response_type):
        """Send the packet, wait for the expected answer, and retry until max_retransmit.
        This is a generator for a APService, and returns True if success, and False otherwise.
        To be used in a APService as:
            success = yield self.from control_round_trip(capw_ctrl_pkt, expected_response_type=4) # expecting join response
        For 'expected_response_type', check scapy.contrib.capwap.

        Args:
            capw_ctrl_pkt (bytes): capwap control packet to send
            expected_response_type (int): type of the expected response (usually capw_ctrl_pkt's message type + 1)
        """
        from ...trex_wireless_config import config
        RetransmitInterval = config.capwap.retransmit_interval

        for _ in range(config.capwap.max_retransmit):
            RetransmitInterval *= 2

            encrypted = self.ap.encrypt(capw_ctrl_pkt)
            if encrypted and encrypted != '':
                tx_pkt_wrapped = self.ap.wrap_capwap_pkt(
                    b'\1\0\0\0' + self.ap.encrypt(capw_ctrl_pkt))

                self.ap.logger.debug("sending packet")
                self.send_pkt(tx_pkt_wrapped)
            else:
                continue

            # wait on expected response
            pkts = yield self.async_recv_pkt(time_sec=RetransmitInterval)
            if not pkts:
                continue
            pkt = pkts[0]

            # parsing
            # FIXME assumes specific length of lower layers (no vxlan, etc...)
            capwap_bytes = pkt[42:]

            capwap_hlen = (struct.unpack('!B', capwap_bytes[1:2])[
                0] & 0b11111000) >> 1
            ctrl_header_type = struct.unpack(
                '!B', capwap_bytes[capwap_hlen + 3:capwap_hlen + 4])[0]
            result_code = CAPWAP_PKTS.parse_message_elements(
                capwap_bytes, capwap_hlen, self.ap, self)


            if result_code in (None, 0, 2) and ctrl_header_type == expected_response_type:
                # good
                return True
            elif result_code != -1:
                # non expected response
                self.ap.logger.warn('Service %s control round trip: Not successful result %s - %s for response type %d.' % (self.name, result_code, capwap_result_codes.get(result_code, 'Unknown'), ctrl_header_type))
                return False
            else:
                continue

        # timeout
        self.ap.logger.info(
                "Service {} control round trip: rollback: timeout: too many trials".format(self.name))
        return False

    class Connection:
        """Connection (e.g. pipe end) wrapper for sending packets from an AP."""

        def __init__(self, connection, ap):
            """Construct a Connection.

            Args:
                connection: a Connection (e.g. pipe end), that has a 'send' method
                ap: an AP
            """
            self.connection = connection

        def send(self, pkt):
            """Send a packet on to connection.
            Send the packet as is with no added layers.
            """
            self.connection.send(pkt)

