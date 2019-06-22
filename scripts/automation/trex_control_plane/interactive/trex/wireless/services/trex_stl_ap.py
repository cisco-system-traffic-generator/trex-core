import base64
import random
import struct
import sys
import threading
import time
from collections import deque
from enum import Enum

import simpy
import wireless.pubsub.pubsub as pubsub
from scapy.all import *
from scapy.contrib.capwap import *
from scapy.contrib.capwap import CAPWAP_PKTS
from trex_openssl import *
# from trex_stl_lib.api import *
from trex.common.services.trex_service import Service, ServiceFilter
from trex.utils.common import PassiveTimer

from ..pubsub.broker import deregister_sub, register_sub, subscribers
from ..pubsub.message import PubSubMessage
from ..trex_wireless_ap_state import *
from ..trex_wireless_client_state import *
from .trex_wireless_service_event import WirelessServiceEvent

'''
FSMs for AP:
   * Discover WLC
   * Establish DTLS session
   * Join WLC
   * Add client (station)
   * Shutdown DTLS session
   * Maintenance (arp, ping, capwap echo request, fetches rx and dispatches to rx_buffer of APs)
'''

sleep_for_packet = 0.3
sleep_for_state = 0.5


'''
Just assign services to AP, it will get packets from AP's rx_buffer
'''


class APJoinDisconnectedEvent(WirelessServiceEvent):
    """Raised when AP is deassociated from the WLC."""
    def __init__(self, env, device):
        service = ServiceAPRun.__name__
        value = "disconnected"
        super().__init__(env, device, service, value)

class APJoinConnectedEvent(WirelessServiceEvent):
    """Raised when AP is Joined to the WLC."""
    def __init__(self, env, device):
        service = ServiceAPRun.__name__
        value = "joined"
        super().__init__(env, device, service, value)

class APVAPReceivedEvent(WirelessServiceEvent):
    """Raised when AP received VAPs from the WLC."""
    def __init__(self, env, device):
        service = ServiceAPRun.__name__
        value = "vap_received"
        super().__init__(env, device, service, value)

class APDTLSEstablishedEvent(WirelessServiceEvent):
    """Raised when AP's DTLS is established with WLC."""
    def __init__(self, env, device):
        service = ServiceAPRun.__name__
        value = "established"
        super().__init__(env, device, service, value)


class ServiceFilterPerAp(ServiceFilter):
    def __init__(self):
        self.services_per_ap = {}

    def add(self, service):
        if service.ap in self.services_per_ap:
            self.services_per_ap[service.ap].append(service)
        else:
            self.services_per_ap[service.ap] = [service]


class ServiceAP(Service):
    requires_dtls = True
    client_concurrent = 0
    max_client_concurrent = 1
    concurrent = 0
    max_concurrent = float('inf')  # changed when called by join_aps

    def __init__(self, worker, ap, env, topics_to_subs, verbose_level=Service.WARN):
        Service.__init__(self, verbose_level)
        self.worker = worker
        self.ap = ap
        self.name = self.__class__.__name__
        self.env = env
        self.topics_to_subs = topics_to_subs

    def raise_event(self, event):
        """Raise a WirelessServiceEvent.

        Args:
            event: the WirelessServiceEvent to raise
        """
        self.ap.pubsub.publish(event.value, event.topics)

        # create a pubsubmessage
        pubsub_message = PubSubMessage(event.value, event.topics)

        # if others are waiting on this event, wake them and deregister
        if self.topics_to_subs:
            subscriptions = subscribers(self.topics_to_subs, pubsub_message)
            for sub in subscriptions:
                sub.trigger()
                deregister_sub(self.topics_to_subs, sub)

    def stop_and_launch_service(self, service, pipe):
        with self.worker.services_lock:
            if self in self.worker.stl_services:
                del self.worker.stl_services[self]
            self.worker.stl_services[service] = {'pipe': pipe}
        return self.ap.register_service(service, self.env.process(service.run(pipe)))

    def get_filter_type(self):
        return ServiceFilterPerAp

    def timeout(self):
        self.ap.warn('Timeout in FSM %s' % self.name)

    def err(self, msg):
        self.ap.logger.warn('Error in FSM %s: %s' % (self.name, msg))

    def send(self, v):
        self.worker.pkt_pipe.send(v)

    def wait(self, pipe, val):
        return pipe.async_wait(val)

    def run(self, pipe):
        try:
            run_gen = self.run_with_buffer()
            send_data = None
            while True:
                try:
                    action = run_gen.send(send_data)
                except StopIteration:
                    action = 'done'
                if type(action) is tuple and len(action) == 2:
                    action, val = action
                elif type(action) is tuple and len(action) == 3:
                    action, val1, val2 = action

                # if action == 'get':
                #     send_data = None
                #     v = wait_for_pkt()
                #     resp = yield v
                #     if resp:
                #         send_data = resp[0]['pkt']

                if action == 'put':
                    # send packet
                    if type(val) is list:
                        for v in val:
                            # pipe.async_tx_pkt(v)
                            self.send(v)
                    else:
                        self.send(val)

                        # pipe.async_tx_pkt(val)
                elif action == 'sleep':
                    # async sleep
                    v = self.wait(pipe, val)
                    yield v
                elif action == 'service':
                    # launch a service in the current service
                    # current service will therefore stop running
                    # when done, give hand back to former service
                    service = val
                    with self.worker.services_lock:
                        if self in self.worker.stl_services:
                            del self.worker.stl_services[self]
                        self.worker.stl_services[service] = {'pipe': pipe}
                    # launch process
                    stop = yield self.ap.register_service(service, self.env.process(service.run(pipe)))
                    if stop:
                        return True
                    # process returned
                    with self.worker.services_lock:
                        if service in self.worker.stl_services:
                            del self.worker.stl_services[service]
                        self.worker.stl_services[self] = {'pipe': pipe}
                elif action == 'done':
                    # stop the service
                    self.ap.logger.debug(
                        'Finished successfully FSM %s' % self.name)
                    break
                elif action == 'process_with_timeout':
                    # yields the given simpy process and a timeout
                    # wakes up when either completes
                    yield simpy.events.AnyOf(self.env, [val1, self.env.timeout(val2)])
                elif action == 'start_ap':
                    # async wait until free spot to continue
                    self.ap.ap_concurrent_request = simpy.resources.resource.Request(
                        ServiceAP.ap_concurrent)
                    yield self.ap.ap_concurrent_request
                elif action == 'done_ap':
                    # free the current spot for other APs to start join process
                    yield simpy.resources.resource.Release(ServiceAP.ap_concurrent, self.ap.ap_concurrent_request)
                elif action == 'err':
                    # for service errors
                    self.err(val)
                elif action == 'time':
                    # for timeouts, stops the service
                    self.timeout()
                    break
                elif not action:
                    break
                else:
                    raise Exception('Incorrect action in FSM %s: %s' %
                                    (self.name, action))
        except simpy.events.Interrupt as e:
            pass

def hex(buf, delimiter=' '):
    if not buf:
        return 'Empty buffer'
    return delimiter.join(['%02x' % (c if type(c) is int else ord(c)) for c in buf])

################ FSMs ##################

class ServiceAPDiscoverWLC(ServiceAP):
    """Discovery Service for one AP."""

    requires_dtls = False

    def run_with_buffer(self):
        from ..trex_wireless_config import config

        # request a slot
        yield ("start_ap")

        self.ap.logger.info("Service DiscoverWLC started")

        self.ap.start_join_time = time.time()

        while True:

            if self.ap.state > APState.DISCOVER:
                return

            yield("service", ServiceAPShutdownDTLS(self.worker, self.ap, self.env, self.topics_to_subs))

            self.ap.reset_vars()

            self.ap.state = APState.DISCOVER

            self.ap.active_service = self

            self.ap._create_ssl(config.openssl.buffer_size)

            self.ap.rx_responses[2] = -1

            self.ap.retries += 1

            # First resolve WLC MAC if needed
            if self.ap.wlc_ip_bytes and not self.ap.wlc_mac_bytes:
                self.ap.logger.info(
                    "Resolving WLC MAC for IP: %s" % self.ap.wlc_ip)
                while not self.ap.wlc_mac_bytes:
                    RetransmitInterval = config.capwap.retransmit_interval
                    for _ in range(config.capwap.max_retransmit):
                        if self.ap.wlc_mac_bytes:
                            break
                        RetransmitInterval *= 2
                        self.ap.logger.debug("sending who-as for WLC IP")
                        arp = self.ap.get_arp_pkt(
                            'who-has', src_mac_bytes=self.ap.mac_bytes, src_ip_bytes=self.ap.ip_bytes, dst_ip_bytes=self.ap.wlc_ip_bytes)
                        yield('put', arp)

                        # waiting for an arp response
                        self.waiting_on = simpy.events.Event(self.env)
                        yield ("process_with_timeout", self.waiting_on, RetransmitInterval)
                        del self.waiting_on

                        if self.ap.wlc_mac_bytes:
                            self.ap.logger.debug("got MAC of WLC")
                            # done
                            break

            if self.ap.wlc_ip_bytes and not self.ap.wlc_mac_bytes:
                self.err(
                    'err', 'Unable to resolve MAC address of WLC for %s' % self.ap.wlc_ip)

            RetransmitInterval = config.capwap.retransmit_interval
            for _ in range(config.capwap.max_retransmit):
                RetransmitInterval *= 2
                discovery_pkt = self.ap.wrap_capwap_pkt(
                    CAPWAP_PKTS.discovery(self.ap), is_discovery=True)
                self.ap.logger.debug("sending discovery request to WLC")
                yield('put', discovery_pkt)

                self.waiting_on = simpy.events.Event(self.env)
                yield ("process_with_timeout", self.waiting_on, RetransmitInterval)
                del self.waiting_on

                try:
                    result_code = self.ap.rx_responses[2]
                except KeyError:
                    result_code = -1
                    yield("err", 'No response')
                if result_code in (None, 0, 2):
                    self.ap.state = APState.DTLS
                    self.ap.logger.info("Service DiscoverWLC finished")
                    yield ("service", ServiceAPEstablishDTLS(self.worker, self.ap, self.env, self.topics_to_subs))
                elif result_code != -1:
                    self.ap.wlc_mac_bytes = None
                    self.ap.wlc_mac = None
                    yield("err", 'Not successful result %s - %s.' % (result_code,
                                                                     capwap_result_codes.get(result_code, 'Unknown')))
                    break

            self.ap.logger.info("DiscoverWLC retries expired")


class ServiceAPEstablishDTLS(ServiceAP):
    """Service which tries to setup DTLS with remote controller."""
    requires_dtls = False
    aps_by_ssl = {}

    @staticmethod
    def openssl_callback(ssl, where, ret):
        pkt_pipe = ServiceAPEstablishDTLS.aps_by_ssl[ssl]['pkt_pipe']
        ap = ServiceAPEstablishDTLS.aps_by_ssl[ssl]['ap']
        logger = ServiceAPEstablishDTLS.aps_by_ssl[ssl]['logger']

        if where & SSL_CONST.SSL_CB_ALERT:
            ap.state = APState.DISCOVER
            ap.logger.info("Received DTLS Alert")
            return 0

        if not ap.mac_bytes:
            return 0

        if libcrypto.BIO_ctrl_pending(ap.out_bio):
            ssl_data = ap.ssl_read()
            ap.nb_write += 1
            if ap.state != APState.DTLS:
                return 0
            if ssl_data:
                # # This is due to a bug in the ordering of UDP packet received... to be investigated
                # if ap.nb_write > 7 and len(ssl_data) > 1000 and not ap.is_handshake_done_libssl():
                #     with ap.ssl_lock:
                #         timeout = libssl.DTLSv1_get_timeout(ap.ssl)
                #         timeout = timeout.tv_sec + timeout.tv_usec * 1e-6
                #     print("Sending bad DTLS packet ????: %d timeout %d %f %s" % (len(ssl_data), ap.timeout_dtls, timeout, ap.name))
                #     return 0
                pkt = ap.wrap_capwap_pkt(b'\1\0\0\0' + ssl_data)
                logger.debug(
                    "sending dtls packet (openssl callback @ DTLS service)")

                pkt_pipe.send(pkt)
        return 0

    ssl_info_callback_type = CFUNCTYPE(c_int, c_void_p, c_int, c_int)
    ssl_info_callback_func = ssl_info_callback_type(openssl_callback.__func__)

    def run(self, pipe):
        from ..trex_wireless_config import config

        # assert self.ap.ssl and (self.ap.ssl not in self.aps_by_ssl)

        self.ap.logger.info("Service ApEstablishDTLS started")

        try:
            while True:
                if self.ap.state != APState.DTLS:
                    self.ap.logger.info("Service ApEstablishDTLS rollback")
                    return

                self.ap.active_service = self

                self.aps_by_ssl[self.ap.ssl] = {
                    'ap': self.ap, 'pkt_pipe': self.worker.pkt_pipe, 'logger': self.ap.logger}
                self.ap.nb_write = 0
                self.ap.timeout_dtls = 0

                with self.ap.ssl_lock:
                    libssl.SSL_clear(self.ap.ssl)
                    libssl.SSL_set_info_callback(
                        self.ap.ssl, self.ssl_info_callback_func)  # set ssl callback
                    timeout = libssl.DTLSv1_get_timeout(self.ap.ssl)
                    timeout = min(
                        max(timeout.tv_sec + timeout.tv_usec * 1e-6, sleep_for_packet), 3)
                    libssl.SSL_do_handshake(self.ap.ssl)
                try:
                    timer = PassiveTimer(config.dtls.timeout)
                    while not timer.has_expired():
                        if self.ap.state != APState.DTLS:
                            return
                        if self.ap.is_handshake_done_libssl():
                            # session established
                            self.ap.state = APState.JOIN
                            event = APDTLSEstablishedEvent(self.env, self.ap.mac)
                            self.raise_event(event)
                            
                            self.ap.logger.info(
                                "Service ApEstablishDTLS finished successfully")
                            stop = yield self.stop_and_launch_service(ServiceAPJoinWLC(self.worker, self.ap, self.env, self.topics_to_subs), pipe)
                            if stop:
                                return True
                            return
                        resps = yield pipe.async_wait_for_pkt(time_sec=timeout, limit=1)
                        self.ap.logger.debug(
                            "got %s packets at DTLS service" % len(resps))
                        if not resps:
                            if self.ap.is_handshake_done_libssl():
                                # session established
                                self.ap.state = APState.JOIN
                                event = APDTLSEstablishedEvent(self.env, self.ap.mac)
                                self.raise_event(event)

                                self.ap.logger.info(
                                    "Service ApEstablishDTLS finished successfully")
                                stop = yield self.stop_and_launch_service(ServiceAPJoinWLC(self.worker, self.ap, self.env, self.topics_to_subs), pipe)
                                if stop:
                                    return True

                                return

                            # Make DTLS timeout to retransmit
                            with self.ap.ssl_lock:
                                libssl.DTLSv1_handle_timeout(self.ap.ssl)
                                timeout = libssl.DTLSv1_get_timeout(self.ap.ssl)
                                timeout = min(
                                    max(timeout.tv_sec + timeout.tv_usec * 1e-6, sleep_for_packet), 3)
                                self.ap.timeout_dtls += 1
                                ret = libssl.SSL_do_handshake(self.ap.ssl)
                                if ret <= 0:
                                    try:
                                        # looking up the error
                                        self.ap.logger.error("SSL Handshake error: %s: %s" % (
                                            ret, SSL_CONST.ssl_err[libssl.SSL_get_error(self.ap.ssl, ret)]))
                                    except:
                                        pass
                            continue
                        if self.ap.state != APState.DTLS:
                            return
                        pkt_bytes = resps[0]['pkt']
                        is_dtls = struct.unpack('?', pkt_bytes[42:43])[0]
                        if is_dtls:
                            self.ap.decrypt(pkt_bytes[46:])

                    if self.ap.state <= APState.DTLS:
                        self.ap.state = APState.DISCOVER
                        self.ap.logger.info(
                            "Service ApEstablishDTLS rollback: timer expired")
                        stop = yield self.stop_and_launch_service(ServiceAPDiscoverWLC(self.worker, self.ap, self.env, self.topics_to_subs), pipe)
                        return stop
                finally:
                    with self.ap.ssl_lock:
                        libssl.SSL_set_info_callback(
                            self.ap.ssl, None)  # remove ssl callback
                    if self.ap.ssl in self.aps_by_ssl:
                        del self.aps_by_ssl[self.ap.ssl]
        except simpy.events.Interrupt:
            self.ap.logger.debug("Service ApEstablishDTLS interrupted")
            return True

class ServiceAPEncryptedControl(ServiceAP):

    def control_round_trip(self, tx_pkt, expected_response_type, debug_msg=None):
        """Send the packet, wait for the expected answer, and retry."""
        from ..trex_wireless_config import config
        self.ap.rx_responses[expected_response_type] = -1
        RetransmitInterval = config.capwap.retransmit_interval
        for i in range(config.capwap.max_retransmit):
            if not self.ap.is_dtls_established:
                yield 'dtls_broke'
                return
            if not self.ap.is_dtls_established:
                break
            RetransmitInterval *= 2
            encrypted = self.ap.encrypt(tx_pkt)
            if encrypted and encrypted != '':
                tx_pkt_wrapped = self.ap.wrap_capwap_pkt(
                    b'\1\0\0\0' + self.ap.encrypt(tx_pkt))

                self.ap.logger.debug("sending packet")
                yield ('put', tx_pkt_wrapped)
            else:
                continue
            timer = PassiveTimer(RetransmitInterval)

            self.waiting_on = simpy.events.Event(self.env)
            yield ("process_with_timeout", self.waiting_on, RetransmitInterval)
            del self.waiting_on

            if expected_response_type in self.ap.rx_responses:
                result_code = self.ap.rx_responses[expected_response_type]
                if result_code in (None, 0, 2):
                    # expected response
                    yield 'good_resp'
                if result_code != -1:
                    # non expected response
                    yield ('err', 'Not successful result %s - %s.' % (result_code, capwap_result_codes.get(result_code, 'Unknown')))
                    return
                else:
                    continue
            else:
                continue
        # timeout
        self.ap.state = APState.DISCOVER
        if self.ap.is_dtls_established:
            self.ap.logger.info(
                "Service ApJoinWLC rollback: dtls broke")
        else:
            self.ap.logger.info(
                "Service ApJoinWLC rollback: timeout: too many trials")
        yield 'time'


class ServiceAPJoinWLC(ServiceAPEncryptedControl):
    """"Join state and Configuration state simulation for an AP."""

    def run_with_buffer(self):
        from ..trex_wireless_config import config

        def rollback(reason=None):
            if reason:
                self.ap.logger.info("Service ApJoinWLC rollback: %s" % reason)
            self.ap.state = APState.DISCOVER
            return ('service', ServiceAPDiscoverWLC(self.worker, self.ap, self.env, self.topics_to_subs))

        try:

            while True:
                if self.ap.state != APState.JOIN:
                    return

                self.ap.logger.info("Service ApJoinWLC started")

                self.ap.active_service = self

                self.ap.logger.debug('Sending Join Request')
                join_req = CAPWAP_PKTS.join(self.ap)

                ctrl_gen = self.control_round_trip(
                    join_req, 4, debug_msg='Join Request')
                send_data = None
                while True:
                    action = ctrl_gen.send(send_data)
                    if action in ('good_resp', 'time', 'dtls_broke', 'err'):
                        ctrl_gen.close()
                        break
                    else:
                        send_data = yield action
                if action == 'dtls_broke':
                    yield(rollback("dtls session broken"))
                    return
                elif action == 'time':
                    yield rollback("timeout: join request")
                    return
                elif action == 'err':
                    yield rollback("error")
                    return

                self.ap.logger.debug('Got Join Response')

                self.ap.logger.debug('Sending Configuration Status Request')
                ctrl_gen = self.control_round_trip(CAPWAP_PKTS.conf_status_req(
                    self.ap), 6, debug_msg='Config status request')
                send_data = None
                while True:
                    action = ctrl_gen.send(send_data)
                    if action in ('good_resp', 'time', 'dtls_broke', 'err'):
                        ctrl_gen.close()
                        break
                    else:
                        send_data = yield action
                if action == 'dtls_broke':
                    yield(rollback("dtls session broken"))
                    return
                elif action == 'time':
                    yield rollback("timeout: join request")
                    return
                elif action == 'err':
                    yield rollback("error")
                    return

                self.ap.logger.debug('Got Configuration Status Response')

                self.ap.logger.debug('Sending Change State Event Request')
                ctrl_gen = self.control_round_trip(CAPWAP_PKTS.change_state(
                    self.ap, radio_id=0), 12, debug_msg='Change state event request')
                send_data = None
                while True:
                    action = ctrl_gen.send(send_data)
                    if action in ('good_resp', 'time', 'dtls_broke', 'err'):
                        ctrl_gen.close()
                        break
                    else:
                        send_data = yield action
                        if action == "err":
                            break
                if action == 'dtls_broke':
                    yield(rollback("dtls session broken"))
                    return
                elif action == 'time':
                    yield rollback("timeout: join request")
                    return
                elif action == 'err':
                    yield rollback("error")
                    return

                self.ap.logger.debug('Got Change State Event Response')

                self.ap.logger.debug('Sending Change State Event Request')
                ctrl_gen = self.control_round_trip(CAPWAP_PKTS.change_state(
                    self.ap, radio_id=1), 12, debug_msg='Change state event request')
                send_data = None
                while True:
                    action = ctrl_gen.send(send_data)
                    if action in ('good_resp', 'time', 'dtls_broke', 'err'):
                        ctrl_gen.close()
                        break
                    else:
                        send_data = yield action
                        if action == "err":
                            break
                if action == 'dtls_broke':
                    yield(rollback("dtls session broken"))
                    return
                elif action == 'time':
                    yield rollback("timeout: join request")
                    return
                elif action == 'err':
                    yield rollback("error")
                    return

                self.ap.logger.debug('Got Change State Event Response')

                self.ap.logger.debug(
                    'Going to ack all config updates and try to get SSID')

                # ack all config updates in worker_traffic_handler thread
                # while not self.ap.last_recv_ts or self.ap.last_recv_ts + 5 >= time.time():
                #     self.waiting_on = simpy.events.Event(self.env)
                #     yield ("process_with_timeout", self.waiting_on, 5)
                #     del self.waiting_on

                #     if not self.ap.is_dtls_established:
                #         yield rollback("dtls not established")
                #         return
                #     elif self.ap.SSID:
                #         break
                #     if not self.ap.last_recv_ts:
                #         break

                self.waiting_on = simpy.events.Event(self.env)
                yield ("process_with_timeout", self.waiting_on, config.capwap.specific.ssid_timeout)
                del self.waiting_on

                if not self.ap.SSID:
                    yield rollback("no SSID")
                    return


                self.ap.logger.info("received SSID, proceding with sending data keep_alive")

                RetransmitInterval = config.capwap.retransmit_interval
                for _ in range(config.capwap.max_retransmit):
                    if self.ap.state == APState.RUN:
                        break
                    if not self.ap.is_dtls_established:
                        yield rollback("dtls not established")
                        return
                    RetransmitInterval *= 2
                    tx_pkt = self.ap.wrap_capwap_pkt(
                        CAPWAP_PKTS.keep_alive(self.ap), dst_port=5247)

                    self.ap.logger.debug("Sending keep-alive")
                    yield ('put', tx_pkt)

                    self.waiting_on = simpy.events.Event(self.env)
                    yield ("process_with_timeout", self.waiting_on, RetransmitInterval)
                    del self.waiting_on

                    if self.ap.got_keep_alive:
                        self.ap.got_keep_alive = False
                        self.ap.logger.debug('Received Keep-alive response.')
                        self.ap.last_echo_req_ts = time.time()
                        self.ap.join_time = time.time()
                        self.ap.join_duration = self.ap.join_time - self.ap.start_join_time
                        self.ap.state = APState.RUN
                        self.ap.logger.info("Service ApJoinWLC finished")
                        # release spot
                        yield("done_ap")
                        yield ('service', ServiceAPRun(self.worker, self.ap, self.env, self.topics_to_subs))
                        return
                    if not self.ap.is_dtls_established:
                        break

                # timeout

                if not self.ap.is_dtls_established:
                    yield rollback("DTLS session broken")
                    return
                # too many trials or failure
                if self.ap.state == APState.JOIN:
                    yield rollback("too many trials")
                    return
        except simpy.events.Interrupt:
            self.ap.logger.debug("Service APJoinWLC interrupted")
            return True

class ServiceAPRun(ServiceAP):
    """Run state simulation for an AP.

    Send periodic Echo Request to the WLC.
    """

    def run_with_buffer(self):
        from ..trex_wireless_config import config

        try:

            ap = self.ap
            self.publish = pubsub.Publisher(ap.pubsub, "").publish
            # publish Joined event
            event = APJoinConnectedEvent(self.env, ap.mac)
            self.raise_event(event)

            def rollback(reason=None):
                if reason:
                    self.ap.logger.info("Service ApRun rollback: %s" % reason)
                self.ap.state = APState.DISCOVER
                return ('service', ServiceAPDiscoverWLC(self.worker, self.ap, self.env, self.topics_to_subs))

            self.ap.logger.info("Service ApRun started")


            wait_time = config.capwap.echo_interval

            while True:
                if ap.state != APState.RUN:
                    break

                if ap.got_disconnect:
                    event = APJoinDisconnectedEvent(self.env, ap.name)
                    self.raise_event(event)

                    ap.got_disconnect = False
                    # if disconnected, disconnect the clients
                    for client in ap.clients:
                        client.got_disconnect = True
                        try:
                            client.got_disconnected_event.succeed()
                        except (RuntimeError, AttributeError):
                            # already triggered or not waiting for this packet
                            pass
                    

                    rollback("got disconnected")


                yield ("sleep", wait_time)
                # echo_resp_timer is set when a response is stil expected (waiting), otherwise is None


                if ap.echo_resp_timer and ap.echo_resp_timer.has_expired():
                    # no echo response received, retry
                    if ap.echo_resp_retry <= config.capwap.max_retransmit:
                        ap.echo_resp_timeout *= 2
                        ap.echo_resp_timer = PassiveTimer(ap.echo_resp_timeout)
                        ap.echo_resp_retry += 1

                        tx_pkt = ap.get_echo_capwap()
                        ap.logger.debug(
                            "sending echo request after timeout, retries: %s" % ap.echo_resp_retry)
                        encrypted = ap.encrypt(tx_pkt)
                        if encrypted:
                            self.worker.pkt_pipe.send(ap.get_echo_wrap(encrypted))
                    else:
                        event = APJoinDisconnectedEvent(self.env, ap.mac)
                        self.raise_event(event)
                        self.ap.logger.warn(
                            "Timeout in echo response, disconnecting AP")
                        ap.echo_resp_timeout = config.capwap.retransmit_interval
                        ap.echo_resp_retry = 0
                        yield rollback("timeout in echo response")

                if time.time() > ap.last_echo_req_ts + config.capwap.echo_interval:
                    # echo_req_timer passed, send new echo
                    tx_pkt = ap.get_echo_capwap()
                    ap.last_echo_req_ts = time.time()
                    ap.echo_resp_timer = PassiveTimer(ap.echo_resp_timeout)
                    ap.logger.debug("sending echo request")
                    encrypted = ap.encrypt(tx_pkt)
                    if encrypted:
                        self.worker.pkt_pipe.send(ap.get_echo_wrap(encrypted))

        except simpy.events.Interrupt:
            self.ap.logger.debug("Service ApRun interrupted")
            return True
        self.ap.logger.info("Service ApRun finished")

class ServiceAPShutdownDTLS(ServiceAP):
    def run(self, pipe):
        yield pipe.async_wait(0)
        self.ap.logger.info("Service ApShutdownDTLS started")
        with self.ap.ssl_lock:
            libssl.SSL_shutdown(self.ap.ssl)
        ssl_data = self.ap.ssl_read()
        if ssl_data:
            try:
                tx_pkt = self.ap.wrap_capwap_pkt(b'\1\0\0\0' + ssl_data)
                self.send(tx_pkt)
            except:
                return


class ServiceAPShutdown(ServiceAP):
    """Service stopping an AP, sending a 'close notify' to the controller if needed.

    When done, the state of the AP is CLOSED.
    """
    aps_by_ssl = {}

    @staticmethod
    def openssl_callback(ssl, where, ret):
        pkt_pipe = ServiceAPShutdown.aps_by_ssl[ssl]['pkt_pipe']
        ap = ServiceAPShutdown.aps_by_ssl[ssl]['ap']
        logger = ServiceAPShutdown.aps_by_ssl[ssl]['logger']

        if libcrypto.BIO_ctrl_pending(ap.out_bio):
            ssl_data = ap.ssl_read()
            ap.nb_write += 1
            if ap.state != APState.CLOSING:
                return 0
            if ssl_data:
                # This is due to a bug in the ordering of UDP packet received... to be investigated
                if ap.nb_write > 7 and len(ssl_data) > 1000 and not ap.is_handshake_done_libssl():
                    with ap.ssl_lock:
                        timeout = libssl.DTLSv1_get_timeout(ap.ssl)
                        timeout = timeout.tv_sec + timeout.tv_usec * 1e-6
                    return 0
                pkt = ap.wrap_capwap_pkt(b'\1\0\0\0' + ssl_data)
                logger.debug(
                    "sending dtls packet (openssl callback @ ApShutdown)")
                pkt_pipe.send(pkt)
        return 0

    ssl_info_callback_type = CFUNCTYPE(c_int, c_void_p, c_int, c_int)
    ssl_info_callback_func = ssl_info_callback_type(openssl_callback.__func__)

    def run(self, pipe):
        try:
            from ..trex_wireless_config import config
            yield pipe.async_wait(0)

            event = APJoinDisconnectedEvent(self.env, self.ap.name)
            self.raise_event(event)

            self.ap.logger.info("Service ApShutdown started")
            if self.ap.state <= APState.DISCOVER or self.ap.state > APState.CLOSING:
                self.ap.state = APState.CLOSED
                self.ap.logger.info("Service ApShutdown stopped : AP is already shut")
                return

            self.ap.active_service = self

            ServiceAPShutdown.aps_by_ssl[self.ap.ssl] = {
                'ap': self.ap, 'pkt_pipe': self.worker.pkt_pipe, 'logger': self.ap.logger}
            self.ap.nb_write = 0
            self.ap.timeout_dtls = 0

            try:
                # DTLS Shutdown
                with self.ap.ssl_lock:
                    libssl.SSL_set_info_callback(
                        self.ap.ssl, self.ssl_info_callback_func)  # set ssl callback
                cnt = 0
                with self.ap.ssl_lock:
                    ret = libssl.SSL_shutdown(self.ap.ssl)
                    # want read / want write
                    while ret == 0 or libssl.SSL_get_error(self.ap.ssl, ret) in (2, 3):
                        # retry
                        timeout = libssl.DTLSv1_get_timeout(self.ap.ssl)
                        timeout = min(
                            max(timeout.tv_sec + timeout.tv_usec * 1e-6, sleep_for_packet), 3)
                        yield pipe.async_wait(timeout)
                        ret = libssl.SSL_shutdown(self.ap.ssl)
                        cnt += 1
                        if cnt > config.dtls.shutdown_max_retransmit:
                            break
                    if ret == 1:
                        self.ap.logger.debug("SSL Shutdown success")
                    else:
                        try:
                            self.ap.logger.warn("SSL Shutdown error: %s: %s" % (
                                ret, SSL_CONST.ssl_err[libssl.SSL_get_error(self.ap.ssl, ret)]))
                        except KeyError:
                            # unknown error
                            pass
            finally:
                with self.ap.ssl_lock:
                    libssl.SSL_set_info_callback(
                        self.ap.ssl, None)  # remove ssl callback
                if self.ap.ssl in ServiceAPShutdown.aps_by_ssl:
                    del ServiceAPShutdown.aps_by_ssl[self.ap.ssl]

            self.ap.logger.info("Service ApShutdown finished")

            self.ap.state = APState.CLOSED
            self.ap.reset_vars()
        except simpy.events.Interrupt:
            self.ap.logger.info("Service ApShutdown interrupted")
            return True

        return

class ServiceInfoEvent(Service):
    """Service gathering info on aps and clients joins."""
    def __init__(self, worker, verbose_level=Service.WARN):
        Service.__init__(self, verbose_level) # TODO
        self.worker = worker
        self.name = ServiceInfoEvent.__class__.__name__

    def get_filter_type(self):
        return None

    def run(self, pipe):
        ap_joined = False
        client_joined = False
        while not ap_joined or not client_joined:
            with self.worker.aps_lock:
                aps = list(self.worker.aps)
            if not ap_joined and len([ap for ap in aps if ap.state != APState.RUN]) == 0:
                # all aps are joined for the first time
                self.worker.ap_join_done_time = time.time()
                ap_joined = True
                self.worker.ap_joined.set()
            with self.worker.clients_lock:
                clients = list(self.worker.clients)
            if clients and ap_joined and not client_joined and (len([c for c in clients if c.state != ClientState.RUN]) == 0):
                self.worker.client_join_done_time = time.time()
                client_joined = True
                # self.worker.client_joined.set()
                
            yield pipe.async_wait(0.5)
            # print( len([ap for ap in self.manager.aps if ap.state != APState.RUN]) )

                    

class ServiceEventManager(Service):
    """Service waiting on queue for events to be 'triggered' (set)."""
    def __init__(self, event_store):
        """Build a ServiceEventManager.

        Args:
            event_store: SynchronziedStore where all elements are events to be 'succeed'.
        """
        super().__init__()
        self.event_store = event_store
        self.name = ServiceEventManager.__class__.__name__

    def get_filter_type(self):
        return None

    def run(self):
        while True:
            events = yield self.event_store.get(None, 1)
            for event in events:
                try:
                    event.succeed()
                except RuntimeError:
                    # already triggered
                    pass
