from trex_stl_lib.api import *
import argparse
import ipaddress
import math

from scapy.compat import raw
from scapy.contrib.roce import AETH, BTH, RETH, opcode


ROCEV2_UDP_PORT = 4791
ETH_FCS_LEN = 4
IPV4_DEFAULT_SRC = "16.0.0.1"
IPV4_DEFAULT_DST = "48.0.0.1"
IPV6_DEFAULT_SRC = "2001:db8:16::1"
IPV6_DEFAULT_DST = "2001:db8:48::1"


def parse_int(value):
    return int(str(value), 0)


def parse_payload_pattern(value):
    if value.startswith("0x"):
        return bytes.fromhex(value[2:])
    return value.encode("ascii")


class STLROCEv2GPU(object):
    """
    Synthetic RoCEv2 RC workload profile for GPU-fabric validation.

    Phase 1 intentionally generates packet streams only. It models QP/GID/IP
    distribution, BTH, RETH/AETH, PSN, DSCP/ECN, VLAN, payload sizing, and ICRC.
    IPv4 GIDs are accepted as IPv4-mapped IPv6 addresses and resolved to the
    IPv4 addresses carried on wire. It does not implement RDMA endpoint state,
    HCA retransmission, completion queues, credits, or NCCL collective behavior.
    """

    def _add_ip(self, address, step, index):
        return str(ipaddress.ip_address(address) + (step * index))

    def _ip_from_gid(self, gid, ip_version):
        gid_addr = ipaddress.ip_address(gid)
        if ip_version == 4:
            if not isinstance(gid_addr, ipaddress.IPv6Address) or gid_addr.ipv4_mapped is None:
                raise ValueError("IPv4 RoCEv2 GID must be an IPv4-mapped IPv6 address")
            return str(gid_addr.ipv4_mapped)

        if not isinstance(gid_addr, ipaddress.IPv6Address):
            raise ValueError("IPv6 RoCEv2 GID must be an IPv6 address")
        return str(gid_addr)

    def _resolve_ip_from_gid(self, gid, ip_addr, ip_version, field_name):
        if gid is None:
            return ip_addr

        resolved_ip = self._ip_from_gid(gid, ip_version)
        if ip_addr is not None and str(ipaddress.ip_address(ip_addr)) != resolved_ip:
            raise ValueError("{0}-gid resolves to a different address than {0}-ip".format(field_name))
        return resolved_ip

    def _swap_if_needed(self, args, direction, left, right):
        if direction == 1 and not args.no_direction_swap:
            return right, left
        return left, right

    def _make_l2_l3_l4(self, args, qp_index, direction):
        src_mac, dst_mac = self._swap_if_needed(args, direction, args.src_mac, args.dst_mac)
        src_ip = self._add_ip(args.src_ip, args.src_ip_step, qp_index)
        dst_ip = self._add_ip(args.dst_ip, args.dst_ip_step, qp_index)
        src_ip, dst_ip = self._swap_if_needed(args, direction, src_ip, dst_ip)

        tos = (args.dscp << 2) | args.ecn
        pkt = Ether(src=src_mac, dst=dst_mac)
        if args.vlan is not None:
            pkt /= Dot1Q(prio=args.pcp, vlan=args.vlan)

        if args.ip_version == 4:
            pkt /= IP(src=src_ip, dst=dst_ip, tos=tos, ttl=args.ttl)
        else:
            pkt /= IPv6(src=src_ip, dst=dst_ip, tc=tos, hlim=args.ttl)

        udp_sport = args.udp_sport + (args.udp_sport_step * qp_index)
        pkt /= UDP(sport=udp_sport, dport=ROCEV2_UDP_PORT)
        return pkt

    def _make_payload(self, args, payload_len):
        if payload_len <= 0:
            return b""
        pattern = parse_payload_pattern(args.payload_pattern)
        if not pattern:
            raise argparse.ArgumentTypeError("payload pattern must not be empty")
        repeats = (payload_len + len(pattern) - 1) // len(pattern)
        return (pattern * repeats)[:payload_len]

    def _make_ext_headers(self, args, ext, dma_len, va_offset, qp_index):
        if ext == "reth":
            return RETH(va=args.virtual_address + va_offset, rkey=args.rkey, dma_len=dma_len)
        if ext == "aeth":
            return AETH(syndrome=args.aeth_syndrome, msn=(args.msn + qp_index) & 0xffffff)
        return None

    def _make_packet(self, args, qp_index, direction, spec):
        qp = args.qp_base + qp_index
        psn = (args.psn + (qp_index * args.psn_step) + spec["psn_delta"]) & 0xffffff
        payload_len = spec["payload_len"]
        pad_count = (-payload_len) % 4

        bth = BTH(
            opcode=opcode("RC", spec["opcode"])[0],
            padcount=pad_count,
            pkey=args.pkey,
            fecn=int(args.fecn),
            becn=int(args.becn),
            dqpn=qp,
            ackreq=int(args.ackreq),
            psn=psn,
        )

        pkt = self._make_l2_l3_l4(args, qp_index, direction) / bth
        ext = self._make_ext_headers(args, spec["ext"], spec["dma_len"], spec["va_offset"], qp_index)
        if ext is not None:
            pkt /= ext

        if payload_len or pad_count:
            pkt /= Raw(self._make_payload(args, payload_len) + (b"\x00" * pad_count))

        return pkt

    def _max_payload_for(self, args, direction, opcode_name, ext):
        spec = {
            "opcode": opcode_name,
            "ext": ext,
            "payload_len": 0,
            "dma_len": args.message_size,
            "va_offset": 0,
            "psn_delta": 0,
        }
        header_len = len(raw(self._make_packet(args, 0, direction, spec)))
        payload_room = args.mtu - ETH_FCS_LEN - header_len
        return max(0, (payload_room // 4) * 4)

    def _write_specs(self, args, direction):
        only_cap = self._max_payload_for(args, direction, "RDMA_WRITE_ONLY", "reth")
        if args.message_size <= only_cap:
            return [{
                "opcode": "RDMA_WRITE_ONLY",
                "ext": "reth",
                "payload_len": args.message_size,
                "dma_len": args.message_size,
                "va_offset": 0,
                "psn_delta": 0,
            }]

        first_cap = self._max_payload_for(args, direction, "RDMA_WRITE_FIRST", "reth")
        mid_cap = self._max_payload_for(args, direction, "RDMA_WRITE_MIDDLE", None)
        last_cap = self._max_payload_for(args, direction, "RDMA_WRITE_LAST", None)
        if not first_cap or not mid_cap or not last_cap:
            raise ValueError("MTU is too small for segmented RoCEv2 write packets")

        specs = [{
            "opcode": "RDMA_WRITE_FIRST",
            "ext": "reth",
            "payload_len": first_cap,
            "dma_len": args.message_size,
            "va_offset": 0,
            "psn_delta": 0,
        }]
        remaining = args.message_size - first_cap
        va_offset = first_cap
        psn_delta = 1

        while remaining > last_cap:
            chunk = min(mid_cap, remaining)
            if remaining - chunk == 0:
                chunk = remaining - last_cap
            specs.append({
                "opcode": "RDMA_WRITE_MIDDLE",
                "ext": None,
                "payload_len": chunk,
                "dma_len": 0,
                "va_offset": va_offset,
                "psn_delta": psn_delta,
            })
            remaining -= chunk
            va_offset += chunk
            psn_delta += 1

        specs.append({
            "opcode": "RDMA_WRITE_LAST",
            "ext": None,
            "payload_len": remaining,
            "dma_len": 0,
            "va_offset": va_offset,
            "psn_delta": psn_delta,
        })
        return specs

    def _read_request_specs(self, args, _direction):
        return [{
            "opcode": "RDMA_READ_REQUEST",
            "ext": "reth",
            "payload_len": 0,
            "dma_len": args.message_size,
            "va_offset": 0,
            "psn_delta": 0,
        }]

    def _read_response_specs(self, args, direction):
        only_cap = self._max_payload_for(args, direction, "RDMA_READ_RESPONSE_ONLY", "aeth")
        if args.message_size <= only_cap:
            return [{
                "opcode": "RDMA_READ_RESPONSE_ONLY",
                "ext": "aeth",
                "payload_len": args.message_size,
                "dma_len": 0,
                "va_offset": 0,
                "psn_delta": 0,
            }]

        first_cap = self._max_payload_for(args, direction, "RDMA_READ_RESPONSE_FIRST", "aeth")
        mid_cap = self._max_payload_for(args, direction, "RDMA_READ_RESPONSE_MIDDLE", None)
        last_cap = self._max_payload_for(args, direction, "RDMA_READ_RESPONSE_LAST", "aeth")
        if not first_cap or not mid_cap or not last_cap:
            raise ValueError("MTU is too small for segmented RoCEv2 read response packets")

        specs = [{
            "opcode": "RDMA_READ_RESPONSE_FIRST",
            "ext": "aeth",
            "payload_len": first_cap,
            "dma_len": 0,
            "va_offset": 0,
            "psn_delta": 0,
        }]
        remaining = args.message_size - first_cap
        psn_delta = 1

        while remaining > last_cap:
            chunk = min(mid_cap, remaining)
            if remaining - chunk == 0:
                chunk = remaining - last_cap
            specs.append({
                "opcode": "RDMA_READ_RESPONSE_MIDDLE",
                "ext": None,
                "payload_len": chunk,
                "dma_len": 0,
                "va_offset": 0,
                "psn_delta": psn_delta,
            })
            remaining -= chunk
            psn_delta += 1

        specs.append({
            "opcode": "RDMA_READ_RESPONSE_LAST",
            "ext": "aeth",
            "payload_len": remaining,
            "dma_len": 0,
            "va_offset": 0,
            "psn_delta": psn_delta,
        })
        return specs

    def _operation_specs(self, args, direction):
        if args.operation == "write":
            return [("write", self._write_specs(args, direction))]
        if args.operation == "read-request":
            return [("read_req", self._read_request_specs(args, direction))]
        if args.operation == "read-response":
            return [("read_rsp", self._read_response_specs(args, direction))]
        return [
            ("read_req", self._read_request_specs(args, direction)),
            ("read_rsp", self._read_response_specs(args, direction)),
        ]

    def _stream_mode(self, args):
        if args.stream_mode == "continuous":
            return STLTXCont(pps=args.pps)
        return STLTXSingleBurst(pps=args.pps, total_pkts=args.total_pkts)

    def _make_vm(self, args):
        l3_layer = "IP" if args.ip_version == 4 else "IPv6"
        return STLScVmRaw([
            # ICRC changes after flow-stat IP marking or VM writes. UDP covers
            # the ICRC trailer, so finish its checksum at transmission time.
            STLVmFixChecksumHw(l3_offset=l3_layer, l4_offset="UDP",
                              l4_type=CTRexVmInsFixHwCs.L4_TYPE_UDP),
            STLVmFixRoceV2Icrc(l3_offset=l3_layer, l4_offset="UDP", bth_offset="BTH"),
        ])

    def _validate_args(self, args):
        if args.ip_version == 4:
            ipaddress.IPv4Address(args.src_ip)
            ipaddress.IPv4Address(args.dst_ip)
        else:
            ipaddress.IPv6Address(args.src_ip)
            ipaddress.IPv6Address(args.dst_ip)

        if not (0 <= args.qp_base <= 0xffffff):
            raise ValueError("qp-base must be in the 24-bit BTH QP range")
        if args.qps < 1:
            raise ValueError("qps must be at least 1")
        if args.qp_base + args.qps - 1 > 0xffffff:
            raise ValueError("qp-base + qps exceeds the 24-bit BTH QP range")
        if not (0 <= args.psn <= 0xffffff):
            raise ValueError("psn must be in the 24-bit BTH PSN range")
        if not (0 <= args.dscp <= 63):
            raise ValueError("dscp must be in range 0..63")
        if not (0 <= args.ecn <= 3):
            raise ValueError("ecn must be in range 0..3")
        if not (0 <= args.pkey <= 0xffff):
            raise ValueError("pkey must be in range 0..0xffff")
        if args.vlan is not None and not (0 <= args.vlan <= 4094):
            raise ValueError("vlan must be in range 0..4094 (4095 is reserved)")
        if not (0 <= args.pcp <= 7):
            raise ValueError("pcp must be in range 0..7")
        max_udp_sport = args.udp_sport + (args.udp_sport_step * (args.qps - 1))
        if not (0 <= args.udp_sport <= 65535 and 0 <= max_udp_sport <= 65535):
            raise ValueError("udp-sport and udp-sport-step must stay in range 0..65535")
        if args.message_size < 1:
            raise ValueError("message-size must be at least 1")
        if not (64 <= args.mtu <= 9216):
            raise ValueError("mtu must be in range 64..9216, including Ethernet FCS")
        for name, maximum in (("ttl", 0xff), ("aeth_syndrome", 0xff),
                              ("msn", 0xffffff), ("psn_step", 0xffffff),
                              ("virtual_address", 0xffffffffffffffff), ("rkey", 0xffffffff)):
            if not (0 <= getattr(args, name) <= maximum):
                raise ValueError("{0} is outside its wire field range".format(name.replace("_", "-")))
        if not math.isfinite(args.pps) or args.pps <= 0:
            raise ValueError("pps must be finite and positive")
        if args.total_pkts < 1:
            raise ValueError("total-pkts must be at least 1")
        if not parse_payload_pattern(args.payload_pattern):
            raise ValueError("payload-pattern must not be empty")
        if not (0 <= args.pg_id_base <= 0xffffffff):
            raise ValueError("pg-id-base must be in the uint32 range")
        if args.flow_stats and (args.qps > 4095 or args.pg_id_base + args.qps - 1 > 0xffffffff):
            raise ValueError("flow stats require at most 4095 QPs with uint32 PGIDs")

    def _parse_args(self, tunables):
        parser = argparse.ArgumentParser(
            description="RoCEv2 RC GPU workload profile",
            formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        )
        parser.add_argument("--operation", choices=["write", "read-request", "read-response", "read"], default="write")
        parser.add_argument("--ip-version", type=int, choices=[4, 6], default=4)
        parser.add_argument("--src-gid", default=None)
        parser.add_argument("--dst-gid", default=None)
        parser.add_argument("--src-ip", default=None)
        parser.add_argument("--dst-ip", default=None)
        parser.add_argument("--src-ip-step", type=parse_int, default=0)
        parser.add_argument("--dst-ip-step", type=parse_int, default=0)
        parser.add_argument("--src-mac", default="00:00:00:00:01:00")
        parser.add_argument("--dst-mac", default="00:00:00:00:02:00")
        parser.add_argument("--no-direction-swap", action="store_true")
        parser.add_argument("--vlan", type=parse_int, default=None)
        parser.add_argument("--pcp", type=parse_int, default=3)
        parser.add_argument("--dscp", type=parse_int, default=26)
        parser.add_argument("--ecn", type=parse_int, default=1)
        parser.add_argument("--ttl", type=parse_int, default=64)
        parser.add_argument("--udp-sport", type=parse_int, default=49152)
        parser.add_argument("--udp-sport-step", type=parse_int, default=1)
        parser.add_argument("--qp-base", type=parse_int, default=0x100)
        parser.add_argument("--qps", type=parse_int, default=1)
        parser.add_argument("--psn", type=parse_int, default=0)
        parser.add_argument("--psn-step", type=parse_int, default=1024)
        parser.add_argument("--pkey", type=parse_int, default=0xffff)
        parser.add_argument("--ackreq", action="store_true")
        parser.add_argument("--fecn", action="store_true")
        parser.add_argument("--becn", action="store_true")
        parser.add_argument("--virtual-address", type=parse_int, default=0x100000000)
        parser.add_argument("--rkey", type=parse_int, default=0x10101010)
        parser.add_argument("--message-size", type=parse_int, default=4096)
        parser.add_argument("--mtu", type=parse_int, default=9000)
        parser.add_argument("--payload-pattern", default="GPU")
        parser.add_argument("--aeth-syndrome", type=parse_int, default=0)
        parser.add_argument("--msn", type=parse_int, default=0)
        parser.add_argument("--stream-mode", choices=["continuous", "burst", "chain"], default="continuous")
        parser.add_argument("--pps", type=float, default=1.0)
        parser.add_argument("--total-pkts", type=parse_int, default=1)
        parser.add_argument("--flow-stats", action="store_true", help="Enable basic packet/byte counters per QP")
        parser.add_argument("--pg-id-base", type=parse_int, default=0, help="First QP's packet group ID")
        args = parser.parse_args(tunables)

        args.src_ip = self._resolve_ip_from_gid(args.src_gid, args.src_ip, args.ip_version, "src")
        args.dst_ip = self._resolve_ip_from_gid(args.dst_gid, args.dst_ip, args.ip_version, "dst")

        if args.src_ip is None:
            args.src_ip = IPV4_DEFAULT_SRC if args.ip_version == 4 else IPV6_DEFAULT_SRC
        if args.dst_ip is None:
            args.dst_ip = IPV4_DEFAULT_DST if args.ip_version == 4 else IPV6_DEFAULT_DST
        self._validate_args(args)
        return args

    def _build_stream_defs(self, args, direction):
        stream_defs = []
        for operation_name, specs in self._operation_specs(args, direction):
            for qp_index in range(args.qps):
                qp = args.qp_base + qp_index
                for seg_index, spec in enumerate(specs):
                    pkt = self._make_packet(args, qp_index, direction, spec)
                    if len(raw(pkt)) + ETH_FCS_LEN > args.mtu:
                        raise ValueError("MTU is too small for the RoCEv2 headers and payload")
                    stream_defs.append({
                        "name": "rocev2_{0}_qp{1:x}_seg{2}".format(operation_name, qp, seg_index),
                        "packet": pkt,
                        "qp_index": qp_index,
                    })
        return stream_defs

    def get_streams(self, direction=0, port_id=0, tunables=None, **kwargs):
        args = self._parse_args(tunables or [])
        stream_defs = self._build_stream_defs(args, direction)

        streams = []
        for idx, stream_def in enumerate(stream_defs):
            next_stream = None
            self_start = True
            if args.stream_mode == "chain":
                next_stream = stream_defs[idx + 1]["name"] if idx + 1 < len(stream_defs) else None
                self_start = idx == 0

            streams.append(
                STLStream(
                    name=stream_def["name"],
                    self_start=self_start,
                    packet=STLPktBuilder(pkt=stream_def["packet"], vm=self._make_vm(args)),
                    mode=self._stream_mode(args),
                    next=next_stream,
                    flow_stats=STLFlowStats(pg_id=args.pg_id_base + stream_def["qp_index"]) if args.flow_stats else None,
                )
            )
        return streams


def register():
    return STLROCEv2GPU()
