#!/router/bin/python

import os
import outer_packages
import zmq
import threading
import logging
import CCustomLogger
from json import JSONDecoder
from common.trex_status_e import TRexStatus

# setup the logger
CCustomLogger.setup_custom_logger('TRexServer')
logger = logging.getLogger('TRexServer')


class ZmqMonitorSession(threading.Thread):
    def __init__(self, trexObj , zmq_port):
        super(ZmqMonitorSession, self).__init__()
        self.stoprequest    = threading.Event()
        self.first_dump     = True
        self.zmq_port       = zmq_port
        self.zmq_publisher  = "tcp://localhost:{port}".format(port=self.zmq_port)
        self.trexObj        = trexObj
        self.expect_trex    = self.trexObj.expect_trex     # used to signal if TRex is expected to run and if data should be considered
        self.decoder        = JSONDecoder()
        logger.info("ZMQ monitor initialization finished")

    def run(self):
        self.context    = zmq.Context()
        self.socket     = self.context.socket(zmq.SUB)
        logger.info("ZMQ monitor started listening @ {pub}".format(pub=self.zmq_publisher))
        self.socket.connect(self.zmq_publisher)
        self.socket.setsockopt(zmq.SUBSCRIBE, '')
        
        while not self.stoprequest.is_set():
            try:
                zmq_dump = self.socket.recv()   # This call is BLOCKING until data received!
                if self.expect_trex.is_set():
                    self.parse_and_update_zmq_dump(zmq_dump)
                    logger.debug("ZMQ dump received on socket, and saved to trexObject.")
            except Exception as e:
                if self.stoprequest.is_set():
                    # allow this exception since it comes from ZMQ monitor termination
                    pass
                else:
                    logger.error("ZMQ monitor thrown an exception. Received exception: {ex}".format(ex=e))
                    raise

    def join(self, timeout=None):
        self.stoprequest.set()
        logger.debug("Handling termination of ZMQ monitor thread") 
        self.socket.close()
        self.context.term()
        logger.info("ZMQ monitor resources has been freed.")
        super(ZmqMonitorSession, self).join(timeout)

    def parse_and_update_zmq_dump(self, zmq_dump):
        try:
            dict_obj = self.decoder.decode(zmq_dump)
        except ValueError:
            logger.error("ZMQ dump failed JSON-RPC decode. Ignoring. Bad dump was: {dump}".format(dump=zmq_dump))
            dict_obj = None

        # add to trex_obj zmq latest dump, based on its 'name' header
        if dict_obj is not None and dict_obj != {}:
            self.trexObj.zmq_dump[dict_obj['name']] = dict_obj
            if self.first_dump:
                # change TRexStatus from starting to Running once the first ZMQ dump is obtained and parsed successfully
                self.first_dump = False
                self.trexObj.set_status(TRexStatus.Running)
                self.trexObj.set_verbose_status("TRex is Running")
                logger.info("First ZMQ dump received and successfully parsed. TRex running state changed to 'Running'.")


if __name__ == "__main__":
    pass

