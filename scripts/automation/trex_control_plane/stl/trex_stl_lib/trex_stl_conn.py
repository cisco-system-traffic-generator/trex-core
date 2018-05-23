
from .trex_stl_types import *
from .trex_stl_jsonrpc_client import JsonRpcClient, BatchMessage, ErrNo as JsonRpcErrNo
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
        self.server = server
        self.port = port
        self.rpc_link = JsonRpcClient(self.server, self.port, client)

        # API handler provided by the server
        self.api_h = None
        
    def get_server (self):
        return self.server

    def get_port (self):
        return self.port

    def connect(self):
        return self.rpc_link.connect()

    def disconnect(self):
        self.api_h = None
        return self.rpc_link.disconnect()

    def transmit(self, method_name, params = None, retry = 0):
        sleep_sec = 0.3
        timeout_sec = 3
        poll_tries = int(timeout_sec / sleep_sec)
        rc = self.rpc_link.invoke_rpc_method(method_name, params, self.api_h, retry = retry)
        while not rc and rc.errno() == JsonRpcErrNo.JSONRPC_V2_ERR_TRY_AGAIN:
            if poll_tries == 0:
                return RC_ERR('Server was busy within %s sec, try again later' % timeout_sec)
            poll_tries -= 1
            time.sleep(sleep_sec)
            rc = self.rpc_link.invoke_rpc_method(method_name, params, self.api_h, retry = retry)
        while not rc and rc.errno() == JsonRpcErrNo.JSONRPC_V2_ERR_WIP:
            try:
                params = {'ticket_id': int(rc.err())}
                if poll_tries == 0:
                    self.rpc_link.invoke_rpc_method('cancel_async_task', params, self.api_h)
                    return RC_ERR('Timeout on processing async command, server did not finish within %s second' % timeout_sec)
                poll_tries -= 1
                time.sleep(sleep_sec)
                rc = self.rpc_link.invoke_rpc_method('get_async_results', params, self.api_h, retry = retry)
            except KeyboardInterrupt:
                self.rpc_link.invoke_rpc_method('cancel_async_task', params, self.api_h)
                raise
        return rc

    def transmit_batch(self, batch_list, retry = 0):

        batch = self.rpc_link.create_batch()

        for command in batch_list:
            batch.add(command.method, command.params, self.api_h)
            
        # invoke the batch
        return batch.invoke(retry = retry)


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
        self.api_ver = {'name': 'STL', 'major': 4, 'minor': 3}

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


        # API sync V2
        rc = self.rpc.transmit("api_sync_v2", params = self.api_ver)
        self.logger.post_cmd(rc)
        
        if not rc:
            # api_sync_v2 is not present in v2.30 and older
            if rc.errno() == JsonRpcErrNo.MethodNotSupported:
                return RC_ERR('Mismatch between client and server versions')
            return rc
        
        
        # get the API_H and provide it to the RPC channel from now on
        self.rpc.api_h = rc.data()['api_h']
            

        # connect async channel
        self.logger.pre_cmd("Connecting to publisher server on {0}:{1}".format(self.conn_info['server'], self.conn_info['async_port']))
        rc = self.async.connect()
        self.logger.post_cmd(rc)

        if not rc:
            return rc


        self.state = (self.CONNECTED, None)

        return RC_OK()


    
