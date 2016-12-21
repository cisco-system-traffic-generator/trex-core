
import time

# a generic abstract class for implementing RX services using the server
class RXServiceAPI(object):

    # specify for which layer this service is
    LAYER_MODE_ANY = 0
    LAYER_MODE_L2  = 1
    LAYER_MODE_L3  = 2
    
    def __init__ (self, port, layer_mode = LAYER_MODE_ANY, queue_size = 100):
        self.port = port
        self.queue_size = queue_size
        self.layer_mode = layer_mode

    ################### virtual methods ######################
    
    def get_name (self):
        """
            returns the name of the service

            :returns:
                str

        """

        raise NotImplementedError()
        
    def pre_execute (self):
        """
            specific class code called before executing

            :returns:
                RC object

        """
        raise NotImplementedError()
        
    def generate_request (self):
        """
            generate a request to be sent to the server

            :returns:
                list of streams

        """
        raise NotImplementedError()

    def on_pkt_rx (self, pkt, start_ts):
        """
            called for each packet arriving on RX

            :parameters:
                'pkt' - the packet received
                'start_ts' - the time recorded when 'start' was called 
                
            :returns:
                None for fetching more packets
                RC object for terminating
                
           

        """
        raise NotImplementedError()

        
    def on_timeout_err (self, retries):
        """
            called when a timeout occurs

            :parameters:
                retries - how many times was the service retring before failing
                
            :returns:
                RC object

        """
        raise NotImplementedError()

        
    ##################### API ######################
    def execute (self, retries = 0):
        
        # sanity check
        rc = self.__sanity()
        if not rc:
            return rc
                                 
        # first cleanup
        rc = self.port.remove_all_streams()
        if not rc:
            return rc


        # start the iteration
        try:

            # add the stream(s)
            self.port.add_streams(self.generate_request())
            rc = self.port.set_rx_queue(size = self.queue_size)
            if not rc:
                return rc

            return self.__execute_internal(retries)

        finally:
            # best effort restore
            self.port.remove_rx_queue()
            self.port.remove_all_streams()


    ##################### Internal ######################
    def __sanity (self):
        if not self.port.is_service_mode_on():
            return self.port.err('port service mode must be enabled for performing {0}. Please enable service mode'.format(self.get_name()))

        if self.layer_mode == RXServiceAPI.LAYER_MODE_L2:
            if not self.port.is_l2_mode():
                return self.port.err('{0} - requires L2 mode configuration'.format(self.get_name()))

        elif self.layer_mode == RXServiceAPI.LAYER_MODE_L3:
            if not self.port.is_l3_mode():
                return self.port.err('{0} - requires L3 mode configuration'.format(self.get_name()))


        # sanity
        if self.port.is_active():
            return self.port.err('{0} - port is active, please stop the port before executing command'.format(self.get_name()))

        # call the specific class implementation
        rc = self.pre_execute()
        if not rc:
            return rc
            
        return True
        

    # main resolve function
    def __execute_internal (self, retries):

        # retry for 'retries'
        index = 0
        while True:
            rc = self.execute_iteration()
            if rc is not None:
                return rc

            if index >= retries:
                return self.on_timeout_err(retries)

            index += 1
            time.sleep(0.1)



    def execute_iteration (self):

        mult = {'op': 'abs', 'type' : 'percentage', 'value': 100}
        rc = self.port.start(mul = mult, force = False, duration = -1, mask = 0xffffffff)
        if not rc:
            return rc

        # save the start timestamp
        self.start_ts = rc.data()['ts']

        # block until traffic finishes
        while self.port.is_active():
            time.sleep(0.01)

        return self.wait_for_rx_response()


    def wait_for_rx_response (self):

        # we try to fetch response for 5 times
        polling = 5

        while polling > 0:

            # fetch the queue
            rx_pkts = self.port.get_rx_queue_pkts()

            # might be an error
            if not rx_pkts:
                return rx_pkts

            # for each packet - examine it
            for pkt in rx_pkts.data():
                rc = self.on_pkt_rx(pkt, self.start_ts)
                if rc is not None:
                    return rc

            if polling == 0:
                return None

            polling -= 1
            time.sleep(0.1)

