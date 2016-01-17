import sys
import os
import time
import cStringIO

api_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(api_path, '../automation/trex_control_plane/client/'))

from trex_stateless_client import CTRexStatelessClient, LoggerApi

class BasicTest(object):

    # exception class
    class Failure(Exception):
        def __init__ (self, rc):
            self.rc = rc

        def __str__ (self):
            s = "\n******\n"
            s += "Test has failed.\n\n"
            s += "Error reported:\n\n  {0}\n".format(str(self.rc.err()))
            return s


    # logger
    class Logger(LoggerApi):
        def __init__ (self):
            LoggerApi.__init__(self)
            self.buffer = cStringIO.StringIO()

        def write (self, msg, newline = True):
            self.buffer.write(str(msg))

            if newline:
                self.buffer.write("\n")

        def flush (self):
            pass


    def __init__ (self):
        self.logger = BasicTest.Logger()
        self.client = CTRexStatelessClient(logger = self.logger)

    def connect (self):
        rc = self.client.connect(mode = "RWF")
        __verify_rc(rc)


    def disconnect (self):
        self.client.disconnect()


    def __verify_rc (self, rc):
        if rc.bad():
            raise self.Failure(rc)

    def inject_profile (self, filename, rate = "1", duration = None):
        cmd = "-f {0} -m {1}".format(filename, rate)
        if duration:
            cmd += " -d {0}".format(duration)

        print cmd
        rc = self.client.cmd_start_line(cmd)
        self.__verify_rc(rc)


    def wait_while_traffic_on (self, timeout = None):
        while self.client.get_active_ports():
            time.sleep(0.1)


def start ():
    test = BasicTest()
    
    try:
        test.connect()
        test.inject_profile("stl/imix_1pkt.yaml", rate = "5gbps", duration = 10)
        test.wait_while_traffic_on()
    finally:
        test.disconnect()


def main ():
    try:
        start()
        print "\nTest has passed :)\n"
    except BasicTest.Failure as e:
        print e



#main()
