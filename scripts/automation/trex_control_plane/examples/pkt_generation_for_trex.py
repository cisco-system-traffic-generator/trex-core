#!/router/bin/python

######################################################################################
###                                                                                ###
###             TRex end-to-end demo script, written by TRex dev-team              ###
###   THIS SCRIPT ASSUMES PyYaml and Scapy INSTALLED ON PYTHON'S RUNNING MACHINE   ###
###      (for any question please contact trex-dev team @ trex-dev@cisco.com)      ###
###                                                                                ###
######################################################################################


import logging
import time
import trex_root_path
from client.trex_client import *
from client_utils.general_utils import *
from client_utils.trex_yaml_gen import *
from pprint import pprint
from argparse import ArgumentParser

# import scapy package
logging.getLogger("scapy.runtime").setLevel(logging.ERROR)  # supress scapy import warnings from being displayed
from scapy.all import *


def generate_dns_packets (src_ip, dst_ip):
    dns_rqst = Ether(src='00:15:17:a7:75:a3', dst='e0:5f:b9:69:e9:22')/IP(src=src_ip,dst=dst_ip,version=4L)/UDP(dport=53, sport=1030)/DNS(rd=1,qd=DNSQR(qname="www.cisco.com"))
    dns_resp = Ether(src='e0:5f:b9:69:e9:22', dst='00:15:17:a7:75:a3')/IP(src=dst_ip,dst=src_ip,version=4L)/UDP(dport=1030, sport=53)/DNS(aa=1L, qr=1L, an=DNSRR(rclass=1, rrname='www.cisco.com.', rdata='100.100.100.100', type=1), ad=0L, qdcount=1, ns=None, tc=0L, rd=0L, ar=None, opcode=0L, ra=1L, cd=0L, z=0L, rcode=0L, qd=DNSQR(qclass=1, qtype=1, qname='www.cisco.com.'))
    return [dns_rqst, dns_resp]

def pkts_to_pcap (pcap_filename, packets):
    wrpcap(pcap_filename, packets)


def main (args):
    # instantiate TRex client
    trex = CTRexClient('trex-dan', verbose = args.verbose)

    if args.steps:
        print "\nNext step: .pcap generation."
        raw_input("Press Enter to continue...")
    # generate TRex traffic.
    pkts = generate_dns_packets('21.0.0.2', '22.0.0.12')    # In this case - DNS traffic (request-response)
    print "\ngenerated traffic:"
    print "=================="
    map(lambda x: pprint(x.summary()) , pkts)
    pkts_to_pcap("dns_traffic.pcap", pkts)              # Export the generated to a .pcap file

    if args.steps:
        print "\nNext step: .yaml generation."
        raw_input("Press Enter to continue...")
    # Generate .yaml file that uses the generated .pcap file
    trex_files_path = trex.get_trex_files_path()        # fetch the path in which packets are saved on TRex server
    yaml_obj        = CTRexYaml(trex_files_path)        # instantiate CTRexYaml obj

    # set .yaml file parameters according to need and use
    ret_idx = yaml_obj.add_pcap_file("dns_traffic.pcap")
    yaml_obj.set_cap_info_param('cps', 1.1, ret_idx)

    # export yaml_ob to .yaml file
    yaml_file_path = trex_files_path + 'dns_traffic.yaml'
    yaml_obj.to_yaml('dns_traffic.yaml')
    print "\ngenerated .yaml file:"
    print "===================="
    yaml_obj.dump()

    if args.steps:
        print "\nNext step: run TRex with provided files."
        raw_input("Press Enter to continue...")
    # push all relevant files to server
    trex.push_files( yaml_obj.get_file_list() )

    print "\nStarting TRex..."
    trex.start_trex(c = 2,
        m = 1.5,
        nc = True,
        p = True,
        d = 30,
        f = yaml_file_path,             # <-- we use out generated .yaml file here
        l = 1000)
    
    if args.verbose:
        print "TRex state changed to 'Running'."
        print "Sampling TRex in 0.2 samples/sec (single sample every 5 secs)"
    
    last_res = dict()
    while trex.is_running(dump_out = last_res):
        print "CURRENT RESULT OBJECT:"
        obj = trex.get_result_obj()
        print obj
        time.sleep(5)


if __name__ == "__main__":
    parser = ArgumentParser(description = 'Run TRex client API end-to-end example.',
        usage = """pkt_generation_for_trex [options]""" )

    parser.add_argument("-s", "--step-by-step", dest="steps",
        action="store_false", help="Switch OFF step-by-step script overview. Default is: ON.", 
        default = True )
    parser.add_argument("--verbose", dest="verbose",
        action="store_true", help="Switch ON verbose option at TRex client. Default is: OFF.",
        default = False )
    args = parser.parse_args()
    main(args)
