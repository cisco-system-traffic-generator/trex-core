import os
from scapy.utils import RawPcapReader, RawPcapWriter

def __ts_key (a):
    return float(a[1][0]) + (float(a[1][1]) / 1e6)

def merge_cap_files (pcap_file_list, out_filename, delete_src = False):

    if not all([os.path.exists(f) for f in pcap_file_list]):
        print("failed to merge cap file list...\nnot all files exist\n")
        return

    out_pkts = []
    for src in pcap_file_list:
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

