
from .trex_stl_types import *
from .trex_stl_jsonrpc_client import JsonRpcClient, BatchMessage
from .trex_stl_async_client import CTRexAsyncClient

import time
import signal
import os


############################     RPC layer     #############################
############################                   #############################
############################                   #############################

class CCommLink(object):
    """Describes the connectivity of the stateless client method"""
    def __init__(self, server="localhost", port=5050, virtual=False, client = None):
        self.virtual = virtual
        self.server = server
        self.port = port
        self.rpc_link = JsonRpcClient(self.server, self.port, client)

    @property
    def is_connected(self):
        if not self.virtual:
            return self.rpc_link.connected
        else:
            return True

    def get_server (self):
        return self.server

    def get_port (self):
        return self.port

    def connect(self):
        if not self.virtual:
            return self.rpc_link.connect()

    def disconnect(self):
        if not self.virtual:
            return self.rpc_link.disconnect()

    def transmit(self, method_name, params = None, api_class = 'core', retry = 0):
        if self.virtual:
            self._prompt_virtual_tx_msg()
            _, msg = self.rpc_link.create_jsonrpc_v2(method_name, params, api_class)
            print(msg)
            return
        else:
            return self.rpc_link.invoke_rpc_method(method_name, params, api_class, retry = retry)

    def transmit_batch(self, batch_list, retry = 0):
        if self.virtual:
            self._prompt_virtual_tx_msg()
            print([msg
                   for _, msg in [self.rpc_link.create_jsonrpc_v2(command.method, command.params, command.api_class)
                                  for command in batch_list]])
        else:
            batch = self.rpc_link.create_batch()
            for command in batch_list:
                batch.add(command.method, command.params, command.api_class)
            # invoke the batch
            return batch.invoke(retry = retry)

    def _prompt_virtual_tx_msg(self):
        print("Transmitting virtually over tcp://{server}:{port}".format(server=self.server,
                                                                         port=self.port))


class Connection(object):
    '''
        Manages that connection to the server

        connection state object
        describes the connection to the server state

        can be either fully disconnected, fully connected
        or marked for disconnection
    '''
    DISCONNECTED          = 1
    CONNECTED             = 2
    MARK_FOR_DISCONNECT   = 3

    def __init__ (self, conn_info, logger, client):

        self.conn_info             = conn_info
        self.logger                = logger
        self.sigint_on_conn_lost   = False

        # API classes
        self.api_vers = [ {'type': 'core', 'major': 3, 'minor': 2 } ]

        # low level RPC layer
        self.rpc = CCommLink(self.conn_info['server'],
                             self.conn_info['sync_port'],
                             self.conn_info['virtual'],
                             client)
        
        
        self.async = CTRexAsyncClient(self.conn_info['server'],
                                      self.conn_info['async_port'],
                                      client)
         
        # save pointers
        self.conn_info  = conn_info

        # init state
        self.state   = (self.DISCONNECTED, None)
        self.api_h   = {'core': None}
        

    def disconnect (self):
        '''
            disconnect from both channels
            sync and async
        '''
        try:
            self.rpc.disconnect()
            self.async.disconnect()

        finally:
            self.state = (self.DISCONNECTED, None)
            self.api_h = {'core': None}
            

    def connect (self):
        '''
            connect to the server (two channels)
        '''

        # first disconnect if already connected
        if self.is_connected():
            self.disconnect()

        # connect
        rc = self.__connect()
        if not rc:
            self.disconnect()

        return rc

        
    def get_api_h (self):
        '''
            return the API handlers for each component
        '''
        return self.api_h
        
        
    def barrier (self):
        '''
            executes a barrier
            when it retruns, an async barrier is guaranteed
            
        '''
        return self.async.barrier()
        
        
    def sync (self):
        '''
            fully sync the client with the server
            must be called after all the config
            was done
        '''
        return self.async.barrier(baseline = True)
 
        
    def mark_for_disconnect (self, cause):
        '''
            A multithread safe call
            any thread can mark the current connection
            as not valid
            and will require the main thread to reconnect
        '''

        # avoid any messages handling for the async thread
        self.async.set_as_zombie()
        
        # change state
        self.state = (self.MARK_FOR_DISCONNECT, cause)

        # if the flag is on, a SIGINT will be sent to the main thread
        # causing the ZMQ RPC to stop what it's doing and report an error
        if self.sigint_on_conn_lost:
            os.kill(os.getpid(), signal.SIGINT)
         
            
    def sigint_on_conn_lost_enable (self):
        '''
            when enabled, if connection
            is lost a SIGINT will be sent
            to the main thread
        '''
        self.sigint_on_conn_lost = True

        
    def sigint_on_conn_lost_disable (self):
        '''
            disable SIGINT dispatching
            on case of connection lost
        '''
        self.sigint_on_conn_lost = False
        
        
    def is_alive (self):
        '''
            return True if any data has arrived 
            the server in the last 3 seconds
        '''
        return ( self.async.last_data_recv_ts is not None and ((time.time() - self.async.last_data_recv_ts) <= 3) )


    def is_connected (self):
        return (self.state[0] == self.CONNECTED)


    def is_marked_for_disconnect (self):
        return self.state[0] == self.MARK_FOR_DISCONNECT


    def get_disconnection_cause (self):
        return self.state[1]


########## private ################

    def __connect (self):
        '''
            connect to the server (two channels)
        '''

        # start with the sync channel
        self.logger.pre_cmd("Connecting to RPC server on {0}:{1}".format(self.conn_info['server'], self.conn_info['sync_port']))
        rc = self.rpc.connect()
        if not rc:
            return rc


        # API sync
        rc = self.rpc.transmit("api_sync", params = {'api_vers': self.api_vers}, api_class = None)
        self.logger.post_cmd(rc)
        
        if not rc:
            return rc
        
        
        # get the API_H
        for api in rc.data()['api_vers']:
            self.api_h[ api['type'] ] = api['api_h']
            

        # connect async channel
        self.logger.pre_cmd("Connecting to publisher server on {0}:{1}".format(self.conn_info['server'], self.conn_info['async_port']))
        rc = self.async.connect()
        self.logger.post_cmd(rc)

        if not rc:
            return rc


        self.state = (self.CONNECTED, None)

        return RC_OK()


    
