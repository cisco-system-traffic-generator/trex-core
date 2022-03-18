#!/usr/bin/env python3
# vim: tw=88 et sts=4 ts=4 sw=4

"""
Connect to a TRex server and send TCP/UDP traffic at varying rates of connections per
second following a binary search algorithm to find a "non-drop rate". The traffic is
only stopped in between rate changes when there were transmission errors to avoid
existing flows from piling up.
"""

import argparse
import json
import pathlib
import sys
import time

# ugly
here = pathlib.Path(__file__).parent
sys.path.insert(0, str(here / "external_libs/texttable-0.8.4"))
sys.path.insert(0, str(here / "external_libs/pyyaml-3.11/python3"))
sys.path.insert(0, str(here / "external_libs/scapy-2.4.3"))
sys.path.insert(0, str(here / "external_libs/pyzmq-ctypes"))
sys.path.insert(0, str(here / "external_libs/simpy-3.0.10"))
sys.path.insert(0, str(here / "external_libs/trex-openssl"))
sys.path.insert(0, str(here / "external_libs/dpkt-1.9.1"))
sys.path.insert(0, str(here / "external_libs/repoze"))
sys.path.insert(0, str(here / "automation/trex_control_plane/interactive"))

from trex.astf.trex_astf_client import ASTFClient
from trex.astf.trex_astf_profile import (
    ASTFAssociationRule,
    ASTFGlobalInfo,
    ASTFIPGen,
    ASTFIPGenDist,
    ASTFIPGenGlobal,
    ASTFProfile,
    ASTFProgram,
    ASTFTCPClientTemplate,
    ASTFTCPServerTemplate,
    ASTFTemplate,
)


MULTIPLIER = 100  # min number of cps


# --------------------------------------------------------------------------------------
def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__.strip(),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-i",
        "--max-iterations",
        type=int,
        help="""
        Max number of iterations. By default, the script will run until interrupted.
        """,
    )
    parser.add_argument(
        "-o",
        "--output",
        metavar="FILE",
        help="""
        Write last no-error stats to file in the JSON format.
        """,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="""
        Be more verbose.
        """,
    )
    g = parser.add_argument_group("trex server options")
    g.add_argument(
        "-S",
        "--server",
        default="localhost",
        help="""
        TRex server name or IP.
        """,
    )
    g.add_argument(
        "-p",
        "--sync-port",
        type=ranged_int(1, 65535),
        default=4501,
        help="""
        RPC port number.
        """,
    )
    g.add_argument(
        "-P",
        "--async-port",
        type=ranged_int(1, 65535),
        default=4500,
        help="""
        Subscriber port number.
        """,
    )
    g.add_argument(
        "-t",
        "--timeout",
        type=int,
        default=5,
        help="""
        TRex connection timeout.
        """,
    )
    g = parser.add_argument_group("traffic profile options")
    g.add_argument(
        "-u",
        "--udp-percent",
        type=ranged_float(0, 100, 1),
        default=0.0,
        help="""
        Percentage of UDP connections.
        """,
    )
    g.add_argument(
        "-n",
        "--num-messages",
        type=ranged_int(1, 10_000),
        default=1,
        help="""
        Number of data messages (request+response) exchanged per connection.
        For UDP, this is the total number of messages per connection.
        TCP has extra protocol overhead packets.
        """,
    )
    g.add_argument(
        "-s",
        "--message-size",
        type=ranged_int(20, 1456),
        default=20,
        help="""
        Size of data messages in bytes.
        """,
    )
    g.add_argument(
        "-w",
        "--server-wait",
        type=ranged_int(0, 500),
        default=0,
        help="""
        Time in ms that the server waits before sending a response to a request.
        """,
    )
    g = parser.add_argument_group("rate options")
    g.add_argument(
        "-m",
        "--min-cps",
        type=ranged_float(MULTIPLIER, 1_000_000),
        default=10_000,
        help="""
        Min number of connections created&destroyed per second.
        Human readable values are accepted (e.g. 2.4k).
        """,
    )
    g.add_argument(
        "-M",
        "--max-cps",
        type=ranged_float(MULTIPLIER, 2_000_000),
        default=500_000,
        help="""
        Max number of connections created&destroyed per second.
        Human readable values are accepted (e.g. 1.2m).
        """,
    )
    g.add_argument(
        "-f",
        "--max-flows",
        type=ranged_float(MULTIPLIER, 4_000_000),
        help="""
        Max number of concurrent active flows.
        Human readable values are accepted (e.g. 4.5m).
        """,
    )
    g.add_argument(
        "-a",
        "--adaptive-window",
        type=ranged_float(0, 100),
        default=0,
        help="""
        Allowed grow percentage on the binary search window after 3 consecutive "good"
        or "bad" iterations. By default, the binary search window can only be shrinking
        at each iteration. If this parameter is non-zero, the upper or lower bounds can
        be extended by ADAPTIVE_WINDOW percent. This allows automatic optimal range
        finding but causes the result to never converge. Use this only for development.
        """,
    )
    g.add_argument(
        "-r",
        "--ramp-up",
        type=ranged_int(3, 20),
        default=3,
        help="""
        Time in seconds before TRex reaches a stable number of connections per second.
        """,
    )
    return parser.parse_args()


# --------------------------------------------------------------------------------------
def ranged_int(min_val: int, max_val: int):
    def type_callback(arg: str) -> int:
        try:
            value = parse_human_readable(arg)
        except ValueError as e:
            raise argparse.ArgumentTypeError(str(e))
        if value < min_val or value > max_val:
            raise argparse.ArgumentTypeError(
                f"value out of bounds [{min_val}, {max_val}]: {arg!r}"
            )
        return int(value)

    return type_callback


# --------------------------------------------------------------------------------------
def ranged_float(min_val: float, max_val: float, precision: int = 1):
    def type_callback(arg: str) -> float:
        try:
            value = parse_human_readable(arg)
        except ValueError as e:
            raise argparse.ArgumentTypeError(str(e))
        if value < min_val or value > max_val:
            raise argparse.ArgumentTypeError(
                f"value out of bounds [{min_val}, {max_val}]: {arg!r}"
            )
        return round(value, precision)

    return type_callback


# --------------------------------------------------------------------------------------
def human_readable(value: float) -> str:
    units = ("K", "M", "G")
    i = 0
    unit = ""
    while value >= 1000 and i < len(units):
        unit = units[i]
        value /= 1000
        i += 1
    if unit == "":
        return str(int(value))
    if value < 100:
        return f"{value:.1f}{unit}"
    return f"{value:.0f}{unit}"


# --------------------------------------------------------------------------------------
def parse_human_readable(value: str) -> float:
    value = value.strip()
    units = {
        "K": 1_000,
        "M": 1_000_000,
        "G": 1_000_000_000,
    }
    if value[-1].upper() in units:
        mult = units[value[-1].upper()]
        value = value[:-1]
    else:
        mult = 1
    return float(value) * mult


# --------------------------------------------------------------------------------------
def generate_traffic_profile(args: argparse.Namespace) -> ASTFProfile:
    ip_gen = ASTFIPGen(
        glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
        dist_client=ASTFIPGenDist(
            ip_range=["16.0.0.0", "16.0.255.255"], distribution="seq"
        ),
        dist_server=ASTFIPGenDist(
            ip_range=["48.0.0.0", "48.0.0.255"], distribution="seq"
        ),
    )

    templates = []

    udp_ratio = args.udp_percent / 100

    if udp_ratio > 0:
        client = ASTFProgram(stream=False)
        server = ASTFProgram(stream=False)
        for _ in range(args.num_messages):
            client.send_msg(args.message_size * b"x")
            server.recv_msg(1)
            if args.server_wait > 0:
                server.delay(args.server_wait * 1000)  # trex wants microseconds
            server.send_msg(args.message_size * b"y")
            client.recv_msg(1)

        if args.max_flows is not None:
            limit = max(1, int(udp_ratio * args.max_flows))
        else:
            limit = None

        templates.append(
            ASTFTemplate(
                client_template=ASTFTCPClientTemplate(
                    program=client,
                    ip_gen=ip_gen,
                    port=5353,
                    cps=max(1, udp_ratio * MULTIPLIER),
                    limit=limit,
                    cont=True,
                ),
                server_template=ASTFTCPServerTemplate(
                    program=server, assoc=ASTFAssociationRule(port=5353)
                ),
            )
        )

    tcp_ratio = 1 - udp_ratio

    if tcp_ratio > 0:
        client = ASTFProgram(stream=True)
        server = ASTFProgram(stream=True)
        for _ in range(args.num_messages):
            client.send(args.message_size * b"x")
            server.recv(args.message_size)
            if args.server_wait > 0:
                server.delay(args.server_wait * 1000)  # trex wants microseconds
            server.send(args.message_size * b"y")
            client.recv(args.message_size)

        if args.max_flows is not None:
            limit = max(1, int(tcp_ratio * args.max_flows))
        else:
            limit = None

        templates.append(
            ASTFTemplate(
                client_template=ASTFTCPClientTemplate(
                    program=client,
                    ip_gen=ip_gen,
                    port=8080,
                    cps=max(1, tcp_ratio * MULTIPLIER),
                    limit=limit,
                    cont=True,
                ),
                server_template=ASTFTCPServerTemplate(
                    program=server, assoc=ASTFAssociationRule(port=8080)
                ),
            )
        )

    info = ASTFGlobalInfo()
    info.scheduler.rampup_sec = args.ramp_up

    return ASTFProfile(
        default_ip_gen=ip_gen,
        default_c_glob_info=info,
        templates=templates,
    )


# --------------------------------------------------------------------------------------
def red(s: str) -> str:
    if not sys.stdout.isatty():
        return s
    return f"\x1b[31m{s}\x1b[0m"


# --------------------------------------------------------------------------------------
def green(s: str) -> str:
    if not sys.stdout.isatty():
        return s
    return f"\x1b[32m{s}\x1b[0m"


# --------------------------------------------------------------------------------------
def main():
    args = parse_args()

    params = vars(args).copy()
    for k, v in list(params.items()):
        if k in (
            "verbose",
            "output",
            "server",
            "timeout",
            "sync_port",
            "async_port",
        ):
            del params[k]
        elif v is None:
            del params[k]

    if args.verbose:
        debug = print
    else:
        debug = lambda *x: None

    debug("...", *(f"{k}={v}" for k, v in params.items()), "...")

    trex = ASTFClient(
        server=args.server,
        sync_port=args.sync_port,
        sync_timeout=args.timeout,
        async_port=args.async_port,
        async_timeout=args.timeout,
    )
    good_stats = {}
    try:
        debug("... connecting to trex ...")
        trex.connect()
        debug("... reseting trex ...")
        trex.reset()
        debug("... loading traffic profile ...")
        trex.load_profile(generate_traffic_profile(args))

        lower_mult = args.min_cps / MULTIPLIER
        upper_mult = args.max_cps / MULTIPLIER
        mult = (upper_mult + lower_mult) / 2  # start half way
        score = 0
        adaptive_ratio = args.adaptive_window / 100

        debug("... starting traffic ...")
        trex.start(mult)

        iterations = 0
        while True:
            debug("... waiting until tx rate is stable ...")
            stats = trex.get_stats()
            pps = stats["global"]["tx_pps"] or 1
            for _ in range(10 * args.ramp_up):
                time.sleep(1)
                stats = trex.get_stats()
                cur_pps = stats["global"]["tx_pps"] or 1
                if abs(cur_pps - pps) / pps < 0.01:
                    # less than 1% rate difference since last second
                    break
                pps = cur_pps

            # gather statistics over 1 second
            trex.clear_stats()
            time.sleep(1)
            stats = trex.get_stats()
            err_flag, err_names = trex.is_traffic_stats_error(stats["traffic"])

            cps = human_readable(stats["global"]["tx_cps"])
            flows = human_readable(stats["global"]["active_flows"])

            tx_pps = human_readable(stats["global"]["tx_pps"])
            tx_bps = human_readable(stats["global"]["tx_bps"])
            rx_pps = human_readable(stats["global"]["rx_pps"])
            rx_bps = human_readable(stats["global"]["rx_bps"])
            size = stats["traffic"].get("client", {}).get("m_avg_size", 0)

            errors = []

            if err_flag:
                sign = red("▼▼▼")
                score = min(score, 0) - 1
                upper_mult = mult
                mult = (lower_mult + mult) / 2
                if score < -3:
                    lower_mult *= 1 - adaptive_ratio
                for sect, names in err_names.items():
                    for n, name in names.items():
                        s = human_readable(stats["traffic"][sect][n])
                        errors.append(f"{sect}: {name} = {s}")
            else:
                sign = green("▲▲▲")
                score = max(score, 0) + 1
                lower_mult = mult
                mult = (upper_mult + mult) / 2
                if score > 3:
                    upper_mult *= 1 + adaptive_ratio
                good_stats = stats

            print(
                sign,
                f"Flows: active {flows} ({cps}/s)",
                f"TX: {tx_bps}b/s ({tx_pps}p/s)",
                f"RX: {rx_bps}b/s ({rx_pps}p/s)",
                f"Size: ~{size:.1f}B",
            )

            pkt_drop = stats["total"]["opackets"] - stats["total"]["ipackets"]
            if errors and pkt_drop > 0:
                print(
                    red("err"),
                    "dropped:",
                    human_readable(pkt_drop),
                    "pkts",
                    f"({human_readable(pkt_drop)}/s)",
                )

            for err in errors:
                debug(red("err"), err)

            iterations += 1
            if args.max_iterations is not None and iterations >= args.max_iterations:
                break

            if errors:
                # traffic must be stopped to avoid existing connections to
                # retransmit the dropped packets.
                debug("... resetting connections ...")
                trex.stop()
                time.sleep(args.ramp_up)
                trex.start(mult)
            else:
                trex.update(mult)

    except KeyboardInterrupt:
        pass

    finally:
        debug("... disconnecting from trex ...")
        trex.disconnect()
        if args.output:
            debug(f"... dumping results to {args.output} ...")
            good_stats["conntrack_ndr_profile"] = params
            with open(args.output, "w", encoding="utf-8") as f:
                json.dump(good_stats, f, indent=2)


# --------------------------------------------------------------------------------------
if __name__ == "__main__":
    main()
