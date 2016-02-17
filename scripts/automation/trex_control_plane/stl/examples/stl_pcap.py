import stl_path
from trex_stl_lib.api import *
import argparse


def create_vm (ip_start, ip_end):
     vm =[

            # dest
            STLVmFlowVar(name="dst", min_value = ip_start, max_value = ip_end, size = 4, op = "inc"),
            STLVmWrFlowVar(fv_name="dst",pkt_offset= "IP.dst"),

            # checksum
            STLVmFixIpv4(offset = "IP")

            ]

     return vm


def inject_pcap (pcap_file, port, loop_count, ipg_usec, use_vm):

    # create client
    c = STLClient()
    
    try:
        if use_vm:
            vm = create_vm("10.0.0.1", "10.0.0.254")
        else:
            vm = None

        profile = STLProfile.load_pcap(pcap_file, ipg_usec = ipg_usec, loop_count = loop_count, vm = vm)

        print "Loaded pcap {0} with {1} packets...\n".format(pcap_file, len(profile))

        # uncomment this for simulator run
        #STLSim().run(profile.get_streams(), outfile = 'out.cap')

        c.connect()
        c.reset(ports = [port])
        stream_ids = c.add_streams(profile.get_streams(), ports = [port])

        c.clear_stats()

        c.start()
        c.wait_on_traffic(ports = [port])

        stats = c.get_stats()
        opackets = stats[port]['opackets']
        print "{0} packets were Tx on port {1}\n".format(opackets, port)

    except STLError as e:
        print e

    finally:
        c.disconnect()


def setParserOptions():
    parser = argparse.ArgumentParser(prog="stl_pcap.py")

    parser.add_argument("-f", help = "pcap file to inject",
                        dest = "pcap",
                        required = True,
                        type = str)

    parser.add_argument("-p", help = "port to inject on",
                        dest = "port",
                        required = True,
                        type = int)

    parser.add_argument("-n", help = "How many times to inject pcap [default is 1, 0 means forever]",
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

    return parser

def main ():
    parser = setParserOptions()
    options = parser.parse_args()

    inject_pcap(options.pcap, options.port, options.loop_count, options.ipg, options.use_vm)

# inject pcap
if __name__ == '__main__':
    main()

