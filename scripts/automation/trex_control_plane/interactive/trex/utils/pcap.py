import os
from scapy.utils import RawPcapReader, RawPcapWriter

def __ts_key (a):
    return float(a[1][0]) + (float(a[1][1]) / 1e6)

def merge_cap_files (pcap_file_list, out_filename, delete_src = False):

    exising_pcaps = [f for f in pcap_file_list if os.path.exists(f)]
    if len(exising_pcaps) != len(pcap_file_list):
        print("WARNING: not all files from DP cores exist\n")

    out_pkts = []
    for src in exising_pcaps:
        pkts = RawPcapReader(src)
        out_pkts += pkts
        if delete_src:
            os.unlink(src)

    # sort by timestamp
    out_pkts = sorted(out_pkts, key = __ts_key)

    writer = RawPcapWriter(out_filename, linktype = 1)

    writer._write_header(None)
    for pkt in out_pkts:
        writer._write_packet(pkt[0], sec=pkt[1][0], usec=pkt[1][1], caplen=pkt[1][2], wirelen=None)

