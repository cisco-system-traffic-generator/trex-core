
Examples
========

Examples in dpkt/examples
-------------------------

print_packets.py
~~~~~~~~~~~~~~~~
.. automodule:: examples.print_packets

**Code Excerpt**

.. code-block:: python

    # For each packet in the pcap process the contents
    for timestamp, buf in pcap:

        # Print out the timestamp in UTC
        print 'Timestamp: ', str(datetime.datetime.utcfromtimestamp(timestamp))

        # Unpack the Ethernet frame (mac src/dst, ethertype)
        eth = dpkt.ethernet.Ethernet(buf)
        print 'Ethernet Frame: ', mac_addr(eth.src), mac_addr(eth.dst), eth.type

        # Make sure the Ethernet frame contains an IP packet
        # EtherType (IP, ARP, PPPoE, IP6... see http://en.wikipedia.org/wiki/EtherType)
        if eth.type != dpkt.ethernet.ETH_TYPE_IP:  
            print 'Non IP Packet type not supported %s\n' % eth.data.__class__.__name__
            continue

        # Now unpack the data within the Ethernet frame (the IP packet) 
        # Pulling out src, dst, length, fragment info, TTL, and Protocol
        ip = eth.data

        # Pull out fragment information (flags and offset all packed into off field, so use bitmasks)
        do_not_fragment = bool(ip.off & dpkt.ip.IP_DF)
        more_fragments = bool(ip.off & dpkt.ip.IP_MF)
        fragment_offset = ip.off & dpkt.ip.IP_OFFMASK

        # Print out the info
        print 'IP: %s -> %s   (len=%d ttl=%d DF=%d MF=%d offset=%d)\n' % \
              (ip_to_str(ip.src), ip_to_str(ip.dst), ip.len, ip.ttl, do_not_fragment, more_fragments, fragment_offset)

**Example Output**

.. code-block:: json

        Timestamp:  2004-05-13 10:17:07.311224
        Ethernet Frame:  00:00:01:00:00:00 fe:ff:20:00:01:00 2048
        IP: 145.254.160.237 -> 65.208.228.223   (len=48 ttl=128 DF=1 MF=0 offset=0)
        
        Timestamp:  2004-05-13 10:17:08.222534
        Ethernet Frame:  fe:ff:20:00:01:00 00:00:01:00:00:00 2048
        IP: 65.208.228.223 -> 145.254.160.237   (len=48 ttl=47 DF=1 MF=0 offset=0)
        
        ...



Jon Oberheide's Examples
-------------------------
[@jonoberheide's](https://twitter.com/jonoberheide) old examples still
apply:

-  `dpkt Tutorial #1: ICMP
   Echo <https://jon.oberheide.org/blog/2008/08/25/dpkt-tutorial-1-icmp-echo/>`__
-  `dpkt Tutorial #2: Parsing a PCAP
   File <https://jon.oberheide.org/blog/2008/10/15/dpkt-tutorial-2-parsing-a-pcap-file/>`__
-  `dpkt Tutorial #3: dns
   spoofing <https://jon.oberheide.org/blog/2008/12/20/dpkt-tutorial-3-dns-spoofing/>`__
-  `dpkt Tutorial #4: AS Paths from
   MRT/BGP <https://jon.oberheide.org/blog/2009/03/25/dpkt-tutorial-4-as-paths-from-mrt-bgp/>`__

Jeff Silverman Docs/Code
------------------------
`Jeff Silverman <https://github.com/jeffsilverm>`__ has some
`code <https://github.com/jeffsilverm/dpkt_doc>`__ and
`documentation <http://www.commercialventvac.com/dpkt.html>`__.

