#!/usr/bin/env python3
"""Build and exercise two native RC processes over isolated Ethernet or UDP.

The AF_PACKET path creates a veth pair inside a new user/network namespace.
It needs neither sudo nor lab ports when unprivileged user namespaces are
available. UDP carries complete Ethernet frames when raw sockets are absent.
This is functional endpoint acceptance, not DPDK integration or a rate test.

Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0.
"""

import argparse
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import socket
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[4]
SCENARIOS = ("none", "drop", "corrupt", "malformed", "blackhole")


def build(directory):
    directory.mkdir(parents=True, exist_ok=True)
    binary = directory / "trex-rocev2-wire"
    native = ROOT / "src/stx/stl"
    sources = [ROOT / "src/sim/trex_sim_rocev2.cpp"]
    sources += [native / ("trex_rocev2_native_" + name + ".cpp")
                for name in ("types", "codec", "qp", "transport", "engine")]
    sources.append(native / "trex_rocev2_icrc.cpp")
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++11", "-O2",
                    "-Wall", "-Wextra", "-I", str(native),
                    *map(str, sources), "-o", str(binary)], check=True)
    return binary


def available_namespace():
    if not shutil.which("unshare") or not shutil.which("ip"):
        return False
    probe = subprocess.run(["unshare", "--user", "--map-root-user", "--net",
                            "sh", "-c", "ip link add rvprobe0 type veth peer name rvprobe1"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return probe.returncode == 0


def create_veth():
    subprocess.run(["ip", "link", "add", "rvleft", "type", "veth", "peer", "name", "rvright"],
                   check=True)
    for interface, mac in (("rvleft", "02:00:00:00:00:0a"),
                           ("rvright", "02:00:00:00:00:14")):
        subprocess.run(["ip", "link", "set", "dev", interface,
                        "address", mac, "mtu", "9216", "up"], check=True)
    subprocess.run(["ip", "link", "set", "lo", "up"], check=True)


def ready(process):
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        if not selector.select(timeout=5):
            raise RuntimeError("responder did not become ready")
        line = process.stdout.readline()
        if not line or not json.loads(line).get("ready"):
            raise RuntimeError("responder initialization failed: " + line)


def result(stdout):
    reports = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
    reports = [report for report in reports if "success" in report]
    if len(reports) != 1:
        raise RuntimeError("endpoint did not produce one result: " + stdout)
    return reports[0]


def scenario(binary, backend, fault, payload_bytes):
    common = [str(binary), "--fault", fault, "--bytes", str(payload_bytes),
              "--deadline-ms", "5000"]
    sockets = []
    if backend == "af-packet":
        responder_args = common + ["--interface", "rvright"]
        requester_args = common + ["--interface", "rvleft", "--requester"]
    else:
        # Reserve distinct ports together before starting the two processes.
        for _ in range(2):
            descriptor = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            descriptor.bind(("127.0.0.1", 0))
            sockets.append(descriptor)
        left_port, right_port = [descriptor.getsockname()[1] for descriptor in sockets]
        responder_args = common + ["--local-port", str(right_port), "--peer-port", str(left_port)]
        requester_args = common + ["--local-port", str(left_port), "--peer-port", str(right_port),
                                   "--requester"]
        for descriptor in sockets:
            descriptor.close()
    responder = subprocess.Popen(responder_args, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, universal_newlines=True)
    try:
        ready(responder)
        requester = subprocess.run(requester_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   universal_newlines=True, timeout=8)
        responder.send_signal(signal.SIGTERM)
        responder_stdout, responder_stderr = responder.communicate(timeout=3)
        if requester.returncode or responder.returncode:
            raise RuntimeError("endpoint process failed\nrequester: " + requester.stdout +
                               requester.stderr + "\nresponder: " + responder_stdout + responder_stderr)
        left = result(requester.stdout)
        right = result(responder_stdout)
        if not left["success"] or not right["success"]:
            raise RuntimeError("endpoint reported failure")
        if fault == "blackhole":
            assert left["completions"] == 0 and left["dropped_frames"] > 0
            assert right["rx_frames"] == 0 and not right["remote_payload_verified"]
        else:
            assert left["completions"] == 2 and right["remote_payload_verified"]
            assert right["read_requests_received"] > 0
            assert left["tx_frames"] > 2 and right["tx_frames"] > 2  # segmented both ways
        if fault in ("drop", "corrupt", "malformed"):
            assert left["retransmit_packets"] > 0
        if fault in ("corrupt", "malformed"):
            assert left["modified_frames"] == 1 and right["malformed_frames"] > 0
        if fault == "corrupt":
            assert right["icrc_errors"] > 0
        return {"scenario": fault, "backend": backend, "requester": left, "responder": right}
    finally:
        if responder.poll() is None:
            responder.kill()
            responder.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, help="use a previously built exerciser")
    parser.add_argument("--build-dir", type=Path,
                        default=ROOT / ".dev/rocev2-wire")
    parser.add_argument("--backend", choices=("auto", "af-packet", "udp"), default="auto")
    parser.add_argument("--scenario", choices=SCENARIOS, action="append")
    parser.add_argument("--bytes", type=int, default=65549)
    parser.add_argument("--output", type=Path, help="write the full JSON evidence report")
    parser.add_argument("--in-namespace", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.bytes <= 1024 or args.bytes > 256 * 1024:
        parser.error("--bytes must be 1025..262144 (256 frames at PMTU 1024)")
    binary = args.binary.resolve() if args.binary else build(args.build_dir.resolve())
    backend = args.backend
    if not args.in_namespace and backend != "udp":
        if available_namespace():
            command = ["unshare", "--user", "--map-root-user", "--net", sys.executable,
                       str(Path(__file__).resolve()), "--binary", str(binary),
                       "--backend", "af-packet", "--in-namespace", "--bytes", str(args.bytes)]
            for fault in args.scenario or []:
                command += ["--scenario", fault]
            if args.output:
                command += ["--output", str(args.output.resolve())]
            return subprocess.run(command).returncode
        if backend == "af-packet":
            parser.error("AF_PACKET requires unprivileged user/network namespaces and iproute2")
        backend = "udp"
        print("AF_PACKET namespace unavailable; using framed UDP carrier.", file=sys.stderr)
    if args.in_namespace:
        create_veth()
    reports = []
    for fault in args.scenario or SCENARIOS:
        report = scenario(binary, backend, fault, args.bytes)
        reports.append(report)
        print("PASS {} {}: {} payload bytes, {} retransmit packets".format(
            backend, fault, args.bytes, report["requester"]["retransmit_packets"]), flush=True)
    evidence = {"scope": "two-process native RC functional wire acceptance",
                "performance_test": False, "results": reports}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(evidence, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
