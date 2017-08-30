import sys
import argparse
sys.path.insert(0, "../")
from trex_nstf_lib.cap_handling import CPcapReader


def process_options():
    parser = argparse.ArgumentParser(
        description='',
        usage="""
        """,
        epilog=" written by ibarnea")

    parser.add_argument("-f",  dest="in_file", required=True, help='Name of pcap file to process')
    parser.add_argument('-o',  dest='out_file', default="prog.py", help='Output file name')
    return parser.parse_args()

if __name__ == "__main__":
    opts = process_options()
    print ("Processing cap file {0}".format(opts.in_file))
    cap = CPcapReader(opts.in_file)
    cap.analyze()
    # cap.dump()
    cap.condense_pkt_data()
    # cap.dump()

    c_out_file_name = "client_" + opts.out_file
    s_out_file_name = "server_" + opts.out_file
    c_out = open(c_out_file_name, 'w')
    s_out = open(s_out_file_name, 'w')
    cap.gen_prog_file(c_out, "c")
    cap.gen_prog_file(s_out, "s")
