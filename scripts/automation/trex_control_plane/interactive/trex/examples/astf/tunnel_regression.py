#!/usr/bin/python


import os
import sys


import astf_path
from trex.astf.api import *

import logging
logging.basicConfig(level=logging.INFO)

import time
import socket

class Tunnel:
    def __init__(self,sip,dip,teid,version, thread_id):
        self.sip = sip
        self.dip = dip
        self.teid = teid
        self.version = version
        self.thread_id = thread_id

class Client:
    def __init__(self, server):
        self.client_db = dict()
        logging.info("Trex Server IP Used (%s)", server)
        self.client = ASTFClient(server = server)
        self.client.connect()
        self.count = 0
        self.load_profile_script ="tunnel_many.py"
        self.add_client_cnt = 10
        self.status_change_client_cnt = 10

    def load_profile_file(self):

        self.client.reset()
        self.client.load_profile(self.load_profile_script)
        self.client.clear_stats()
        self.client.set_port_attr(promiscuous = True, multicast = True)
        self.client.start()

    def build_client_record(self, client_ip, sip, dip, teid, version, thread_id = 0):
        logging.debug('Client Add %s', self.uint32_to_ip(client_ip))
        self.client_db[client_ip] = Tunnel(sip, dip, teid, version, thread_id)

    def insert_records(self, add_activate=False):
        try:
           self.client.insert_tunnel_in_client_record(self.client_db)
           count = 0
           for k, v in self.client_db.items():
               logging.debug('Client All (%s : %d) ==> Count: %d', self.uint32_to_ip(k), k, count)
               count +=1
               self.count += 1

           self.client_db.clear();
           logging.info("Clients Added [%d]", count)

        except:
           logging.error("Connection Lost while inserting")

    def delete_records(self, client_ip_list = None):
        rc = self.client.delete_tunnel_from_client_record(client_ip_list)          
        for record in client_ip_list:
            logging.debug('Client Delete [%d], thread_id [%d]', record[0], record[1])

    def enable_disable_records(self, thread_id, client_ip_list = None, is_enable = False):
        count = 0
        if client_ip_list is not None:
            for v in client_ip_list:
               logging.debug('Client Status Change (%s : %d ) ==>Enable Flag: %d ==>Count : %d', self.uint32_to_ip(v), v, is_enable, count)
               count +=1

        try:
            rc = self.client.enable_disable_client(thread_id, is_enable, client_ip_list)
        except:
           logging.debug("Connection Lost while state change")

    def get_clients_stats(self):
        records = self.client.get_clients_stats()
        stats = list()
        for rec in records:
           rec = str(rec)
           stats = rec.split()
           if 'Thread' in rec:
               continue
           elif 'Total:' in rec:
               logging.debug(stats)
               return stats

        return stats

    def ip_to_uint32(self, ip):
        t = socket.inet_aton(ip)
        return struct.unpack("!I", t)[0]

    def uint32_to_ip(self, ipn):
        t = struct.pack("!I", ipn)
        return socket.inet_ntoa(t)


    def add_clients_and_verify(self):
        ip_prefix = "11.11.0."
        sip = "11.11.0.1"
        dip = "1.1.1.11"
        teid = 1
        while teid <= self.add_client_cnt:
            c_ip = ip_prefix + str(teid)
            c_ip = self.ip_to_uint32(c_ip)
            self.build_client_record(c_ip, sip, dip, teid, 1)
            teid += 1

        self.insert_records()
        stats = self.get_clients_stats()
        # Sample ['Total:', '10', 'Active:', '0', 'Inactive:', '10', 'Tunnel:', '10']
        if len(stats) == 0:
            logging.error("Add Client test Failed")

        # All clients Added should be inactive by default
        if int(stats[1]) == self.add_client_cnt and int(stats[5]) == self.add_client_cnt and int(stats[7]) == self.add_client_cnt:
           logging.info("Add Client : Passed => Added [%d] and Found [%d]", self.add_client_cnt, int(stats[1]))
           return True
        else:
           logging.info("Add Client: Failed => Expected Total [%d] : Inactive [%d] : Tunnel [%d] but found Total [%d] : Inactive [%d] : Tunnel [%d]", self.add_client_cnt, self.add_client_cnt, self.add_client_cnt, int(stats[1]), int(stats[5]), int(stats[7]))
           return False 

    def status_change_clients_and_verify(self, is_enable):
        ip_prefix = "11.11.0."
        cnt = 1
        client_ip_list = list()
        while cnt <= self.status_change_client_cnt:
            c_ip = ip_prefix + str(cnt)
            c_ip = self.ip_to_uint32(c_ip)
            client_ip_list.append(c_ip)
            cnt += 1

        self.enable_disable_records(0, client_ip_list, is_enable = is_enable)

        stats = self.get_clients_stats()
        # Sample ['Total:', '10', 'Active:', '10', 'Inactive:', '0', 'Tunnel:', '10']
        if len(stats) == 0:
            logging.error("Status Change for clients test Failed")

        # All clients Added should be inactive by default
        if is_enable is True:
            if int(stats[3]) == self.status_change_client_cnt and int(stats[5]) == 0:
                logging.info("Status Change : Passed => Mark Active [%d] and Found Active[%d]", self.status_change_client_cnt, int(stats[3]))
                return True
            else:
                logging.info("Status Change : Passed => Mark Active [%d] and Found Active[%d]", self.status_change_client_cnt, int(stats[3]))
                return False
        else:
            if int(stats[5]) == self.status_change_client_cnt and int(stats[3]) == 0:
                logging.info("Status Change : Passed => Mark InActive [%d] and Found InActive[%d]", self.status_change_client_cnt, int(stats[5]))
                return True
            else:
                logging.info("Status Change : Passed => Mark InActive [%d] and Found InActive[%d]", self.status_change_client_cnt, int(stats[5]))
                return False

    def del_clients_and_verify(self):
        ip_prefix = "11.11.0."
        cnt = 1
        client_ip_list = list(list())
        while cnt <= self.add_client_cnt:
            c_ip = ip_prefix + str(cnt)
            c_ip = self.ip_to_uint32(c_ip)
            cnt += 1
            record = [c_ip, 0]
            client_ip_list.append(record)

        self.delete_records(client_ip_list)
        stats = self.get_clients_stats()
        # Sample ['Total:', '0', 'Active:', '0', 'Inactive:', '0', 'Tunnel:', '0']
        if len(stats) == 0:
            logging.error("Delete Client test Failed")

        if int(stats[7]) == 0:
           logging.info("Delete Client : Passed => Expected Tunnel [%d] and found Tunnel [%d]", 0, int(stats[7]))
           return True
        else:
           logging.info("Delete Client : Failed => Expected Tunnel [%d] but found Tunnel [%d]", 0, int(stats[7]))
           return False
 
def run_test ():

        client = Client("127.0.0.1")
        client.load_profile_file()

        if client.add_clients_and_verify() is True:
            client.status_change_clients_and_verify(True)
            client.status_change_clients_and_verify(False)
            client.del_clients_and_verify()
        else:
           logging.info("Add Client test Failed, skipping other test cases") 



if __name__ == "__main__":
    run_test()
