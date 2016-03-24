import stl_path
from trex_stl_lib.api import *
import argparse
import sys

def create_vm (ip_start, ip_end):
     vm =[

            # dest
            STLVmFlowVar(name="dst", min_value = ip_start, max_value = ip_end, size = 4, op = "inc"),
            STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst"),

            # checksum
            STLVmFixIpv4(offset = "IP")

            ]

     return vm

# warning: might make test slow
def alter_streams(streams, remove_fcs, vlan_id):
    for stream in streams:
        packet = Ether(stream.pkt)
        if vlan_id >= 0 and vlan_id <= 4096:
            packet_l3 = packet.payload
            packet = Ether() / Dot1Q(vlan = vlan_id) / packet_l3
        if remove_fcs and packet.lastlayer().name == 'Padding':
            packet.lastlayer().underlayer.remove_payload()
        packet = STLPktBuilder(packet)
        stream.fields['packet'] = packet.dump_pkt()
        stream.pkt = base64.b64decode(stream.fields['packet']['binary'])

def inject_pcap (pcap_file, server, port, loop_count, ipg_usec, use_vm, remove_fcs, vlan_id):

    # create client
    c = STLClient(server = server)
    
    try:
        if use_vm:
            vm = create_vm("10.0.0.1", "10.0.0.254")
        else:
            vm = None

        profile = STLProfile.load_pcap(pcap_file, ipg_usec = ipg_usec, loop_count = loop_count, vm = vm)

        print("Loaded pcap {0} with {1} packets...\n".format(pcap_file, len(profile)))
        streams = profile.get_streams()
        if remove_fcs or (vlan_id >= 0 and vlan_id <= 4096):
            alter_streams(streams, remove_fcs, vlan_id)

        # uncomment this for simulator run
        #STLSim().run(profile.get_streams(), outfile = '/auto/srg-sce-swinfra-usr/emb/users/ybrustin/out.pcap')

        c.connect()
        c.reset(ports = [port])
        stream_ids = c.add_streams(streams, ports = [port])

        c.clear_stats()

        c.start()
        c.wait_on_traffic(ports = [port])

        stats = c.get_stats()
        opackets = stats[port]['opackets']
        print("{0} packets were Tx on port {1}\n".format(opackets, port))

    except STLError as e:
        print(e)
        sys.exit(1)

    finally:
        c.disconnect()


def setParserOptions():
    parser = argparse.ArgumentParser(prog="stl_pcap.py")

    parser.add_argument("-f", "--file", help = "pcap file to inject",
                        dest = "pcap",
                        required = True,
                        type = str)

    parser.add_argument("-s", "--server", help = "TRex server address",
                        dest = "server",
                        default = 'localhost',
                        type = str)

    parser.add_argument("-p", "--port", help = "port to inject on",
                        dest = "port",
                        required = True,
                        type = int)

    parser.add_argument("-n", "--number", help = "How many times to inject pcap [default is 1, 0 means forever]",
                        dest = "loop_count",
                        default = 1,
                        type = int)

    parser.add_argument("-i", help = "IPG in usec",
                        dest = "ipg",
                        default = 10.0,
                        type = float)

    parser.add_argument("-x", help = "Iterate over IP dest",
                        dest = "use_vm",
                        default = False,
                        action = "store_true")

    parser.add_argument("-r", "--remove-fcs", help = "Remove FCS if exists. Limited by Scapy capabilities.",
                        dest = "remove",
                        default = False,
                        action = "store_true")

    parser.add_argument("-v", "--vlan", help = "Add VLAN header with this ID. Limited by Scapy capabilities.",
                        dest = "vlan",
                        default = -1,
                        type = int)

    return parser

def main ():
    parser = setParserOptions()
    options = parser.parse_args()

    inject_pcap(options.pcap, options.server, options.port, options.loop_count, options.ipg, options.use_vm, options.remove, options.vlan)

# inject pcap
if __name__ == '__main__':
    main()

