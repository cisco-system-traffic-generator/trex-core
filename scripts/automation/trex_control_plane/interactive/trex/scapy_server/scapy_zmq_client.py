
import sys
import zmq
import json
from argparse import *
from pprint import pprint

class Scapy_server_wrapper():
    def __init__(self,dest_scapy_port=5555,server_ip_address='localhost'):
        self.server_ip_address = server_ip_address
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.dest_scapy_port =dest_scapy_port
        self.socket.connect("tcp://"+str(self.server_ip_address)+":"+str(self.dest_scapy_port)) 

    def call_method(self,method_name,method_params):
        json_rpc_req = { "jsonrpc":"2.0","method": method_name ,"params": method_params, "id":"1"}
        request = json.dumps(json_rpc_req)
        self.socket.send(request.encode('utf-8'))
        #  Get the reply.
        message = self.socket.recv()
        message = message.decode()

        message_parsed = json.loads(message)
        if 'result' in message_parsed.keys():
            result = message_parsed['result']
        else:
            result = {'error':message_parsed['error']}
        return result

    def get_all(self):
        return self.call_method('get_all',[])

    def check_update(self,db_md5,field_md5):
        result = self.call_method('check_update',[db_md5,field_md5])
        if result!=True:
            if 'error' in result.keys():
                if "Fields DB is not up to date" in result['error']['message:']:
                    raise Exception("Fields DB is not up to date")
                if "Protocol DB is not up to date" in result['error']['message:']:
                    raise Exception("Protocol DB is not up to date")
        return result

    def build_pkt(self,pkt_descriptor):
        return self.call_method('build_pkt',[pkt_descriptor])

    def _input(self, *args, **kwargs):
        try:
            return raw_input(*args, **kwargs)
        except NameError:
            return input(*args, **kwargs)

    def _get_all_pkt_offsets(self,pkt_desc):
        return self.call_method('_get_all_pkt_offsets',[pkt_desc])

    def _activate_console(self):
        context = zmq.Context()
        #  Socket to talk to server
        print('Connecting:')
        socket = context.socket(zmq.REQ)
        socket.connect("tcp://"+str(self.server_ip_address)+":"+str(self.dest_scapy_port)) 
        try:
            print('This is a simple console to communicate with Scapy server.\nInvoke supported_methods (with 1 parameter = all) to see supported commands\n')
            while True:
                command = self._input("enter RPC command [enter quit to exit]:\n")
                if (command == 'quit'):
                    break      
                parameter_num = 0
                params = []
                while True:
                    try:
                        parameter_num = int(self._input('Enter number of parameters to command:\n'))
                        break
                    except Exception:
                        print('Invalid input. Try again')
                for i in range(1,parameter_num+1,1):
                    print("input parameter %d:" % i)
                    user_parameter = self._input()
                    params.append(user_parameter)
                pprint_output = self._input('pprint the output [y/n]? ')
                while ((pprint_output!= 'y')  and (pprint_output!='n')):
                    pprint_output = self._input('pprint the output [y/n]? ')
                json_rpc_req = { "jsonrpc":"2.0","method": command ,"params":params, "id":"1"}
                request = json.dumps(json_rpc_req)
                print("Sending request in json format %s " % request)
                socket.send(request.encode('utf-8'))

                #  Get the reply.
                message = socket.recv()
                message = message.decode()
                print('received reply:')
                parsed_message = json.loads(message)
                if (pprint_output == 'y'):
                    pprint(parsed_message)
                else:
                    print(message)
        except KeyboardInterrupt:
                        print('Terminated By Ctrl+C')
        finally:
            socket.close()
            context.destroy()



if __name__=='__main__':
    parser = ArgumentParser(description='Example of client module for Scapy server ')
    parser.add_argument('-p','--dest-scapy-port',type=int, default = 4507, dest='dest_scapy_port',
                        help='Select port to which this Scapy Server client will send to.\n default is 4507\n',action='store')
    parser.add_argument('-s','--server',type=str, default = 'localhost', dest='dest_scapy_ip',
                        help='Remote server IP address .\n default is localhost\n',action='store')
    parser.add_argument('-c','--console',
                        help='Run simple client console for Scapy server.\nrun with \'-s\' and \'-p\' to determine IP and port of the server\n',
                        action='store_true',default = False)
    args = parser.parse_args()
    if (args.console):
        s = Scapy_server_wrapper(args.dest_scapy_port,args.dest_scapy_ip)
        sys.exit(s._activate_console())
    else:
        print('Scapy client: for interactive console re-run with \'-c\', else import as seperate module.')

