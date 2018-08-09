import sys
import os

cpu_vendor = 'arm' if os.uname()[4] == 'aarch64' else 'intel'
cpu_bits   = '64bit' if sys.maxsize > 0xffffffff else '32bit'

par = os.pardir
ext_libs = os.path.abspath(os.path.join(os.path.dirname(__file__), par, par, par, 'external_libs'))
zmq_path = os.path.join(ext_libs, 'pyzmq-ctypes', cpu_vendor, cpu_bits)
if zmq_path not in sys.path:
   sys.path.append(zmq_path)

import zmq
import json
from argparse import *

parser = ArgumentParser(description=' Runs a Scapy Server Client example ')
parser.add_argument('-p','--dest-scapy-port',type=int, default = 4507, dest='dest_scapy_port',
                    help='Select port to which this Scapy Server client will send to.\n default is 4507\n',action='store')
parser.add_argument('-s','--server',type=str, default = 'localhost', dest='dest_scapy_ip',
                    help='Remote server IP address .\n default is localhost\n',action='store')

args = parser.parse_args()

dest_scapy_port = args.dest_scapy_port
dest_scapy_ip = args.dest_scapy_ip

context = zmq.Context()

#  Socket to talk to server
print 'Connecting:'
socket = context.socket(zmq.REQ)
socket.connect("tcp://"+str(dest_scapy_ip)+":"+str(dest_scapy_port)) 
try:
    while True:
        command = raw_input("enter RPC command [enter quit to exit]:\n")
        if (command == 'quit'):
            break
        user_parameter = raw_input("input for command [should be left blank if not needed]:\n")
        json_rpc_req = { "jsonrpc":"2.0","method": command ,"params":[user_parameter], "id":"1"}
        request = json.dumps(json_rpc_req)
        print("Sending request in json format %s" % request)
        socket.send(request)

        #  Get the reply.
        message = socket.recv()
        print("Received reply %s [ %s ]" % (request, message))
except KeyboardInterrupt:
                print('Terminated By Ctrl+C')
                socket.close()
                context.destroy()

