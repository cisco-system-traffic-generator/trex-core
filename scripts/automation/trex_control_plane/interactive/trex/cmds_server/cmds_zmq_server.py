#!/bin/python3
from argparse import *
import sys
import zmq
from argparse import *
import json
import subprocess
import time
import os

class Cmds_server:
    def __init__(self, args):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind("ipc://{}".format(args.ipc_file_path))
        self.func = {0 : self.kill, 1 : self.popen, 2 : self.new_client}
        self.num_clients = 0

    def activate(self):
        killed = False
        try:
            while not killed:
                try:
                    message = self.socket.recv().decode()
                    request_json = json.loads(message)
                    if request_json['type'] not in self.func:
                        response_json = json.dumps({"returncode" : -1, "output": 'Does not support the request type: {0}'.format(request_json['type'])})
                    else:
                        killed, response_json = self.func[request_json['type']](request_json)

                    self.socket.send(response_json.encode('utf-8'))
                    
                except zmq.ZMQError as e:
                    if e.errno != errno.EINTR:
                        print(e)
                    continue

        except KeyboardInterrupt:
                print('Terminated By local user')

        finally:
            self.socket.close()
            self.context.destroy()

        time.sleep(2)

    def kill(self, request):
        self.num_clients-=1
        response = json.dumps({"returncode" : 0, "output": ''})
        if self.num_clients <= 0:
            return True, response
        return False, response

    def new_client(self, request):
        self.num_clients+=1
        return False, json.dumps({"returncode" : 0, "output": ''})

    def popen(self, request):
        p = subprocess.Popen(request['cmd'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        output, err = p.communicate()
        if p.returncode == 0:
            answer = output
        else:
            answer = err
        return False, json.dumps({"returncode" : p.returncode, "output": answer.decode()})


def main(args):
    s = Cmds_server(args)
    s.activate()

if __name__=='__main__':
        parser = ArgumentParser(description=' Runs Cmds Server ')
        parser.add_argument('-f','--file-path-ipc',type=str, default = "/tmp/cmds.ipc", dest='ipc_file_path',
                            help='Select the ipc file path.',action='store')
        args = parser.parse_args()
        sys.exit(main(args))