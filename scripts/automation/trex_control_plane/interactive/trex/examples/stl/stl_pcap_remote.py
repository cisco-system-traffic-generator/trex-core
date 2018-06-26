import stl_path
from trex.stl.api import *
import argparse
import sys


def inject_pcap (c, pcap_file, port, loop_count, ipg_usec, duration):

    pcap_file = os.path.abspath(pcap_file)

    c.reset(ports = [port])
    c.push_remote(pcap_file, ports = [port], ipg_usec = ipg_usec, speedup = 1.0, count = loop_count, duration = duration)
    # assume 100 seconds is enough - but can be more
    c.wait_on_traffic(ports = [port], timeout = 100)

    stats = c.get_stats()
    opackets = stats[port]['opackets']

    return opackets
    #print("{0} packets were Tx on port {1}\n".format(opackets, port))

    

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
                        default = None,
                        type = float)

    parser.add_argument("-d", help = "duration in seconds",
                        dest = "duration",
                        default = -1,
                        type = float)

    return parser

def sizeof_fmt(num, suffix='B'):
    for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
        if abs(num) < 1024.0:
            return "%3.1f%s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f%s%s" % (num, 'Yi', suffix)


def read_txt_file (filename):

    with open(filename) as f:
        lines = f.readlines()

    caps = []
    for raw in lines:
       raw = raw.rstrip()
       if raw[0] == '#':
           continue
       ext=os.path.splitext(raw)[1]
       if ext not in ['.cap', '.pcap', '.erf']:
           # skip unknown format
           continue
    
       caps.append(raw)

    return caps


def start (args):
    
    parser = setParserOptions()
    options = parser.parse_args(args)
    
    ext = os.path.splitext(options.pcap)[1]
    if ext == '.txt':
        caps = read_txt_file(options.pcap)
    elif ext in ['.cap', '.pcap']:
        caps = [options.pcap]
    else:
        print("unknown file extension for file {0}".format(options.pcap))
        return

    c = STLClient(server = options.server)
    try:
        c.connect()
        for i, cap in enumerate(caps, start = 1):
            before = time.time()
            print ("{:} CAP {:} @ {:} - ".format(i, cap, sizeof_fmt(os.path.getsize(cap)))),
            injected = inject_pcap(c, cap, options.port, options.loop_count, options.ipg, options.duration)
            print("took {:.2f} seconds for {:} packets").format(time.time() - before, injected)

    except STLError as e:
        print(e)
        return

    finally:
        c.disconnect()

def main ():
    start(sys.argv[1:])

# inject pcap
if __name__ == '__main__':
    main()

