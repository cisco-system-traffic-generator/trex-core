#!/usr/bin/python


import os
import sys


import astf_path
from trex.astf.api import *

import logging
logging.basicConfig(level=logging.INFO)

import time
import socket

from trex.astf.trex_astf_client import Tunnel

class Client:
    def __init__(self, server):
        self.client_db = dict()
        logging.info("Trex Server IP Used (%s)", server)
        self.client = ASTFClient(server = server)
        self.client.connect()
        self.client.reset()
        self.count = 0
        self.load_profile_script ="http_many.py"
        self.add_client_cnt = 10
        self.status_change_client_cnt = 10


    def load_profile_file(self):

        self.client.load_profile(self.load_profile_script)
        self.client.clear_stats()
        self.client.set_port_attr(promiscuous = True, multicast = True)
        self.client.start()

    def build_client_record(self, client_ip, sip, dip, sport, teid, version):
        logging.debug('Client Add %s', self.uint32_to_ip(client_ip))
        self.client_db[client_ip] = Tunnel(sip, dip, sport, teid, version)

    def insert_records(self, add_activate=False):
        try:
           self.client.update_tunnel_client_record(self.client_db, 1)
           count = 0
           for k, v in self.client_db.items():
               logging.debug('Client All (%s : %d) ==> Count: %d', self.uint32_to_ip(k), k, count)
               count +=1
               self.count += 1

           self.client_db.clear()
           logging.info("Clients Added [%d]", (count/len(self.client.get_all_ports())) * 2)

        except Exception as e:
           logging.error(e)

    def enable_disable_records(self, thread_id, client_ip_list = None, is_enable = False, is_range=False):
        try:
            rc = None
            if is_range:
                rc = self.client.set_client_enable_range(client_ip_list[0], client_ip_list[1], is_enable)
            else:
                rc = self.client.set_client_enable(client_ip_list, is_enable)
        except:
           logging.debug("Connection Lost while state change")

    def ip_to_uint32(self, ip):
        t = socket.inet_aton(ip)
        return struct.unpack("!I", t)[0]

    def uint32_to_ip(self, ipn):
        t = struct.pack("!I", ipn)
        return socket.inet_ntoa(t)


    def add_clients(self):
        loop = len(self.client.get_all_ports()) / 2
        ip_prefix = "11.11.0."
        ip_offset = 0
        sip = "11.11.0.1"
        dip = "1.1.1.11"
        sport = 5000
        teid = 1
        for _ in range(loop):
            teid = 1
            while teid <= self.add_client_cnt:
                c_ip = ip_prefix + str(teid)
                c_ip = self.ip_to_uint32(c_ip) + ip_offset
                self.build_client_record(c_ip, sip, dip, sport, teid, 4)
                teid += 1
            ip_offset += self.ip_to_uint32("1.0.0.0")

        self.insert_records()

    def status_change_clients(self, is_enable, is_range, s, e):
        ip_prefix = "11.11.0."
        cnt = 1
        client_ip_list = list()
        if is_range:
           ip = list()
           c_ip = self.ip_to_uint32(s)
           ip.append(c_ip)
           c_ip = self.ip_to_uint32(e)
           ip.append(c_ip)
           client_ip_list.append(ip)
           return self.enable_disable_records(0, ip, is_enable=is_enable, is_range=is_range)

        while cnt <= self.status_change_client_cnt:
            c_ip = ip_prefix + str(cnt)
            c_ip = self.ip_to_uint32(c_ip)
            client_ip_list.append(c_ip)
            cnt += 1

        self.enable_disable_records(0, client_ip_list, is_enable=is_enable, is_range=is_range)

    def get_clients_info(self, is_range, s, e):
        ip_prefix = "11.11.0."
        cnt = 1
        client_ip_list = list()
        if is_range:
           ip = list()
           c_ip = self.ip_to_uint32(s)
           ip.append(c_ip)
           c_ip = self.ip_to_uint32(e)
           ip.append(c_ip)
           return self.client.get_clients_info_range(ip[0], ip[1])

        while cnt <= self.status_change_client_cnt:
            c_ip = ip_prefix + str(cnt)
            c_ip = self.ip_to_uint32(c_ip)
            client_ip_list.append(c_ip)
            cnt += 1

        logging.error(client_ip_list)

        return self.client.get_clients_info(client_ip_list)

def run_test ():

        client = Client("127.0.0.1")
        try:
            client.client.activate_tunnel(tunnel_type=1, activate=True, loopback=False)
        except Exception as e:
            logging.error(e)
            return

        client.load_profile_file()

        logging.info(client.get_clients_info(False, "", "").data())
        client.add_clients()
        client.status_change_clients(True, True, "11.11.0.1", "11.11.0.10")

if __name__ == "__main__":
    run_test()
