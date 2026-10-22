#!/usr/bin/env python3
"""Configure reciprocal RoCEv2 devices on connected physical-port pairs.

The factory uses deterministic demonstration MAC/IP addresses. Configure the
DUT to forward those addresses, or build ASTFRoceV2Host/Device objects with
your topology's addresses. --dry-run prints the complete profile without
connecting to a server or acquiring ports.

Copyright (c) 2026 Cisco Systems, Inc. Licensed under the Apache License, Version 2.0.
"""

import argparse
import json
from pathlib import Path

try:
    from . import astf_path
except ImportError:
    import astf_path
from trex.astf.api import (ASTFClient, ASTFRoceV2Device, ASTFRoceV2Host,
                           ASTFRoceV2Scale, TRexError)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', default='127.0.0.1')
    parser.add_argument('--ports', nargs='+', type=int, default=[0, 1])
    parser.add_argument('--devices-per-port', type=int)
    parser.add_argument('--qps-per-device', type=int)
    parser.add_argument('--ip-version', choices=('ipv4', 'ipv6', 'dual'), default='ipv4')
    parser.add_argument('--message-size', type=int, default=4096)
    parser.add_argument('--outstanding', type=int, default=1)
    parser.add_argument('--receive-depth', type=int, default=0)
    parser.add_argument('--max-memory-bytes', type=int, help='registered-memory budget; device default 128 MiB')
    parser.add_argument('--max-native-storage-bytes', type=int, default=512 * 1024 * 1024)
    parser.add_argument('--iterations', type=int, default=1)
    parser.add_argument('--duration', type=float, default=10)
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--settings', type=Path, help='JSON scale settings; explicit CLI options override these')
    parser.add_argument('--capture', type=Path, help='save a RoCEv2 PCAP')
    parser.add_argument('--capture-device', type=int, help='restrict capture to one device ID')
    parser.add_argument('--force', action='store_true', help='take ownership from another ASTF client')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    if args.capture_device is not None and args.capture is None:
        parser.error('--capture-device requires --capture')
    settings = json.loads(args.settings.read_text()) if args.settings else {}
    if not isinstance(settings, dict) or set(settings) - {'devices_per_port', 'qps_per_device'}:
        parser.error('settings must be an object containing devices_per_port and/or qps_per_device')
    for name in ('devices_per_port', 'qps_per_device'):
        if getattr(args, name) is not None:
            settings[name] = getattr(args, name)
    scale = ASTFRoceV2Scale(**settings)
    profile = scale.paired_profile(ports=args.ports, ip_version=args.ip_version,
                                   message_size=args.message_size, outstanding=args.outstanding,
                                   receive_depth=args.receive_depth, iterations=args.iterations,
                                   duration=args.duration, timeout_exp=20,
                                   max_memory_bytes=args.max_memory_bytes,
                                   max_native_storage_bytes=args.max_native_storage_bytes)
    if args.dry_run:
        print(json.dumps(profile.to_json(), indent=2))
        return 0
    capture_ports, capture_filter = args.ports, 'udp dst port 4791'
    if args.capture_device is not None:
        selected = next((device for device in profile.to_json()['devices']
                         if device['device_id'] == args.capture_device), None)
        if selected is None:
            parser.error('--capture-device is absent from the generated profile')
        device = ASTFRoceV2Device(selected['device_id'],
                                  ASTFRoceV2Host(port=selected['port'], **selected['host']),
                                  node_guid=selected['node_guid'], gids=selected['gids'])
        capture_ports, capture_filter = [selected['port']], device.capture_filter()
    client = ASTFClient(server=args.server)
    capture = None
    configured = False
    try:
        client.connect()
        client.acquire(force=args.force)
        client.set_port_attr(ports=args.ports, promiscuous=True)
        configuration = client.configure_rocev2(profile)
        configured = True
        if args.capture:
            capture = client.start_capture(tx_ports=capture_ports, rx_ports=capture_ports,
                                            bpf_filter=capture_filter, limit=10000)
        client.start_rocev2()
        verdict = client.wait_on_rocev2(timeout=args.timeout)
        print(json.dumps(dict(configuration=configuration, validation=verdict,
                              stats=client.get_rocev2_stats()), indent=2))
        return 0 if verdict.get('passed') else 1
    finally:
        try:
            if capture is not None:
                client.stop_capture(capture['id'], output=str(args.capture))
        finally:
            try:
                if configured:
                    client.stop_rocev2()
            finally:
                client.disconnect()


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (TRexError, ValueError, OSError) as error:
        raise SystemExit(str(error))
