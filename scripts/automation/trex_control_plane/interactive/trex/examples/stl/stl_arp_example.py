import stl_path
from trex.stl.api import *
import pprint
import zmq

import time

def simple ():
    pprint.pprint("Hey")
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.connect("tcp://64.103.125.49:5555")
    cnt = 0 
    while True:
        socket.send(" hey".encode('utf-8'))
        #  Get the reply.
        message = socket.recv()
        cnt = cnt + 1
        if cnt % 10000 ==0:
            print("* {} ".format(time.clock()))
        message = message.decode()
        #print(message)





# run the tests
simple()

