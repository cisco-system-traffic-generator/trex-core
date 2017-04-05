#!/usr/bin/python
import os
import socket
import sys
import argparse
import outer_packages
from scapy.all import *
import texttable


class sock_driver(object):
    args = None;
    cap_server_port = None
    proto = None

def fail(msg):
    print('\nError: %s\n' % msg)
    sys.exit(1)

class CPcapFileReader:
    def __init__ (self, file_name):
        self.file_name = file_name
        self.pkts = []
        self.__build_pkts()
        if not self.pkts:
            fail('No payloads were found in the pcap.')

    def __build_pkts(self):
        if not os.path.exists(self.file_name):
            fail('Filename %s does not exist!' % self.file_name)
        pcap = RawPcapReader(self.file_name).read_all()
        if not pcap:
            fail('Empty pcap!')
        first_tuple = None

        for index, (raw_pkt, _) in enumerate(pcap):
            scapy_pkt = Ether(raw_pkt)

            # check L3
            ipv4 = scapy_pkt.getlayer('IP')
            ipv6 = scapy_pkt.getlayer('IPv6')
            garbage = 0
            if ipv4 and ipv6:
                scapy_pkt.show2()
                fail('Packet #%s in pcap has both IPv4 and IPv6!' % index)
            elif ipv4:
                l3 = ipv4
                garbage = len(ipv4) - ipv4.len
            elif ipv6:
                l3 = ipv6
                garbage = len(ipv6) - ipv6.plen - 40
            else:
                scapy_pkt.show2()
                fail('Packet #%s in pcap is not IPv4/6.' % index)

            # check L4
            tcp = scapy_pkt.getlayer('TCP')
            udp = scapy_pkt.getlayer('UDP')
            if tcp and udp:
                scapy_pkt.show2()
                fail('Packet #%s in pcap has both TCP and UDP!' % index)
            elif tcp:
                l4 = tcp
                if sock_driver.proto not in (None, 'TCP'):
                    fail('You have mix of TCP and %s in the pcap!' % sock_driver.proto)
                sock_driver.proto = 'TCP'
            elif udp:
                l4 = udp
                if sock_driver.proto not in (None, 'UDP'):
                    fail('You have mix of UDP and %s in the pcap!' % sock_driver.proto)
                sock_driver.proto = 'UDP'
            else:
                scapy_pkt.show2()
                fail('Packet #%s in pcap is not TCP or UDP.' % index)

            pkt = {}
            pkt['src_ip'] = l3.src
            pkt['dst_ip'] = l3.dst
            pkt['src_port'] = l4.sport
            pkt['dst_port'] = l4.dport
            if tcp:
                pkt['tcp_seq'] = tcp.seq
            if garbage:
                pkt['pld'] = bytes(l4.payload)[:-garbage]
            else:
                pkt['pld'] = bytes(l4.payload)

            if index == 0:
                if tcp and tcp.flags != 2:
                    scapy_pkt.show2()
                    fail('First TCP packet should be with SYN')
                first_tuple = (l3.dst, l3.src, l4.dport, l4.sport, l4.name)
                sock_driver.cap_server_port = l4.dport

            if first_tuple == (l3.dst, l3.src, l4.dport, l4.sport, l4.name):
                pkt['is_client'] = True
            elif first_tuple == (l3.src, l3.dst, l4.sport, l4.dport, l4.name):
                pkt['is_client'] = False
            else:
                fail('More than one flow in this pcap.\nFirst tuple is: %s,\nPacket #%s has tuple: %s' % (first_tuple, (l3.dst, l3.src, l4.dport, l4.sport, l4.name)))

            if pkt['pld']: # has some data
                if tcp:
                    is_retransmission = False
                    # check retransmission and out of order
                    for old_pkt in reversed(self.pkts):
                        if old_pkt['is_client'] == pkt['is_client']: # same side
                            if old_pkt['tcp_seq'] == tcp.seq:
                                is_retransmission = True
                            if old_pkt['tcp_seq'] > tcp.seq:
                                fail('Out of order in TCP packet #%s, please check the pcap manually.' % index)
                        break
                    if is_retransmission:
                        continue # to next packet

                self.pkts.append(pkt)


    def dump_info(self):
        t = texttable.Texttable(max_width = 200)
        t.set_deco(0)
        t.set_cols_align(['r', 'c', 'l', 'l', 'r', 'r', 'r'])
        t.header(['Index', 'Side', 'Dst IP', 'Src IP', 'Dst port', 'Src port', 'PLD len'])
        for index, pkt in enumerate(self.pkts):
            t.add_row([index, 'Client' if pkt['is_client'] else 'Server', pkt['dst_ip'], pkt['src_ip'], pkt['dst_port'], pkt['src_port'], len(pkt['pld'])])
        print(t.draw())
        print('')


class CClientServerCommon(object):

    def send_pkt(self, pkt):
        if sock_driver.proto == 'TCP':
            self.connection.sendall(pkt)
        else:
            self.connection.sendto(pkt, self.send_addr)
        print('>>> sent %d bytes' % (len(pkt)))

    def rcv_pkt(self, pkt):
        size = len(pkt)
        rcv = b''
        while len(rcv) < size:
            chunk, addr = self.connection.recvfrom(min(size - len(rcv), 2048))
            if sock_driver.proto == 'UDP':
                self.send_addr = addr
            if not chunk:
                raise Exception('Socket connection broken')
            rcv += chunk
        if len(rcv) != size:
            fail('Size of data does not match.\nExpected :s, got: %s' % (size, len(rcv)))
        if len(rcv) != len(pkt):
            for i in range(size):
                if rcv[i] != pkt[i]:
                    fail('Byte number #%s is expected to be: %s, got: %s.' % (i, rcv[i], pkt[i]))
        print('<<< recv %d bytes' % (len(rcv)))

    def process(self, is_client):
        for pkt in self.pcapr.pkts:
            time.sleep(sock_driver.args.ipg)
            if is_client ^ pkt['is_client']:
                self.rcv_pkt(pkt['pld'])
            else:
                self.send_pkt(pkt['pld'])


class CServer(CClientServerCommon) :
    def __init__ (self, pcapr, port):
        if sock_driver.proto == 'TCP':
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        else:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        server_address = ('', port)
        print('Starting up on %sport %s' % ('%s ' % server_address[0] if server_address[0] else '', server_address[1]))
        try:
            sock.bind(server_address)
        except socket.error as e:
            fail(e)

        if sock_driver.proto == 'TCP':
            sock.listen(1)
            self.connection = None
        else:
            self.connection = sock

        self.pcapr=pcapr; # save the info

        while True:
            try:
                # Wait for a connection
                print('Waiting for new connection')
                if sock_driver.proto == 'TCP':
                    self.connection, client_address = sock.accept()
                    print('Got connection from %s:%s' % client_address)

                self.process(is_client = False)

            except KeyboardInterrupt:
                print('    Ctrl+C')
                break
            except Exception as e:
                print(e)
            finally:
                if self.connection and sock_driver.proto == 'TCP':
                   self.connection.close()
                   self.connection = None


class CClient(CClientServerCommon):
    def __init__ (self, pcapr, ip, port):
        if sock_driver.proto == 'TCP':
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        else:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        server_address = (ip, port)
        self.send_addr = server_address
        self.pcapr = pcapr # save the info

        try:
            if sock_driver.proto == 'TCP':
                print('Connecting to %s:%s' % server_address)
                sock.connect(server_address)
            self.connection = sock

            self.process(is_client = True);
        except KeyboardInterrupt:
            print('    Ctrl+C')
        finally:
            if self.connection:
               self.connection.close()
               self.connection = None


def process_options ():
    parser = argparse.ArgumentParser(
        description = 'Simulates TCP application in low rate by sending payloads of given pcap. Requires single flow in pcap. Usage with UDP is experimential and assumes no drops.',
        usage="""
    Server side: (should be run first, need sudo permissions to use server side ports lower than 1024.)
        sock.py -s -f filename
    Client side:
        sock.py -c -f filename --ip <server ip>""",
        epilog=" written by hhaim");

    parser.add_argument("-f",  dest = "file_name", required = True, help='pcap filename to process')
    parser.add_argument('-c', action = 'store_true', help = 'client side flag')
    parser.add_argument('-s', action = 'store_true', help = 'server side flag')
    parser.add_argument('--port', type = int, help = 'server port, default is taken from the cap')
    parser.add_argument('--ip', default = '127.0.0.1', help = 'server ip')
    parser.add_argument('-i', '--ipg', type = float, default = 0.001, help = 'ipg (msec)')
    parser.add_argument('-v', '--verbose', action='store_true', help = 'verbose')
    sock_driver.args = parser.parse_args();

    if not (sock_driver.args.c ^ sock_driver.args.s):
        fail('You must choose either client or server side with -c or -s flags.');


def main ():
    process_options ()
    pcapr = CPcapFileReader(sock_driver.args.file_name)
    if sock_driver.args.verbose:
        pcapr.dump_info()

    port = sock_driver.cap_server_port
    if sock_driver.args.port:
        port = sock_driver.args.port
    if port == 22:
        fail('Port 22 is used by ssh, exiting.')

    if sock_driver.args.c:
        CClient(pcapr, sock_driver.args.ip, port)
    if sock_driver.args.s:
        CServer(pcapr, port)

main()

