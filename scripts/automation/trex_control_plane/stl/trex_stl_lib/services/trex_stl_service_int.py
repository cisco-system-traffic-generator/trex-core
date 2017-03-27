"""
Internal objects for service implementation

Description:
  Internal objects used by the library to implement
  service capabilities

  Objects from this file should not be
  directly created by the user

Author:
  Itay Marom

"""

import simpy
from simpy.core import BoundClass
from ..trex_stl_exceptions import STLError
from scapy.layers.l2 import Ether


##################           
#
# STL service context
#
#
##################

class STLServiceCtx(object):
    '''
        service context provides the
        envoirment for running many services
        and their spawns in parallel
    '''
    def __init__ (self, client, port):
        self.client     = client
        self.port       = port
        self.env        = simpy.rt.RealtimeEnvironment(factor = 1)
        self.services   = []
        self.tx_buffer  = []

        self.active_services = 0

######### API functions              #########

    def pipe (self):
        return STLServicePipe(self.env, self.__tx_pkt)


    def err (self, msg):
        raise STLError(msg)


    def run (self):
        
        # launch all the services
        for service in self.services:
            service.run()


        # set promiscuous mode
        self.client.set_port_attr(ports = self.port, promiscuous = True)
        # move to service mode
        self.client.set_service_mode(ports = self.port)

        try:
            self.capture_id = self.client.start_capture(rx_ports = self.port)['id']

            # add the maintenace process
            tick_process = self.env.process(self.tick_process())

            # start the RT simulation - exit when the tick process dies
            self.env.run(until = tick_process)

        finally:
            self.client.stop_capture(self.capture_id)
            self.client.reset(ports = self.port)
            

######### internal functions              #########

    def _register_service (self, service):
        self.services.append(service)

    def _add_task (self, task):
        p = self.env.process(task)
        self._on_process_create(p)


    def _on_process_create (self, p):
        self.active_services += 1
        p.callbacks.append(self._on_process_exit)


    def _on_process_exit (self, event):
        self.active_services -= 1


    def __tx_pkt (self, pkt):
        self.tx_buffer.append(pkt)


    def tick_process (self):
        while True:
            # if no other process exists - exit
            if self.active_services == 0:
                return

            # if any packets are pending - send them
            if self.tx_buffer:
                # send all the packets
                self.client.push_packets(ports = self.port, pkts = self.tx_buffer, force = True)
                self.tx_buffer = []

            # poll for RX
            pkts = []
            self.client.fetch_capture_packets(self.capture_id, pkts)
            
            scapy_pkts = [Ether(pkt['binary']) for pkt in pkts]

            # for each packet - try to forward to each service until we hit
            for scapy_pkt in scapy_pkts:
                for service in self.services:
                    if service.consume(scapy_pkt):
                        # found some service that accepts the packet
                        break


            # backoff
            yield self.env.timeout(0.1)

#

class PktRX(simpy.resources.store.StoreGet):
    '''
        An event waiting for RX packets
    '''
    def __init__ (self, store, timeout_sec = None):
        super(PktRX, self).__init__(store)

        if timeout_sec is not None:
            self.timeout = store._env.timeout(timeout_sec)
            self.timeout.callbacks.append(self.on_get_timeout)

    def on_get_timeout (self, event):
        '''
            Called when a timeout for RX packet has occured
            The event will be cancled (removed from queue)
            and a None value will be returend
        '''
        if not self.triggered:
            self.cancel()
            self.succeed([])


class Pkt(simpy.resources.store.Store):

    get = BoundClass(PktRX)

    def _do_get (self, event):
        if self.items:
            event.succeed(self.items)
            self.items = []


##################
#
# STL service pipe
#
#
##################

class STLServicePipe(object):
    '''
        A pipe used to communicate between
        each instance spawn by the service

        Services usually spawn many pipes for many parallel
        tasks
    '''

    def __init__ (self, env, tx_cb):
        self.env    = env
        self.tx_cb  = tx_cb
        self.pkt    = Pkt(self.env)

    def async_wait (self, time_sec):
        '''
            Async wait for 'time_sec' seconds
        '''
        return self.env.timeout(time_sec)


    def async_wait_for_pkt (self, time_sec = None):
        '''
            Wait for packet arrival for 'time_sec'

            if 'time_sec' is None will wait infinitly.
            if 'time_sec' is zero it will return immeaditly.
        '''

        return self.pkt.get(time_sec)


    def tx_pkt (self, tx_pkt):
        '''
            Called by the sender side
            to transmit a packet
        '''
        self.tx_cb(tx_pkt)


    def rx_pkt (self, pkt):
        '''
            Called by the reciver side
            (the service)
        '''
        self.pkt.put(pkt)

