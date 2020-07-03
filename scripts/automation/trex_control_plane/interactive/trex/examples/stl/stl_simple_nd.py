import stl_path
from trex.stl.api import *
import time
import ipaddress
import sys


def inc_ip(ip, add):
    ip_arg = ip

    if sys.version_info < (3,):
        ip_arg = ip.decode()

    return str(ipaddress.ip_address(ip_arg) + add)


c = STLClient()
passed = True

port = 0
source_ipv6_start = '2001:db8::1:100'
target_ipv6 = '2001:db8::1:1'
mcast_dst_ipv6 = 'ff02::1:ff01:1'
mcast_dst_mac = '33:33:00:01:00:01'
count = 10

try:
    # connect to server
    c.connect()

    # preparing the stream
    pkt_base = Ether(dst=mcast_dst_mac)/IPv6(src=source_ipv6_start, dst=mcast_dst_ipv6, hlim=255)/ICMPv6ND_NS(tgt=target_ipv6)
    pkt_base /= ICMPv6NDOptSrcLLAddr(lladdr=c.get_port(port).get_layer_cfg()['ether']['src'])
    vm = [
        STLVmFlowVar(name="src", min_value=1, max_value=count, size=1, op="inc"),
        STLVmWrFlowVar(fv_name="src", pkt_offset= "IPv6.src", offset_fixup=15),
        STLVmFixIcmpv6(l3_offset = "IPv6", l4_offset=ICMPv6ND_NS().name)
    ]
    pkt = STLPktBuilder(pkt = pkt_base, vm = vm)
    stream = STLStream(packet = pkt, mode = STLTXSingleBurst(pps = 100, total_pkts = count))
    

    # prepare our ports
    c.reset(ports = port)
    c.set_port_attr(promiscuous=True)

    # add stream to port
    c.add_streams(stream, ports = port)

    # starting the capture
    c.set_service_mode(port)
    

    capture_id = c.start_capture(rx_ports = port)['id']

    # starting and waiting for traffic
    c.start(ports = port, force = True)
    c.wait_on_traffic(ports = port)
    time.sleep(5)

    # getting the captured packets
    packets = []
    c.stop_capture(capture_id, output=packets)

    # validating NA response presents within captured packets
    expected_targets = [inc_ip(source_ipv6_start, 1 + i) for i in range(count)]

    for p in packets:
        scapy_pkt = Ether(p['binary'])
        if ICMPv6ND_NA in scapy_pkt:
            found_ip = scapy_pkt[IPv6].dst
            print('Found NA for {}'.format(found_ip))
            expected_targets.remove(found_ip)

    for target in expected_targets:
        print('Warning, NA for {} was not found'.format(target))


    passed = not expected_targets

except STLError as e:
    passed = False
    print(e)

finally:
    c.disconnect()

if c.get_warnings():
        print("\n\n*** test had warnings ****\n\n")
        for w in c.get_warnings():
            print(w)

if passed and not c.get_warnings():
    print("\nTest has passed :-)\n")
else:
    print("\nTest has failed :-(\n")


