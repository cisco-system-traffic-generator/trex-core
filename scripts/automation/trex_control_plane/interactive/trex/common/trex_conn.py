
import time
import signal
import os

from .trex_types import RC_OK, RC_ERR
from .trex_req_resp_client import JsonRpcClient, BatchMessage, ErrNo as JsonRpcErrNo
from .trex_subscriber import TRexSubscriber


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

    def __init__ (self, ctx):

        # hold pointer to context
        self.ctx = ctx

        self.sigint_on_conn_lost   = False

        # low level RPC layer
        self.rpc = JsonRpcClient(ctx)
        
        # async
        self.async = TRexSubscriber(self.ctx, self.rpc)
         
        # init state
        self.state   = (self.DISCONNECTED, None)
        

    def probe_server (self):
        rpc = JsonRpcClient(self.ctx)
        rpc.set_timeout_sec(self.rpc.get_timeout_sec())
        try:
            rpc.connect()
            return rpc.transmit('get_version')
        finally:
            rpc.disconnect()


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
        self.ctx.logger.pre_cmd("Connecting to RPC server on {0}:{1}".format(self.ctx.server, self.ctx.sync_port))
        rc = self.rpc.connect()
        if not rc:
            return rc


        # API sync V2
        rc = self.rpc.transmit("api_sync_v2", params = self.ctx.api_ver)
        self.ctx.logger.post_cmd(rc)
        
        if not rc:
            # api_sync_v2 is not present in v2.30 and older
            if rc.errno() == JsonRpcErrNo.MethodNotSupported:
                return RC_ERR('Mismatch between client and server versions')
            return rc
        
        
        # get the API_H and provide it to the RPC channel from now on
        self.rpc.set_api_h(rc.data()['api_h'])
            

        # connect async channel
        self.ctx.logger.pre_cmd("Connecting to publisher server on {0}:{1}".format(self.ctx.server, self.ctx.async_port))
        rc = self.async.connect()
        self.ctx.logger.post_cmd(rc)

        if not rc:
            return rc


        self.state = (self.CONNECTED, None)

        return RC_OK()


    
