#!/usr/bin/python
import time
from pprint import pformat
from nose.tools import assert_raises

from .stl_software_general_test import CStlSoftwareGeneral_Test, CTRexScenario
from trex_stl_lib.api import *


class TaggedPacketGroup_Test(CStlSoftwareGeneral_Test):
    """Test for Tagged Packet Group"""

    MIN_VLAN, MAX_VLAN = 1, (1 << 12) - 1
    QINQ_INNER_VLAN, QINQ_OUTTER_VLAN = 1, 100
    LINE_LENGTH = 35

    def setUp(self):
        """
        Provide a setUp callback for nose to call prior to each test.
        """
        CStlSoftwareGeneral_Test.setUp(self)
        if not self.is_loopback:
            self.skip("Tagged Packet Group Tests require loopback.")
        self.c = CTRexScenario.stl_trex
        self.c.connect()
        port_info = self.c.get_port_info()[0]
        self.drv_name = port_info['driver']
        self.mlx5 = "net_mlx5" in self.drv_name 

    def tearDown(self):
        """
        Provide a tearDown callback for nose to call post each test.
        """
        # In case some tests fail and we don't get to disable, disable through tearDown.
        status = self.c.get_tpg_status()
        if status["enabled"]:
            self.c.disable_tpg()

    def wait_for_packets(self, ports):
        # wait for the traffic to finish
        self.c.wait_on_traffic(ports=ports)

        # Traffic might have ended but takes some time to parse the packets and update the stats.
        time.sleep(0.5)

    def get_tpg_tags(self, qinq, max_tag):
        """
        Get the Tagged Packet Groups Tag Mapping.
        This Mapping is used to map QinQ, Dot1Q to Tag Id.

        Args:
            qinq (bool):  Add a QinQ tag.
            max_tag (int): Max Tag.

        Returns: dict
            Dictionary of mapping QinQ, Dot1Q to tags.
        """
        tpg_conf = []
        # Add one QinQ example, Tag 0.
        if qinq:
            tpg_conf.append(
                {
                    "type": "QinQ",
                    "value": {
                        "vlans": [TaggedPacketGroup_Test.QINQ_INNER_VLAN, TaggedPacketGroup_Test.QINQ_OUTTER_VLAN]
                    }
                }
            )
        # Add All Vlans, Vlan i = Tag i
        biggest_vlan = min(TaggedPacketGroup_Test.MAX_VLAN, max_tag)
        for i in range(TaggedPacketGroup_Test.MIN_VLAN, biggest_vlan):
            tpg_conf.append(
                {
                    "type": "Dot1Q",
                    "value": {
                        "vlan": i,
                    }
                }
            )
        return tpg_conf

    def get_streams(self, burst_size, pps, qinq, vlans, pkt_size=None, untagged=False):
        """
        Get Single Burst Streams with given pps and burst size.

        Args:
            burst_size (int): Burst size for STL Single Burst Stream.
            pps (int): Packets per Second in Stream.
            qinq (bool): Should add a QinQ stream?
            vlans (int): Number of Vlans starts from 1. Each Vlan is a separate stream -> Separate TPGID.
            pkt_size (int): Size of packet in bytes. Optional. If not defined, it is a small packet (78 bytes)
            untagged (bool): Add an untagged packet in the end.

        Returns: list
            List of Single Burst Streams.
        """
        streams = []
        total_pkts = burst_size
        tpgid = 0
        if qinq:
            pkt = Ether()/Dot1Q(vlan=TaggedPacketGroup_Test.QINQ_INNER_VLAN)/Dot1Q(vlan=TaggedPacketGroup_Test.QINQ_OUTTER_VLAN) \
                                        /IP(src="16.0.0.1", dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed'

            if pkt_size is not None and pkt_size > len(pkt):
                pad = (pkt_size - len(pkt)) * 'x'
                pkt = STLPktBuilder(pkt=pkt/pad)
            else:
                pkt = STLPktBuilder(pkt=pkt)

            streams.append(STLStream(name="QinQ",
                                    packet=pkt,
                                    flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                    mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                        pps=pps)))
            tpgid += 1
        for i in range(TaggedPacketGroup_Test.MIN_VLAN, vlans + 1):
            pkt = Ether()/Dot1Q(vlan=i)/IP(src="16.0.0.1", dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed'

            if pkt_size is not None and pkt_size > len(pkt):
                pad = (pkt_size - len(pkt)) * 'x'
                pkt = STLPktBuilder(pkt=pkt/pad)
            else:
                pkt = STLPktBuilder(pkt=pkt)

            # Mlx can't handle bursts of different flows.
            # We add some isg to avoid bursts
            isg = 30 * i if self.mlx5 else 0

            streams.append(STLStream(name="vlan_{}".format(i),
                                    packet=pkt,
                                    isg = isg,
                                    flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                    mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                        pps=pps)))
            tpgid += 1

        if untagged:
            pkt = Ether()/IP(src="16.0.0.1", dst="48.0.0.1")/UDP(dport=12, sport=1025)/'at_least_16_bytes_are_needed'

            if pkt_size is not None and pkt_size > len(pkt):
                pad = (pkt_size - len(pkt)) * 'x'
                pkt = STLPktBuilder(pkt=pkt/pad)
            else:
                pkt = STLPktBuilder(pkt=pkt)

            streams.append(STLStream(name="untagged",
                                    packet=pkt,
                                    flow_stats=STLTaggedPktGroup(tpgid=tpgid),
                                    mode=STLTXSingleBurst(total_pkts=total_pkts,
                                                        pps=pps)))
            tpgid += 1

        return streams

    def verify_stream_stats(self, rx_stats, tx_stats, rx_port, tx_port, pkt_len, burst_size, tpgid, untagged):
        """
        Verify Tagged Packet Group Stats per stream.

        Args:
            rx_stats (dict): Dictionary of Rx stats collected for this tpgid.
            tx_stats (dict): Dictionary of Tx stats collected for this tpgid.
            rx_port (int): Port on which the stats are collected.
            tx_port (int): Port on which the stats are transmitted.
            pkt_len (int): Length of the packet in the stream.
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            tpgid (int): Tagged Packet Group Identifier
            untagged (bool): Verify untagged stats

        Returns: (bool, str)
            True iff the stream stats are valid. False + Message in case of failure.

        """

        ##### Verify Tx Stats #######

        tx_port_stats = tx_stats.get(str(tx_port), None)
        if tx_port_stats is None:
            return (False, "Port {} not found in Tx stats\n".format(tx_port))
        tx_tpgid_stats = tx_port_stats.get(str(tpgid), None)
        if tpgid is None:
            return (False, "tpgid {} not found in port {} Tx stats\n".format(tpgid, tx_port))
        tx_exp = {
            "pkts": burst_size,
            "bytes": burst_size * pkt_len
        }

        if tx_exp != tx_tpgid_stats:
            msg = "tpgid {} Tx stats differ from expected:\n".format(tpgid)
            msg += "Want: {}\n".format(pformat(tx_exp))
            msg += "Have: {}\n".format(pformat(tx_tpgid_stats))
            return (False, msg)

        ##### Verify Rx Stats #######

        # We have build the streams in such a way that Stream with tpgid = i should
        # have traffic on tag number i only.
        tag = tpgid
        port_stats = rx_stats.get(str(rx_port), None)
        if port_stats is None:
            return (False,"Port {} not found in Rx stats".format(rx_port))
        tpgid_stats = port_stats.get(str(tpgid), None)
        if tpgid is None:
            return (False, "tpgid {} not found in port {} Rx stats".format(tpgid, rx_port))

        exp = {
                "pkts": burst_size,
                "bytes": burst_size * pkt_len,
                "ooo": 0,
                "dup": 0,
                "seq_err": 0,
                "seq_err_too_big": 0,
                "seq_err_too_small": 0
            }

        if untagged:
            untagged_stats =  tpgid_stats.get("untagged", None)
            if untagged_stats is None:
                return (False, "untagged stats not found in tpgid {} Rx stats".format(tpgid))
            if exp != untagged_stats:
                msg = "Untagged Rx stats differ from expected for tpgid {}: \n".format(tpgid)
                msg += "Want: {}\n".format(pformat(exp))
                msg += "Have: {}\n".format(pformat(untagged_stats))
                return (False, msg)
        else:
            tag_stats = tpgid_stats.get(str(tag), None)
            if tag_stats is None:
                return (False, "tag {} not found in tpgid {} Rx stats".format(tag, tpgid))
            if exp != tag_stats:
                msg = "Tag {} Rx stats differ from expected: \n".format(tag)
                msg += "Want: {}\n".format(pformat(exp))
                msg += "Have: {}\n".format(pformat(tag_stats))
                return (False, msg)

        return (True, "")

    def tpg_iteration(self, tx_ports, rx_ports, streams, num_tags, burst_size, untagged, verbose):
        """
        Run one TPG iteration, meaning:
        1. Enable
        2. Start some streams and verify stats
        3. Disable

        Args:
            tx_ports (list): List of ports on which we transmit. Note, we assume: tx_ports[i] <-> rx_ports[i]
            rx_ports (list): List of ports on which we receive. Note, we assume: tx_ports[i] <-> rx_ports[i]
            streams (list): List of streams to starts.
            num_tags (list): Number of Vlan Tags, which also equals the number of streams since each TPGID has its own tag.
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            untagged (bool): Verify untagged stats.
            verbose (bool): Should be verbose and print errors.

        """
        num_streams = len(streams)

        # add the streams
        self.c.add_streams(streams, ports=tx_ports)

        # start the traffic
        self.c.start(ports=tx_ports, force=True)

        # Wait for packets to arrive and get parsed by Rx
        self.wait_for_packets(ports=list(set(tx_ports + rx_ports)))

        self.c.remove_all_streams()

        for i in range(num_streams):
            # stream number = tpgid
            untagged_flag = (i == num_streams - 1) and untagged # Only the last tpgid may be untagged
            if verbose:
                print("*"*TaggedPacketGroup_Test.LINE_LENGTH)
                print("Verifying stats for tpgid {}".format(i))

            for j in range(len(tx_ports)):
                # Assumes tx_ports[i] <-> rx_ports[i]
                tx_stats = self.c.get_tpg_tx_stats(port=tx_ports[j], tpgid=i)
                rx_stats = self.c.get_tpg_stats(port=rx_ports[j], tpgid=i, min_tag=0, max_tag=num_tags, max_sections=num_tags, untagged=untagged_flag)[0]

                tpgid_passed, msg = self.verify_stream_stats(rx_stats, tx_stats, rx_ports[j], tx_ports[j], streams[i].get_pkt_len(), burst_size, i, untagged_flag)
                if verbose:
                    indicator = "PASSED" if tpgid_passed else "FAILED"
                    print(indicator)
                assert tpgid_passed, msg

    def _single_burst_skeleton(self, rx_ports, tx_ports, burst_size, pps, qinq, vlans, warmup, 
                               pkt_size=None, untagged=False, disable=True, verbose=False):
        """
        A skeleton that provides a single burst TPG test that can be configured 
        in many different constellations.

        Args:
            rx_ports (list): List of ports on which we receive. Note, we assume: tx_ports[i] <-> rx_ports[i]
            tx_ports (list): List of ports on which we transmit. Note, we assume: tx_ports[i] <-> rx_ports[i]
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            pps (int): Packets per second in each stream.
            qinq (bool): Should add a QinQ stream?
            vlans (int): Number of Vlans. Each Vlan is sent in a separate stream.
            warmup (bool): Send some warmup streams with low pps to stabilize the system. It is important because the counters are allocated with CopyOnWrite.
            pkt_size (int, optional): Specify the size of the packet. Defaults to None, which sends a small packet.
            untagged (bool): Should send untagged traffic. Defaults to False.
            disable (bool): Disable TPG at the end. Defaults to True.
            verbose (bool): Should be verbose and print errors. Defaults to False.
        """
        if verbose:
            print("\nTPG Single burst: {} packets with pps {} per stream. QinQ={}, Num Streams/Vlans = {}".format(burst_size, pps, qinq, vlans))

        streams = self.get_streams(burst_size, pps, qinq, vlans, pkt_size, untagged)
        num_streams = len(streams)

        ports = list(set(rx_ports + tx_ports)) # all ports, convert to set for unique.

        # prepare our ports
        self.c.reset(ports=ports)

        # get tpg tags configuration
        tags = self.get_tpg_tags(qinq, max_tag=vlans+1)

        num_tags = len(tags)

        # enable tpg
        self.c.enable_tpg(num_tpgids=num_streams, tags=tags, rx_ports=rx_ports)

        current_burst_size = 0

        # In case we want to warmup the Rx stats with some slow streams
        if warmup:
            current_burst_size = 1
            warmup_streams = self.get_streams(current_burst_size, 1, qinq, vlans, pkt_size, untagged) # one packet just to warmup the memory
            self.tpg_iteration(tx_ports=tx_ports, rx_ports=rx_ports, streams=warmup_streams, num_tags=num_tags, burst_size=current_burst_size, untagged=untagged, verbose=verbose)

        current_burst_size += burst_size

        # one iteration
        self.tpg_iteration(tx_ports=tx_ports, rx_ports=rx_ports, streams=streams, num_tags=num_tags, burst_size=current_burst_size, untagged=untagged, verbose=verbose)

        # disable tpg
        if disable:
            self.c.disable_tpg()

    ################################################################
    # Test Api Sanity in invalid cases
    ################################################################
    def test_tpg_api_invalid(self):
        """
        Test the TPG Api in Invalid Cases.
        """
        # Assert TPG is disabled
        status = self.c.get_tpg_status()
        assert status["enabled"] == False, "TPG is not disabled when starting the test"

        # Try and run invalid cmds when TPG is disabled.
        with assert_raises(TRexError):
            self.c.disable_tpg()
        with assert_raises(TRexError):
            self.c.get_tpg_tx_stats(port=0, tpgid=0)
        with assert_raises(TRexError):
            self.c.get_tpg_stats(port=0, tpgid=0, min_tag=0, max_tag=10)


        # Enable
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self.c.reset(ports=[tx_port, rx_port])
        # get tpg tags configuration
        tags = self.get_tpg_tags(qinq=False, max_tag=100)
        num_tags = len(tags)

        num_tpgids = 10
        # enable tpg
        self.c.enable_tpg(num_tpgids=num_tpgids, tags=tags, rx_ports=[rx_port])

        with assert_raises(TRexError):
            # Re-enable will raise
            self.c.enable_tpg(num_tpgids=num_tpgids, tags=tags, rx_ports=[rx_port])

        with assert_raises(TRexError):
            # Invalid tpgid
            self.c.get_tpg_tx_stats(port=tx_port, tpgid=num_tpgids+1)

        with assert_raises(TRexError):
            # Invalid port
            self.c.get_tpg_stats(port=tx_port, tpgid=num_tpgids-1, min_tag=0, max_tag=num_tags-1)

        with assert_raises(TRexError):
            # Invalid tpgid
            self.c.get_tpg_stats(port=tx_port, tpgid=num_tpgids+1, min_tag=0, max_tag=num_tags-1)

        with assert_raises(TRexError):
            # Invalid tag
            self.c.get_tpg_stats(port=tx_port, tpgid=num_tpgids-1, min_tag=0, max_tag=num_tags+1)

        with assert_raises(TRexError):
            # Invalid port
            self.c.get_tpg_unknown_tags(port=tx_port)

        with assert_raises(TRexError):
            # Invalid port
            self.c.clear_tpg_unknown_tags(port=tx_port)

        # Assert TPG is enabled
        status = self.c.get_tpg_status()
        assert status["enabled"] == True, "TPG should be enabled"

        self.c.disable_tpg()

    def test_tpg_unknown_tags(self,):
        num_tags = 10 # Dot1Q(1) to Dot1Q(10)
        num_tpgids = 15
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        tags = self.get_tpg_tags(qinq=False, max_tag=num_tags+1)

        # QinQ(1, 100), Dot1Q(num_tags + 1) and Dot1Q(num_tags + 2) will be unknown
        streams = self.get_streams(1, 1, qinq=True, vlans=num_tags + 2, pkt_size=100) # one packet but num_tpgids streams

        # acquire ports
        self.c.reset(ports=[tx_port, rx_port])

        # enable tpg
        self.c.enable_tpg(num_tpgids=num_tpgids, tags=tags, rx_ports=[rx_port])

        # add the streams
        self.c.add_streams(streams, ports=[tx_port])

        # start the traffic
        self.c.start(ports=[tx_port], force=True)

        # Wait for packets to arrive and get parsed by Rx
        self.wait_for_packets(ports=[tx_port, rx_port])

        self.c.remove_all_streams()

        unknown_tags = self.c.get_tpg_unknown_tags(rx_port)

        expected_unknowns = [
             {'tag': {'type': 'Dot1Q', 'value': {'vlan': 12}}, 'tpgid': 12},
             {'tag': {'type': 'QinQ', 'value': {'vlans': [1, 100]}}, 'tpgid': 0},
             {'tag': {'type': 'Dot1Q', 'value': {'vlan': 11}}, 'tpgid': 11}
        ]

        port_unknown_tags = unknown_tags.get(str(rx_port), None)
        assert port_unknown_tags is not None, "Unknown Tags not found for port {}".format(rx_port)
        assert len(expected_unknowns) == len(port_unknown_tags), "Length Expected Unknown Tags != Length Unknown Tags"

        for tag in port_unknown_tags:
            assert tag in expected_unknowns, "Tag {} unknown but not expected.".format(pformat(tag))

        # Clear TPG Unknown Tags
        self.c.clear_tpg_unknown_tags(rx_port)

        # Get again and expect empty.
        unknown_tags = self.c.get_tpg_unknown_tags(rx_port)
        port_unknown_tags = unknown_tags.get(str(rx_port), None)
        assert port_unknown_tags is not None, "Unknown Tags not found for port {}".format(rx_port)
        assert len(port_unknown_tags) == 0, "Unknown Tags not empty after clear."

    ################################################################
    # Enable Disable Test.
    # Verify that the enablement doesn't fail. Send one packet
    # and verify stats.
    # NOTE: num_tpgids >= num_tags
    ################################################################
    def enable_disable_tpg(self, num_iterations, num_tpgids, num_tags, tx_port, rx_port):
        """
        Test skeleton to test enabling and disabling TPG. It is important to stress test
        the allocation of counters.

        Args:
            num_iterations (int): Number of times to Enable->Collect->Disable.
            num_tpgids (int): Number of tpgids in each iteration.
            num_tags (int): Number of Tags to enable the TPG. NOTE: num_tpgids >= num_tags since each tag gets its own stream.
            tx_port (int): Port on which we transmit.
            rx_port (int): Port on which we receive.
        """

        tags = self.get_tpg_tags(qinq=False, max_tag=num_tags+1)

        warmup_streams = self.get_streams(1, 1, qinq=False, vlans=num_tags, pkt_size=100) # one packet just to warmup the memory
        self.c.reset(ports=[tx_port, rx_port])

        for _ in range(num_iterations):
            self.c.enable_tpg(num_tpgids=num_tpgids, tags=tags, rx_ports=[rx_port])
            self.tpg_iteration(tx_ports=[tx_port], rx_ports=[rx_port], streams=warmup_streams, num_tags=num_tags, burst_size=1, untagged=False, verbose=False)
            self.c.disable_tpg()

    def test_enable_disable_tpg_small(self):
        """
        Small allocations with a few streams but a lot of iterations.
        """
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self.enable_disable_tpg(num_iterations=100, num_tpgids=10, num_tags=10, tx_port=tx_port, rx_port=rx_port)

    def test_enable_disable_tpg_medium(self):
        """
        Average number of streams and tags and 50 allocations.
        """
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self.enable_disable_tpg(num_iterations=50, num_tpgids=100, num_tags=100, tx_port=tx_port, rx_port=rx_port)

    def test_enable_disable_tpg_large(self):
        """
        A few iterations, a lot of tags and allocations.
        """
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self.enable_disable_tpg(num_iterations=10, num_tpgids=1000, num_tags=1000, tx_port=tx_port, rx_port=rx_port)

    def test_enable_disable_tpg_huge(self):
        """
        Massive allocations and a lot of streams.
        """
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self.enable_disable_tpg(num_iterations=3, num_tpgids=10000, num_tags=4000, tx_port=tx_port, rx_port=rx_port)

    ################################################################
    # Single Burst - 1 Rx port only
    # Different Num of Streams
    ################################################################
    def single_burst_single_rx(self, burst_size, pps, qinq, vlans, warmup, pkt_size=None, untagged=False, verbose=False):
        """
        Base function to call for sending TPG in single burst with 1 Tx port and 1 Rx port.

        Args:
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            pps (int): Packets per second in each stream.
            qinq (bool): Should add a QinQ stream?
            vlans (int): Number of Vlans. Each Vlan is sent in a separate stream.
            warmup (bool): Send some warmup streams with low pps to stabilize the system. It is important because the counters are allocated with CopyOnWrite.
            pkt_size (int, optional): Specify the size of the packet. Defaults to None, which sends a small packet.
            untagged (bool): Should send and verify untagged traffic too?
            verbose (bool): Should be verbose and print errors.
        """
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self._single_burst_skeleton([rx_port], [tx_port], burst_size, pps, qinq, vlans, warmup, pkt_size, untagged, verbose)

    def test_single_burst_one_stream(self):
        pps = int(2.5e6) if not self.mlx5 else int(1e6) # 2.5 Mpps (a bit lower than max so we avoid random failures)
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=1, warmup=True)

    def test_single_burst_one_stream_qinq(self):
        pps = int(2e6) if not self.mlx5 else int(1e6) # 2 Mpps
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=True, vlans=0, warmup=True)

    def test_single_burst_few_streams(self):
        num_streams = 5
        pps = int(4e5) if not self.mlx5 else int(2e5) # 400 Kpps
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True)

    def test_single_burst_mini(self):
        num_streams = 100
        pps = 100 # 100
        longevity = 1  # 5 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=False, verbose=False)

    def test_single_burst_mini_qinq(self):
        num_streams = 100
        pps = 100 # 100
        longevity = 5  # 5 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=True, vlans=num_streams, warmup=False, verbose=False)

    def test_single_burst_one_stream_big_pkt(self):
        pps = int(1.5e6) if not self.mlx5 else int(1e6) # 1.5 Mpps
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        pkt_size = 1400
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=True, vlans=0, warmup=True, pkt_size=pkt_size, verbose=False)

    def test_single_burst_1k_streams(self):
        num_streams = 1000 # 1k streams
        pps = 1000 # 1000 pps per stream x 1k streams = 1Mpps
        longevity = 10  # 10 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True, verbose=False)

    def test_single_burst_4k_streams(self):
        num_streams = 4000 # 4k streams
        pps = 250 # 250 pps per stream x 4k streams = 1Mpps
        longevity = 10  # 10 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True, verbose=False)

    def test_singe_burst_mini_with_untagged(self):
        num_streams = 100
        pps = 100 # 100
        longevity = 1  # 5 seconds
        burst_size = pps * longevity
        self.single_burst_single_rx(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True, untagged=True, verbose=False)

    ################################################################
    # Single Burst - BiDir
    ################################################################
    def single_burst_bi_dir(self, burst_size, pps, qinq, vlans, warmup, pkt_size=None, untagged=False, verbose=False):
        """
        Base function to call for sending TPG with 1 Tx port and 1 Rx port but bi directional.

        Args:
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            pps (int): Packets per second in each stream.
            qinq (bool): Should add a QinQ stream?
            vlans (int): Number of Vlans. Each Vlan is sent in a separate stream.
            warmup (bool): Send some warmup streams with low pps to stabilize the system. It is important because the counters are allocated with CopyOnWrite.
            pkt_size (int, optional): Specify the size of the packet. Defaults to None, which sends a small packet.
            untagged (bool): Should send and verify untagged traffic too?
            verbose (bool): Should be verbose and print errors.
        """
        tx, rx = CTRexScenario.ports_map['bi'][0]
        self._single_burst_skeleton([tx, rx], [rx, tx], burst_size, pps, qinq, vlans, warmup, pkt_size, untagged, verbose)

    def test_single_burst_one_stream_bi_dir(self):
        pps = int(1e6) if not self.mlx5 else int(5e5)  # 1 Mpps x 2 Dir = 2 Mpps
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        self.single_burst_bi_dir(burst_size=burst_size, pps=pps, qinq=False, vlans=1, warmup=True)

    def test_single_burst_1k_streams_bi_dir(self):
        num_streams = 1000 # 1k streams
        pps = 1000 if not self.mlx5 else 500 # 1000 pps per stream x 1k streams x 2 Dir = 2Mpps
        longevity = 20  # 20 seconds
        burst_size = pps * longevity
        self.single_burst_bi_dir(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True)

    def test_single_burst_1k_streams_bi_dir_with_untagged(self):
        num_streams = 1000 # 1k streams
        pps = 1000 if not self.mlx5 else 500 # 1000 pps per stream x 1k streams x 2 Dir = 2Mpps
        longevity = 20  # 20 seconds
        burst_size = pps * longevity
        self.single_burst_bi_dir(burst_size=burst_size, pps=pps, qinq=False, vlans=num_streams, warmup=True, untagged=True)

    ################################################################
    # Single Burst - One Dir - All ports
    ################################################################
    def single_burst_all_ports(self, burst_size, pps, qinq, vlans, warmup, pkt_size=None, untagged=False, verbose=False):
        """
        Base function to call for sending TPG one directional but in all dual ports.

        Args:
            burst_size (int): Burst Size, equals number of packets that we attempted to transmit.
            pps (int): Packets per second in each stream.
            qinq (bool): Should add a QinQ stream?
            vlans (int): Number of Vlans. Each Vlan is sent in a separate stream.
            warmup (bool): Send some warmup streams with low pps to stabilize the system. It is important because the counters are allocated with CopyOnWrite.
            pkt_size (int, optional): Specify the size of the packet. Defaults to None, which sends a small packet.
            untagged (bool): Should send and verify untagged traffic too?
            verbose (bool): Should be verbose and print errors.
        """
        tx_ports, rx_ports = zip(*CTRexScenario.ports_map['bi'])
        self._single_burst_skeleton(list(rx_ports), list(tx_ports), burst_size, pps, qinq, vlans, warmup, pkt_size, untagged, verbose)

    def test_single_burst_one_stream_all_ports(self):
        pps = int(2e5) # 200k pps - we don't know how many ports are transmitting
        longevity = 20 # 20 seconds
        burst_size = pps * longevity
        self.single_burst_all_ports(burst_size=burst_size, pps=pps, qinq=False, vlans=1, warmup=True)

    ################################################################
    # Clear
    ################################################################
    def test_clear_stats(self):
        num_streams = 100
        pps = 100 # 100
        longevity = 5  # 5 seconds
        burst_size = pps * longevity
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self._single_burst_skeleton(rx_ports=[rx_port], tx_ports=[tx_port], burst_size=burst_size, pps=pps, 
                                    qinq=False, vlans=num_streams, warmup=False, untagged=False, disable=False)

        for tpgid in range(num_streams):
            # Clear tpgid stats
            self.c.clear_tpg_tx_stats(tx_port, tpgid)
            self.c.clear_tpg_stats(rx_port, tpgid, tpgid, tpgid+1)
            tx_stats = self.c.get_tpg_tx_stats(port=tx_port, tpgid=tpgid)
            rx_stats = self.c.get_tpg_stats(port=rx_port, tpgid=tpgid, min_tag=tpgid, max_tag=tpgid+1, max_sections=num_streams, untagged=False)[0]

            tpgid_passed, msg = self.verify_stream_stats(rx_stats, tx_stats, rx_port, tx_port, 0, 0, tpgid, False)

            assert tpgid_passed, msg

        self.c.disable_tpg()

    def test_clear_untagged_stats(self):
        num_streams = 100
        pps = 100 # 100
        longevity = 5  # 5 seconds
        burst_size = pps * longevity
        tx_port, rx_port = CTRexScenario.ports_map['bi'][0]
        self._single_burst_skeleton(rx_ports=[rx_port], tx_ports=[tx_port], burst_size=burst_size, pps=pps, 
                                    qinq=False, vlans=num_streams, warmup=False, untagged=True, disable=False)

        exp = {
                "pkts": 0,
                "bytes": 0,
                "ooo": 0,
                "dup": 0,
                "seq_err": 0,
                "seq_err_too_big": 0,
                "seq_err_too_small": 0
            }

        for tpgid in range(num_streams):
            # Clear tpgid stats
            self.c.clear_tpg_stats(rx_port, tpgid, tpgid, tpgid+1, untagged=True)
            rx_stats = self.c.get_tpg_stats(port=rx_port, tpgid=tpgid, min_tag=tpgid, max_tag=tpgid+1, max_sections=num_streams, untagged=True)[0]

            port_stats = rx_stats.get(str(rx_port), None)
            assert port_stats is not None, "Port {} not found in Rx stats".format(rx_port)
            tpgid_stats = port_stats.get(str(tpgid), None)
            assert tpgid_stats is not None, "tpgid {} not found in port {} Rx stats".format(tpgid, rx_port)


            untagged_stats =  tpgid_stats.get("untagged", None)
            assert untagged_stats is not None, "untagged stats not found in tpgid {} Rx stats".format(tpgid)
            if exp != untagged_stats:
                    msg = "Untagged Rx stats differ from expected for tpgid {}: \n".format(tpgid)
                    msg += "Want: {}\n".format(pformat(exp))
                    msg += "Have: {}\n".format(pformat(untagged_stats))
                    assert False, msg

        self.c.disable_tpg()
