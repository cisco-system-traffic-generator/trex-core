

import zmq
import json


class Scapy_server_wrapper():
    def __init__(self,dest_scapy_port=5555,server_ip_address='localhost'):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.dest_scapy_port =dest_scapy_port
        self.socket.connect("tcp://"+str(server_ip_address)+":"+str(self.dest_scapy_port)) #ip address of csi-trex-11

    def call_method(self,method_name,method_params):
        json_rpc_req = { "jsonrpc":"2.0","method": method_name ,"params": method_params, "id":"1"}
        request = json.dumps(json_rpc_req)
        self.socket.send_string(request)
        #  Get the reply.
        message = self.socket.recv_string()
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
        
    def _get_all_pkt_offsets(self,pkt_desc):
        return self.call_method('_get_all_pkt_offsets',[pkt_desc])
