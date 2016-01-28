import sys
sys.path.insert(0, "../")

from trex_stl_api import *
import dpkt
import time

def simple_burst ():
    
    # build a simple packet

    pkt_bld = STLPktBuilder()
    pkt_bld.add_pkt_layer("l2", dpkt.ethernet.Ethernet())
    # set Ethernet layer attributes
    pkt_bld.set_eth_layer_addr("l2", "src", "00:15:17:a7:75:a3")
    pkt_bld.set_eth_layer_addr("l2", "dst", "e0:5f:b9:69:e9:22")
    pkt_bld.set_layer_attr("l2", "type", dpkt.ethernet.ETH_TYPE_IP)
    # set IP layer attributes
    pkt_bld.add_pkt_layer("l3_ip", dpkt.ip.IP())
    pkt_bld.set_ip_layer_addr("l3_ip", "src", "21.0.0.2")
    pkt_bld.set_ip_layer_addr("l3_ip", "dst", "22.0.0.12")
    pkt_bld.set_layer_attr("l3_ip", "p", dpkt.ip.IP_PROTO_TCP)
    # set TCP layer attributes
    pkt_bld.add_pkt_layer("l4_tcp", dpkt.tcp.TCP())
    pkt_bld.set_layer_attr("l4_tcp", "sport", 13311)
    pkt_bld.set_layer_attr("l4_tcp", "dport", 80)
    pkt_bld.set_layer_attr("l4_tcp", "flags", 0)
    pkt_bld.set_layer_attr("l4_tcp", "win", 32768)
    pkt_bld.set_layer_attr("l4_tcp", "seq", 0)
    #pkt_bld.set_pkt_payload("abcdefgh")
    pkt_bld.set_layer_attr("l3_ip", "len", len(pkt_bld.get_layer('l3_ip')))

   
    # create client
    c = STLClient()
    passed = True

    try:
        
        #c.set_verbose("high")

        # create two bursts and link them
        s1 = STLStream(packet = pkt_bld,
                       mode = STLTXSingleBurst(total_pkts = 5000)
                       )

        s2 = STLStream(packet = pkt_bld,
                       mode = STLTXSingleBurst(total_pkts = 3000),
                       next_stream_id = s1.get_id())


        # connect to server
        c.connect()

        # prepare our ports
        c.reset(ports = [0, 1])

        # add both streams to ports
        stream_ids = c.add_streams([s1, s2], ports = [0, 1])

        # run 5 times
        for i in xrange(1, 6):
            c.clear_stats()
            c.start(ports = [0, 1], mult = "1gbps")
            c.wait_on_traffic(ports = [0, 1])

            stats = c.get_stats()
            ipackets  = stats['total']['ipackets']

            print "Test iteration {0} - Packets Received: {1} ".format(i, ipackets)
            # (5000 + 3000) * 2 ports = 16,000
            if (ipackets != (16000)):
                passed = False

    except STLError as e:
        passed = False
        print e

    finally:
        c.disconnect()

    if passed:
        print "\nTest has passed :-)\n"
    else:
        print "\nTest has failed :-(\n"


# run the tests
simple_burst()

