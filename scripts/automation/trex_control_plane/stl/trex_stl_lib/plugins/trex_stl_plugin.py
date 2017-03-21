"""
Base plugin development


Description:
  Base classes used to implement a plugin

Author:
  Itay Marom

"""

import simpy
from ..trex_stl_exceptions import STLError

class STLPluginCtx(object):
    def __init__ (self, client, port):
        self.client     = client
        self.port       = port
        self.env        = simpy.rt.RealtimeEnvironment(factor=1)
        self.plugins    = []
        self.tx_buffer  = []
        
######### API functions              #########

    def pipe (self):
        return STLPluginPipe(self.env, self.__tx_pkt)

        
    def err (self, msg):
        raise Exception(msg)


    def run (self):
        # launch the plugins
        for plugin in self.plugins:
            plugin.run()
        
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
            self.client.set_service_mode(ports = self.port, enabled = False)
            

######### internal functions              #########

    def _register_plugin (self, plugin):
        self.plugins.append(plugin)

    def _add_task (self, task):
        self.env.process(task)

    def __tx_pkt (self, pkt):
        self.tx_buffer.append(pkt)
        

    def tick_process (self):
        while True:
            #print('tick')
            # if no other process exists - exit
            if len(self.env._queue) == 0:
                print('nothing to do')
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
            
            # for each packet - try to forward to each plugin until we hit
            for scapy_pkt in scapy_pkts:
                scapy_pkt.show2()
                for plugin in self.plugins:
                    if plugin.consume(scapy_pkt):
                        # found some plugin that accepts the packet
                        break
                        
                
            # backoff
            yield self.env.timeout(1)
            
            

class STLPluginPipe(object):
    '''
        A pipe used to communicate between
        each instance spawn by the plugin
    '''
   
    def __init__ (self, env, tx_cb):
        self.env    = env
        self.tx_cb  = tx_cb
        self.store  = simpy.Store(self.env)

        
        
    def tx_pkt (self, tx_pkt):
        self.tx_cb(tx_pkt)

    def wait_for_timeout (self, timeout_sec):
        return self.env.timeout(timeout_sec)

    def wait_for_next_pkt (self, timeout_sec):
        return self.store.get() | self.env.timeout(timeout_sec)

    def is_timeout (self, event):
        return any([isinstance(x, simpy.Timeout) for x in event])
        
    def on_pkt_rx (self, pkt):
        self.store.put(pkt)


class STLPlugin(object):

######### implement-needed functions #########
    def consume (self, pkt):
        ''' 
            handles a packet if it belongs to this module 
            returns True if packet was consumed and False
            if not
            
            pkt - A scapy formatted packet
        '''
        raise NotImplementedError
        
    
    def run (self, ctx):
        '''
            executes the plugin in a run until completion
            model
        '''
        raise NotImplementedError
        

