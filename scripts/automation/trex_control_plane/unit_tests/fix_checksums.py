#!/usr/bin/python
import os
import sys
import argparse
import outer_packages
from scapy.all import *
import traceback

def fail(msg):
    print(msg)
    sys.exit(1)

parser = argparse.ArgumentParser(
    description = 'Corrects IPv4/6, TCP/UDP checksums, removes FCS and extra paddings.',
    usage = 'fix_checksums.py -f input.pcap -o output.pcap')

parser.add_argument('-i', '-f', dest = 'input_pcap', required = True, help = 'pcap filename to process')
parser.add_argument('-o', dest = 'output_pcap', required = True, help = 'output filename')
parser.add_argument('--skip-bad', dest = 'skip_bad', action = 'store_true', help = 'skip bad packets (default: False)')

args = parser.parse_args()
if os.path.exists(args.output_pcap):
    fail('Destination path exists')

w = RawPcapWriter(args.output_pcap, linktype = 1)
w._write_header(None)

written = 0
skipped = 0
for i, pkt in enumerate(RawPcapReader(args.input_pcap)):
    try:
        scapy_pkt = Ether(pkt[0])
        if IP in scapy_pkt:
            del scapy_pkt[IP].chksum
            extra_data = len(bytes(scapy_pkt[IP])) - scapy_pkt[IP].len
        elif IPv6 in scapy_pkt:
            extra_data = len(bytes(scapy_pkt[IPv6].payload)) - scapy_pkt[IPv6].plen
        else:
            #scapy_pkt.show2()
            raise Exception('Not IPv4/6 packet')

        if TCP in scapy_pkt:
            del scapy_pkt[TCP].chksum
        elif UDP in scapy_pkt:
            if scapy_pkt[UDP].chksum != 0:
                del scapy_pkt[UDP].chksum
        else:
            #scapy_pkt.show2()
            raise Exception('Not TCP/UDP packet')

        pkt_buf = bytes(scapy_pkt)
        if extra_data: # remove FCS, extra paddings etc.
            pkt_buf = pkt_buf[:-extra_data]
        w._write_packet(pkt_buf, pkt[1][0], pkt[1][1])
        written += 1

    except Exception as e:
        err = 'Error in packet #%s - %s' % (i + 1, e)
        if args.skip_bad:
            skipped += 1
            print(err)
        else:
            traceback.print_exc()
            os.remove(args.output_pcap)
            fail(err)

w.flush()
w.close()

print('Done. (%s written, %s skipped)' % (written, skipped))

